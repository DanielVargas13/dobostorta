#include <QAbstractItemView>
#include <QApplication>
#include <QCompleter>
#include <QKeyEvent>
#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QShortcut>
#include <QStandardPaths>
#include <QStringListModel>
#include <QUrlQuery>
#include <QWebEngineFullScreenRequest>
#include <QWebEngineProfile>
#include <QWebEngineSettings>
#include <QWebEngineView>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlQuery>


#define HOMEPAGE         "http://google.com"
#define SEARCH_ENDPOINT  "http://google.com/search"
#define SEARCH_QUERY     "q"

#define HTTPS_FRAME_COLOR        "#00ff00"
#define HTTPS_ERROR_FRAME_COLOR  "#ff0000"
#define DEFAULT_FRAME_COLOR      "#808080"

#define SHORTCUT_META     (Qt::CTRL)

#define SHORTCUT_FORWARD  {SHORTCUT_META + Qt::Key_I}
#define SHORTCUT_BACK     {SHORTCUT_META + Qt::Key_O}
#define SHORTCUT_BAR      {SHORTCUT_META + Qt::Key_Colon}
#define SHORTCUT_FIND     {SHORTCUT_META + Qt::Key_Slash}
#define SHORTCUT_ESCAPE   {SHORTCUT_META + Qt::Key_BracketLeft}
#define SHORTCUT_DOWN     {SHORTCUT_META + Qt::Key_J}
#define SHORTCUT_UP       {SHORTCUT_META + Qt::Key_K}
#define SHORTCUT_LEFT     {SHORTCUT_META + Qt::Key_H}
#define SHORTCUT_RIGHT    {SHORTCUT_META + Qt::Key_L}
#define SHORTCUT_TOP      {SHORTCUT_META + Qt::Key_G, SHORTCUT_META + Qt::Key_G}
#define SHORTCUT_BOTTOM   {SHORTCUT_META + Qt::SHIFT + Qt::Key_G}
#define SHORTCUT_ZOOMIN   {SHORTCUT_META + Qt::Key_Plus}
#define SHORTCUT_ZOOMOUT  {SHORTCUT_META + Qt::Key_Minus}
#define SHORTCUT_NEXT     {SHORTCUT_META + Qt::Key_N}
#define SHORTCUT_PREV     {SHORTCUT_META + Qt::Key_P}

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
        db.setDatabaseName(QStandardPaths::writableLocation(QStandardPaths::DataLocation) + "/db");
        db.open();

        db.exec("CREATE TABLE IF NOT EXISTS history                 \
                   (timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP,  \
                    scheme TEXT NOT NULL,                           \
                    url TEXT NOT NULL)");

        db.exec("CREATE VIEW IF NOT EXISTS recently AS  \
                   SELECT MAX(timestamp) last_access,   \
                          COUNT(timestamp) count,       \
                          scheme,                       \
                          url                           \
                   FROM history                         \
                   GROUP BY scheme || url               \
                   ORDER BY MAX(timestamp) DESC");

        append.prepare("INSERT INTO history (scheme, url) VALUES (?, ?)");
        append.setForwardOnly(true);

        search.prepare("SELECT scheme || ':' || url  \
                          FROM recently              \
                          WHERE url LIKE ?           \
                          ORDER BY count DESC");
        search.setForwardOnly(true);
    }

    ~TortaDatabase() {
        db.close();
    }

    void appendHistory(const QUrl &url) {
        append.bindValue(0, url.scheme());
        append.bindValue(1, url.url().right(url.url().length() - url.scheme().length() - 1));
        append.exec();
        append.clear();
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
    QStringListModel model;
    TortaDatabase &db;

protected:
    virtual void keyPressEvent(QKeyEvent *e) override {
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
    TortaBar(TortaDatabase &db, QWidget *parent=Q_NULLPTR)
            : QLineEdit(parent), completer(this), db(db) {
        completer.setModel(&model);
        completer.setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        setCompleter(&completer);
        connect(this, &QLineEdit::textEdited, this, &TortaBar::update);
    }

signals:
private slots:
    void update(const QString &word) {
        const QueryType type(GuessQueryType(word));
        QStringList list;

        if (type == SearchWithoutScheme)
            list << "find:" + word << "http://" + word;
        else if (type == URLWithoutScheme)
            list << "search:" + word << "find:" + word;

        list << db.searchHistory(word);
        model.setStringList(list);
    }
};


class TortaPage : public QWebEnginePage {
Q_OBJECT

protected:
    virtual bool certificateError(const QWebEngineCertificateError &error) override {
        qWarning() << "ssl error:" << error.errorDescription();
        emit sslError();
        return true;
    }

public:
    TortaPage(QObject *parent=Q_NULLPTR) : QWebEnginePage(parent) {
        profile()->setHttpUserAgent("DobosTorta");
    }

signals:
    void sslError();
};


class TortaView : public QWebEngineView {
private:
    TortaDatabase &db;

protected:
    virtual QWebEngineView *createWindow(QWebEnginePage::WebWindowType type) override;

public:
    TortaView(TortaDatabase &db, QWidget *parent=Q_NULLPTR) : QWebEngineView(parent), db(db) {
        setPage(new TortaPage());
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
    void addShortcut(QWidget *sender,
                     QKeySequence key,
                     const typename QtPrivate::FunctionPointer<Func>::Object *receiver,
                     Func method) {
        connect(new QShortcut(key, sender), &QShortcut::activated, receiver, method);
    }

    template<class Func> void addShortcut(QWidget *sender, QKeySequence key, Func functor) {
        connect(new QShortcut(key, sender), &QShortcut::activated, functor);
    }

    void setupShortcuts() {
        addShortcut(this, SHORTCUT_FORWARD,          &view, &QWebEngineView::forward);
        addShortcut(this, {Qt::ALT + Qt::Key_Right}, &view, &QWebEngineView::forward);
        addShortcut(this, SHORTCUT_BACK,             &view, &QWebEngineView::back);
        addShortcut(this, {Qt::ALT + Qt::Key_Left},  &view, &QWebEngineView::back);

        addShortcut(this, SHORTCUT_BAR,     this, &DobosTorta::toggleBar);
        addShortcut(this, SHORTCUT_FIND,    this, &DobosTorta::toggleFind);
        addShortcut(&bar, SHORTCUT_ESCAPE,  this, &DobosTorta::escapeBar);
        addShortcut(&bar, {Qt::Key_Escape}, this, &DobosTorta::escapeBar);

        addShortcut(this, SHORTCUT_DOWN,    [&]{ scroll(0, SCROLL_STEP_Y);  });
        addShortcut(this, {Qt::Key_Down},   [&]{ scroll(0, SCROLL_STEP_Y);  });
        addShortcut(this, SHORTCUT_UP,      [&]{ scroll(0, -SCROLL_STEP_Y); });
        addShortcut(this, {Qt::Key_Up},     [&]{ scroll(0, -SCROLL_STEP_Y); });
        addShortcut(this, SHORTCUT_RIGHT,   [&]{ scroll(SCROLL_STEP_X, 0);  });
        addShortcut(this, {Qt::Key_Right},  [&]{ scroll(SCROLL_STEP_X, 0);  });
        addShortcut(this, SHORTCUT_LEFT,    [&]{ scroll(-SCROLL_STEP_X, 0); });
        addShortcut(this, {Qt::Key_Left},   [&]{ scroll(-SCROLL_STEP_X, 0); });

        addShortcut(this, {Qt::Key_PageDown}, [&]{
            view.page()->runJavaScript("window.scrollBy(0, window.innerHeight / 2)");
        });
        addShortcut(this, {Qt::Key_PageUp}, [&]{
            view.page()->runJavaScript("window.scrollBy(0, -window.innerHeight / 2)");
        });

        addShortcut(this, SHORTCUT_TOP, [&]{
            view.page()->runJavaScript("window.scrollTo(0, 0);");
        });
        addShortcut(this, {Qt::Key_Home}, [&]{
            view.page()->runJavaScript("window.scrollTo(0, 0);");
        });
        addShortcut(this, SHORTCUT_BOTTOM, [&]{
            view.page()->runJavaScript("window.scrollTo(0, document.body.scrollHeight);");
        });
        addShortcut(this, {Qt::Key_End}, [&]{
            view.page()->runJavaScript("window.scrollTo(0, document.body.scrollHeight);");
        });

        addShortcut(this, SHORTCUT_ZOOMIN,  [&]{ view.setZoomFactor(view.zoomFactor() + ZOOM_STEP); });
        addShortcut(this, SHORTCUT_ZOOMOUT, [&]{ view.setZoomFactor(view.zoomFactor() - ZOOM_STEP); });

        addShortcut(this, SHORTCUT_NEXT, [&]{
            if (bar.isVisible() && GuessQueryType(bar.text()) == InSiteSearch)
                inSiteSearch(bar.text(), true);
        });
        addShortcut(this, SHORTCUT_PREV, [&]{
            if (bar.isVisible() && GuessQueryType(bar.text()) == InSiteSearch)
                inSiteSearch(bar.text(), false);
        });
    }

    void setupBar() {
        connect(&bar, &QLineEdit::textChanged, this, &DobosTorta::barChanged);
        connect(&bar, &QLineEdit::returnPressed, [&]{
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

        connect(static_cast<TortaPage *>(view.page()), &TortaPage::sslError, [&]{
            setStyleSheet("QMainWindow { background-color: " HTTPS_ERROR_FRAME_COLOR "; }");
        });

        setCentralWidget(&view);
    }

    void scroll(int x, int y) {
        view.page()->runJavaScript(QString("window.scrollBy(%1, %2)").arg(x).arg(y));
    }

    void openBar(QString prefix, QString content) {
        bar.setText(prefix + content);
        bar.setVisible(true);
        bar.setFocus(Qt::ShortcutFocusReason);
        bar.setSelection(prefix.length(), content.length());
    }

    void webSearch(const QString &queryString) {
        QUrl url(SEARCH_ENDPOINT);
        QUrlQuery query;

        query.addQueryItem(SEARCH_QUERY, queryString);
        url.setQuery(query);
        view.load(url);
    }

    void inSiteSearch(const QString &query, bool forward) {
        view.findText(query.right(query.length() - 5),
                      forward ? QWebEnginePage::FindFlags() : QWebEnginePage::FindBackward);
    }

public:
    DobosTorta(TortaDatabase &db) : bar(db, this), view(db, this), db(db) {
        setupBar();
        setupView();
        setupShortcuts();

        setContentsMargins(2, 2, 2, 2);
        setStyleSheet("QMainWindow { background-color: " DEFAULT_FRAME_COLOR "; }");
    }

    void load(const QString query) {
        switch (GuessQueryType(query)) {
        case URLWithScheme:
            view.load(query);
            break;
        case URLWithoutScheme:
            view.load("http://" + query);
            break;
        case SearchWithScheme:
            webSearch(query.right(query.length() - 7));
            break;
        case SearchWithoutScheme:
            webSearch(query);
            break;
        case InSiteSearch:
            inSiteSearch(query, true);
            break;
        }
    }

signals:
private slots:
    void barChanged() {
        const QString query(bar.text());

        if (GuessQueryType(query) == InSiteSearch)
            inSiteSearch(query, true);
        else
            view.findText("");
    }

    void linkHovered(const QUrl &url) {
        const QString str(url.toDisplayString());

        if (str.length() == 0)
            setWindowTitle(view.title());
        else
            setWindowTitle(str);
    }

    void urlChanged(const QUrl &u) {
        db.appendHistory(u);

        if (u.scheme() == "https")
            setStyleSheet("QMainWindow { background-color: " HTTPS_FRAME_COLOR "; }");
        else
            setStyleSheet("QMainWindow { background-color: " DEFAULT_FRAME_COLOR "; }");
    }

    void toggleBar() {
        if (!bar.hasFocus())
            openBar("", view.url().toDisplayString());
        else if (GuessQueryType(bar.text()) == InSiteSearch)
            openBar("", bar.text().right(bar.text().length() - 5));
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
        view.findText("");
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
    window->show();
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
        for (int i=1; i<argc; i++) {
            auto window = new DobosTorta(db);
            window->load(argv[i]);
            window->show();
        }
    } else {
        auto window = new DobosTorta(db);
        window->load(HOMEPAGE);
        window->show();
    }

    return app.exec();
}


#include "main.moc"
