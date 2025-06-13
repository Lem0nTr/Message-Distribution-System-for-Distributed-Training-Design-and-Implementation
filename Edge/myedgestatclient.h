#ifndef MYEDGESTATCLIENT_H
#define MYEDGESTATCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QDebug>
#include <QDataStream>

class MyEdgeStatClient : public QObject
{
    Q_OBJECT
public:
    explicit MyEdgeStatClient(QObject *parent = nullptr);
    void connectToServer(const QString &host, quint16 port);  // 连接服务器
    void sendMessage(const QString &msg);                    // 发送消息
    QString getLastMessage() const;                          // 获取最后接收的消息

signals:
    void receivedMessage(QString msg);                       // 收到消息时触发的信号

private slots:
    void onReadyRead();                                      // 读取服务器数据

private:
    QTcpSocket *socket;                                      // TCP客户端Socket
    QString lastMessage;                                     // 存储最后接收的消息
    quint16 blockSize;
};

#endif // MYEDGESTATCLIENT_H
