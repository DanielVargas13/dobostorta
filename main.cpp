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


#define HOMEPAGE  "http://google.com"

#define SHORTCUT_META     (Qt::CTRL)

#define SHORTCUT_FORWARD  (SHORTCUT_META + Qt::Key_I)
#define SHORTCUT_BACK     (SHORTCUT_META + Qt::Key_O)
#define SHORTCUT_BAR      (SHORTCUT_META + Qt::Key_Colon)


QMessageLogger logger;


class TortaCompleter : public QCompleter {
Q_OBJECT

private:
    QStringList list;
    QStringListModel model;

public:
    TortaCompleter(QLineEdit *line, QObject *parent=0) : QCompleter(parent) {
        list << "hello" << "world" << "foo" << "bar";
        model.setStringList(list);

        setModel(&model);
        setCompletionMode(QCompleter::UnfilteredPopupCompletion);
        setModelSorting(QCompleter::CaseInsensitivelySortedModel);
        setCaseSensitivity(Qt::CaseInsensitive);

        connect(line, SIGNAL(textChanged(QString)), this, SLOT(update(QString)));
    }

signals:
private slots:
    void update(const QString &word) {
        logger.debug(word.toStdString().c_str());
    }
};


class DobosTorta : public QMainWindow {
Q_OBJECT

private:
    QLineEdit bar;
    QWebEngineView view;


    void setShortcuts() {
        connect(new QShortcut(QKeySequence(SHORTCUT_FORWARD), this), SIGNAL(activated()), &view, SLOT(forward()));
        connect(new QShortcut(QKeySequence(SHORTCUT_BACK), this), SIGNAL(activated()), &view, SLOT(back()));

        connect(new QShortcut(QKeySequence(SHORTCUT_BAR), this), SIGNAL(activated()), this, SLOT(toggleBar()));
    }

    void setupBar() {
        connect(&bar, SIGNAL(returnPressed()), this, SLOT(executeBar()));

        bar.setCompleter(new TortaCompleter(&bar, this));

        setMenuWidget(&bar);
    }

    void setupView() {
        connect(&view, SIGNAL(titleChanged(QString)), this, SLOT(setWindowTitle(QString)));
        connect(&view, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));

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
        view.load(bar.text());
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
