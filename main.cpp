#include <QApplication>
#include <QCompleter>
#include <QLineEdit>
#include <QMainWindow>
#include <QMessageLogger>
#include <QStringListModel>
#include <QVBoxLayout>
#include <QWebEngineView>
#include <qtwebengineglobal.h>


#define HOMEPAGE  "http://google.com"


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

public:
	DobosTorta() : bar(HOMEPAGE, this), view(this) {
		connect(&bar, SIGNAL(returnPressed()), this, SLOT(executeBar()));

		connect(&view, SIGNAL(titleChanged(QString)), this, SLOT(setWindowTitle(QString)));
		connect(&view, SIGNAL(urlChanged(QUrl)), this, SLOT(urlChanged(QUrl)));
		view.load(QUrl(HOMEPAGE));

		setMenuWidget(&bar);
		setCentralWidget(&view);

		bar.setCompleter(new TortaCompleter(&bar, this));
	}

signals:
private slots:
	void executeBar() {
		view.load(bar.text());
	}

	void urlChanged(const QUrl &url) {
		bar.setText(url.toString());
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
