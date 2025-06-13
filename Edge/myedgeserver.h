#ifndef MYEDGESERVER_H
#define MYEDGESERVER_H

#include<QTcpServer>
#include<QTcpSocket>
#include<QDebug>
#include<QObject>
#include"QtMqtt/qmqttclient.h"
#include<QFile>
#include <QThread>
#include <QDir>
#include<QCoreApplication>
#include<QTimer>

class MyEdgeServer
    : public QObject{
    Q_OBJECT
public:
    //MyEdgeServer();
    explicit MyEdgeServer(QObject *parent = nullptr);
    ~MyEdgeServer();

    void ConnectSuccessSlot();

    void ReceiveSlot(const QByteArray &ba , const QMqttTopicName & topic);

    void publishModel(const QString &filePath);

    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);

    void onConnected();

    void subscribe(const QString &topic);

    void startListening();

    void cleanupOldRounds(const QString &clientId, int keepNum);

    //void publishModel(const QString &filePath);

    QString getCurrentRoundId() const { return m_currentRoundId; }

    //void startListening();
    QMqttClient *client;

    // 在客户端类中添加成员变量
    QByteArray receivedModelData;
    int expectedSize = 0;

    //QByteArray receivedModelData;  // 用于最终合并的数据
    QList<QByteArray> receivedChunks; // 新增：按顺序存储分块
    //int expectedSize = 0;

    QString m_receiveDir;  // 接收文件存储目录
    QDateTime m_roundStartTime;

    QTimer * m_roundTimer;

    QString m_currentRoundId; // 当前全局轮次ID
signals:
    void modelReceived(const QString &clientId, const QString &modelPath);

};

#endif // MYEDGESERVER_H
