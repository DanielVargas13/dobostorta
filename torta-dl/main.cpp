#include <QApplication>
#include <QDataStream>
#include <QErrorMessage>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QIODevice>
#include <QLabel>
#include <QLocalServer>
#include <QLocalSocket>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProgressBar>
#include <QPushButton>
#include <QVBoxLayout>
#include <QWidget>


#define USER_AGENT  "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)" \
                    "Chrome/55.0.0.0 Safari/537.36 DobosTorta/dev-" GIT_VERSION

#define CONNECTION_NAME  "dobostorta-downloader.sock"


class TortaRequestHandler : public QLocalServer {
Q_OBJECT

private:
    TortaRequestHandler() {
        listen(CONNECTION_NAME);
        connect(this, &QLocalServer::newConnection, this, &TortaRequestHandler::newConnection);
    }

    void newConnection() {
        QLocalSocket *sock = nextPendingConnection();
        connect(sock, &QLocalSocket::disconnected, sock, &QLocalSocket::close);

        sock->waitForReadyRead();

        QDataStream stream(sock);
        QByteArray data;
        stream >> data;
        emit receivedRequest({data});
    }

public:
    ~TortaRequestHandler() {
        close();
    }

    static TortaRequestHandler *open() {
        auto server = new TortaRequestHandler();
        if (server->isListening())
            return server;

        delete server;
        return nullptr;
    }

    static bool request(const QUrl &url) {
        QLocalSocket sock;
        sock.connectToServer(CONNECTION_NAME);

        if (!sock.isValid()) {
            qCritical() << tr("Failed to open socket: ") << sock.errorString();
            return false;
        }

        QByteArray block;
        QDataStream stream(&block, QIODevice::WriteOnly);
        stream << url.toEncoded();
        sock.write(block);

        sock.waitForBytesWritten();

        return true;
    }

signals:
    void receivedRequest(const QUrl &url);
};


class TortaDownload : public QWidget {
Q_OBJECT

private:
    QNetworkReply * const reply;
    QVBoxLayout layout;
    QProgressBar progress;
    QPushButton button;


    void saveTo(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            (new QErrorMessage(this))->showMessage(tr("Failed open %1: %2")
                                                   .arg(path).arg(file.errorString()));
            return;
        }

        file.write(reply->readAll());
        file.close();
    }

public:
    TortaDownload(QWidget *parent, QNetworkReply *reply, const QString &filePath)
            : QWidget(parent), reply(reply), layout(this), progress(this), button("cancel", this) {
        setLayout(&layout);

        auto horizontal = new QHBoxLayout;
        layout.addLayout(horizontal);

        auto left = new QVBoxLayout;
        horizontal->addLayout(left, 1);

        QFileInfo info(filePath);
        auto path = new QHBoxLayout;
        path->setAlignment(Qt::AlignLeft);
        path->setSpacing(0);
        left->addLayout(path);
        auto fpath = new QLabel(info.dir().path() + "/", this);
        path->addWidget(fpath);
        auto fname = new QLabel(info.fileName(), this);
        fname->setStyleSheet("font-weight: bold;");
        path->addWidget(fname);

        auto url = new QLabel(QString("<a href=\"%1\">%1</a>").arg(reply->url().toString()),
                              this);
        url->setOpenExternalLinks(true);
        left->addWidget(url);

        horizontal->addWidget(&button);
        connect(&button, &QPushButton::clicked, [this, reply]{
            if (reply->isRunning())
                reply->abort();
            else
                emit clear();
        });

        layout.addWidget(&progress);

        connect(reply, &QNetworkReply::downloadProgress, [this](qint64 received, qint64 total){
            progress.setRange(0, total);
            progress.setValue(received);
        });
        connect(reply, &QNetworkReply::finished, [this, filePath, reply]{
            if (reply->error() && reply->error() != QNetworkReply::OperationCanceledError) {
                (new QErrorMessage(this))->showMessage(tr("Failed download: %1")
                                                       .arg(reply->errorString()));
            } else {
                progress.setValue(progress.maximum());
                saveTo(filePath);
            }
            button.setText("clear");
        });
    }

signals:
    void clear();
};


class TortaDL : public QWidget {
Q_OBJECT

private:
    QVBoxLayout layout;
    QNetworkAccessManager manager;
    TortaRequestHandler *handler;

protected:
    void closeEvent(QCloseEvent *e) {
        handler->close();
        QWidget::closeEvent(e);
    }
  
public:
    TortaDL(TortaRequestHandler *handler) : handler(handler) {
        setWindowTitle("Dobostorta downloader");

        layout.setAlignment(Qt::AlignTop);
        setLayout(&layout);

        connect(handler, &TortaRequestHandler::receivedRequest,
                [this](const QUrl &url){ startDownload(url); });
    }

    void startDownload(const QUrl &url, const QString &fname) {
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", USER_AGENT);

        auto dl = new TortaDownload(this, manager.get(request), fname);

        layout.addWidget(dl);

        connect(dl, &TortaDownload::clear, [this, dl]{
            layout.removeWidget(dl);
            delete dl;
        });
    }

    void startDownload(const QUrl &url) {
        const QString path(QFileDialog::getSaveFileName(this, tr("Save file"), url.fileName()));
        if (path != "")
            startDownload(url, path);
    }
};


int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);

    auto handler = TortaRequestHandler::open();
    if (handler == nullptr) {
        for (int i=1; i<argc; i++) {
            if (!TortaRequestHandler::request({argv[i]}))
                return -1;
        }

        return 0;
    }

    TortaDL win(handler);
    win.show();

    for (int i=1; i<argc; i++)
        win.startDownload({argv[i]});

    return app.exec();
}


#include "main.moc"
