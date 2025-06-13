#include "mymqttclient.h"

MyMqttClient::MyMqttClient() {
    m_client = new QMqttClient(this);
    m_client->setHostname("192.168.44.129");
    m_client->setPort(1883);
    m_client->setUsername("Client01");
    m_client->setPassword("Client01");


    // 连接信号槽
    connect(m_client, &QMqttClient::connected, this, &MyMqttClient::onConnected);
    connect(m_client, &QMqttClient::messageReceived, this, &MyMqttClient::onMessageReceived);
    // connect(m_client, SIGNAL(messageReceived(QByteArray,QMqttTopicName)),
    //         this, SLOT(onMessageReceived(QByteArray,QMqttTopicName)));

    m_client->connectToHost();

    //調試，觸發一次訓練
    //emit startTrainingRequested();
}

MyMqttClient::~MyMqttClient() {
    delete m_client;
}

void MyMqttClient::connectToBroker(){
    m_client->connectToHost();

}

void MyMqttClient::publish(const QString &topic, const QByteArray &message){

    if (m_client->state() == QMqttClient::Connected) {
        m_client->publish(topic, message);
    }

}

void MyMqttClient::subscribe(const QString &topic){

    auto subscription = m_client->subscribe(topic);
    if (!subscription) {
        qDebug() << "Subscribe failed";
        return;
    }
    qDebug() << "Subscribed to:" << topic;


}

void MyMqttClient::onConnected(){
    qDebug() << "Connected to MQTT Broker!";
    subscribe("edge/model/#");  // 通配符订阅所有子主题
    receivedModelData.clear();        // 清空之前的数据
    expectedSize = 0;                // 重置期望大小
}



void MyMqttClient::onMessageReceived(const QByteArray &message, const QMqttTopicName &topic) {
    qDebug() << "Received topic:" << topic.name() << "size:" << message.size();

    if (topic.name().startsWith("edge/model/chunk_")) {
        receivedModelData.append(message);
        qDebug() << "Chunk received. Current total size:" << receivedModelData.size();
    }
    else if (topic.name() == "edge/model/end") {
        expectedSize = message.toInt();
        if (receivedModelData.size() == expectedSize) {

            // 生成时间戳目录名（格式：yyyyMMdd_HHmmss）
            QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");

            // 构建保存路径：项目目录/received_models/时间戳/
            QString saveDir = QString("%1/received_models/%2")
                                  .arg(QCoreApplication::applicationDirPath(), timestamp);

            // 确保目录存在
            QDir dir;
            if (!dir.mkpath(saveDir)) {
                qDebug() << "Failed to create directory:" << saveDir;
                receivedModelData.clear();
                return;
            }

            // 构建完整文件路径
            QString filePath = QString("%1/received_model.pt").arg(saveDir);


            QFile file(filePath); // 修改此处
            if (file.open(QIODevice::WriteOnly)) {
                file.write(receivedModelData);
                qDebug() << "Model saved successfully at:" << filePath;
                emit ModelSaved(filePath);
                qDebug() << "Size:" << expectedSize;
            } else {
                qDebug() << "Failed to open file:" << filePath;
            }
        } else {
            qDebug() << "Error: Size mismatch. Expected:" << expectedSize
                     << "Actual:" << receivedModelData.size();
        }
        receivedModelData.clear();
    }
}


void MyMqttClient::publishModel(const QString &filePath) {
    bool success = false;

    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        qDebug() << "File open error:" << file.errorString();
        return;
    }

    QByteArray modelData = file.readAll();
    file.close();

    // 发送文件哈希（校验用）
    // QMqttTopicName hashTopic("client01/model/end");
    // QByteArray hash = QCryptographicHash::hash(modelData, QCryptographicHash::Md5);
    // m_client->publish(hashTopic, hash);

    // 分块发送（确保顺序）
    const int chunkSize = 512 * 1024;
    for (int i = 0; i < modelData.size(); i += chunkSize) {
        QByteArray chunk = modelData.mid(i, chunkSize);
        QString topic = QString("client01/model/chunk_%1").arg(i / chunkSize);
        m_client->publish(topic, chunk, 1);  // QoS 1 确保送达
        QThread::msleep(10);  // 轻微延迟
    }

    // 发送结束标记
    //m_client->publish("client01/model/end", QByteArray::number(modelData.size()), 1);
    QMqttTopicName endTopic("client01/model/end");
    if (m_client->publish(endTopic, QByteArray::number(modelData.size())) != -1) {
        success = true;
    }
    emit modelPublished(success);

}

