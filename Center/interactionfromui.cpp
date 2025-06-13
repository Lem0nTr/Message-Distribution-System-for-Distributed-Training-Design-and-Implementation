#include "interactionfromui.h"


InteractionFromUI::InteractionFromUI(QObject *parent)
    : QTcpServer(parent), clientSocket(nullptr),
    m_webSocketServer(new QWebSocketServer("ProgressServer", QWebSocketServer::NonSecureMode, this)),
    m_webSocketClients()
{
    // 监听所有网络接口的12000端口
    if (!this->listen(QHostAddress::Any, 12000)) {
        qDebug() << "Server could not start!";
    } else {
        qDebug() << "InteractionFromUI Server started on port 12000";
    }

    connect(this ,&InteractionFromUI::UICommend , this , &InteractionFromUI::handleUICommand);

    // WebSocket服务器监听12001端口
    if (!m_webSocketServer->listen(QHostAddress::Any, 12001)) {
        qDebug() << "WebSocket server failed to start";
    } else {
        qDebug() << "WebSocket Server started on port 12001";
        connect(m_webSocketServer, &QWebSocketServer::newConnection,
                this, &InteractionFromUI::onNewWebSocketConnection);
    }



}

void InteractionFromUI::incomingConnection(qintptr socketDescriptor)
{
    // 创建新的客户端连接
    clientSocket = new QTcpSocket(this);
    clientSocket->setSocketDescriptor(socketDescriptor);

    // 连接信号与槽
    connect(clientSocket, &QTcpSocket::readyRead, this, &InteractionFromUI::readClientData);
    connect(clientSocket, &QTcpSocket::disconnected, this, &InteractionFromUI::handleDisconnected);

    qDebug() << "New client connected:" << clientSocket->peerAddress().toString();
}


void InteractionFromUI::readClientData()
{
    QTcpSocket *clientSocket = qobject_cast<QTcpSocket*>(sender());
    if (!clientSocket)
        return;

    QByteArray requestData = clientSocket->readAll();
    qDebug() << "Received data:" << requestData;

    QString request = QString(requestData);
    QStringList requestLines = request.split("\r\n");

    // 检查是否为 OPTIONS 请求
    if (requestLines[0].startsWith("OPTIONS")) {
        // 返回允许跨域的响应头
        QString response = "HTTP/1.1 200 OK\r\n";
        response += "Access-Control-Allow-Origin: *\r\n"; // 允许所有来源
        response += "Access-Control-Allow-Methods: POST, GET, OPTIONS\r\n"; // 允许的请求方法
        response += "Access-Control-Allow-Headers: content-type\r\n"; // 允许的请求头
        response += "Content-Length: 0\r\n";
        response += "Connection: close\r\n";
        response += "\r\n";

        clientSocket->write(response.toUtf8());
        //clientSocket->disconnectFromHost();
        m_currentClientForDownload = clientSocket;
        return;
    }

    // 检查是否为 POST 请求
    if (requestLines[0].startsWith("POST")) {
        qDebug() << "完整请求头：" << requestData;
        // 查找 Content-Length
        int contentLength = 0;
        for (const QString& line : requestLines) {
            if (line.startsWith("Content-Length:")) {
                contentLength = line.split(":")[1].trimmed().toInt();
                break;
            }
        }

        // 提取 POST 数据
        if (contentLength > 0) {
            QByteArray postData = requestData.right(contentLength);
            qDebug() << "POST data:" << postData;

            parseUICommand(postData , m_command , m_param);
            emit UICommend(m_command , m_param);

        }

        // 返回 HTTP 响应
        // QString response = "HTTP/1.1 200 OK\r\n";
        // response += "Access-Control-Allow-Origin: *\r\n"; // 允许所有来源
        // response += "Content-Type: text/plain\r\n";
        // response += "Connection: close\r\n";
        // response += "\r\n";
        // response += "Hello, this is the server response!";

        // clientSocket->write(response.toUtf8());
        //clientSocket->disconnectFromHost();
        m_currentClientForDownload = clientSocket;
    }
}

void InteractionFromUI::handleDisconnected()
{
    qDebug() << "Client disconnected:" << clientSocket->peerAddress().toString();
    clientSocket->deleteLater();
}

//解析函數
bool InteractionFromUI::parseUICommand(const QByteArray &postData, QString &command, QString &params)
{
    qDebug() << "原始POST数据：" << postData; // 新增调试输出
    // 解析JSON结构
    QJsonParseError jsonError;
    QJsonDocument doc = QJsonDocument::fromJson(postData, &jsonError);

    if (jsonError.error != QJsonParseError::NoError) {
        qWarning() << "JSON格式错误：" << jsonError.errorString();
        return false;
    }

    QJsonObject jsonObj = doc.object();
    if (!jsonObj.contains("data")) {
        qWarning() << "缺少data字段";
        return false;
    }

    QString rawCommand = jsonObj["data"].toString().trimmed();
    if (rawCommand.isEmpty()) {
        qWarning() << "空命令";
        return false;
    }

    // 提取命令字符（第一个非空字符）
    command = rawCommand[0].toUpper();

    // 提取参数部分（去除命令字符及前后空格）
    params = rawCommand.mid(1).trimmed();

    return true;
}

// 专用命令处理器處理訓練次數
bool InteractionFromUI::processTrainingCommand(const QString& params, int& times)
{
    // 使用正则表达式匹配 E数字 格式
    QRegularExpression re("E(\\d+)");
    QRegularExpressionMatch match = re.match(params);

    if (!match.hasMatch()) {
        qWarning() << "训练次数格式错误，应为 E+数字";
        return false;
    }

    bool ok;
    times = match.captured(1).toInt(&ok);

    if (!ok || times <= 0) {
        qWarning() << "无效的训练次数值：" << match.captured(1);
        return false;
    }

    return true;
}

void InteractionFromUI::handleUICommand(const QString& cmdType ,const QString& parameters)
{

    if (cmdType == "T") {
        int trainTimes;
        if (processTrainingCommand(parameters, trainTimes)) {
            //startTraining(trainTimes);

            m_totalSteps = trainTimes;
            m_currentStep = 0;
            m_trainingFinished = false;
            sendProgressUpdate(); // 初始化进度显示

            qDebug() << "解析到训练命令，训练次数为" <<trainTimes ;
            emit parsedTrainCommandFromUI(trainTimes);

        }
    }
    else if (cmdType == "U") {

        if (parameters.isEmpty()) {
            qWarning() << "上传命令需要指定参数";
            return;
        }


        handleUpload(parameters); // 无需判断返回值

        // 通过检查文件路径和客户端连接状态来判断
        if (!m_currentModelPath.isEmpty() && m_currentClientForDownload) {
            sendFileToWebpage(m_currentClientForDownload);
            m_currentClientForDownload = nullptr; // 重置指针
        } else {
            // 发送错误响应
            QString errorResponse =
                "HTTP/1.1 404 Not Found\r\n"
                "Access-Control-Allow-Origin: *\r\n"
                "Content-Length: 0\r\n\r\n";
            if (m_currentClientForDownload) {
                m_currentClientForDownload->write(errorResponse.toUtf8());
                m_currentClientForDownload->disconnectFromHost();
            }
        }
        // 处理下载请求
        // handleUpload(parameters);
        // // 立即触发文件传输
        // if (!m_currentModelPath.isEmpty()) {
        //     emit readyToSendFile(m_currentModelPath);
        // } else {
        //     qWarning() << "没有可用的模型文件";
        // }
    }
    else {
        qWarning() << "未知命令类型：" << cmdType;
    }
}

// 新增WebSocket处理函数
void InteractionFromUI::onNewWebSocketConnection()
{
    QWebSocket *client = m_webSocketServer->nextPendingConnection();
    connect(client, &QWebSocket::disconnected,
            [client, this]() {
                m_webSocketClients.removeOne(client);
                client->deleteLater();
            });
    m_webSocketClients.append(client);

    // 立即发送当前状态
    sendProgressUpdate();
}

// 新增进度更新函数
void InteractionFromUI::incrementProgress()
{
    if (m_currentStep < m_totalSteps) {
        m_currentStep++;
        sendProgressUpdate();
    }
}

// 新增下载按钮控制函数
void InteractionFromUI::enableDownload()
{
    QJsonObject msg;
    msg["type"] = "showDownload";
    broadcastMessage(msg);
}

// 私有辅助函数
void InteractionFromUI::sendProgressUpdate()
{
    qDebug() << "发送进度更新:" << m_currentStep << "/" << m_totalSteps;
    QJsonObject msg;
    msg["type"] = "progress";
    msg["current"] = m_currentStep;
    msg["total"] = m_totalSteps;
    broadcastMessage(msg);
}

void InteractionFromUI::broadcastMessage(const QJsonObject &msg)
{
    QJsonDocument doc(msg);
    const QByteArray data = doc.toJson();

    for (QWebSocket *client : qAsConst(m_webSocketClients)) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->sendTextMessage(data);
        }
    }
}

void InteractionFromUI::setTotalSteps(int total) {
    qDebug() << "设置总步数:" << total;
    m_totalSteps = total;
    m_currentStep = 0;
    sendProgressUpdate();
}

void InteractionFromUI::handleUpload(const QString& timestamp)
{
    // 安全验证时间戳格式
    if (!validateTimestamp(timestamp)) {
        qWarning() << "无效的时间戳格式:" << timestamp;
        return;
    }

    // 构建模型目录路径
    QString basePath = "/home/stl/qt/qtdata/Central_Server/build/Desktop_Qt_6_5_3_GCC_64bit-Debug/received_models/";
    QDir modelDir(basePath + timestamp + "/");

    // 检查目录是否存在
    if (!modelDir.exists()) {
        qWarning() << "模型目录不存在:" << modelDir.path();
        return;
    }

    // 查找模型文件
    m_currentModelPath = findModelFile(modelDir.path());
    if (m_currentModelPath.isEmpty()) {
        qWarning() << "未找到模型文件";
        return;
    }

    qDebug() << "准备下载文件:" << m_currentModelPath;

    // QTcpSocket* currentClient = qobject_cast<QTcpSocket*>(sender());
    // if (currentClient) {
    //     sendFileToWebpage(currentClient);
    // } else {
    //     qWarning() << "无法获取当前客户端连接";
    // }

}

void InteractionFromUI::sendFileToWebpage(QTcpSocket* clientSocket)
{
    if (!clientSocket || clientSocket->state() != QAbstractSocket::ConnectedState) {
        qWarning() << "客户端连接已断开，无法发送文件";
        return;
    }

    if (m_currentModelPath.isEmpty()) {
        qWarning() << "文件路径为空，无法发送";
        return;
    }

    QFile file(m_currentModelPath);
    if (!file.open(QIODevice::ReadOnly)) {
        qWarning() << "无法打开文件:" << m_currentModelPath;

        // 发送404响应
        QString errorResponse =
            "HTTP/1.1 404 Not Found\r\n"
            "Access-Control-Allow-Origin: *\r\n"
            "Content-Type: text/plain\r\n"
            "Content-Length: 0\r\n\r\n";
        clientSocket->write(errorResponse.toUtf8());
        clientSocket->disconnectFromHost();
        return;
    }

    QByteArray fileData = file.readAll();
    file.close();

    QString responseHeader =
        "HTTP/1.1 200 OK\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Content-Type: application/octet-stream\r\n"
        "Content-Disposition: attachment; filename=\""
        + QFileInfo(file).fileName() + "\"\r\n"
                                       "Content-Length: " + QString::number(fileData.size()) + "\r\n"
                                             "Connection: close\r\n\r\n"; // 保持连接直到传输完成

    // 先发送响应头
    clientSocket->write(responseHeader.toUtf8());

    // 然后发送文件内容
    clientSocket->write(fileData);

    // 通过bytesWritten信号检测发送完成
    connect(clientSocket, &QTcpSocket::bytesWritten, this, [=](){
        if (clientSocket->bytesToWrite() == 0) {
            qDebug() << "文件传输完成";
            clientSocket->disconnectFromHost();
            m_currentModelPath.clear();
        }
    });
}

bool InteractionFromUI::validateTimestamp(const QString& timestamp)
{

    // 验证时间戳格式（20250415_204422）
    QRegularExpression regex(R"((\d{8}_\d{6}))");
    return regex.match(timestamp).hasMatch();
}

QString InteractionFromUI::findModelFile(const QString& dirPath)
{
    // 定义可能的模型文件扩展名
    QStringList extensions { "*.pt", "*.zip" };

    QDir directory(dirPath);
    QStringList files = directory.entryList(extensions, QDir::Files);

    // 优先返回压缩包文件
    foreach (const QString &file, files) {
        if (file.endsWith(".zip")) {
            return directory.filePath(file);
        }
    }

    // 返回找到的第一个文件
    return files.isEmpty() ? QString() : directory.filePath(files.first());
}

void InteractionFromUI::addHistoryEntry(const QString& rawTimestamp)
{
    // 格式化时间戳
    QString formattedTime = formatTimestamp(rawTimestamp);
    if(formattedTime.isEmpty()) return;

    // 获取模型文件名
    QString basePath = "/home/stl/qt/qtdata/Central_Server/build/Desktop_Qt_6_5_3_GCC_64bit-Debug/received_models/";
    QDir modelDir(basePath + rawTimestamp + "/");
    QString filename = QFileInfo(findModelFile(modelDir.path())).fileName();

    // 发送WebSocket消息
    QJsonObject msg;
    msg["type"] = "history";
    msg["time"] = formattedTime;
    msg["filename"] = filename;
    broadcastMessage(msg);
}

QString InteractionFromUI::formatTimestamp(const QString& rawTimestamp)
{
    // 格式示例：20240415_222139 -> 2024-04-15 22:21:39
    if(rawTimestamp.length() != 15 || !rawTimestamp.contains("_")){
        qWarning() << "Invalid timestamp format:" << rawTimestamp;
        return QString();
    }

    QString datePart = rawTimestamp.left(8);
    QString timePart = rawTimestamp.mid(9);

    QDateTime dt = QDateTime::fromString(datePart + timePart, "yyyyMMddHHmmss");
    if(!dt.isValid()){
        qWarning() << "Invalid timestamp:" << rawTimestamp;
        return QString();
    }

    return dt.toString("yyyy-MM-dd HH:mm:ss");
}

void InteractionFromUI::sendTextToWebPage(const QString& message)
{
    QJsonObject msg;
    msg["type"] = "log";
    msg["content"] = message;

    QJsonDocument doc(msg);
    const QByteArray data = doc.toJson();

    for (QWebSocket* client : qAsConst(m_webSocketClients)) {
        if (client && client->state() == QAbstractSocket::ConnectedState) {
            client->sendTextMessage(data);
        }
    }
}
