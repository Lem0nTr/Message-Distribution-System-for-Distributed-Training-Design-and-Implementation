#include "mystatserver.h"


MyStatServer::MyStatServer(QObject *parent) : QObject(parent), clientSocket(nullptr), blockSize(0)
{
    tcpServer = new QTcpServer(this);
    connect(tcpServer, &QTcpServer::newConnection, this, &MyStatServer::onNewConnection);

    quint16 port = 12300;
    startServer(port);
}

bool MyStatServer::startServer(quint16 port)
{
    qDebug() << "StatServer Start on Port :" << port;
    return tcpServer->listen(QHostAddress::Any, port);
}

QString MyStatServer::getLastMessage() const
{
    return lastMessage;
}

void MyStatServer::onNewConnection()
{
    // 断开旧连接（仅支持单客户端）
    if (clientSocket) {
        clientSocket->disconnectFromHost();
        clientSocket->deleteLater();
    }

    // 获取新连接的Socket
    clientSocket = tcpServer->nextPendingConnection();
    connect(clientSocket, &QTcpSocket::readyRead, this, &MyStatServer::onReadyRead);
}

void MyStatServer::onReadyRead()
{
    QDataStream in(clientSocket);
    in.setVersion(QDataStream::Qt_5_15);

    while (clientSocket->bytesAvailable() > 0) {
        if (blockSize == 0) {
            if (clientSocket->bytesAvailable() < sizeof(quint32)) break;
            in >> blockSize;

            // 修改最大块大小为 10MB
            const quint32 MAX_BLOCK_SIZE = 10 * 1024 * 1024;
            if (blockSize > MAX_BLOCK_SIZE || in.status() != QDataStream::Ok) {
                qWarning() << "Invalid block size:" << blockSize;
                clientSocket->disconnectFromHost();
                blockSize = 0;
                return;
            }
        }

        if (clientSocket->bytesAvailable() < blockSize) break;

        QByteArray data;
        in >> data; // 读取二进制数据
        QString msg = QString::fromUtf8(data); // 明确使用 UTF-8 解码

        if (in.status() != QDataStream::Ok) {
            qWarning() << "Stream corrupted";
            clientSocket->disconnectFromHost();
            blockSize = 0;
            return;
        }

        lastMessage = msg;
        qDebug() << "接收到的状态信息为：" << msg;
        emit receivedMessage(msg);
        blockSize = 0;
    }
}
