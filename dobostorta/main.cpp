#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QDir>
#include <QFileInfo>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QProcess>
#include <QShortcut>
#include <QStandardPaths>
#include <QStringListModel>
#include <QUrlQuery>
#include <QWebEngineContextMenuData>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>
#include <QListView>


#define HOMEPAGE    "http://google.com"
#define USER_AGENT  "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)" \
                    "Chrome/55.0.0.0 Safari/537.36 DobosTorta/dev-" GIT_VERSION

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
    static const QRegExp hasScheme("^[a-zA-Z0-9]+://");
    static const QRegExp address("^[^/]+(\\.[^/]+|:[0-9]+)");
    if (str.startsWith("search:"))
        return SearchWithScheme;
    else if (str.startsWith("find:"))
        return InSiteSearch;
    else if (hasScheme.indexIn(str) != -1)
        return URLWithScheme;
    else if (address.indexIn(str) != -1)
        return URLWithoutScheme;
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

        add.prepare("INSERT INTO history (scheme, address) VALUES (?, ?)");
        add.setForwardOnly(true);

        search.prepare("SELECT scheme || ':' || address AS uri FROM history WHERE address LIKE ?  \
                          GROUP BY uri ORDER BY COUNT(timestamp) DESC, MIN(timestamp)");
        search.setForwardOnly(true);

        forward.prepare("SELECT scheme, address FROM history                                       \
                           WHERE (scheme = 'search' AND address LIKE ?)                            \
                              OR (scheme != 'search' AND LTRIM(address, '/') LIKE ?)               \
                           GROUP BY scheme, address ORDER BY COUNT(timestamp) DESC, MIN(timestamp) \
                           LIMIT 1");
        forward.setForwardOnly(true);
    }

    ~TortaDatabase() {
        db.exec("DELETE FROM history WHERE timestamp < DATETIME('now', '-1 year')");
    }

    void appendHistory(const QString &scheme, const QString &address) {
        add.bindValue(0, scheme);
        add.bindValue(1, address);
        add.exec();
    }

    QStringList searchHistory(QString query) {
        search.bindValue(0, "%" + query.replace("%", "\\%") + "%");
        search.exec();
        QStringList r;
        while (search.next())
            r << search.value(0).toString();
        return r;
    }

    QString firstForwardMatch(QString query) {
        query = query.replace("%", "\\%") + "%";
        forward.bindValue(0, query);
        forward.bindValue(1, query);
        forward.exec();
        if (!forward.next())
            return "";
        else if (forward.value(0).toString() == "search")
            return forward.value(1).toString();
        else
            return forward.value(1).toString().right(forward.value(1).toString().length() - 2);
    }
};


class TortaBar : public QLineEdit {
    QListView suggest;


    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Escape
        || QKeySequence(e->key() + e->modifiers()) == QKeySequence(SHORTCUT_ESCAPE)) {
            suggest.hide();
            setVisible(false);
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
            event(e);
            const int key = static_cast<QKeyEvent *>(e)->key();
            return key != Qt::Key_Up && key != Qt::Key_Down;
        }
        return false;
    }

public:
    TortaBar(QWidget *parent, TortaDatabase &db) : QLineEdit(parent) {
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
            suggest.resize(width(), qMin(parent->height() - height(),
                                      4 + suggest.sizeHintForRow(0) * suggest.model()->rowCount()));
            suggest.selectionModel()->clear();
            suggest.show();
        });
    }
};


class TortaPage : public QWebEnginePage {
    Q_OBJECT


    bool certificateError(const QWebEngineCertificateError &error) override {
        qWarning() << error.errorDescription();
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
    TortaView(QWidget *parent, bool incognito) : QWebEngineView(parent) {
        QWebEngineProfile *profile = incognito ? new QWebEngineProfile(this)
                                               : new QWebEngineProfile("Default", this);
        profile->setHttpUserAgent(USER_AGENT);
        setPage(new TortaPage(profile, this));
    }
};


class DobosTorta : public QMainWindow {
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
                openBar("", view.url().toDisplayString());
            else if (guessQueryType(bar.text()) == InSiteSearch)
                openBar("", bar.text().remove(0, 5));
            else
                escapeBar();
        };
        shortcuts.append({SHORTCUT_BAR,     toggleBar});
        shortcuts.append({SHORTCUT_BAR_ALT, toggleBar});
        shortcuts.append({SHORTCUT_FIND,    [this]{
            if (!bar.hasFocus())
                openBar("find:", "");
            else if (guessQueryType(bar.text()) != InSiteSearch)
                openBar("find:", bar.text());
            else
                escapeBar();
        }});

        QWebEnginePage *p = view.page();
        auto js = [&](const QString &script){ return [p, script]{ p->runJavaScript(script); }; };
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
        connect(&bar, &QLineEdit::textChanged, [this]{ inSiteSearch(bar.text()); });
        connect(&bar, &QLineEdit::returnPressed, [this]{
            load(bar.text());
            if (guessQueryType(bar.text()) != InSiteSearch)
                escapeBar();
        });
        connect(new QShortcut(SHORTCUT_ESCAPE, &bar),  &QShortcut::activated, [&]{ escapeBar(); });
        connect(new QShortcut({Qt::Key_Escape}, &bar), &QShortcut::activated, [&]{ escapeBar(); });

        bar.setVisible(false);
        setMenuWidget(&bar);
    }

    void setupView() {
        TortaPage *page = static_cast<TortaPage *>(view.page());
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

    void openBar(const QString &prefix, const QString &content) {
        bar.setText(prefix + content);
        bar.setVisible(true);
        bar.setFocus(Qt::ShortcutFocusReason);
        bar.setSelection(prefix.length(), content.length());
    }

    void escapeBar() {
        setFocus(Qt::ShortcutFocusReason);
        bar.setVisible(false);
        bar.setText("");
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
            setStyleSheet(incognito ? "QMainWindow { background-color: #0000FF; }"
                                    : "QMainWindow { background-color: #808080; }");
        else if (!error)
            setStyleSheet(incognito ? "QMainWindow { background-color: #00D0FF; }"
                                    : "QMainWindow { background-color: #00ff00; }");
        else
            setStyleSheet(incognito ? "QMainWindow { background-color: #D000FF; }"
                                    : "QMainWindow { background-color: #ff0000; }");
    }

public:
    DobosTorta(TortaDatabase &db, bool incognito=false)
            : bar(this, db), view(this, incognito), db(db), incognito(incognito) {
        setupBar();
        setupView();
        setupShortcuts();

        setContentsMargins(2, 2, 2, 2);
        updateFrameColor();

        show();
    }

    void load(QString query) {
        switch (guessQueryType(query)) {
        case URLWithScheme:
            view.load(query);
            break;
        case URLWithoutScheme:
            view.load("http://" + query);
            break;
        case SearchWithScheme:
            webSearch(query.remove(0, 7));
            break;
        case SearchWithoutScheme:
            webSearch(query);
            break;
        case InSiteSearch:
            inSiteSearch(query);
            break;
        }
    }
};


QWebEngineView *TortaView::createWindow(QWebEnginePage::WebWindowType type) {
    auto window = new DobosTorta(static_cast<DobosTorta *>(parentWidget())->db,
                                 static_cast<DobosTorta *>(parentWidget())->incognito);
    if (type == QWebEnginePage::WebBrowserBackgroundTab)
        parentWidget()->activateWindow();
    return &window->view;
}


int main(int argc, char **argv) {
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
