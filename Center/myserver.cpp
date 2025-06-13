#include "myserver.h"
#include <QDebug>

MyServer::MyServer(QObject *parent)
    : QObject(parent),
    m_tcpServer(new QTcpServer(this))
{
    connect(m_tcpServer, &QTcpServer::newConnection, this, &MyServer::handleNewConnection);
    QDir().mkpath(m_modelStorageDir);

    //connect(m_socket, &QTcpSocket::readyRead, this, &Client::receiveFile);
    // 在中心服务器的连接处理部分
    //connect(socket, &QTcpSocket::readyRead, this, &CenterServer::handleReadyRead);
    // 在构造函数中初始化
    m_transferTimer.setInterval(30000); // 30秒超时
    connect(&m_transferTimer, &QTimer::timeout, this, &MyServer::checkTransfers);


}

MyServer::~MyServer(){

}

void MyServer::Listen(const QString &port)
{

    if (!m_tcpServer->listen(QHostAddress::Any, port.toUShort())) {
        qDebug() << "Listen failed:" << m_tcpServer->errorString();
    } else {
        qDebug() << "Listening on port:" << port;
    }

}


void MyServer::handleNewConnection() {
    QTcpSocket* rawClient = m_tcpServer->nextPendingConnection();
    if (!rawClient) return;

    // 使用共享指针管理生命周期，并指定删除方式
    QSharedPointer<QTcpSocket> clientPtr(rawClient, &QObject::deleteLater);

    // 立即存入容器（原子操作）
    {
        QMutexLocker locker(&m_clientsMutex);
        m_clients.insert(rawClient, clientPtr);
    }

    qDebug() << "New client connected:" << rawClient->peerAddress().toString()
             << "| Ptr:" << rawClient;

    // 使用弱指针捕获以避免循环引用
    QWeakPointer<QTcpSocket> weakPtr(clientPtr);

    connect(rawClient, &QTcpSocket::readyRead, this, [this, weakPtr]() {
        if (auto strongRef = weakPtr.toStrongRef()) {
            handleClientReadyRead(strongRef.data());
        }
    }, Qt::QueuedConnection);

    // 连接信号时强制使用队列连接
    connect(rawClient, &QTcpSocket::disconnected, this, [this, weakPtr]() {
        if (auto strongPtr = weakPtr.toStrongRef()) {
            QMetaObject::invokeMethod(this, [this, strongPtr]() {
                handleClientDisconnected(strongPtr.data());
            }, Qt::QueuedConnection); // 确保在事件队列中处理
        }
    }, Qt::DirectConnection);

    connect(rawClient, &QTcpSocket::errorOccurred, this, [weakPtr](QAbstractSocket::SocketError error) {
        if (auto strongRef = weakPtr.toStrongRef()) {
            qDebug() << "Socket error from" << strongRef->peerAddress().toString()
            << ":" << error;
        }
    }, Qt::QueuedConnection);


    //m_clients.insert(rawClient, client);
    //qDebug() << "New client connected:" << rawClient->peerAddress().toString();

    qDebug() << "New client connected:" << rawClient->peerAddress().toString()
             << "| Socket state:" << rawClient->state()
             << "| Bytes available:" << rawClient->bytesAvailable();

    //調試用測試發送模型
    //QString str ="/home/stl/qt/qtdata/Central_Server/yolov8n.pt";
    //broadcastModel(str);
}

// void MyServer::handleClientReadyRead(QTcpSocket *client)
// {
//     // 关键修改：持续读取直到无数据
//     while (client->bytesAvailable() > 0) {
//         QByteArray data = client->readAll();
//         processIncomingData(client, data);

//         // 添加延迟等待更多数据（针对大文件）
//         if (client->bytesAvailable() == 0) {
//             client->waitForReadyRead(100); // 等待100ms
//         }
//     }
// }

void MyServer::handleClientReadyRead(QTcpSocket *client)
{
    // 修改为只读一次数据，避免自动断开
    if (client->bytesAvailable() > 0) {
        QByteArray data = client->readAll();
        processIncomingData(client, data);
    }
}

void MyServer::initializeModelTransfer(QTcpSocket *client)
{
    ModelTransfer transfer;
    transfer.startTime = QDateTime::currentSecsSinceEpoch();
    m_transfers.insert(client, transfer);
}


void MyServer::processIncomingData(QTcpSocket* client, const QByteArray& data)
{
    ModelTransfer& transfer = m_transfers[client];
    // 如果是第一次接收数据，启动计时器
    if (transfer.receivedSize == 0) {
        transfer.lastUpdateTimer.start();  // 启动计时器
    } else {
        transfer.lastUpdateTimer.restart();  // 重置计时器
    }

    transfer.buffer.append(data);
    transfer.receivedSize = transfer.buffer.size();  // 实时更新
    // 调试输出（十六进制查看前16字节）
    qDebug() << "[Raw Data] First 16 bytes:" << data.left(16).toHex();


    // 包头处理
    if (!transfer.headerReceived) {
        if (transfer.buffer.size() >= sizeof(quint64)) {
            QDataStream stream(transfer.buffer);
            //stream.setByteOrder(QDataStream::BigEndian);
            //stream >> transfer.expectedSize;

            // 改为手动处理字节序
            quint64 rawSize;
            stream.readRawData(reinterpret_cast<char*>(&rawSize), sizeof(quint64));
            transfer.expectedSize = qFromBigEndian(rawSize); // 关键修正！

            // 增加恶意数据校验
            const quint64 MAX_FILE_SIZE = 100 * 1024 * 1024; // 100MB
            if (transfer.expectedSize == 0 || transfer.expectedSize > MAX_FILE_SIZE) {
                qCritical() << "[Invalid Header] Size:" << transfer.expectedSize;
                client->disconnectFromHost();
                m_transfers.remove(client);
                return;
            }

            // 新增：校验文件大小有效性
            if (transfer.expectedSize <= 0 || transfer.expectedSize > 100*1024*1024) {
                qCritical() << "[Invalid Header] Size:" << transfer.expectedSize;
                client->disconnectFromHost();
                return;
            }

            transfer.headerReceived = true;
            transfer.buffer = transfer.buffer.mid(sizeof(quint64));
            qDebug() << "[Header] Expecting:" << transfer.expectedSize << "bytes";
        }
        return;
    }

    // 数据接收完成时触发保存
    if (transfer.headerReceived && transfer.receivedSize == transfer.expectedSize) {
        qDebug() << "[Complete] All data received. Triggering save...";
        saveReceivedModel(client); // 调用保存函数
        m_transfers.remove(client); // 清理传输状态
        //client->disconnectFromHost(); // 可选：关闭连接
    }

    if (transfer.headerReceived) {
        // 增加越界检查（防御性编程）
        const quint64 MAX_BUFFER_SIZE = 2 * transfer.expectedSize; // 允许少量溢出
        if (transfer.receivedSize > MAX_BUFFER_SIZE) {
            qCritical() << "[Buffer Overflow] Received:" << transfer.receivedSize
                        << "Max allowed:" << MAX_BUFFER_SIZE;
            client->abort();
            m_transfers.remove(client);
            return;
        }
    }

    // 进度跟踪
    qDebug() << "[Progress]" << transfer.receivedSize << "/" << transfer.expectedSize
             << "(" << (transfer.receivedSize*100/transfer.expectedSize) << "%)";



    if (transfer.headerReceived) {
        if (transfer.receivedSize > transfer.expectedSize) {
            qCritical() << "[Data Overflow] Expected:" << transfer.expectedSize
                        << "Actual:" << transfer.receivedSize;
            client->disconnectFromHost();
            return;
        }
        // 超时机制（示例：30秒无进展则断开）
        if (transfer.lastUpdateTimer.hasExpired(30 * 1000)) {  // 30秒超时
            qCritical() << "[Timeout] No data progress for 30 seconds. Disconnecting client.";
            client->disconnectFromHost();
            m_transfers.remove(client);
            return;
        }
    }


}



void MyServer::saveReceivedModel(QTcpSocket* client) {
    if (!m_transfers.contains(client)) return;

    ModelTransfer& transfer = m_transfers[client];
    qDebug() << "[Saving] Buffer size:" << transfer.buffer.size()
             << "Expected:" << transfer.expectedSize;

    // 生成带时间戳的保存路径
    QString fileName = transfer.fileName.isEmpty() ?
                           "model_" + QDateTime::currentDateTime().toString("hhmmss") + ".pt" :
                           transfer.fileName;
    QString savePath = generateModelSavePath(fileName);

    // 原子写入流程
    QTemporaryFile tmpFile(savePath + ".tmp");
    if (!tmpFile.open()) {
        qCritical() << "[Error] Cannot create temp file:" << tmpFile.errorString();
        return;
    }

    // 写入临时文件
    qint64 written = tmpFile.write(transfer.buffer);
    tmpFile.close();

    // 校验完整性
    if (written != transfer.expectedSize) {
        qCritical() << "[Error] Temp file incomplete. Expected:"
                    << transfer.expectedSize << "Actual:" << written;
        tmpFile.remove();
        return;
    }

    // 重命名为目标文件
    if (!QFile::rename(tmpFile.fileName(), savePath)) {
        qCritical() << "[Error] Rename failed. Temp:" << tmpFile.fileName()
        << "Target:" << savePath << "Error:" << tmpFile.errorString();
        return;
    }

    qDebug() << "[Success] Saved to:" << savePath;
    emit modelReceived(savePath);
    //m_transfers.remove(client);
}


void MyServer::sendModel(const QString& modelPath)
{
    QFile file(modelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Failed to open model file:" << modelPath;
        return;
    }

    QByteArray fileData = file.readAll();
    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);
    stream << static_cast<qint64>(fileData.size());

    foreach (auto clientPtr, m_clients) {
        if (clientPtr->state() == QTcpSocket::ConnectedState) {
            clientPtr->write(header + fileData);
            if (!clientPtr->waitForBytesWritten(3000)) {
                qWarning() << "Failed to send model to client";
            }
        }
    }
}


void MyServer::DisConnect()
{
    qDebug() << "Initiating server shutdown...";

    // 先关闭监听
    if (m_tcpServer->isListening()) {
        m_tcpServer->close();
        qDebug() << "Server stopped listening";
    }

    // 断开所有客户端连接
    auto clients = m_clients.keys();
    for (auto client : clients) {
        if (client->state() != QAbstractSocket::UnconnectedState) {
            qDebug() << "Disconnecting client:" << client->peerAddress().toString();
            client->disconnectFromHost();
            if (client->state() != QAbstractSocket::UnconnectedState) {
                client->waitForDisconnected(1000);
            }
        }
        // 通过容器自动清理资源
        m_clients.remove(client);
        m_transfers.remove(client);
    }

    qDebug() << "Server shutdown completed";
}




void MyServer::handleClientDisconnected(QTcpSocket* rawClient)
{
    // 关键修改：取消自动资源清理
    if (m_clients.contains(rawClient)) {
        qDebug() << "Client connection closed by peer, keeping socket instance";
        m_clients.remove(rawClient);
        m_transfers.remove(rawClient);
        rawClient->deleteLater(); // 延迟释放资源
    }
}

void MyServer::checkClientConnections()
{
    auto clients = m_clients.keys();
    foreach (auto client, clients) {
        // 发送心跳包
        if (client->state() == QTcpSocket::ConnectedState) {
            if (!client->write("\x00", 1)) { // 发送单字节心跳
                qDebug() << "Heartbeat failed for" << client->peerAddress().toString();
                client->abort();
            }
        }
    }
}

// QString MyServer::generateStoragePath()
// {
//     return QString("%1/model_%2")
//     .arg(m_modelStorageDir)
//         .arg(QDateTime::currentDateTime().toString("yyyyMMdd_hhmmsszzz"));
// }

QString MyServer::generateStoragePath(const QString& originalName)
{
    QDateTime now = QDateTime::currentDateTime();
    QString timestamp = now.toString("yyyyMMdd_hhmmsszzz");

    // 创建副本进行清理操作
    QString sanitized = originalName;
    sanitized.replace(QRegularExpression("[^a-zA-Z0-9_.-]"), "_");

    return QString("%1/%2_%3")
        .arg(m_modelStorageDir)
        .arg(timestamp)
        .arg(sanitized);
}

void MyServer::checkTransfers()
{
    QMutexLocker locker(&m_clientsMutex);

    auto it = m_transfers.begin();
    while (it != m_transfers.end()) {
        if (QDateTime::currentSecsSinceEpoch() - it->startTime > 30) {
            qWarning() << "Transfer timeout for" << it->fileName;
            if (m_clients.contains(it.key())) {
                m_clients[it.key()]->abort();
            }
            it = m_transfers.erase(it);
        } else {
            ++it;
        }
    }
}

QString MyServer::generateModelSavePath(const QString& fileName)
{
    // 确保文件名有效
    QString validFileName = fileName;
    if (fileName.isEmpty()) {
        validFileName = QString("model_%1.pt")
        .arg(QDateTime::currentDateTime().toString("hhmmsszzz"));
    }

    // 创建带时间戳的目录
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    QString saveDir = QString("%1/%2").arg(m_modelStorageDir).arg(timestamp);
    QDir().mkpath(saveDir); // 确保目录存在

    // 返回完整路径（目录+文件名）
    return QString("%1/%2").arg(saveDir).arg(validFileName);
}

void MyServer::startHeartbeat()
{
    QTimer *timer = new QTimer(this);
    connect(timer, &QTimer::timeout, [this](){
        for (auto client : m_clients.keys()) {
            if (client->state() == QTcpSocket::ConnectedState) {
                client->write("\x00"); // 心跳包
            }
        }
    });
    timer->start(30000); // 30秒间隔
}

void MyServer::sendModelToClient(QTcpSocket* client, const QString& modelPath)
{
    QFile file(modelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "Cannot open model file:" << modelPath;
        return;
    }

    // 定义关键变量（修复作用域问题）
    const qint64 chunkSize = 64 * 1024; // 64KB
    const qint64 fileSize = file.size();
    qint64 bytesSent = 0;

    // 构造头部（仅文件大小）
    QByteArray header;
    QDataStream headerStream(&header, QIODevice::WriteOnly);
    headerStream.setByteOrder(QDataStream::BigEndian);
    headerStream << qint64(fileSize); // 8字节文件大小

    // 发送头部
    if (client->write(header) != header.size() || !client->waitForBytesWritten(3000)) {
        qWarning() << "Header send failed";
        file.close();
        return;
    }

    // 分块发送文件内容
    while (!file.atEnd()) {
        QByteArray chunk = file.read(chunkSize);
        qint64 written = 0;
        qint64 remaining = chunk.size();

        // 确保完整发送每个分块
        while (remaining > 0) {
            written = client->write(chunk.constData() + (chunk.size() - remaining), remaining);
            if (written == -1 || !client->waitForBytesWritten(5000)) {
                qWarning() << "Model send failed to" << client->peerAddress();
                file.close();
                return;
            }
            remaining -= written;
            bytesSent += written;
            emit transferProgress(client->peerAddress().toString(),
                                  bytesSent, fileSize); // 直接使用fileSize变量
        }
    }

    file.close();
    qDebug() << "Model sent to" << client->peerAddress()
             << "Size:" << fileSize; // 使用已定义的fileSize
}

void MyServer::broadcastModel(const QString& modelPath)
{
    QMutexLocker locker(&m_clientsMutex);
    foreach (auto client, m_clients.keys()) {
        if (client->state() == QTcpSocket::ConnectedState) {
            sendModelToClient(client, modelPath);
            QThread::msleep(50); // 防止同时发送导致阻塞
        }
    }
}
