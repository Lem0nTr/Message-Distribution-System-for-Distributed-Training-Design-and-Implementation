#include "myedgeserver.h"
#include<QDebug>


MyEdgeServer::MyEdgeServer(QObject *parent)  // 添加父对象参数
    : QObject(parent)
{
    client = new QMqttClient;

    //設置ip地址
    client->setHostname("192.168.44.129");
    client->setPort(1883);
    client->setUsername("EdgeServer01");
    client->setPassword("EdgeServer01");

    // 初始化接收目录
    m_receiveDir = QCoreApplication::applicationDirPath() + "/received_models";
    QDir dir(m_receiveDir);
    if (!dir.exists()) {
        dir.mkpath(".");
        qDebug() << "Created receive directory:" << m_receiveDir;
    }

    // 连接信号槽
    connect(client, &QMqttClient::connected, this, &MyEdgeServer::onConnected);
    connect(client, &QMqttClient::messageReceived, this, &MyEdgeServer::onMessageReceived);
    connect(client, &QMqttClient::disconnected, [](){
        qDebug() << "Disconnected from broker";
    });

    client->connectToHost();

    m_roundTimer = new QTimer(this);
    m_roundTimer->setInterval(300000); // 5分钟无新模型则重置
    connect(m_roundTimer, &QTimer::timeout, [this](){
        qDebug() << "轮次超时重置:" << m_currentRoundId;
        m_currentRoundId.clear();
    });

    //測試發送模型
    // QString str = "/home/stl/qt/qtdata/Edge_Server01/build/Desktop_Qt_6_5_3_GCC_64bit-Debug/received_model.pt";
    // qDebug() << "開始發送訓練模型";
    // publishModel(str);
}



MyEdgeServer::~MyEdgeServer() {
    delete client;
}

void MyEdgeServer::ConnectSuccessSlot(){

    qDebug() << "連接成功！！" ;


}

void MyEdgeServer::ReceiveSlot(const QByteArray &ba , const QMqttTopicName & topic){

    QString str = topic.name() + QString(ba);

    qDebug() << str;

}



void MyEdgeServer::publishModel(const QString &filePath) {
    bool success = false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "File open error:" << file.errorString();
        return;
    }

    QByteArray modelData = file.readAll();
    file.close();

    // 发送文件哈希（校验用）
    // QMqttTopicName hashTopic("edge/model/end");
    // QByteArray hash = QCryptographicHash::hash(modelData, QCryptographicHash::Md5);
    // client->publish(hashTopic, hash);

    // 分块发送（确保顺序）
    const int chunkSize = 512 * 1024;
    for (int i = 0; i < modelData.size(); i += chunkSize) {
        QByteArray chunk = modelData.mid(i, chunkSize);
        QString topic = QString("edge/model/chunk_%1").arg(i / chunkSize);
        client->publish(topic, chunk, 1);  // QoS 1 确保送达
        QThread::msleep(10);  // 轻微延迟
    }

    // 发送结束标记
    //m_client->publish("client01/model/end", QByteArray::number(modelData.size()), 1);
    QMqttTopicName endTopic("edge/model/end");
    if (client->publish(endTopic, QByteArray::number(modelData.size())) != -1) {
        success = true;
    }
    //emit modelPublished(success);

}



void MyEdgeServer::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic)
{
    qDebug() << "[Edge] Received topic:" << topic.name() << "size:" << message.size();

    // 每次收到消息时重置计时器
    m_roundTimer->stop();       // 停止之前的计时器（如果正在运行）
    m_roundTimer->start(360000); // 重新设置为6分钟（360,000毫秒）

    if (m_currentRoundId.isEmpty()) {
        m_roundTimer->start(); // 收到第一个分块启动计时器
        m_roundStartTime = QDateTime::currentDateTime();
        m_currentRoundId = m_roundStartTime.toString("yyyyMMdd_HHmmss");
        qDebug() << "新轮次开始 ID:" << m_currentRoundId;
    }

    // 解析客户端ID
    const QStringList parts = topic.name().split('/');
    if (parts.size() < 3) {
        qDebug() << "Invalid topic format";
        return;
    }
    const QString clientId = parts[0];

    // 创建统一轮次目录（示例路径: received_models/round_20240330_1430/client01/）
    QString clientDir = QString("%1/round_%2/%3/")
                            .arg(m_receiveDir)
                            .arg(m_currentRoundId)
                            .arg(clientId);

    QDir dir;
    if (!dir.mkpath(clientDir)) {
        qDebug() << "Failed to create client directory";
        return;
    }

    // 处理分块数据
    if (topic.name().contains("/model/chunk_")) {
        const int chunkId = parts.last().split('_').last().toInt();
        QFile file(clientDir + QString("chunk_%1").arg(chunkId, 4, 10, QLatin1Char('0')));
        if (file.open(QIODevice::WriteOnly)) {
            file.write(message);
            qDebug() << "Saved chunk" << chunkId << "for" << clientId;
        }
    }
    // 处理结束标记
    else if (topic.name().endsWith("/model/end")) {
        const int expectedSize = message.toInt();
        QByteArray modelData;

        // 收集所有分块并合并
        dir.setPath(clientDir);
        const QStringList chunks = dir.entryList({"chunk_*"}, QDir::Files, QDir::Name);
        for (const QString &chunk : chunks) {
            QFile file(dir.filePath(chunk));
            if (file.open(QIODevice::ReadOnly)) {
                modelData.append(file.readAll());
                file.remove();
            }
        }

        // 验证并保存完整模型
        if (modelData.size() == expectedSize) {
            QFile modelFile(clientDir + "model.pt");
            if (modelFile.open(QIODevice::WriteOnly)) {
                modelFile.write(modelData);
                qDebug() << "Model saved for" << clientId << "size:" << expectedSize;
                cleanupOldRounds(clientId, 3);  // 保留最近3个版本
                emit modelReceived(clientId, modelFile.fileName());
            }
        } else {
            qDebug() << "Size mismatch for" << clientId
                     << "Expected:" << expectedSize
                     << "Actual:" << modelData.size();
        }

        // 保存文件并定义fullPath
        QString fullPath = clientDir + "model.pt"; // 明确路径定义
        QFile modelFile(fullPath);
        if (modelFile.open(QIODevice::WriteOnly)) {
            modelFile.write(modelData);
            qDebug() << "模型保存至:" << fullPath;

            // 现在可以安全校验
            if (modelFile.size() != expectedSize) {
                qWarning() << "文件大小不符！路径:" << fullPath
                           << "预期:" << expectedSize
                           << "实际:" << modelFile.size();
            }
        }



    }

    // qDebug() << "模型参数类型验证:";
    // for (auto& [key, value] : model.state_dict()) {
    //     qDebug() << QString::fromStdString(key) << "类型:" << value.type();
    // }

}

// 辅助函数：清理旧轮次数据
void MyEdgeServer::cleanupOldRounds(const QString &clientId, int keep)
{
    QDir clientDir(QString("%1/%2").arg(m_receiveDir).arg(clientId));
    const QStringList rounds = clientDir.entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Time);

    for (int i = keep; i < rounds.size(); ++i) {
        clientDir.cd(rounds[i]);
        clientDir.removeRecursively();
        qDebug() << "Cleaned old round:" << rounds[i];
    }
}

void MyEdgeServer::onConnected(){
    qDebug() << "Connected to MQTT Broker!";
    subscribe("client01/model/#");
    subscribe("client02/model/#");    // 通配符订阅所有子主题
    receivedModelData.clear();        // 清空之前的数据
    expectedSize = 0;                // 重置期望大小
}

void MyEdgeServer::subscribe(const QString &topic){

    auto subscription = client->subscribe(topic);
    if (!subscription) {
        qDebug() << "Subscribe failed";
        return;
    }
    qDebug() << "Subscribed to:" << topic;


}

void MyEdgeServer::startListening()
{
    if (client->state() == QMqttClient::Disconnected) {
        client->connectToHost();
        qDebug() << "Starting MQTT listener...";
    } else {
        qDebug() << "Listener already running";
    }
}




