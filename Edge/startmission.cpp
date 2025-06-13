#include "startmission.h"


StartMission::StartMission(QObject *parent)
    : QObject(parent),
    m_edgeClient(new MyEdgeClient(this)),
    m_edgeServer(new MyEdgeServer(this)),
    m_aggregator(new ModelAggregator(this)),
    m_edgeStatClient(new MyEdgeStatClient(this))

{

    // 连接边缘服务器信号
    connect(m_edgeServer, &MyEdgeServer::modelReceived,
            this, &StartMission::handleNewModelReceived);

    // 连接聚合器信号
    connect(m_aggregator, &ModelAggregator::aggregationCompleted,
            this, &StartMission::handleAggregationResult);
    connect(m_aggregator, &ModelAggregator::aggregationFailed,
            this, [](const QString &error) {
                qWarning() << "聚合失败:" << error;
            });

    //连接中心客户端信号
    connect(m_edgeClient, &MyEdgeClient::modelSent,
            this, &StartMission::handleModelSendResult);

    connect(m_edgeClient , &MyEdgeClient::saveCenterModel , this , &StartMission::handleNewModelFromCenter);



}

StartMission::~StartMission()
{
    // QObject的子对象会自动删除，不需要手动delete
}

void StartMission::setCenterServerParams(const QString &ip, quint16 port)
{
    qDebug() << "设置中心服务器参数: IP:" << ip << "Port:" << port;
    m_edgeClient->connectToCenter(ip, port);

    // 添加连接结果检查
    QTimer::singleShot(1000, [this]() {
        qDebug() << "连接状态检查:" << m_edgeClient->isConnected();
    });
}



void StartMission::start()
{
    qDebug() << "Starting mission...";
    m_edgeServer->startListening();
}


void StartMission::handleNewModelReceived(const QString &clientId, const QString &modelPath)
{
    qDebug() << "收到来自客户端" << clientId << "的模型：" << modelPath;

    //Step5:边缘服务器接收到客户端1模型
    //Step6:边缘服务器接收到客户端2模型
    if(clientId == "client01"){
        QString Step5 = "Step5:边缘服务器接收到客户端1模型";
        m_edgeStatClient->sendMessage(Step5);
    }
    if(clientId == "client02"){
        QString Step6 = "Step6:边缘服务器接收到客户端2模型";
        m_edgeStatClient->sendMessage(Step6);
    }

    // 创建带时间戳的聚合目录
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    m_currentRoundId = QString("round_%1").arg(timestamp);

    // 添加模型到聚合器（自动触发聚合检查）
    m_aggregator->addClientModel(clientId, modelPath);
}


void StartMission::handleAggregationResult(const QString &aggregatedModelPath)
{
    //Step7:边缘服务器触发聚合
    QString Step7 = "Step7:边缘服务器触发聚合";
    m_edgeStatClient->sendMessage(Step7);

    qDebug() << "聚合完成，原始模型路径：" << aggregatedModelPath;

    // 验证文件有效性
    QFileInfo fileInfo(aggregatedModelPath);
    if (!fileInfo.exists() || fileInfo.size() == 0) {
        qCritical() << "无效的模型文件";
        emit aggregationFailed("无效的模型文件");
        return;
    }

    // 创建归档目录
    const QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    m_currentRoundId = QString("round_%1").arg(timestamp);

    QString archiveDir = QString("aggregated_results/%1").arg(m_currentRoundId);
    QDir().mkpath(archiveDir);

    // 生成最终保存路径
    QString finalPath = QString("%1/%2").arg(archiveDir).arg(fileInfo.fileName());

    // 复制文件到归档位置
    if (QFile::exists(finalPath)) {
        QFile::remove(finalPath);
    }

    if (!QFile::copy(aggregatedModelPath, finalPath)) {
        qCritical() << "模型归档失败! 源:" << aggregatedModelPath << "目标:" << finalPath;
        emit aggregationFailed("文件存储失败");
        return;
    }

    qDebug() << "模型已归档至：" << finalPath
             << "大小:" << QFileInfo(finalPath).size() << "字节";

    m_currentAggregatedModel = finalPath;

    // 发送模型前检查连接状态
    if (!m_edgeClient->isConnected()) {
        qWarning() << "无法发送模型：未连接到中心服务器";
        emit aggregationFailed("未连接到中心服务器");
        return;
    }

    qDebug() << "准备发送模型到中心服务器..."
             << "\n- 文件路径:" << finalPath
             << "\n- 文件大小:" << QFileInfo(finalPath).size() << "字节"
             << "\n- 连接状态:" << m_edgeClient->isConnected();

    // 发送模型
    m_edgeClient->sendModel(finalPath);
    emit aggregationCompleted(finalPath);
}


void StartMission::handleModelSendResult(bool success)
{
    if (success) {
        qDebug() << "Model successfully sent to center server";
    } else {
        qWarning() << "Failed to send model to center server";
    }
    emit modelSentToCenter(success);

    //Step8:边缘服务器将模型发送中心服务器
    QString Step8 = "Step8:边缘服务器将模型发送中心服务器";
    m_edgeStatClient->sendMessage(Step8);

}

void StartMission::testPublish(){

    m_edgeServer->publishModel("/home/stl/qt/qtdata/Edge_Server01/build/Desktop_Qt_6_5_3_GCC_64bit-Debug/received_model.pt");

}

void StartMission::handleNewModelFromCenter(const QString &modelPath){

    qDebug() << "发送模型给客户端";
    m_edgeServer->publishModel(modelPath);

    //Step2:边缘服务发送全局模型
    QString Step2 = "Step2:边缘服务发送全局模型";
    m_edgeStatClient->sendMessage(Step2);

    //Step3:客户端1接收模型并训练
    //Step4:客户端2接收模型并等待并训练
    QString Step3 = "Step3:客户端1接收模型并训练";
    QString Step4 = "Step4:客户端2接收模型并等待并训练";
    m_edgeStatClient->sendMessage(Step3);
    // //1min的延迟，适配客户端2的训练延迟
    m_edgeStatClient->sendMessage(Step4);


}

void StartMission::testSendStatus(){

    QString msg ="hello! this is edgestatclient";
    QTimer::singleShot(500, [=](){ // 500ms 后发送
        m_edgeStatClient->sendMessage(msg);
    });
    qDebug() << "mes = " << msg;

}
