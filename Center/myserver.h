#ifndef MYSERVER_H
#define MYSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QFile>
#include <QDir>
#include <QDateTime>
#include<QTimer>
#include <QWeakPointer>
#include <QMutex>
#include<QPointer>
#include<QRegularExpression>
#include<QThread>
#include<QFileInfo>
#include<QTemporaryFile>
#include<QElapsedTimer>
#include<QtEndian>

class MyServer : public QObject
{
    Q_OBJECT
public:
    explicit MyServer(QObject *parent = nullptr);
    ~MyServer();

    // 基础网络功能
    void Listen(const QString &port);
    //void SendMessage(const QString &message);
    void DisConnect();

    void startHeartbeat();

    // 模型传输功能
    void sendModel(const QString &modelPath);
    QString currentModelPath() const { return m_currentModelPath; }
    void setCurrentModelPath(const QString &path) { m_currentModelPath = path; }

    void checkClientConnections();
    void cleanupClient(QTcpSocket* client);

    QString generateStoragePath(const QString& originalName);

    QString generateModelSavePath(const QString& fileName);

    void broadcastModel(const QString& modelPath);

public slots:
    void sendModelToClient(QTcpSocket* client, const QString& modelPath);
signals:

    void modelsReceived(const QStringList &modelPaths);
    void modelReceived(const QString &modelPath);
    void modelReceiveFailed(const QString& clientAddress, const QString& error);
    void newClientConnected(QString address);
    void clientDisconnected(QString address);
    // 新增传输进度信号
    void transferProgress(const QString& clientIP, qint64 sent, qint64 total);


private slots:
    void handleNewConnection();
    void handleClientReadyRead(QTcpSocket* client);  // 添加参数
    void handleClientDisconnected(QTcpSocket* client); // 添加参数
    void checkTransfers();
private:
    // 网络相关成员
    QTcpServer *m_tcpServer;
    QMap<QTcpSocket*, QSharedPointer<QTcpSocket>> m_clients;
    //QMap<QTcpSocket*, QWeakPointer<QTcpSocket>> m_clients;

    QTimer *m_heartbeatTimer;
    QTimer m_transferTimer;
    // 模型传输相关结构体
    struct ModelTransfer {
        QByteArray buffer;
        qint64 expectedSize = 0;
        qint64 receivedSize = 0;
        QString fileName;
        bool headerReceived = false;
        qint64 startTime = 0;
        QElapsedTimer lastUpdateTimer;  // 新增：计时器对象
    };

    QMap<QTcpSocket*, ModelTransfer> m_transfers;
    QString m_currentModelPath;
    QString m_modelStorageDir = "./received_models";
    QString m_currentModelName;  // 当前模型文件名
    //QString m_currentModelPath;  // 当前模型完整路径

    // 私有方法

    void processIncomingData(QTcpSocket *client, const QByteArray &data);
    QString generateStoragePath();
    void initializeModelTransfer(QTcpSocket *client);
    void saveReceivedModel(QTcpSocket *client);
private:
    mutable QMutex m_clientsMutex;
// private:
//     QString m_currentModelPath;
// public:
//     QString currentModelPath() const { return m_currentModelPath; }
//     void setCurrentModelPath(const QString &path) { m_currentModelPath = path; }
};
#endif // MYSERVER_H
