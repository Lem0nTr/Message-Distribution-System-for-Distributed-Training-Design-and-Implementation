#ifndef MYMQTTCLIENT_H
#define MYMQTTCLIENT_H
#include<QObject>
#include<QDebug>
#include"QtMqtt/qmqttclient.h"
#include<QFile>
#include <QThread>
#include <QDateTime>
#include <QDir>
#include <QCoreApplication>

class MyMqttClient:public QObject
{
    Q_OBJECT
public:
    MyMqttClient();
    ~MyMqttClient();

    void connectToBroker();

    void publish(const QString &topic, const QByteArray &message);

    void subscribe(const QString &topic);

    void publishModel(const QString &filePath);

//public slots:  // 添加这个部分
    void onConnected();

    void onMessageReceived(const QByteArray &message, const QMqttTopicName &topic);


signals:
    void startTrainingRequested();  // 新增信号
    void modelPublished(bool success);

    void publishModelSingal(bool success);
    void ModelSaved(const QString &filePath);

private:
    QMqttClient *m_client;

    // 在客户端类中添加成员变量
    QByteArray receivedModelData;
    int expectedSize = 0;
};

#endif // MYMQTTCLIENT_H
