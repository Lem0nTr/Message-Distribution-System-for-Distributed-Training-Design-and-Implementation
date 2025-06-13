#include "myedgestatclient.h"



MyEdgeStatClient::MyEdgeStatClient(QObject *parent) : QObject(parent), blockSize(0)
{
    socket = new QTcpSocket(this);
    connect(socket, &QTcpSocket::readyRead, this, &MyEdgeStatClient::onReadyRead);

    // 新增连接状态监控
    connect(socket, &QTcpSocket::disconnected, this, [this]() {
        qWarning() << "Connection closed by peer";
        socket->deleteLater();
        socket = nullptr; // 防止野指针
    });

    connect(socket, QOverload<QAbstractSocket::SocketError>::of(&QAbstractSocket::errorOccurred),
            this, [this](QAbstractSocket::SocketError error) {
                qWarning() << "Socket error:" << error;
                if (socket->state() != QAbstractSocket::ConnectedState) {
                    socket->abort();
                }
            });

    QString host = "192.168.44.129";
    quint16 port = 12300;
    connectToServer(host , port);
}

void MyEdgeStatClient::connectToServer(const QString &host, quint16 port)
{
    socket->connectToHost(host, port);
}

void MyEdgeStatClient::sendMessage(const QString &msg)
{
    if (!socket || !socket->isValid() || !socket->isWritable()) {
        qWarning() << "Socket not ready for writing";
        return;
    }

    if (!socket->isOpen()) return;

    QByteArray block;
    QDataStream out(&block, QIODevice::WriteOnly);
    out.setVersion(QDataStream::Qt_5_15);
    out << quint32(0) << msg.toUtf8(); // 明确使用 UTF-8 编码
    out.device()->seek(0);
    out << quint32(block.size() - sizeof(quint32)); // 写入 4 字节块大小

    if (socket->write(block) == -1) { // 检查写入是否失败
        qWarning() << "Write failed:" << socket->errorString();
        socket->disconnectFromHost();
    }
}

QString MyEdgeStatClient::getLastMessage() const
{
    return lastMessage;
}

void MyEdgeStatClient::onReadyRead()
{
    QDataStream in(socket);
    in.setVersion(QDataStream::Qt_5_15);

    while (socket->bytesAvailable() > 0) {
        if (blockSize == 0) {
            // 确保至少有 2 字节可读
            if (socket->bytesAvailable() < sizeof(quint16))
                break;

            in >> blockSize;

            // 检查流状态和块大小合理性
            if (in.status() != QDataStream::Ok || blockSize > 1024 * 1024) {
                qWarning() << "Invalid block size or stream error:" << blockSize;
                socket->disconnectFromHost();
                blockSize = 0;
                return;
            }
        }

        // 检查剩余数据是否足够
        if (socket->bytesAvailable() < blockSize)
            break;

        // 读取消息内容
        QString msg;
        in >> msg;

        // 检查流状态
        if (in.status() != QDataStream::Ok) {
            qWarning() << "Message read failed";
            socket->disconnectFromHost();
            blockSize = 0;
            return;
        }

        lastMessage = msg;
        emit receivedMessage(msg);
        blockSize = 0; // 重置为下一个消息做准备
    }
}
