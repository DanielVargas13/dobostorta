#include <QtSql>
#include <QtWebEngineWidgets>
#include <QtWidgets>


#define HOMEPAGE    "http://google.com"
#define USER_AGENT  "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)" \
                    "Chrome/55.0.0.0 Safari/537.36 Dobostorta/" GIT_VERSION

#define SHORTCUT_META           (Qt::CTRL)
#define SHORTCUT_FORWARD        {SHORTCUT_META + Qt::Key_I}
#define SHORTCUT_BACK           {SHORTCUT_META + Qt::Key_O}
#define SHORTCUT_RELOAD         {SHORTCUT_META + Qt::Key_R}
#define SHORTCUT_BAR            {SHORTCUT_META + Qt::Key_Colon}
#define SHORTCUT_BAR_ALT        {SHORTCUT_META + Qt::SHIFT + Qt::Key_Colon}
#define SHORTCUT_FIND           {SHORTCUT_META + Qt::Key_Slash}
#define SHORTCUT_ESCAPE         {SHORTCUT_META + Qt::Key_BracketLeft}
#define SHORTCUT_DOWN           {SHORTCUT_META + Qt::Key_J}
#define SHORTCUT_UP             {SHORTCUT_META + Qt::Key_K}
#define SHORTCUT_LEFT           {SHORTCUT_META + Qt::Key_H}
#define SHORTCUT_RIGHT          {SHORTCUT_META + Qt::Key_L}
#define SHORTCUT_TOP            {SHORTCUT_META + Qt::Key_G, SHORTCUT_META + Qt::Key_G}
#define SHORTCUT_BOTTOM         {SHORTCUT_META + Qt::SHIFT + Qt::Key_G}
#define SHORTCUT_NEXT           {SHORTCUT_META + Qt::Key_N}
#define SHORTCUT_PREV           {SHORTCUT_META + Qt::Key_P}
#define SHORTCUT_ZOOMIN         {SHORTCUT_META + Qt::Key_Plus}
#define SHORTCUT_ZOOMIN_ALT     {SHORTCUT_META + Qt::SHIFT + Qt::Key_Plus}
#define SHORTCUT_ZOOMOUT        {SHORTCUT_META + Qt::Key_Minus}
#define SHORTCUT_ZOOMRESET      {SHORTCUT_META + Qt::Key_0}
#define SHORTCUT_NEW_WINDOW     {SHORTCUT_META + Qt::SHIFT + Qt::Key_N}
#define SHORTCUT_NEW_INCOGNITO  {SHORTCUT_META + Qt::SHIFT + Qt::Key_P}

#define SCROLL_STEP_X  20
#define SCROLL_STEP_Y  20
#define ZOOM_STEP      0.1


enum QueryType {
    URLWithScheme,
    URLWithoutScheme,
    SearchWithScheme,
    SearchWithoutScheme,
    InSiteSearch
};

QueryType guessQueryType(const QString &str) {
    if (str.startsWith("search:"))
        return SearchWithScheme;
    else if (str.startsWith("find:"))
        return InSiteSearch;
    else if (QRegExp("^[^/ \t]+((\\.[^/ \t]+)*\\.[^/0-9 \t]+|:[0-9]+)").indexIn(str) != -1)
        return URLWithoutScheme;
    else if (QRegExp("^[a-zA-Z0-9]+:.+").indexIn(str) != -1)
        return URLWithScheme;
    else
        return SearchWithoutScheme;
}


QString expandFilePath(const QString &path) {
    if (path.startsWith("~/"))
        return QFileInfo(QDir::home(), path.right(path.length() - 2)).absoluteFilePath();
    else
        return QFileInfo(QDir::current(), path).absoluteFilePath();
}


class TortaDatabase {
    QSqlDatabase db;
    QSqlQuery add;
    QSqlQuery search;
    QSqlQuery forward;

public:
    TortaDatabase() : db(QSqlDatabase::addDatabase("QSQLITE")), add(db), search(db), forward(db) {
        db.setDatabaseName(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                           + "/history");
        db.open();

        db.exec("CREATE TABLE IF NOT EXISTS history                        \
                   (timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP UNIQUE,  \
                    scheme TEXT NOT NULL, address TEXT NOT NULL)");
        db.exec("CREATE INDEX IF NOT EXISTS history_index ON history(timestamp);");

        add.prepare("INSERT INTO history (scheme, address) VALUES (:scheme, :address)");
        add.setForwardOnly(true);

        search.prepare("SELECT scheme||':'||address AS uri FROM history WHERE address LIKE :query  \
                       GROUP BY uri ORDER BY COUNT(timestamp) DESC, MAX(timestamp) DESC LIMIT 500");
        search.setForwardOnly(true);

        forward.prepare("SELECT scheme, address, scheme||':'||address AS uri FROM history          \
                        WHERE (scheme = 'search' AND address LIKE :search_query)                   \
                           OR (scheme != 'search' AND SUBSTR(address, 3) LIKE :other_query)        \
                        GROUP BY uri ORDER BY COUNT(timestamp) DESC, MAX(timestamp) DESC  LIMIT 1");
        forward.setForwardOnly(true);
    }

    ~TortaDatabase() {
        db.exec("DELETE FROM history WHERE timestamp < DATETIME('now', '-1 year')");
    }

    void appendHistory(const QString &scheme, const QString &address) {
        add.bindValue(":scheme", scheme);
        add.bindValue(":address", address);
        add.exec();
    }

    QStringList searchHistory(QString query) {
        search.bindValue(":query", "%" + query.replace("%", "\\%") + "%");
        search.exec();
        QStringList r;
        while (search.next())
            r << search.value("uri").toString();
        return r;
    }

    QString firstForwardMatch(QString query) {
        forward.bindValue(":search_query", query.replace("%", "\\%") + "%");
        forward.bindValue(":other_query", forward.boundValue(":search_query").toString());
        forward.exec();
        if (!forward.next())
            return "";
        else if (forward.value("scheme").toString() == "search")
            return forward.value(1).toString();
        else
            return forward.value(1).toString().right(forward.value(1).toString().length() - 2);
    }
};


class DobosTorta;


class TortaBar : public QLineEdit {
    QListView suggest;


    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape
        || QKeySequence(e->key() + e->modifiers()) == QKeySequence(SHORTCUT_ESCAPE)) {
            close();
        } else {
            QLineEdit::keyPressEvent(e);
        }
    }

    bool eventFilter(QObject *obj, QEvent *e) override {
        if (obj == &suggest && e->type() == QEvent::MouseButtonPress) {
            suggest.hide();
            setFocus();
            return true;
        } else if (obj == &suggest && e->type() == QEvent::KeyPress) {
            auto sel = suggest.selectionModel();
            const auto keyEv = static_cast<QKeyEvent *>(e);
            if (QKeySequence(keyEv->key() + keyEv->modifiers()) == QKeySequence(SHORTCUT_NEXT))
                sel->setCurrentIndex(suggest.model()->index(sel->currentIndex().row() + 1, 0),
                                     QItemSelectionModel::ClearAndSelect);
            else if (QKeySequence(keyEv->key() + keyEv->modifiers()) == QKeySequence(SHORTCUT_PREV))
                sel->setCurrentIndex(suggest.model()->index(sel->currentIndex().row() - 1, 0),
                                     QItemSelectionModel::ClearAndSelect);
            else
                event(e);
            return keyEv->key() != Qt::Key_Up && keyEv->key() != Qt::Key_Down;
        } else if (e->type() == QEvent::InputMethod || e->type() == QEvent::InputMethodQuery) {
            event(e);
            return true;
        }
        return false;
    }

public:
    TortaBar(DobosTorta *parent, TortaDatabase &db, bool incognito)
            : QLineEdit(reinterpret_cast<QWidget *>(parent)) {
        suggest.setModel(new QStringListModel(&suggest));
        suggest.setWindowFlags(Qt::Popup);
        suggest.setFocusPolicy(Qt::NoFocus);
        suggest.setFocusProxy(this);
        suggest.installEventFilter(this);

        connect(this, &QLineEdit::returnPressed, [this]{ suggest.hide(); });
        connect(suggest.selectionModel(), &QItemSelectionModel::currentChanged,
                [&](const QModelIndex &c, const QModelIndex &_){ setText(c.data().toString()); });
        connect(this, &QLineEdit::textEdited, [this, &db, parent](const QString &word){
            if (word.isEmpty())
                return suggest.hide();

            static QString before;
            const QString match = db.firstForwardMatch(word);
            if (!before.startsWith(word) && !match.isEmpty()) {
                setText(match);
                setSelection(word.length(), match.length());
            }
            before = word;

            QStringList list;
            if (guessQueryType(word) == SearchWithoutScheme)
                list << "search:" + word << "http://" + word;
            else if (guessQueryType(word) == URLWithoutScheme)
                list << "http://" + word << "search:" + word;

            if (word.startsWith("~/") || word.startsWith("/"))
                list << "file://" + expandFilePath(word);

            list << "find: " + word << db.searchHistory(word);
            static_cast<QStringListModel *>(suggest.model())->setStringList(list);
            suggest.move(mapToGlobal(QPoint(0, height())));
            suggest.resize(width(),
                           5 + suggest.sizeHintForRow(0) * qMin(20, suggest.model()->rowCount()));
            suggest.selectionModel()->clear();
            suggest.show();
        });

        if (incognito) {
            setStyleSheet("background-color: dimgray; color: white;");
            suggest.setStyleSheet("background-color: dimgray; color: white;");
        }

        setVisible(false);
    }

    void open(const QString &prefix, const QString &content) {
        setFixedWidth(parentWidget()->width() - 4);
        setText(prefix + content);
        setVisible(true);
        setFocus(Qt::ShortcutFocusReason);
        setSelection(prefix.length(), content.length());
    }

    void close();
};


class TortaPage : public QWebEnginePage {
Q_OBJECT

    bool certificateError(const QWebEngineCertificateError &_) override {
        emit sslError();
        return true;
    }

public:
    TortaPage(QWebEngineProfile *profile, QObject *parent) : QWebEnginePage(profile, parent) {}

    void triggerAction(WebAction wa, bool checked=false) override {
        if (wa == QWebEnginePage::DownloadImageToDisk || wa == QWebEnginePage::DownloadMediaToDisk)
            QProcess::startDetached("torta-dl", {contextMenuData().mediaUrl().toString()});
        else if (wa == QWebEnginePage::DownloadLinkToDisk)
            QProcess::startDetached("torta-dl", {contextMenuData().linkUrl().toString()});
        else
            QWebEnginePage::triggerAction(wa, checked);
    }

signals:
    void sslError();
};


class TortaView : public QWebEngineView {
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

public:
    TortaView(DobosTorta *parent, bool incognito)
            : QWebEngineView(reinterpret_cast<QWidget *>(parent)) {
        QWebEngineProfile *profile = incognito ? new QWebEngineProfile(this)
                                               : new QWebEngineProfile("Default", this);
        profile->setHttpUserAgent(USER_AGENT);
        profile->setHttpAcceptLanguage(QLocale().bcp47Name());
        setPage(new TortaPage(profile, this));
    }
};


class DobosTorta : public QMainWindow {
    friend class TortaBar;
    friend class TortaView;

    TortaBar bar;
    TortaView view;
    TortaDatabase &db;
    QVector<QPair<const QKeySequence, const std::function<void(void)>>> shortcuts;
    const bool incognito;


    void keyPressEvent(QKeyEvent *e) override {
        static QKeySequence key;
        const QKeySequence seq(key[0], e->key() + e->modifiers());
        key = QKeySequence(e->key() + e->modifiers());
        for (const auto &sc: shortcuts) {
            if (sc.first == key || sc.first == seq)
                return sc.second();
        }
        QMainWindow::keyPressEvent(e);
    }

    void setupShortcuts() {
        shortcuts.append({SHORTCUT_FORWARD,          [this]{ view.forward(); }});
        shortcuts.append({{Qt::ALT + Qt::Key_Right}, [this]{ view.forward(); }});
        shortcuts.append({SHORTCUT_BACK,             [this]{ view.back();    }});
        shortcuts.append({{Qt::ALT + Qt::Key_Left},  [this]{ view.back();    }});
        shortcuts.append({SHORTCUT_RELOAD,           [this]{ view.reload();  }});

        auto toggleBar = [this]{
            if (!bar.hasFocus())
                bar.open("", view.url().toDisplayString());
            else if (guessQueryType(bar.text()) == InSiteSearch)
                bar.open("", bar.text().remove(0, 5));
            else
                bar.close();
        };
        shortcuts.append({SHORTCUT_BAR,     toggleBar});
        shortcuts.append({SHORTCUT_BAR_ALT, toggleBar});
        shortcuts.append({SHORTCUT_FIND,    [this]{
            if (!bar.hasFocus())
                bar.open("find:", "");
            else if (guessQueryType(bar.text()) != InSiteSearch)
                bar.open("find:", bar.text());
            else
                bar.close();
        }});

        auto js = [&](const QString &s){ return [this, s]{ view.page()->runJavaScript(s); }; };
        auto sc = [&](int x, int y){ return js(QString("window.scrollBy(%1, %2)").arg(x).arg(y)); };

        shortcuts.append({SHORTCUT_DOWN,  sc(0, SCROLL_STEP_Y)});
        shortcuts.append({SHORTCUT_UP,    sc(0, -SCROLL_STEP_Y)});
        shortcuts.append({SHORTCUT_RIGHT, sc(SCROLL_STEP_X, 0)});
        shortcuts.append({SHORTCUT_LEFT,  sc(-SCROLL_STEP_X, 0)});

        shortcuts.append({{Qt::Key_PageDown}, js("window.scrollBy(0, window.innerHeight / 2)")});
        shortcuts.append({{Qt::Key_PageUp},   js("window.scrollBy(0, -window.innerHeight / 2)")});

        shortcuts.append({SHORTCUT_TOP,    js("window.scrollTo(0, 0);")});
        shortcuts.append({{Qt::Key_Home},  js("window.scrollTo(0, 0);")});
        shortcuts.append({SHORTCUT_BOTTOM, js("window.scrollTo(0, document.body.scrollHeight);")});
        shortcuts.append({{Qt::Key_End},   js("window.scrollTo(0, document.body.scrollHeight);")});

        auto f = [&](QWebEnginePage::FindFlags f){ return [&, f]{ inSiteSearch(bar.text(), f); }; };
        shortcuts.append({SHORTCUT_NEXT, f(QWebEnginePage::FindFlags())});
        shortcuts.append({SHORTCUT_PREV, f(QWebEnginePage::FindBackward)});

        auto zoom = [&](float x){ return [this, x]{ view.setZoomFactor(view.zoomFactor() + x); }; };
        shortcuts.append({SHORTCUT_ZOOMIN,     zoom(+ZOOM_STEP)});
        shortcuts.append({SHORTCUT_ZOOMIN_ALT, zoom(+ZOOM_STEP)});
        shortcuts.append({SHORTCUT_ZOOMOUT,    zoom(-ZOOM_STEP)});
        shortcuts.append({SHORTCUT_ZOOMRESET,  [this]{ view.setZoomFactor(1.0); }});

        shortcuts.append({SHORTCUT_NEW_WINDOW, [this]{ (new DobosTorta(db))->load(HOMEPAGE); }});
        shortcuts.append({SHORTCUT_NEW_INCOGNITO,
                [this]{ (new DobosTorta(db, true))->load(HOMEPAGE); }});
    }

    void setupBar() {
        connect(&bar, &QLineEdit::textChanged,   [this]{ inSiteSearch(bar.text()); });
        connect(&bar, &QLineEdit::returnPressed, [this]{
            load(bar.text());
            if (guessQueryType(bar.text()) != InSiteSearch)
                bar.close();
        });
        setMenuWidget(&bar);
    }

    void setupView() {
        TortaPage * const page = static_cast<TortaPage *>(view.page());
        connect(&view, &QWebEngineView::titleChanged,
            [&](const QString &title){ setWindowTitle((incognito ? "incognito: " : "") + title); });
        connect(&view, &QWebEngineView::urlChanged, [this](const QUrl &url){
            updateFrameColor();
            if (!incognito)
                db.appendHistory(url.scheme(), url.url().remove(0, url.scheme().length() + 1));
        });
        connect(page, &QWebEnginePage::linkHovered, [this](const QUrl &url){
            setWindowTitle((incognito ? "incognito: " : "")
                           + (url.isEmpty() ? view.title() : url.toDisplayString()));
        });
        connect(page, &QWebEnginePage::iconChanged, this, &QWidget::setWindowIcon);
        connect(page, &QWebEnginePage::fullScreenRequested, [this](QWebEngineFullScreenRequest r){
            if (r.toggleOn())
                showFullScreen();
            else
                showNormal();

            if (isFullScreen() == r.toggleOn())
                r.accept();
            else
                r.reject();
        });
        connect(page, &TortaPage::sslError, [this]{ updateFrameColor(true); });

        setCentralWidget(&view);
    }

    void webSearch(const QString &queryString) {
        if (!incognito)
            db.appendHistory("search", queryString);

        QUrl url("https://google.com/search");
        QUrlQuery query;
        query.addQueryItem("q", queryString);
        url.setQuery(query);

        view.load(url);
    }

    void inSiteSearch(QString query, QWebEnginePage::FindFlags flags={}) {
        if (!query.isEmpty() && guessQueryType(query) == InSiteSearch)
            view.findText(query.remove(0, 5), flags);
        else
            view.findText("");
    }

    void updateFrameColor(bool error=false) {
        if (view.url().scheme() != "https")
            setStyleSheet(incognito ? "QMainWindow { background-color: blue; }"
                                    : "QMainWindow { background-color: dimgray; }");
        else if (!error)
            setStyleSheet(incognito ? "QMainWindow { background:                                   \
                                                      qlineargradient(x1:0, y1:0, x2:1, y2:1,      \
                                                                      stop:0 lime, stop:0.3 blue,  \
                                                                      stop:0.7 blue, stop:1 lime) }"
                                    :  "QMainWindow{ background-color: lime; }");
        else
            setStyleSheet(incognito ? "QMainWindow { background:                                  \
                                                      qlineargradient(x1:0, y1:0, x2:1, y2:1,     \
                                                                      stop:0 red, stop:0.3 blue,  \
                                                                      stop:0.7 blue, stop:1 red) }"
                                    :  "QMainWindow{ background-color: red; }");
    }

public:
    DobosTorta(TortaDatabase &db, bool incognito=false)
            : bar(this, db, incognito), view(this, incognito), db(db), incognito(incognito) {
        setupBar();
        setupView();
        setupShortcuts();

        setContentsMargins(2, 2, 2, 2);
        updateFrameColor();

        show();
    }

    void load(QString query) {
        const QueryType type(guessQueryType(query));
        if (type == URLWithScheme)
            view.load(query);
        else if (type == URLWithoutScheme)
            view.load("http://" + query);
        else if (type == SearchWithScheme)
            webSearch(query.remove(0, 7));
        else if (type == SearchWithoutScheme)
            webSearch(query);
        else if (type == InSiteSearch)
            inSiteSearch(query);
    }
};


void TortaBar::close() {
    static_cast<DobosTorta *>(parent())->view.setFocus(Qt::ShortcutFocusReason);
    suggest.hide();
    setVisible(false);
    setText("");
}


QWebEngineView *TortaView::createWindow(QWebEnginePage::WebWindowType type) {
    auto window = new DobosTorta(static_cast<DobosTorta *>(parentWidget())->db,
                                 static_cast<DobosTorta *>(parentWidget())->incognito);
    if (type == QWebEnginePage::WebBrowserBackgroundTab)
        parentWidget()->activateWindow();
    return &window->view;
}


int main(int argc, char **argv) {
    qInfo("Dobostorta " GIT_VERSION);

    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    QWebEngineSettings::defaultSettings()->setAttribute(
            QWebEngineSettings::FullScreenSupportEnabled, true);

    TortaDatabase db;

    if (argc == 1)
        (new DobosTorta(db))->load(HOMEPAGE);

    for (auto arg: app.arguments().mid(1)) {
        if (arg.startsWith("/") || arg.startsWith("~/") || arg.startsWith("./"))
            (new DobosTorta(db))->load("file://" + expandFilePath(arg));
        else
            (new DobosTorta(db))->load(arg);
    }

    return app.exec();
}

#include "main.moc"
