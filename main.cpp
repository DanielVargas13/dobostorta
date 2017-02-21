#include <QApplication>
#include <QCompleter>
#include <QKeySequence>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageLogger>
#include <QShortcut>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <qtwebengineglobal.h>


#define HOMEPAGE       "http://google.com"
#define SEARCH_ENGINE  "http://google.com/search?q=%1"

#define SHORTCUT_META     (Qt::CTRL)

#define SHORTCUT_FORWARD  (SHORTCUT_META + Qt::Key_I)
#define SHORTCUT_BACK     (SHORTCUT_META + Qt::Key_O)
#define SHORTCUT_BAR      (SHORTCUT_META + Qt::Key_Colon)


QMessageLogger logger;


enum QueryType {
    URLWithSchema,
    URLWithoutSchema,
    SearchWithSchema,
    SearchWithoutSchema
};


QueryType GuessQueryType(const QString &str) {
    static const QRegExp hasSchema("^[a-zA-Z0-9]+://");
    static const QRegExp address("^[^/]+(\\.[^/]+|:[0-9]+)");

    if (str.startsWith("search:")) {
        return SearchWithSchema;
    } else if (hasSchema.indexIn(str) != -1) {
        return URLWithSchema;
    } else if (address.indexIn(str) != -1) {
        return URLWithoutSchema;
    } else {
        return SearchWithoutSchema;
    }
}


class TortaCompleter : public QCompleter {
Q_OBJECT

private:
    QStringListModel model;

public:
    TortaCompleter(QLineEdit *line, QObject *parent=0) : QCompleter(parent) {
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
        if (type == SearchWithoutSchema) {
            list << "http://" + word;
        } else if (type == URLWithoutSchema) {
            list << "search:" + word;
        }
        model.setStringList(list);
    }
};


class DobosTorta : public QMainWindow {
Q_OBJECT

private:
    QLineEdit bar;
    QWebEngineView view;


    void setShortcuts() {
        connect(new QShortcut(QKeySequence(SHORTCUT_FORWARD), this), &QShortcut::activated, &view, &QWebEngineView::forward);
        connect(new QShortcut(QKeySequence(SHORTCUT_BACK), this), &QShortcut::activated, &view, &QWebEngineView::back);

        connect(new QShortcut(QKeySequence(SHORTCUT_BAR), this), &QShortcut::activated, this, &DobosTorta::toggleBar);
    }

    void setupBar() {
        connect(&bar, &QLineEdit::returnPressed, this, &DobosTorta::executeBar);

        bar.setCompleter(new TortaCompleter(&bar, this));

        setMenuWidget(&bar);
    }

    void setupView() {
        connect(&view, &QWebEngineView::titleChanged, this, &QWidget::setWindowTitle);
        connect(&view, &QWebEngineView::urlChanged, this, &DobosTorta::urlChanged);

        view.load(QUrl(HOMEPAGE));

        setCentralWidget(&view);
    }

public:
    DobosTorta() : bar(HOMEPAGE, this), view(this) {
        setupBar();
        setupView();
        setShortcuts();
    }

signals:
private slots:
    void executeBar() {
        QString query(bar.text());

        switch (GuessQueryType(query)) {
        case URLWithSchema:
            view.load(query);
            break;
        case URLWithoutSchema:
            view.load("http://" + query);
            break;
        case SearchWithSchema:
            view.load(QString(SEARCH_ENGINE).arg(query.right(query.length() - 7)));
            break;
        case SearchWithoutSchema:
            view.load(QString(SEARCH_ENGINE).arg(query));
            break;
        }
    }

    void urlChanged(const QUrl &url) {
        bar.setText(url.toString());
    }

    void toggleBar() {
        if (!bar.hasFocus()) {
            bar.setFocus();
            bar.selectAll();
        } else {
            view.setFocus();
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
