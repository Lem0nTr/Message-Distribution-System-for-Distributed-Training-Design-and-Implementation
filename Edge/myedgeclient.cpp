#include "myedgeclient.h"
#include<QDebug>

const qint64 MyEdgeClient::CHUNK_SIZE = 64 * 1024;

MyEdgeClient::MyEdgeClient(QObject *parent)  // 添加父对象参数
    : QObject(parent)
{
    m_tcpSocket = new QTcpSocket(this);  // 设置父对象


    connect(m_tcpSocket, &QTcpSocket::connected, this, &MyEdgeClient::onConnected);
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, &MyEdgeClient::onDisconnected);
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            this, &MyEdgeClient::onErrorOccurred);
    connect(m_tcpSocket, &QTcpSocket::bytesWritten,
            this, &MyEdgeClient::onBytesWritten);

}

MyEdgeClient::~MyEdgeClient() {
    //disconnectFromCenter();
    m_tcpSocket->disconnect(); // 断开所有信号槽
    delete m_tcpSocket;
}

void MyEdgeClient::ConnectSlot(QString IP, QString Port) {
    m_receivedData.clear();
    m_expectedSize = 0;
    m_sizeData.clear();


    if (IP.isEmpty() || Port.isEmpty()) return;

    m_tcpSocket->connectToHost(IP, Port.toUShort());
    connect(m_tcpSocket, &QTcpSocket::connected, this, [this]() {
        qDebug() << "Connected to central server!";
        // 连接成功后绑定数据接收槽
        connect(m_tcpSocket, &QTcpSocket::readyRead, this, &MyEdgeClient::readModel);
    });
    connect(m_tcpSocket, QOverload<QAbstractSocket::SocketError>::of(&QTcpSocket::errorOccurred),
            [](QAbstractSocket::SocketError error) {
                qDebug() << "Connection error:" << error;
            });
}


void MyEdgeClient::readModel() {

    if (m_receivedData.size() >= m_expectedSize) {
        saveModelToFile(m_receivedData.left(m_expectedSize));
        m_receivedData = m_receivedData.mid(m_expectedSize); // 保留剩余数据
        m_expectedSize = 0;
        m_sizeData.clear();
        readModel(); // 继续处理剩余数据
    }

    if (m_tcpSocket->state() != QTcpSocket::ConnectedState) {
        qDebug() << "Socket not connected!";
        return;
    }

    // 1. 读取8字节长度头
    if (m_expectedSize == 0 && m_tcpSocket->bytesAvailable() >= 8) {
        m_sizeData = m_tcpSocket->read(8);
        bool ok;
        m_expectedSize = m_sizeData.toLongLong(&ok);
        if (!ok || m_expectedSize <= 0) {
            qDebug() << "Invalid size header:" << m_sizeData;
            m_tcpSocket->disconnectFromHost();
            return;
        }
        qDebug() << "Expecting model size:" << m_expectedSize << "bytes";
    }

    // 2. 分块接收数据
    if (m_expectedSize > 0) {
        QByteArray newData = m_tcpSocket->readAll();  // 读取所有可用数据
        m_receivedData.append(newData);
        qDebug() << "Progress:" << m_receivedData.size() << "/" << m_expectedSize;

        // 3. 接收完成后保存
        if (m_receivedData.size() >= m_expectedSize) {
            saveModelToFile(m_receivedData.left(m_expectedSize));  // 确保不写入多余数据
            m_receivedData.clear();
            m_expectedSize = 0;
            m_sizeData.clear();
        }
    }
}

void MyEdgeClient::saveModelToFile(const QByteArray &data) {
    qDebug() << "Data size to save:" << data.size();  // 检查实际接收到的数据长度
    if (data.isEmpty()) {
        qDebug() << "Error: Received data is empty!";
        return;
    }

    QString savePath = "received_model.pt";
    //QString savePath ="test01.txt";
    QFile file(savePath);
    if (file.open(QIODevice::WriteOnly)) {
        qint64 bytesWritten = file.write(data);  // 记录实际写入的字节数
        file.close();
        qDebug() << "Bytes written to file:" << bytesWritten;
        qDebug() << "File exists:" << QFileInfo(file).exists()
                 << "Size:" << QFileInfo(file).size() << "bytes";
    } else {
        qDebug() << "File open error:" << file.errorString();
    }
}


void MyEdgeClient::connectToCenter(const QString &ip, quint16 port)
{
    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "[Warning] Already connected/connecting. Current state:" << m_tcpSocket->state();
        return;
    }

    // 发送成功后清理资源
    connect(m_tcpSocket, &QTcpSocket::disconnected, this, [this](){
        qDebug() << "[Socket Closed] Ready for next transfer";
        if (m_currentFile) {
            m_currentFile->close();
            delete m_currentFile;
            m_currentFile = nullptr;
        }
        m_bytesSent = 0;
        m_fileSize = 0;
    });

    m_ip = ip;
    m_port = port;
    qDebug() << "[Connecting] To:" << ip << ":" << port;
    m_tcpSocket->connectToHost(ip, port);

    // 设置连接超时
    if (!m_tcpSocket->waitForConnected(5000)) {
        QString errorMsg = QString("Connection timeout: %1").arg(m_tcpSocket->errorString());
        qDebug() << "[Error]" << errorMsg;
        emit errorOccurred(errorMsg);
        abortTransfer();
    }

    connect(m_tcpSocket, &QTcpSocket::readyRead, this, [this]() {
        handleModelData();
    });

    //測試發送
    //QString str = "/home/stl/qt/qtdata/Edge_Server01/build/Desktop_Qt_6_5_3_GCC_64bit-Debug/aggregated_results/round_20250405_213110/aggregated_model.pt";
    //sendModel(str);

    connect(m_tcpSocket, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError error) {
        qDebug() << "[Socket Error]" << error << m_tcpSocket->errorString();

        // 自动重试逻辑（示例：3秒后重连）
        if (error == QAbstractSocket::RemoteHostClosedError) {
            QTimer::singleShot(3000, this, [this]() {
                qDebug() << "Attempting to reconnect...";
                m_tcpSocket->connectToHost(m_ip, m_port);
            });
        }
    });

}

void MyEdgeClient::disconnectFromCenter()
{
    if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
        qDebug() << "[Disconnecting] From center server";
        m_tcpSocket->disconnectFromHost();
        if (m_tcpSocket->state() != QAbstractSocket::UnconnectedState) {
            m_tcpSocket->waitForDisconnected(1000);
        }
    }
}

bool MyEdgeClient::isConnected() const
{
    return m_tcpSocket->state() == QAbstractSocket::ConnectedState;
}


void MyEdgeClient::sendModel(const QString &modelPath)
{

    qDebug() << "[发送前检查]"
             << "\n- 文件存在:" << QFile::exists(modelPath)
             << "\n- 文件大小:" << QFileInfo(modelPath).size()
             << "\n- Socket状态:" << m_tcpSocket->state();
    // 检查连接状态，必要时重新连接
    if (m_tcpSocket->state() != QTcpSocket::ConnectedState) {
        qDebug() << "[Reconnecting] Socket not connected, attempting to reconnect...";
        m_tcpSocket->connectToHost(m_ip, m_port); // 需要提前保存主机和端口信息

        // 等待连接建立（示例等待2秒）
        if (!m_tcpSocket->waitForConnected(2000)) {
            qCritical() << "[Error] Reconnect failed:" << m_tcpSocket->errorString();
            emit errorOccurred("Connection failed");
            return;
        }
    }

    // 确保文件路径有效
    QFileInfo fileInfo(modelPath);
    if (!fileInfo.exists()) {
        emit errorOccurred(tr("文件不存在: %1").arg(modelPath));
        return;
    }

    // 仅保留文件名（不含路径）
    m_currentFileName = fileInfo.fileName();

    // 验证文件名有效性
    if (m_currentFileName.isEmpty() || m_currentFileName.contains("/")) {
        emit errorOccurred(tr("非法文件名: %1").arg(m_currentFileName));
        return;
    }

    // 打开文件
    if (m_currentFile) {
        m_currentFile->close();
        delete m_currentFile;
    }

    m_currentFile = new QFile(modelPath);
    if (!m_currentFile->open(QIODevice::ReadOnly)) {
        emit errorOccurred(tr("无法打开文件: %1").arg(modelPath));
        delete m_currentFile;
        m_currentFile = nullptr;
        return;
    }

    // 初始化传输
    m_fileSize = m_currentFile->size();
    m_bytesSent = 0;
    qDebug() << "[Edge] Starting to send model:" << modelPath
             << "Size:" << m_fileSize << "bytes";
    // 发送包头
    sendFileHeader();
    sendFileData();
}


// void MyEdgeClient::sendFileHeader()
// {
//     QByteArray header;
//     QDataStream stream(&header, QIODevice::WriteOnly);
//     stream.setVersion(QDataStream::Qt_5_15);
//     stream.setByteOrder(QDataStream::BigEndian);

//     // 写入文件名长度和文件名
//     QByteArray fileNameBytes = m_currentFileName.toUtf8();
//     stream << static_cast<quint16>(fileNameBytes.size());
//     stream.writeRawData(fileNameBytes.constData(), fileNameBytes.size());

//     // 写入文件大小
//     stream << m_fileSize;

//     m_tcpSocket->write(header);
// }

// 在 sendFileHeader() 中增加更严格的校验
void MyEdgeClient::sendFileHeader()
{
    if (m_fileSize <= 0) {
        qDebug() << "[Error] Invalid file size:" << m_fileSize;
        emit errorOccurred("Invalid file size");
        abortTransfer();
        return;
    }

    QByteArray header;
    QDataStream stream(&header, QIODevice::WriteOnly);
    stream.setByteOrder(QDataStream::BigEndian);

    // 使用固定8字节表示文件大小
    quint64 networkOrderSize = qToBigEndian(static_cast<quint64>(m_fileSize));
    stream.writeRawData(reinterpret_cast<const char*>(&networkOrderSize), sizeof(quint64));

    qDebug() << "[Edge] Sending size header:" << m_fileSize << "bytes | Header:" << header.toHex();

    qint64 written = m_tcpSocket->write(header);
    if (written != header.size()) {
        qDebug() << "[Error] Header send failed. Expected:" << header.size()
        << "Actual:" << written << "Error:" << m_tcpSocket->errorString();
        emit errorOccurred("Header send failed");
        abortTransfer();
    }

    // 确保头部立即发送
    if (!m_tcpSocket->waitForBytesWritten(3000)) {
        qDebug() << "[Error] Header send timeout";
        emit errorOccurred("Header send timeout");
        abortTransfer();
    }
}

void MyEdgeClient::sendNextChunk()
{
    if (!isConnected() || !m_currentFile || !m_currentFile->isOpen()) return;

    // 每次发送64KB数据
    const qint64 chunkSize = 64 * 1024;
    QByteArray buffer = m_currentFile->read(chunkSize);

    if (!buffer.isEmpty()) {
        qint64 written = m_tcpSocket->write(buffer);
        if (written == -1) {
            emit errorOccurred(tr("Error writing to socket"));
            abortTransfer();
        } else {
            m_bytesSent += written;
            emit transferProgress(m_bytesSent, m_fileSize);
        }
    }

    // 传输完成
    if (m_bytesSent >= m_fileSize) {
        finishTransfer();
    }
}

void MyEdgeClient::onBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes)
    if (m_currentFile && m_currentFile->isOpen()) {
        sendNextChunk();
    }
}

void MyEdgeClient::onConnected()
{
    emit connectionStatusChanged(true);
}

void MyEdgeClient::onDisconnected()
{
    emit connectionStatusChanged(false);
    abortTransfer();
}

void MyEdgeClient::onErrorOccurred(QAbstractSocket::SocketError error)
{
    QString errorMsg = QString("Socket error: %1 - %2")
    .arg(error)
        .arg(m_tcpSocket->errorString());
    qDebug() << "[Error]" << errorMsg;
    emit errorOccurred(errorMsg);
    abortTransfer();
}

void MyEdgeClient::abortTransfer()
{
    m_transferInProgress = false;

    if (m_tcpSocket) {
        m_tcpSocket->abort(); // 立即中止连接
        disconnect(m_tcpSocket, &QTcpSocket::bytesWritten, this, nullptr);
    }

    if (m_currentFile) {
        m_currentFile->close();
        delete m_currentFile;
        m_currentFile = nullptr;
    }

    emit errorOccurred("Transfer aborted");
    qDebug() << "[Abort] Transfer cancelled, connection kept";
}


void MyEdgeClient::finishTransfer()
{
    // 添加心跳机制
    m_keepAliveTimer.start(15000); // 15秒心跳间隔

    // 设置socket选项
    m_tcpSocket->setSocketOption(QAbstractSocket::KeepAliveOption, true);
    m_tcpSocket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
    // 仅清理文件资源，不要操作Socket
    if (m_currentFile) {
        m_currentFile->close();
        delete m_currentFile;
        m_currentFile = nullptr;
    }

    // 关键修改：禁止自动关闭连接
    m_tcpSocket->setProperty("keepAlive", true); // 标记保持连接

    qDebug() << "[Final] Transfer complete. Connection kept alive."
             << "Socket state:" << m_tcpSocket->state();

    emit modelSent(true);
}


// 异步发送模式改造
void MyEdgeClient::sendFileData()
{
    if (!m_currentFile || !m_currentFile->isOpen() ||
        m_tcpSocket->state() != QTcpSocket::ConnectedState)
    {
        abortTransfer();
        return;
    }

    m_transferInProgress = true;
    connect(m_tcpSocket, &QTcpSocket::bytesWritten, this, &MyEdgeClient::handleBytesWritten);
    handleBytesWritten(0); // 触发首次发送
}

// 新增异步处理槽函数
void MyEdgeClient::handleBytesWritten(qint64 bytes)
{
    Q_UNUSED(bytes);

    if (!m_transferInProgress) return;

    while (m_bytesSent < m_fileSize) {
        // 计算剩余量和位置
        qint64 remaining = m_fileSize - m_bytesSent;
        qint64 chunkSize = qMin(CHUNK_SIZE, remaining);
        qint64 expectedPos = m_bytesSent;

        // 强制对齐文件指针
        if (m_currentFile->pos() != expectedPos) {
            if (!m_currentFile->seek(expectedPos)) {
                qCritical() << "[Seek Failed] Actual pos:" << m_currentFile->pos()
                << "Expected:" << expectedPos;
                abortTransfer();
                return;
            }
        }

        // 读取数据块
        QByteArray chunk = m_currentFile->read(chunkSize);
        if (chunk.size() != chunkSize) {
            qCritical() << "[Read Error] Expected:" << chunkSize
                        << "Actual:" << chunk.size();
            abortTransfer();
            return;
        }

        // 发送数据块
        qint64 written = m_tcpSocket->write(chunk);
        if (written == -1) {
            qCritical() << "[Write Error]" << m_tcpSocket->errorString();
            abortTransfer();
            return;
        }
        m_bytesSent += written;

        // 防止缓冲区堆积
        if (m_tcpSocket->bytesToWrite() > 256 * 1024) {
            break; // 暂停发送，等待下一次bytesWritten信号
        }
    }

    // 传输完成检测
    if (m_bytesSent >= m_fileSize) {
        m_transferInProgress = false;
        disconnect(m_tcpSocket, &QTcpSocket::bytesWritten, this, nullptr);
        qDebug() << "[Transfer Complete] All data queued, waiting for flush...";

        // 监听连接关闭事件（关键修改！）
        connect(m_tcpSocket, &QTcpSocket::disconnected, this, [this](){
            qDebug() << "[Socket Closed] All data confirmed flushed";
            if (m_currentFile) {
                m_currentFile->close();
                delete m_currentFile;
                m_currentFile = nullptr;
            }
            emit modelSent(true);
        });

        // 安全关闭连接（等待缓冲区刷新）
        //m_tcpSocket->disconnectFromHost();
    }
}


void MyEdgeClient::handleModelData()
{
    QDataStream stream(m_tcpSocket);
    stream.setByteOrder(QDataStream::BigEndian);

    while (m_tcpSocket->bytesAvailable() > 0) {
        if (!m_transfer.headerReceived) {
            // 新协议头只需 8 字节（文件大小）
            if (m_tcpSocket->bytesAvailable() < 8) return;

            // 读取文件大小（8字节）
            stream >> m_transfer.expectedSize;

            // 跳过原时间戳和文件名占用的 16 字节（14+2）
            // 如果中心服务器已完全移除这些字段，则不需要此操作
            // stream.skipRawData(16); // 根据实际协议调整

            m_transfer.headerReceived = true;
            qDebug() << "Expecting model size:" << m_transfer.expectedSize;
        }

        // 计算剩余需要接收的字节数
        qint64 bytesNeeded = m_transfer.expectedSize - m_transfer.buffer.size();
        if (bytesNeeded <= 0) break;

        // 精确读取所需字节数（避免读取过多）
        QByteArray chunk = m_tcpSocket->read(bytesNeeded);
        if (chunk.isEmpty()) break; // 无数据可读

        m_transfer.buffer.append(chunk);

        // 接收完成处理
        if (m_transfer.buffer.size() >= m_transfer.expectedSize) {
            if (m_transfer.expectedSize > 0) { // 校验有效性
                saveReceivedModel();
                emit modelReceived(m_transfer.savePath);
            }
            m_transfer = ModelTransfer(); // 重置状态
        }
    }
}

void MyEdgeClient::saveReceivedModel()
{
    // 生成时间戳
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

    // 构建存储路径：项目目录/时间戳/
    //QString saveDir = QDir::currentPath() + "/" + timestamp;

    // 构建新存储路径结构
    QString saveDir = QString("%1/received_fromcentermodels/%2")
                          .arg(QDir::currentPath())  // 项目目录
                          .arg(timestamp);           // 时间戳子目录

    // 固定文件名
    QString fileName = "received_models.pt";

    // 组合完整路径
    QString fullPath = QString("%1/%2").arg(saveDir).arg(fileName);

    // 创建目录（递归创建）
    QDir dir(saveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // 保存文件
    QFile file(fullPath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(m_transfer.buffer);
        file.close();
        emit saveCenterModel(fullPath);
        qDebug() << "Model saved to:" << fullPath;
    } else {
        qCritical() << "Save failed:" << file.errorString()
        << "Path:" << fullPath;
    }

    // 重置传输状态
    m_transfer = ModelTransfer();
}
