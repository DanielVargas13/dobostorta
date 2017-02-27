#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
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


#define USER_AGENT  "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)" \
                    "Chrome/55.0.0.0 Safari/537.36 DobosTorta/dev-" GIT_VERSION

#define HOMEPAGE         "http://google.com"
#define SEARCH_ENDPOINT  "http://google.com/search"
#define SEARCH_QUERY     "q"

#define FRAME_WIDTH              2, 2, 2, 2
#define HTTPS_FRAME_COLOR        "#00ff00"
#define HTTPS_ERROR_FRAME_COLOR  "#ff0000"
#define DEFAULT_FRAME_COLOR      "#808080"

#define SHORTCUT_META  (Qt::CTRL)

#define SHORTCUT_FORWARD    {SHORTCUT_META + Qt::Key_I}
#define SHORTCUT_BACK       {SHORTCUT_META + Qt::Key_O}
#define SHORTCUT_RELOAD     {SHORTCUT_META + Qt::Key_R}
#define SHORTCUT_BAR        {SHORTCUT_META + Qt::Key_Colon}
#define SHORTCUT_BAR_ALT    {SHORTCUT_META + Qt::SHIFT + Qt::Key_Colon}
#define SHORTCUT_FIND       {SHORTCUT_META + Qt::Key_Slash}
#define SHORTCUT_ESCAPE     {SHORTCUT_META + Qt::Key_BracketLeft}
#define SHORTCUT_DOWN       {SHORTCUT_META + Qt::Key_J}
#define SHORTCUT_UP         {SHORTCUT_META + Qt::Key_K}
#define SHORTCUT_LEFT       {SHORTCUT_META + Qt::Key_H}
#define SHORTCUT_RIGHT      {SHORTCUT_META + Qt::Key_L}
#define SHORTCUT_TOP        {SHORTCUT_META + Qt::Key_G, SHORTCUT_META + Qt::Key_G}
#define SHORTCUT_BOTTOM     {SHORTCUT_META + Qt::SHIFT + Qt::Key_G}
#define SHORTCUT_ZOOMIN     {SHORTCUT_META + Qt::Key_Plus}
#define SHORTCUT_ZOOMOUT    {SHORTCUT_META + Qt::Key_Minus}
#define SHORTCUT_ZOOMRESET  {SHORTCUT_META + Qt::Key_0}
#define SHORTCUT_NEXT       {SHORTCUT_META + Qt::Key_N}
#define SHORTCUT_PREV       {SHORTCUT_META + Qt::Key_P}

#define SCROLL_STEP_X  20
#define SCROLL_STEP_Y  20
#define ZOOM_STEP      0.1

#define DOWNLOAD_COMMAND  "wget"


enum QueryType {
    URLWithScheme,
    URLWithoutScheme,
    SearchWithScheme,
    SearchWithoutScheme,
    InSiteSearch
};


QueryType GuessQueryType(const QString &str) {
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


class TortaDatabase {
private:
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
                          GROUP BY uri ORDER BY MAX(timestamp) DESC, COUNT(timestamp) DESC");
        search.setForwardOnly(true);

        forward.prepare("SELECT scheme, address FROM history                          \
                           WHERE (scheme = 'search' AND address LIKE ?)               \
                              OR (scheme != 'search' AND LTRIM(address, '/') LIKE ?)  \
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
        search.bindValue(0, "%%" + query.replace("%%", "\\%%") + "%%");
        search.exec();
        QStringList r;
        while (search.next())
            r << search.value(0).toString();
        return r;
    }

    QString firstForwardMatch(QString query) {
        forward.bindValue(0, query.replace("%%", "\\%%") + "%%");
        forward.bindValue(1, query.replace("%%", "\\%%") + "%%");
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
Q_OBJECT

private:
    QCompleter completer;
    TortaDatabase &db;

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (!completer.popup()->isVisible())
            return QLineEdit::keyPressEvent(e);

        if (e->key() == Qt::Key_Escape
        || QKeySequence(e->key() + e->modifiers()) == QKeySequence(SHORTCUT_ESCAPE)) {
            completer.popup()->setVisible(false);
            setVisible(false);
        } else {
            QLineEdit::keyPressEvent(e);
        }
    }

public:
    TortaBar(QWidget *parent, TortaDatabase &db) : QLineEdit(parent), completer(this), db(db) {
        completer.setModel(new QStringListModel(&completer));
        completer.setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        setCompleter(&completer);
        connect(this, &QLineEdit::textEdited, this, &TortaBar::update);
    }

private slots:
    void update(const QString &word) {
        static QString before;
        if (!word.isEmpty() && !before.startsWith(word)) {
            auto match = db.firstForwardMatch(word);
            if (!match.isEmpty() && match != word) {
                setText(match);
                setSelection(word.length(), match.length());
            }
        }
        before = word;

        const QueryType type(GuessQueryType(word));
        QStringList list;

        if (type == SearchWithoutScheme)
            list << "find:" + word << "http://" + word;
        else if (type == URLWithoutScheme)
            list << "search:" + word << "find:" + word;

        list << db.searchHistory(word);
        static_cast<QStringListModel *>(completer.model())->setStringList(list);
    }
};


class TortaPage : public QWebEnginePage {
Q_OBJECT

protected:
    bool certificateError(const QWebEngineCertificateError &error) override {
        qWarning() << "ssl error:" << error.errorDescription();
        emit sslError();
        return true;
    }

public:
    TortaPage(QObject *parent) : QWebEnginePage(parent) {
        profile()->setHttpUserAgent(USER_AGENT);
    }

    void triggerAction(WebAction action, bool checked=false) override {
        if (action == QWebEnginePage::DownloadImageToDisk
        || action == QWebEnginePage::DownloadMediaToDisk) {
            QProcess::startDetached(DOWNLOAD_COMMAND, {contextMenuData().mediaUrl().toString()});
        } else if (action == QWebEnginePage::DownloadLinkToDisk) {
            QProcess::startDetached(DOWNLOAD_COMMAND, {contextMenuData().linkUrl().toString()});
        } else {
            QWebEnginePage::triggerAction(action, checked);
        }
    }

signals:
    void sslError();
};


class TortaView : public QWebEngineView {
private:
    TortaDatabase &db;

protected:
    QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

public:
    TortaView(QWidget *parent, TortaDatabase &db) : QWebEngineView(parent), db(db) {
        setPage(new TortaPage(this));
    }
};


class DobosTorta : public QMainWindow {
Q_OBJECT

    friend class TortaView;

private:
    TortaBar bar;
    TortaView view;
    TortaDatabase &db;
    QList<QPair<const QKeySequence, std::function<void(void)>>> shortcuts;


    void keyPressEvent(QKeyEvent *e) override {
        static int oldKey = 0;
        const int key = e->key() + e->modifiers();
        for (const auto &sc: shortcuts) {
            if (sc.first == QKeySequence(key) || sc.first == QKeySequence(oldKey, key))
                return sc.second();
        }
        oldKey = key;
        QMainWindow::keyPressEvent(e);
    }

    void setupShortcuts() {
        shortcuts.append({SHORTCUT_FORWARD,          [this]{ view.forward(); }});
        shortcuts.append({{Qt::ALT + Qt::Key_Right}, [this]{ view.forward(); }});
        shortcuts.append({SHORTCUT_BACK,             [this]{ view.back();    }});
        shortcuts.append({{Qt::ALT + Qt::Key_Left},  [this]{ view.back();    }});
        shortcuts.append({SHORTCUT_RELOAD,           [this]{ view.reload();  }});

        shortcuts.append({SHORTCUT_BAR,     [this]{ toggleBar();  }});
        shortcuts.append({SHORTCUT_BAR_ALT, [this]{ toggleBar();  }});
        shortcuts.append({SHORTCUT_FIND,    [this]{ toggleFind(); }});

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

        auto zoom = [&](float x){ return [this, x]{ view.setZoomFactor(view.zoomFactor() + x); }; };
        shortcuts.append({SHORTCUT_ZOOMIN,  zoom(+ZOOM_STEP)});
        shortcuts.append({SHORTCUT_ZOOMOUT, zoom(-ZOOM_STEP)});
        shortcuts.append({SHORTCUT_ZOOMRESET, [this]{ view.setZoomFactor(1.0); }});

        auto find = [&](QWebEnginePage::FindFlags f){ return [&]{ inSiteSearch(bar.text(), f); }; };
        shortcuts.append({SHORTCUT_NEXT, find(QWebEnginePage::FindFlags())});
        shortcuts.append({SHORTCUT_PREV, find(QWebEnginePage::FindBackward)});
    }

    void setupBar() {
        connect(&bar, &QLineEdit::textChanged, [this]{ inSiteSearch(bar.text()); });
        connect(&bar, &QLineEdit::returnPressed, [this]{
            load(bar.text());

            if (GuessQueryType(bar.text()) != InSiteSearch)
                escapeBar();
        });
        connect(new QShortcut(SHORTCUT_ESCAPE, &bar),  &QShortcut::activated,
                this, &DobosTorta::escapeBar);
        connect(new QShortcut({Qt::Key_Escape}, &bar), &QShortcut::activated,
                this, &DobosTorta::escapeBar);

        bar.setVisible(false);
        setMenuWidget(&bar);
    }

    void setupView() {
        connect(&view, &QWebEngineView::titleChanged, this, &QWidget::setWindowTitle);
        connect(&view, &QWebEngineView::urlChanged, this, &DobosTorta::urlChanged);
        connect(view.page(), &QWebEnginePage::linkHovered, [this](const QUrl &url){
            setWindowTitle(url.isEmpty() ? view.title() : url.toDisplayString());
        });
        connect(view.page(), &QWebEnginePage::iconChanged, this, &QWidget::setWindowIcon);
        connect(view.page(), &QWebEnginePage::fullScreenRequested, this, &DobosTorta::fullScreen);

        connect(static_cast<TortaPage *>(view.page()), &TortaPage::sslError, [this]{
            setStyleSheet("QMainWindow { background-color: " HTTPS_ERROR_FRAME_COLOR "; }");
        });

        setCentralWidget(&view);
    }

    void openBar(const QString &prefix, const QString &content) {
        bar.setText(prefix + content);
        bar.setVisible(true);
        bar.setFocus(Qt::ShortcutFocusReason);
        bar.setSelection(prefix.length(), content.length());
    }

    void webSearch(const QString &queryString) {
        db.appendHistory("search", queryString);

        QUrl url(SEARCH_ENDPOINT);
        QUrlQuery query;

        query.addQueryItem(SEARCH_QUERY, queryString);
        url.setQuery(query);
        view.load(url);
    }

    void inSiteSearch(QString query, QWebEnginePage::FindFlags flags=QWebEnginePage::FindFlags()) {
        if (!query.isEmpty() && GuessQueryType(query) == InSiteSearch)
            view.findText(query.remove(0, 5), flags);
        else
            view.findText("");
    }

public:
    DobosTorta(TortaDatabase &db) : bar(this, db), view(this, db), db(db) {
        setupBar();
        setupView();
        setupShortcuts();

        setContentsMargins(FRAME_WIDTH);
        setStyleSheet("QMainWindow { background-color: " DEFAULT_FRAME_COLOR "; }");

        show();
    }

    void load(QString query) {
        switch (GuessQueryType(query)) {
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

private slots:
    void urlChanged(const QUrl &url) {
        db.appendHistory(url.scheme(), url.url().remove(0, url.scheme().length() + 1));

        if (url.scheme() == "https")
            setStyleSheet("QMainWindow { background-color: " HTTPS_FRAME_COLOR "; }");
        else
            setStyleSheet("QMainWindow { background-color: " DEFAULT_FRAME_COLOR "; }");
    }

    void toggleBar() {
        if (!bar.hasFocus())
            openBar("", view.url().toDisplayString());
        else if (GuessQueryType(bar.text()) == InSiteSearch)
            openBar("", bar.text().remove(0, 5));
        else
            escapeBar();
    }

    void toggleFind() {
        if (!bar.hasFocus())
            openBar("find:", "");
        else if (GuessQueryType(bar.text()) != InSiteSearch)
            openBar("find:", bar.text());
        else
            escapeBar();
    }

    void escapeBar() {
        setFocus(Qt::ShortcutFocusReason);
        bar.setVisible(false);
        bar.setText("");
    }

    void fullScreen(QWebEngineFullScreenRequest r){
        if (r.toggleOn())
            showFullScreen();
        else
            showNormal();

        if (isFullScreen() == r.toggleOn())
            r.accept();
        else
            r.reject();
    }
};


QWebEngineView *TortaView::createWindow(QWebEnginePage::WebWindowType type) {
    auto window = new DobosTorta(db);
    if (type != QWebEnginePage::WebBrowserBackgroundTab)
        window->setFocus();
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

    for (int i=1; i<argc; i++)
        (new DobosTorta(db))->load(argv[i]);

    return app.exec();
}


#include "main.moc"
