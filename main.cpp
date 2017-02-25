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
    QSqlQuery append;
    QSqlQuery search;

public:
    TortaDatabase() : db(QSqlDatabase::addDatabase("QSQLITE")), append(db), search(db) {
        db.setDatabaseName(QStandardPaths::writableLocation(QStandardPaths::DataLocation)
                           + "/history");
        db.open();

        db.exec("CREATE TABLE IF NOT EXISTS history                 \
                   (timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,  \
                    scheme TEXT NOT NULL, address TEXT NOT NULL)");

        append.prepare("INSERT INTO history (scheme, address) VALUES (?, ?)");
        append.setForwardOnly(true);

        search.prepare("SELECT scheme || ':' || address AS uri FROM history WHERE address LIKE ?  \
                          GROUP BY uri ORDER BY MAX(timestamp) DESC, COUNT(*) DESC");
        search.setForwardOnly(true);
    }

    ~TortaDatabase() {
        db.exec("DELETE FROM history WHERE timestamp < DATETIME('now', '-1 year')");
    }

    void appendHistory(const QString &scheme, const QString &address) {
        append.bindValue(0, scheme);
        append.bindValue(1, address);
        append.exec();
    }

    QStringList searchHistory(QString query) {
        search.bindValue(0, "%%" + query.replace("%%", "\\%%") + "%%");
        search.exec();

        QStringList r;
        while (search.next())
            r << search.value(0).toString();
        return r;
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
        || QKeySequence(e->key() + e->modifiers()).matches(SHORTCUT_ESCAPE)) {
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
        if (action == QWebEnginePage::DownloadLinkToDisk) {
            QProcess::startDetached(DOWNLOAD_COMMAND, {contextMenuData().linkUrl().toString()});
        } else if (action == QWebEnginePage::DownloadImageToDisk
                   || action == QWebEnginePage::DownloadMediaToDisk) {
            QProcess::startDetached(DOWNLOAD_COMMAND, {contextMenuData().mediaUrl().toString()});
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


    template<class Func>
    void addShortcut(QWidget *from, QKeySequence key,
                     const typename QtPrivate::FunctionPointer<Func>::Object *to, Func method) {
        connect(new QShortcut(key, from), &QShortcut::activated, to, method);
    }

    template<class Func> void addShortcut(QWidget *from, QKeySequence key, Func to) {
        connect(new QShortcut(key, from), &QShortcut::activated, to);
    }

    void setupShortcuts() {
        addShortcut(this, SHORTCUT_FORWARD,          &view, &QWebEngineView::forward);
        addShortcut(this, {Qt::ALT + Qt::Key_Right}, &view, &QWebEngineView::forward);
        addShortcut(this, SHORTCUT_BACK,             &view, &QWebEngineView::back);
        addShortcut(this, {Qt::ALT + Qt::Key_Left},  &view, &QWebEngineView::back);
        addShortcut(this, SHORTCUT_RELOAD,           &view, &QWebEngineView::reload);

        addShortcut(this, SHORTCUT_BAR,     this, &DobosTorta::toggleBar);
        addShortcut(this, SHORTCUT_FIND,    this, &DobosTorta::toggleFind);
        addShortcut(&bar, SHORTCUT_ESCAPE,  this, &DobosTorta::escapeBar);
        addShortcut(&bar, {Qt::Key_Escape}, this, &DobosTorta::escapeBar);

        addShortcut(this, SHORTCUT_DOWN,    [this]{ scroll(0, SCROLL_STEP_Y);  });
        addShortcut(this, {Qt::Key_Down},   [this]{ scroll(0, SCROLL_STEP_Y);  });
        addShortcut(this, SHORTCUT_UP,      [this]{ scroll(0, -SCROLL_STEP_Y); });
        addShortcut(this, {Qt::Key_Up},     [this]{ scroll(0, -SCROLL_STEP_Y); });
        addShortcut(this, SHORTCUT_RIGHT,   [this]{ scroll(SCROLL_STEP_X, 0);  });
        addShortcut(this, {Qt::Key_Right},  [this]{ scroll(SCROLL_STEP_X, 0);  });
        addShortcut(this, SHORTCUT_LEFT,    [this]{ scroll(-SCROLL_STEP_X, 0); });
        addShortcut(this, {Qt::Key_Left},   [this]{ scroll(-SCROLL_STEP_X, 0); });

        QWebEnginePage *p = view.page();

        addShortcut(this, {Qt::Key_PageDown},
                    [p]{ p->runJavaScript("window.scrollBy(0, window.innerHeight / 2)"); });
        addShortcut(this, {Qt::Key_PageUp},
                    [p]{ p->runJavaScript("window.scrollBy(0, -window.innerHeight / 2)"); });

        addShortcut(this, SHORTCUT_TOP, [p]{ p->runJavaScript("window.scrollTo(0, 0);"); });
        addShortcut(this, {Qt::Key_Home}, [p]{ p->runJavaScript("window.scrollTo(0, 0);"); });
        addShortcut(this, SHORTCUT_BOTTOM,
                    [p]{ p->runJavaScript("window.scrollTo(0, document.body.scrollHeight);"); });
        addShortcut(this, {Qt::Key_End},
                    [p]{ p->runJavaScript("window.scrollTo(0, document.body.scrollHeight);"); });

        addShortcut(this, SHORTCUT_ZOOMIN,
                    [this]{ view.setZoomFactor(view.zoomFactor() + ZOOM_STEP); });
        addShortcut(this, SHORTCUT_ZOOMOUT,
                    [this]{ view.setZoomFactor(view.zoomFactor() - ZOOM_STEP); });
        addShortcut(this, SHORTCUT_ZOOMRESET, [&]{ view.setZoomFactor(1.0); });

        addShortcut(this, SHORTCUT_NEXT,
                    [this]{ inSiteSearch(bar.text(), QWebEnginePage::FindFlags()); });
        addShortcut(this, SHORTCUT_PREV,
                    [this]{ inSiteSearch(bar.text(), QWebEnginePage::FindBackward); });
    }

    void setupBar() {
        connect(&bar, &QLineEdit::textChanged, this, &DobosTorta::barChanged);
        connect(&bar, &QLineEdit::returnPressed, [this]{
            load(bar.text());

            if (GuessQueryType(bar.text()) != InSiteSearch)
                escapeBar();
        });

        bar.setVisible(false);
        setMenuWidget(&bar);
    }

    void setupView() {
        connect(&view, &QWebEngineView::titleChanged, this, &QWidget::setWindowTitle);
        connect(&view, &QWebEngineView::urlChanged, this, &DobosTorta::urlChanged);
        connect(view.page(), &QWebEnginePage::linkHovered, this, &DobosTorta::linkHovered);
        connect(view.page(), &QWebEnginePage::iconChanged, this, &QWidget::setWindowIcon);
        connect(view.page(), &QWebEnginePage::fullScreenRequested,
                this, &DobosTorta::toggleFullScreen);

        connect(static_cast<TortaPage *>(view.page()), &TortaPage::sslError, [this]{
            setStyleSheet("QMainWindow { background-color: " HTTPS_ERROR_FRAME_COLOR "; }");
        });

        setCentralWidget(&view);
    }

    void scroll(int x, int y) {
        view.page()->runJavaScript(QString("window.scrollBy(%1, %2)").arg(x).arg(y));
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
    void barChanged() {
        inSiteSearch(bar.text());
    }

    void linkHovered(const QUrl &url) {
        setWindowTitle(url.isEmpty() ? view.title() : url.toDisplayString());
    }

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
        bar.setText("");
        view.setFocus(Qt::ShortcutFocusReason);
        bar.setVisible(false);
    }

    void toggleFullScreen(QWebEngineFullScreenRequest r) {
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

    if (argc > 1) {
        for (int i=1; i<argc; i++)
            (new DobosTorta(db))->load(argv[i]);
    } else {
        (new DobosTorta(db))->load(HOMEPAGE);
    }

    return app.exec();
}


#include "main.moc"
