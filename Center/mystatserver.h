#ifndef MYSTATSERVER_H
#define MYSTATSERVER_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QMap>
#include <QDebug>
#include <QDataStream>


class MyStatServer : public QObject
{ 
    Q_OBJECT
public:
    explicit MyStatServer(QObject *parent = nullptr);
    bool startServer(quint16 port);          // 启动服务器
    QString getLastMessage() const;          // 获取最后接收的消息

signals:
    void receivedMessage(QString msg);       // 收到消息时触发的信号

private slots:
    void onNewConnection();                  // 处理新连接
    void onReadyRead();                      // 读取客户端数据

private:
    QTcpServer *tcpServer;                   // TCP服务器对象
    QTcpSocket *clientSocket;                // 当前连接的客户端Socket
    QString lastMessage;                     // 存储最后接收的消息
    //quint16 blockSize;                       // 数据块大小（用于分块传输）
    quint32 blockSize = 0; // 原先是 quint16
};

#endif // MYSTATSERVER_H
