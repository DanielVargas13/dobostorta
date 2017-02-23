#include <QApplication>
#include <QCompleter>
#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageLogger>
#include <QShortcut>
#include <QStandardPaths>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QWebEngineProfile>
#include <QWebEngineView>
#include <QtSql/QSqlDatabase>
#include <QtSql/QSqlError>
#include <QtSql/QSqlQuery>
#include <qtwebengineglobal.h>


#define HOMEPAGE       "http://google.com"
#define SEARCH_ENGINE  "http://google.com/search?q=%1"

#define HTTPS_FRAME_COLOR        "#00ff00"
#define HTTPS_ERROR_FRAME_COLOR  "#ff0000"
#define DEFAULT_FRAME_COLOR      "#808080"

#define SHORTCUT_META     (Qt::CTRL)

#define SHORTCUT_FORWARD  (SHORTCUT_META + Qt::Key_I)
#define SHORTCUT_BACK     (SHORTCUT_META + Qt::Key_O)
#define SHORTCUT_BAR      (SHORTCUT_META + Qt::Key_Colon)
#define SHORTCUT_FIND     (SHORTCUT_META + Qt::Key_Slash)
#define SHORTCUT_DOWN     (SHORTCUT_META + Qt::Key_J)
#define SHORTCUT_UP       (SHORTCUT_META + Qt::Key_K)
#define SHORTCUT_LEFT     (SHORTCUT_META + Qt::Key_H)
#define SHORTCUT_RIGHT    (SHORTCUT_META + Qt::Key_L)
#define SHORTCUT_ZOOMIN   (SHORTCUT_META + Qt::Key_Plus)
#define SHORTCUT_ZOOMOUT  (SHORTCUT_META + Qt::Key_Minus)

#define SCROLL_STEP_X  "20"
#define SCROLL_STEP_Y  "20"
#define ZOOM_STEP      0.1


QMessageLogger logger;


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

    if (str.startsWith("search:")) {
        return SearchWithScheme;
    } else if (str.startsWith("find:")) {
        return InSiteSearch;
    } else if (hasScheme.indexIn(str) != -1) {
        return URLWithScheme;
    } else if (address.indexIn(str) != -1) {
        return URLWithoutScheme;
    } else {
        return SearchWithoutScheme;
    }
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

        QSqlQuery(db).exec("CREATE TABLE IF NOT EXISTS history (timestamp TIMESTAMP DEFAULT CURRENT_TIMESTAMP, scheme TEXT NOT NULL, url TEXT NOT NULL)");
        QSqlQuery(db).exec("CREATE VIEW IF NOT EXISTS recently AS SELECT MAX(timestamp) last_access, COUNT(timestamp) count, scheme, url FROM history GROUP BY scheme || url ORDER BY MAX(timestamp) DESC");

        append.prepare("INSERT INTO history (scheme, url) values (?, ?)");
        append.setForwardOnly(true);

        search.prepare("SELECT scheme || ':' || url FROM recently WHERE url LIKE ? ORDER BY count DESC");
        search.setForwardOnly(true);
    }

    ~TortaDatabase() {
        db.close();
    }

    void appendHistory(const QUrl &url) {
        append.bindValue(0, url.scheme());
        append.bindValue(1, url.url().right(url.url().length() - url.scheme().length() - 1));
        append.exec();
    }

    QStringList searchHistory(QString query) {
        QStringList r;

        search.bindValue(0, "%%" + query.replace("%%", "\\%%") + "%%");
        search.exec();
        while (search.next()) {
            r << search.value(0).toString();
        }

        return r;
    }
};


class TortaCompleter : public QCompleter {
Q_OBJECT

private:
    QStringListModel model;
    TortaDatabase &db;

public:
    TortaCompleter(QLineEdit *line, TortaDatabase &db, QObject *parent=0) : QCompleter(parent), db(db) {
        setModel(&model);
        setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        setModelSorting(QCompleter::CaseInsensitivelySortedModel);
        setCaseSensitivity(Qt::CaseInsensitive);

        connect(line, &QLineEdit::textChanged, this, &TortaCompleter::update);
    }

signals:
private slots:
    void update(const QString &word) {
        QStringList list;
        auto type = GuessQueryType(word);
        if (type == SearchWithoutScheme) {
            list << "find:" + word << "http://" + word;
        } else if (type == URLWithoutScheme) {
            list << "search:" + word << "find:" + word;
        }
        list << db.searchHistory(word);
        model.setStringList(list);
    }
};


class TortaPage : public QWebEnginePage {
    Q_OBJECT

protected:
    virtual bool certificateError(const QWebEngineCertificateError &error) {
        logger.warning(("ssl error: " + error.errorDescription().toStdString()).c_str());
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


class DobosTorta : public QMainWindow {
Q_OBJECT

private:
    QLineEdit bar;
    QWebEngineView view;
    TortaDatabase db;


    void setShortcuts() {
        connect(new QShortcut(QKeySequence(SHORTCUT_FORWARD), this), &QShortcut::activated, &view, &QWebEngineView::forward);
        connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_Right), this), &QShortcut::activated, &view, &QWebEngineView::forward);
        connect(new QShortcut(QKeySequence(SHORTCUT_BACK), this), &QShortcut::activated, &view, &QWebEngineView::back);
        connect(new QShortcut(QKeySequence(Qt::ALT + Qt::Key_Left), this), &QShortcut::activated, &view, &QWebEngineView::back);

        connect(new QShortcut(QKeySequence(SHORTCUT_BAR), this), &QShortcut::activated, this, &DobosTorta::toggleBar);
        connect(new QShortcut(QKeySequence(SHORTCUT_FIND), this), &QShortcut::activated, this, &DobosTorta::toggleFind);

        connect(new QShortcut(QKeySequence(SHORTCUT_DOWN), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(0, " SCROLL_STEP_Y ")"); });
        connect(new QShortcut(QKeySequence(Qt::Key_Down), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(0, " SCROLL_STEP_Y ")"); });
        connect(new QShortcut(QKeySequence(SHORTCUT_UP), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(0, -" SCROLL_STEP_Y ")"); });
        connect(new QShortcut(QKeySequence(Qt::Key_Up), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(0, -" SCROLL_STEP_Y ")"); });
        connect(new QShortcut(QKeySequence(SHORTCUT_RIGHT), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(" SCROLL_STEP_X ", 0)"); });
        connect(new QShortcut(QKeySequence(Qt::Key_Right), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(" SCROLL_STEP_X ", 0)"); });
        connect(new QShortcut(QKeySequence(SHORTCUT_LEFT), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(-" SCROLL_STEP_X ", 0)"); });
        connect(new QShortcut(QKeySequence(Qt::Key_Left), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(-" SCROLL_STEP_X ", 0)"); });

        connect(new QShortcut(QKeySequence(Qt::Key_PageDown), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(0, window.innerHeight / 2)"); });
        connect(new QShortcut(QKeySequence(Qt::Key_PageUp), this), &QShortcut::activated, [&](){ view.page()->runJavaScript("window.scrollBy(0, -window.innerHeight / 2)"); });

        connect(new QShortcut(QKeySequence(SHORTCUT_ZOOMIN), this), &QShortcut::activated, [&](){ view.setZoomFactor(view.zoomFactor() + ZOOM_STEP); });
        connect(new QShortcut(QKeySequence(SHORTCUT_ZOOMOUT), this), &QShortcut::activated, [&](){ view.setZoomFactor(view.zoomFactor() - ZOOM_STEP); });
    }

    void setupBar() {
        connect(&bar, &QLineEdit::textChanged, this, &DobosTorta::barChanged);
        connect(&bar, &QLineEdit::returnPressed, this, &DobosTorta::executeBar);

        bar.setCompleter(new TortaCompleter(&bar, db, this));

        bar.setVisible(false);
        setMenuWidget(&bar);
    }

    void setupView() {
        auto page = new TortaPage();
        view.setPage(page);

        connect(&view, &QWebEngineView::titleChanged, this, &QWidget::setWindowTitle);
        connect(&view, &QWebEngineView::urlChanged, this, &DobosTorta::urlChanged);
        connect(view.page(), &QWebEnginePage::linkHovered, this, &DobosTorta::linkHovered);
        connect(view.page(), &QWebEnginePage::iconChanged, this, &DobosTorta::setWindowIcon);

        connect(page, &TortaPage::sslError, [&](){ setStyleSheet("QMainWindow { background-color: " HTTPS_ERROR_FRAME_COLOR "; }"); });

        view.load(QUrl(HOMEPAGE));

        setCentralWidget(&view);
    }

public:
    DobosTorta() : bar(HOMEPAGE, this), view(this) {
        setupBar();
        setupView();
        setShortcuts();

        setContentsMargins(2, 2, 2, 2);
        setStyleSheet("QMainWindow { background-color: " DEFAULT_FRAME_COLOR "; }");
    }

signals:
private slots:
    void executeBar() {
        const QString query(bar.text());

        switch (GuessQueryType(query)) {
        case URLWithScheme:
            view.load(query);
            bar.setVisible(false);
            break;
        case URLWithoutScheme:
            view.load("http://" + query);
            bar.setVisible(false);
            break;
        case SearchWithScheme:
            view.load(QString(SEARCH_ENGINE).arg(query.right(query.length() - 7)));
            bar.setVisible(false);
            break;
        case SearchWithoutScheme:
            view.load(QString(SEARCH_ENGINE).arg(query));
            bar.setVisible(false);
            break;
        case InSiteSearch:
            view.page()->findText(query.right(query.length() - 5));
            break;
        }
    }

    void barChanged() {
        const QString query(bar.text());
        if (GuessQueryType(query) == InSiteSearch) {
            view.findText(query.right(query.length() - 5));
        } else {
            view.findText("");
        }
    }

    void linkHovered(const QUrl &url) {
        auto str(url.toDisplayString());

        if (str.length() == 0) {
            setWindowTitle(view.title());
        } else {
            setWindowTitle(str);
        }
    }

    void urlChanged(const QUrl &u) {
        db.appendHistory(u);

        if (u.scheme() == "https") {
            setStyleSheet("QMainWindow { background-color: " HTTPS_FRAME_COLOR "; }");
        } else {
            setStyleSheet("QMainWindow { background-color: " DEFAULT_FRAME_COLOR "; }");
        }
    }

    void toggleBar() {
        if (!bar.hasFocus()) {
            bar.setVisible(true);
            bar.setText(view.url().toDisplayString());
            bar.setFocus(Qt::ShortcutFocusReason);
       } else if (GuessQueryType(bar.text()) == InSiteSearch) {
           view.findText("");
           bar.setText(bar.text().right(bar.text().length() - 5));
           bar.selectAll();
        } else {
            view.setFocus(Qt::ShortcutFocusReason);
            bar.setVisible(false);
        }
    }

    void toggleFind() {
        if (!bar.hasFocus()) {
            bar.setVisible(true);
            bar.setFocus(Qt::ShortcutFocusReason);
            bar.setText("find:");
        } else if (GuessQueryType(bar.text()) != InSiteSearch) {
            bar.setText("find:" + bar.text());
            bar.setSelection(5, bar.text().length());
        } else {
            view.findText("");
            view.setFocus(Qt::ShortcutFocusReason);
            bar.setVisible(false);
        }
    }
};


int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    QtWebEngine::initialize();

    DobosTorta window;
    window.show();

    return app.exec();
}


#include "main.moc"
