#include <QtSql>
#include <QtWebEngineWidgets>
#include <QtWidgets>


#define HOMEPAGE    "http://google.com"
#define USER_AGENT  "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko) " \
                    "Chrome/55.0.0.0 Safari/537.36 Dobostorta/" GIT_VERSION

#define SHORTCUT_META           (Qt::CTRL)
#define SHORTCUT_FORWARD        QKeySequence(SHORTCUT_META + Qt::Key_I)
#define SHORTCUT_BACK           QKeySequence(SHORTCUT_META + Qt::Key_O)
#define SHORTCUT_RELOAD         QKeySequence(SHORTCUT_META + Qt::Key_R)
#define SHORTCUT_BAR            QKeySequence(SHORTCUT_META + Qt::Key_Colon)
#define SHORTCUT_BAR_ALT        QKeySequence(SHORTCUT_META + Qt::SHIFT + Qt::Key_Colon)
#define SHORTCUT_FIND           QKeySequence(SHORTCUT_META + Qt::Key_Slash)
#define SHORTCUT_ESCAPE         QKeySequence(SHORTCUT_META + Qt::Key_BracketLeft)
#define SHORTCUT_DOWN           QKeySequence(SHORTCUT_META + Qt::Key_J)
#define SHORTCUT_UP             QKeySequence(SHORTCUT_META + Qt::Key_K)
#define SHORTCUT_LEFT           QKeySequence(SHORTCUT_META + Qt::Key_H)
#define SHORTCUT_RIGHT          QKeySequence(SHORTCUT_META + Qt::Key_L)
#define SHORTCUT_TOP            QKeySequence(SHORTCUT_META + Qt::Key_G, SHORTCUT_META + Qt::Key_G)
#define SHORTCUT_BOTTOM         QKeySequence(SHORTCUT_META + Qt::SHIFT + Qt::Key_G)
#define SHORTCUT_NEXT           QKeySequence(SHORTCUT_META + Qt::Key_N)
#define SHORTCUT_PREV           QKeySequence(SHORTCUT_META + Qt::Key_P)
#define SHORTCUT_ZOOMIN         QKeySequence(SHORTCUT_META + Qt::Key_Plus)
#define SHORTCUT_ZOOMIN_ALT     QKeySequence(SHORTCUT_META + Qt::SHIFT + Qt::Key_Plus)
#define SHORTCUT_ZOOMOUT        QKeySequence(SHORTCUT_META + Qt::Key_Minus)
#define SHORTCUT_ZOOMRESET      QKeySequence(SHORTCUT_META + Qt::Key_0)
#define SHORTCUT_NEW_WINDOW     QKeySequence(SHORTCUT_META + Qt::SHIFT + Qt::Key_N)
#define SHORTCUT_NEW_INCOGNITO  QKeySequence(SHORTCUT_META + Qt::SHIFT + Qt::Key_P)


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
        return QFileInfo(QDir::home(), path.mid(3)).absoluteFilePath();
    else
        return QFileInfo(QDir::current(), path).absoluteFilePath();
}


class TortaDatabase {
    QSqlDatabase db;
    QSqlQuery add;
    QSqlQuery forward;

public:
    TortaDatabase() : db(QSqlDatabase::addDatabase("QSQLITE")), add(db), forward(db) {
        db.setDatabaseName(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                           + "/history");
        db.open();

        db.exec("CREATE TABLE IF NOT EXISTS history                        \
                   (timestamp TIMESTAMP UNIQUE DEFAULT CURRENT_TIMESTAMP,  \
                    scheme TEXT NOT NULL, address TEXT NOT NULL)           ");
        db.exec("CREATE INDEX IF NOT EXISTS history_index ON history(timestamp);");

        add.prepare("INSERT INTO history (scheme, address) VALUES (:scheme, :address)");

        forward.prepare("SELECT scheme, address AS addr, scheme||':'||address AS uri FROM history  \
                        WHERE (scheme = 'search' AND address LIKE :query)                          \
                           OR (scheme!='search' AND scheme!='file' AND SUBSTR(addr,3) LIKE :query) \
                        GROUP BY uri ORDER BY COUNT(timestamp) DESC, MAX(timestamp) DESC  LIMIT 1");
    }

    ~TortaDatabase() {
        db.exec("DELETE FROM history WHERE timestamp < DATETIME('now', '-1 year')");
    }

    void append(const QString &scheme, const QString &address) {
        add.bindValue(":scheme", scheme);
        add.bindValue(":address", address);
        add.exec();
    }

    QStringList search(const QStringList &query) {
        QSqlQuery search(QString("SELECT scheme||':'||address AS uri FROM history WHERE %1         \
                                  GROUP BY uri ORDER BY COUNT(timestamp) DESC, MAX(timestamp) DESC \
                                  LIMIT 500").arg(QString(" AND address LIKE ?")
                                                    .repeated(query.length()).remove(0, 5)), db);
        for (QString q: query)
            search.addBindValue("%" + q.replace("%", "\\%").replace("_", "\\_") + "%");
        QStringList r;
        for (search.exec(); search.next(); )
            r << search.value("uri").toString();
        return r;
    }

    QString firstForwardMatch(QString query) {
        forward.bindValue(":query", query.replace("%", "\\%").replace("_", "\\_") + "%");
        if (!forward.exec() || !forward.next())
            return "";
        else if (forward.value("scheme").toString() == "search")
            return forward.value("addr").toString();
        else
            return forward.value("addr").toString().remove(0, 2);
    }
};


class DobosTorta;


class TortaBar : public QLineEdit {
    QListView suggest;


    void keyPressEvent(QKeyEvent *e) override;

    bool eventFilter(QObject *obj, QEvent *e) override {
        if (obj == &suggest && e->type() == QEvent::MouseButtonPress) {
            suggest.hide();
            setFocus();
            return true;
        } else if (obj == &suggest && e->type() == QEvent::KeyPress) {
            auto sel = suggest.selectionModel();
            auto idx = [&](int x){ return suggest.model()->index(sel->currentIndex().row()+x, 0); };
            const auto keyEv = static_cast<QKeyEvent *>(e);
            if (QKeySequence(keyEv->key() + keyEv->modifiers()) == SHORTCUT_NEXT)
                sel->setCurrentIndex(idx(+1), QItemSelectionModel::ClearAndSelect);
            else if (QKeySequence(keyEv->key() + keyEv->modifiers()) == SHORTCUT_PREV)
                sel->setCurrentIndex(idx(-1), QItemSelectionModel::ClearAndSelect);
            else
                event(e);
            return keyEv->key() != Qt::Key_Up && keyEv->key() != Qt::Key_Down;
        } else if (e->type() == QEvent::InputMethod || e->type() == QEvent::InputMethodQuery) {
            return event(e);
        }
        return false;
    }

public:
    TortaBar(DobosTorta * const parent, TortaDatabase &db, bool incognito)
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

            list << "find: " + word << db.search(word.split(' ', QString::SkipEmptyParts));
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


    void contextMenuEvent(QContextMenuEvent *e) override {
        QMenu *menu = page()->createStandardContextMenu();
        auto it = std::find(menu->actions().cbegin(), menu->actions().cend(),
                            page()->action(QWebEnginePage::OpenLinkInThisWindow));
        if (it != menu->actions().cend())
            menu->insertAction(*(it + 1), page()->action(QWebEnginePage::OpenLinkInNewWindow));
        connect(menu, &QMenu::aboutToHide, menu, &QObject::deleteLater);
        menu->popup(e->globalPos());
    }

public:
    TortaView(DobosTorta * const parent, bool incognito)
            : QWebEngineView(reinterpret_cast<QWidget *>(parent)) {
        QWebEngineProfile *profile = incognito ? new QWebEngineProfile(this)
                                               : new QWebEngineProfile("Default", this);
        connect(profile, &QWebEngineProfile::downloadRequested, [](QWebEngineDownloadItem *d){
            QProcess::startDetached("torta-dl", {d->url().toString()});
        });
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
        if (!executeShortcuts(e))
            QMainWindow::keyPressEvent(e);
    }

    void setupShortcuts() {
        shortcuts.append({SHORTCUT_FORWARD,          [this]{ view.forward(); }});
        shortcuts.append({{Qt::ALT + Qt::Key_Right}, [this]{ view.forward(); }});
        shortcuts.append({SHORTCUT_BACK,             [this]{ view.back();    }});
        shortcuts.append({{Qt::ALT + Qt::Key_Left},  [this]{ view.back();    }});
        shortcuts.append({SHORTCUT_RELOAD,           [this]{ view.reload();  }});

        auto toggleBar = [this]{
            if (guessQueryType(bar.text()) == InSiteSearch)
                bar.open("", bar.text().remove(0, 5));
            else if (!bar.isVisible())
                bar.open("", view.url().toDisplayString());
            else
                bar.close();
        };
        shortcuts.append({SHORTCUT_BAR,     toggleBar});
        shortcuts.append({SHORTCUT_BAR_ALT, toggleBar});
        shortcuts.append({SHORTCUT_FIND,    [this]{
            if (!bar.isVisible() || guessQueryType(bar.text()) != InSiteSearch)
                bar.open("find:", bar.text());
            else
                bar.close();
        }});

        auto js = [&](const QString &s){ return [this, s]{ view.page()->runJavaScript(s); }; };
        auto sc = [&](int x, int y){ return js(QString("window.scrollBy(%1, %2)").arg(x).arg(y)); };

        shortcuts.append({SHORTCUT_DOWN,  sc(0, 40)});
        shortcuts.append({SHORTCUT_UP,    sc(0, -40)});
        shortcuts.append({SHORTCUT_RIGHT, sc(40, 0)});
        shortcuts.append({SHORTCUT_LEFT,  sc(-40, 0)});
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
        shortcuts.append({SHORTCUT_ZOOMIN,     zoom(+0.1)});
        shortcuts.append({SHORTCUT_ZOOMIN_ALT, zoom(+0.1)});
        shortcuts.append({SHORTCUT_ZOOMOUT,    zoom(-0.1)});
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
                db.append(url.scheme(), url.url().remove(0, url.scheme().length() + 1));
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
            db.append("search", queryString);

        QUrl url("https://google.com/search");
        QUrlQuery query;
        query.addQueryItem("q", queryString);
        url.setQuery(query);

        view.load(url);
    }

    void inSiteSearch(const QString &q, QWebEnginePage::FindFlags f={}) {
        view.findText((!q.isEmpty() && guessQueryType(q) == InSiteSearch) ? q.mid(5) : "", f);
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

    void load(const QString &query) {
        const QueryType type(guessQueryType(query));
        if (type == URLWithScheme)
            view.load(query);
        else if (type == URLWithoutScheme)
            view.load("http://" + query);
        else if (type == SearchWithScheme)
            webSearch(query.mid(7));
        else if (type == SearchWithoutScheme)
            webSearch(query);
        else if (type == InSiteSearch)
            inSiteSearch(query);
    }

    bool executeShortcuts(const QKeyEvent *e) {
        static QKeySequence key;
        const QKeySequence seq(key[0], e->key() + e->modifiers());
        key = QKeySequence(e->key() + e->modifiers());
        for (const auto &sc: shortcuts) {
            if (sc.first == key || sc.first == seq) {
                sc.second();
                return true;
            }
        }
        return false;
    }
};


void TortaBar::close() {
    static_cast<DobosTorta *>(parent())->view.setFocus(Qt::ShortcutFocusReason);
    suggest.hide();
    setVisible(false);
    setText("");
}


void TortaBar::keyPressEvent(QKeyEvent *e) {
    if (e->key() == Qt::Key_Escape || QKeySequence(e->key() + e->modifiers()) == SHORTCUT_ESCAPE)
        close();
    else if (!static_cast<DobosTorta *>(parent())->executeShortcuts(e))
        QLineEdit::keyPressEvent(e);
}


QWebEngineView *TortaView::createWindow(QWebEnginePage::WebWindowType type) {
    const DobosTorta * const torta = static_cast<const DobosTorta *>(parentWidget());
    DobosTorta * const window = new DobosTorta(torta->db, torta->incognito);
    if (type == QWebEnginePage::WebBrowserBackgroundTab)
        parentWidget()->activateWindow();
    return &window->view;
}


int main(int argc, char **argv) {
    QApplication app(argc, argv);
    app.setApplicationName("Dobostorta");
    app.setApplicationVersion(GIT_VERSION);
    app.setAttribute(Qt::AA_EnableHighDpiScaling);
    QWebEngineSettings::defaultSettings()->setAttribute(
            QWebEngineSettings::FullScreenSupportEnabled, true);

    QCommandLineParser parser;
    parser.addPositionalArgument("[URL|PATH...]", "URL or file path.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption(QCommandLineOption(QStringList() << "i" << "incognito", "set incognito mode"));
    parser.process(app.arguments());

    TortaDatabase db;

    if (parser.positionalArguments().empty())
        (new DobosTorta(db, parser.isSet("incognito")))->load(HOMEPAGE);

    for (auto arg: parser.positionalArguments()) {
        if (arg.startsWith("/") || arg.startsWith("~/") || arg.startsWith("./"))
            (new DobosTorta(db, parser.isSet("incognito")))->load("file://" + expandFilePath(arg));
        else
            (new DobosTorta(db, parser.isSet("incognito")))->load(arg);
    }

    return app.exec();
}

#include "main.moc"
