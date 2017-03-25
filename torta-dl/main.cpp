#include <QtNetwork>
#include <QtWidgets>


#define USER_AGENT  "Mozilla/5.0 (X11; Linux x86_64) AppleWebKit/537.36 (KHTML, like Gecko)" \
                    "Chrome/55.0.0.0 Safari/537.36 Dobostorta/" GIT_VERSION

#define CONNECTION_NAME  "dobostorta-downloader.sock"


class TortaRequestHandler : public QLocalServer {
    Q_OBJECT


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


    QNetworkReply * const reply;
    QVBoxLayout layout;
    QProgressBar progress;
    QPushButton actionButton;
    QPushButton clearButton;
    QTimer intervalTimer;
    QElapsedTimer elapsedTimer;


    void saveTo(const QString &path) {
        QFile file(path);
        if (!file.open(QIODevice::WriteOnly)) {
            QMessageBox message(QMessageBox::Critical,
                                reply->url().toString(),
                                tr("Failed create %1\n%2").arg(path, file.errorString()),
                                QMessageBox::Retry | QMessageBox::Abort,
                                this);
            if (message.exec() == QMessageBox::Retry)
                saveTo(path);
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

    void finished(const QString &filePath) {
        if (reply->error() && reply->error() != QNetworkReply::OperationCanceledError) {
            QMessageBox message(QMessageBox::Critical,
                                reply->url().toString(),
                                tr("Failed download\n%1").arg(reply->errorString()),
                                QMessageBox::Retry | QMessageBox::Abort,
                                this);
            if (message.exec() == QMessageBox::Retry)
                emit retry();
        }
        clearButton.show();

        intervalTimer.stop();
        if (!reply->error()) {
            saveTo(filePath);
            progress.setFormat(QString("done [%1]").arg(bytesToKMG(progress.maximum())));
            setProgressBarColor(Qt::gray);
            actionButton.setText("open");
        } else {
            progress.setFormat(QString("%p% [%1] %2").arg(bytesToKMG(progress.maximum()))
                                                     .arg(reply->errorString()));
            setProgressBarColor(Qt::darkRed);
            actionButton.setText("retry");
        }
    }

public:
    TortaDownload(QWidget *parent, QNetworkReply *reply, const QString &filePath)
            : QWidget(parent), reply(reply), layout(this), progress(this),
              actionButton("cancel", this), clearButton("clear", this) {
        setLayout(&layout);

        auto horizontal = new QHBoxLayout;
        layout.addLayout(horizontal);

        auto left = new QVBoxLayout;
        horizontal->addLayout(left, 1);

        QFileInfo info(filePath);
        auto path = new QLabel(info.dir().path() + "/<b>" + info.fileName() + "</b>", this);
        path->setWordWrap(true);
        left->addWidget(path);

        auto url = new QLabel(QString("<a href=\"%1\">%1</a>").arg(reply->url().toString()), this);
        url->setOpenExternalLinks(true);
        url->setWordWrap(true);
        left->addWidget(url);

        horizontal->addWidget(&actionButton);
        horizontal->addWidget(&clearButton);
        clearButton.hide();
        connect(&actionButton, &QPushButton::clicked, [this, reply, filePath]{
            if (reply->isRunning())
                reply->abort();
            else if (reply->error())
                emit retry();
            else
                QDesktopServices::openUrl(QUrl("file://" + filePath));
        });
        connect(&clearButton,  &QPushButton::clicked, [this]{ emit clear(); });

        progress.setFormat("%p% [%vB / %mB]");
        setProgressBarColor(Qt::darkGray);
        layout.addWidget(&progress);

        connect(reply, &QNetworkReply::downloadProgress, [this](qint64 received, qint64 total){
            updateProgressFormat();

            progress.setRange(0, total);
            progress.setValue(received);
        });
        connect(reply, &QNetworkReply::finished, [this, filePath]{ finished(filePath); });
        intervalTimer.setSingleShot(false);
        connect(&intervalTimer, &QTimer::timeout, this, &TortaDownload::updateProgressFormat);
        intervalTimer.start(1000);
        elapsedTimer.start();
    }

signals:
    void clear();
    void retry();

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


    QVBoxLayout layout;
    QNetworkAccessManager manager;
    TortaRequestHandler * const handler;

protected:
    void closeEvent(QCloseEvent *e) override {
        handler->close();
        QWidget::closeEvent(e);
    }
  
public:
    TortaDL(TortaRequestHandler *handler) : handler(handler) {
        setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
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

        connect(dl, &TortaDownload::retry, [this, url, fname]{ startDownload(url, fname); });
        connect(dl, &TortaDownload::clear, [this, dl]{
            layout.removeWidget(dl);
            delete dl;
        });
    }

    bool startDownload(const QUrl &url) {
        const QString filter(QMimeDatabase().mimeTypeForFile(url.fileName()).filterString());
        const QString path(QFileDialog::getSaveFileName(
            this,
            tr("Save file"),
            QFileInfo(QFileDialog().directory(), url.fileName()).absoluteFilePath(),
            filter + tr(";; All files (*)")
        ));
        if (path != "")
            startDownload(url, path);
        return path != "";
    }
};


int main(int argc, char **argv) {
    QCoreApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
    QApplication app(argc, argv);
    app.setApplicationName("Torta-DL");
    app.setApplicationVersion(GIT_VERSION);

    QCommandLineParser parser;
    parser.addPositionalArgument("URL...", "URL that you want download.");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.process(app.arguments());

    if (parser.positionalArguments().empty())
        parser.showHelp(-1);

    auto handler = TortaRequestHandler::open();
    if (handler == nullptr) {
        for (auto url: parser.positionalArguments()) {
            if (!TortaRequestHandler::request({url}))
                return -1;
        }

        return 0;
    }

    TortaDL win(handler);

    bool started = false;
    for (auto url: parser.positionalArguments())
        started = started || win.startDownload({url});

    if (!started) {
        win.close();
        return 1;
    }

    win.show();

    return app.exec();
}


#include "main.moc"
