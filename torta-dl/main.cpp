#include <QtNetwork>
#include <QtWidgets>


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
    QTimer intervalTimer;
    QElapsedTimer elapsedTimer;


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

    static QString bytesToKMG(int bytes) {
        static const char* units[] = {"B", "KB", "MB", "GB", "TB", "PB", nullptr};
        for (int i=0; units[i+1] != nullptr; i++) {
            if (bytes < qPow(1024, i + 1)) {
                if (qPow(1024, i + 1) / 10 < bytes)
                    return QString("%1%2")
                             .arg(static_cast<float>(bytes) / qPow(1024, i + 1), 0, 'f', 1)
                             .arg(units[i + 1]);
                else if (bytes < qPow(1024, i + 1) / 100)
                    return QString("%1%2")
                             .arg(static_cast<float>(bytes) / qPow(1024, i), 0, 'f', 1)
                             .arg(units[i]);
                else
                    return QString("%1%2").arg(bytes / qPow(1024, i), 0, 'f', 0).arg(units[i]);
            }
        }
        return QString("%1PB").arg(bytes / qPow(1024, 5), 0, 'f', 0);
    }

    void setProgressBarColor(const QColor &color) {
        QPalette p;
        p.setColor(QPalette::Highlight, color);
        progress.setPalette(p);
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

        progress.setFormat("%p% [%vB / %mB]");
        setProgressBarColor(Qt::darkGray);
        layout.addWidget(&progress);

        connect(reply, &QNetworkReply::downloadProgress, [this](qint64 received, qint64 total){
            updateProgressFormat();

            progress.setRange(0, total);
            progress.setValue(received);
        });
        connect(reply, &QNetworkReply::finished, [this, filePath, reply]{
            if (reply->error() && reply->error() != QNetworkReply::OperationCanceledError) {
                (new QErrorMessage(this))->showMessage(tr("Failed download: %1")
                                                       .arg(reply->errorString()));
            } else {
                saveTo(filePath);
            }
            button.setText("clear");

            intervalTimer.stop();
            if (!reply->error()) {
                progress.setFormat(QString("done [%1]").arg(bytesToKMG(progress.maximum())));
                setProgressBarColor(Qt::gray);
            } else {
                progress.setFormat(QString("%p% [%1] %2").arg(bytesToKMG(progress.maximum()))
                                                         .arg(reply->errorString()));
                setProgressBarColor(Qt::darkRed);
            }
        });
        intervalTimer.setSingleShot(false);
        connect(&intervalTimer, &QTimer::timeout, this, &TortaDownload::updateProgressFormat);
        intervalTimer.start(1000);
        elapsedTimer.start();
    }

signals:
    void clear();

private slots:
    void updateProgressFormat() {
        const int remain = qMax(
            0.0f,
            ((progress.maximum() * elapsedTimer.elapsed()) / static_cast<float>(progress.value())
            - elapsedTimer.elapsed()) / 1000
        );

        QString remainStr;
        if (remain < 60)
            remainStr = QString("%1 sec").arg(remain);
        else if (remain < 60 * 60)
            remainStr = QString("%1' %2\"").arg(remain/60).arg(remain % 60, 2, 'd', 0, '0');
        else
            remainStr = QString("%1:%2'").arg(remain/60/60).arg(remain/60 % 60, 2, 'd', 0, '0');

        progress.setFormat("%p% " + QString("[%1 / %2] %3").arg(bytesToKMG(progress.value()))
                                                           .arg(bytesToKMG(progress.maximum()))
                                                           .arg(remainStr));
    }
};


class TortaDL : public QScrollArea {
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
        auto listArea = new QWidget(this);
        listArea->setLayout(&layout);
        setWidget(listArea);
        setWidgetResizable(true);

        connect(handler, &TortaRequestHandler::receivedRequest,
                [this](const QUrl &url){ startDownload(url); });
    }

    void startDownload(const QUrl &url, const QString &fname) {
        QNetworkRequest request(url);
        request.setRawHeader("User-Agent", USER_AGENT);

        auto dl = new TortaDownload(widget(), manager.get(request), fname);

        layout.addWidget(dl);

        connect(dl, &TortaDownload::clear, [this, dl]{
            layout.removeWidget(dl);
            delete dl;
        });
    }

    bool startDownload(const QUrl &url) {
        const QString path(QFileDialog::getSaveFileName(
            this,
            tr("Save file"),
            QFileInfo(QFileDialog().directory(), url.fileName()).absoluteFilePath()
        ));
        if (path != "")
            startDownload(url, path);
        return path != "";
    }
};


int main(int argc, char **argv) {
    if (argc == 1) {
        qWarning("Dobostorta Downloader\n$ %s URL...", argv[0]);
        return -1;
    }

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

    bool started = false;
    for (int i=1; i<argc; i++)
        started = started || win.startDownload({argv[i]});

    if (!started) {
        win.close();
        return 1;
    }

    win.show();

    return app.exec();
}


#include "main.moc"
