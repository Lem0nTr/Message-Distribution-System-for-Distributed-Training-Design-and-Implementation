#ifndef MYEDGECLIENT_H
#define MYEDGECLIENT_H

#include <QObject>
#include<QTcpServer>
#include<QTcpSocket>
#include<QDebug>
#include<QFile>
#include<QElapsedTimer>
#include<QFileInfo>
#include<QByteArray>
#include<QTimer>
#include <QtEndian>
#include<QDir>

class MyEdgeClient  :public QObject
{
    Q_OBJECT
public:
    explicit MyEdgeClient(QObject *parent = nullptr);
    ~MyEdgeClient();


    void ConnectSlot(QString IP , QString Port);

    void readModel();

    void saveModelToFile(const QByteArray &data);

    void abortTransfer();

    void finishTransfer();

    void saveReceivedModel();

    // 连接/断开连接
    void connectToCenter(const QString &ip, quint16 port);
    void disconnectFromCenter();
    bool isConnected() const;

    // 文件传输接口
    void sendModel(const QString &modelPath);

    void sendFileData();

    void handleModelData();
signals:
    void connectionStatusChanged(bool connected);
    void transferProgress(qint64 bytesSent, qint64 totalBytes);
    //void modelSent(bool success, const QString &filename);
    void errorOccurred(const QString &error);

    void modelSent(bool success);


signals:
    void modelReceived(const QString& savedPath);
    void saveCenterModel(const QString& savedPath);
private slots:
    void onConnected();
    void onDisconnected();
    void onErrorOccurred(QAbstractSocket::SocketError error);
    void onBytesWritten(qint64 bytes);

private:
    void sendFileHeader();
    void sendNextChunk();


    //QTcpSocket * m_tcpsocket;

    bool isok;
    bool isok2;

    // QString m_IP;
    // QString m_Port;

    QString m_ip;
    quint16 m_port;

    QByteArray m_sizeData;      // 存储接收到的长度数据
    QByteArray m_receivedData;  // 存储接收到的模型数据
    qint64 m_expectedSize = 0;  // 期望接收的模型大小

    QTcpSocket *m_tcpSocket;
    QFile *m_currentFile = nullptr;
    qint64 m_fileSize = 0;
    qint64 m_bytesSent = 0;
    QString m_currentFileName;
    QTimer m_keepAliveTimer;

private:
    struct ModelTransfer {
        qint64 expectedSize = 0;  // 期望接收的文件大小
        QByteArray buffer;         // 接收缓冲区
        QString timestamp;         // 时间戳（来自服务器头部的14字节）
        QString fileName;          // 原始文件名（来自服务器头部）
        QString savePath;          // 新增：最终保存路径
        bool headerReceived = false;
    };

    ModelTransfer m_transfer;
    QString m_saveBaseDir = "./received_models";

    bool m_transferInProgress = false;
    static const qint64 CHUNK_SIZE;
private slots: // 必须使用 slots 限定符
    void handleBytesWritten(qint64 bytes);
    //static const qint64 CHUNK_SIZE = 64 * 1024; // 64KB

};

#endif // MYEDGECLIENT_H
