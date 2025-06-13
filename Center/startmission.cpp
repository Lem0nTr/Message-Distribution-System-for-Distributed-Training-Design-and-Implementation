#include "startmission.h"


StartMission::StartMission(QObject *parent)
    : QObject(parent),
    m_server(new MyServer(this)),
    m_aggregator(new ModelAggregator(this)),
    m_interactionFromUI(new InteractionFromUI(this)),
    m_myStatServer(new MyStatServer(this))
{

    // connect(m_server, &MyServer::modelsReceived,
    //         this, &StartMission::handleNewModels);
    connect(m_server, &MyServer::modelReceived,
            this, &StartMission::handleModelReceived);


    connect(m_interactionFromUI , &InteractionFromUI::parsedTrainCommandFromUI ,
            this ,&StartMission::handleStartTrain);

    QMetaObject::invokeMethod(m_interactionFromUI, "threadSafeIncrement",
                              Qt::QueuedConnection);

    connect(m_myStatServer , &MyStatServer::receivedMessage ,
            this ,&StartMission::handleStatServerReceivedMessage);

}

void StartMission::setServerPort(QString Port){

    if(Port == nullptr) return;
    m_server->Listen(Port);

}

void StartMission::testSendMessage(){

    QString msg = "你好，這裏是中心服務器發送測試消息！";
    //m_server->SendMessage(msg);

}

StartMission::~StartMission() {

    //delete m_server;
}




void StartMission::handleAggregationCompleted(const QString& aggModelPath)
{
    if (QFile::exists(aggModelPath)) {
        //m_server->setCurrentModelPath(aggModelPath);
        //m_iteration++;
        qDebug() << "Aggregation completed. New global model:" << aggModelPath;

    } else {
        qCritical() << "Aggregated model not found:" << aggModelPath;
    }
}


void StartMission::handleModelReceived(const QString& modelPath)
{
    qDebug() << "modelReceived 信号触发,且输出路径为：" << modelPath;


    QFile sourceFile(modelPath);
    if (!sourceFile.exists()) {
        qCritical() << "Source model not found:" << modelPath;
        return;
    }

    // 更新最新模型路径
    m_LastbroadcastModelPath = modelPath;
    qDebug() << "m_LastbroadcastModelPath已更新为:" << modelPath;
    m_interactionFromUI->incrementProgress();//前端进度设置更新

    QString Step9 = "Step9:中心服务器接收模型并判断是否结束";
    m_interactionFromUI->sendTextToWebPage(Step9);
    qDebug() << Step9;

    // 检查是否需要继续训练
    if (m_remainingTrainTimes > 0) {
        qDebug() << "准备下一轮训练...";

        QString TotalStatus = "训练任务未结束，准备进行下一轮训练";
        m_interactionFromUI->sendTextToWebPage(TotalStatus);

        QTimer::singleShot(0, this, &StartMission::sendNextModel); // 通过事件循环确保时序
    } else {

        QString TotalStatus = "训练流程全部完成";
        m_interactionFromUI->sendTextToWebPage(TotalStatus);

        qDebug() << "训练流程全部完成";
        m_DecomposedTimestamp = extractTimestampFromPath(modelPath);
        qDebug() << "拆解出的时间戳为： " << m_DecomposedTimestamp;
        m_interactionFromUI->addHistoryEntry(m_DecomposedTimestamp);

        m_LastbroadcastModelPath = "/home/stl/qt/qtdata/Central_Server/yolov8n.pt";
        qDebug() << "m_LastbroadcastModelPath已经重置为:" << m_LastbroadcastModelPath;
    }
}

QString StartMission::getArchiveBasePath() const
{
    // 使用可配置路径或相对路径
    return QCoreApplication::applicationDirPath() + "/received_models";
}

void StartMission::StartAggregation(const QString &aggModelPath){

    m_aggregator->startAggregation(aggModelPath , m_LastbroadcastModelPath ,m_currentRound);


    m_LastbroadcastModelPath = aggModelPath;//將當前聚合的模型路徑變成上一次聚合的模型路徑

}

void StartMission::handleAggregationFailed(const QString &errorMessage){

    qDebug().noquote() << "[聚合失败]" << errorMessage;


}

void StartMission::handleStartTrain(const int traintimes)
{
    qDebug() << "handleStartTrain信号触发，接收到的训练次数为" << traintimes;

    if (traintimes <= 0) return;

    m_remainingTrainTimes = traintimes; // 初始化剩余次数

    m_interactionFromUI->setTotalSteps(traintimes);//设置前端初始进度

    QString TotalStatus = "训练任务开始";
    m_interactionFromUI->sendTextToWebPage(TotalStatus);

    //m_interactionFromUI->incrementProgress(); //前端进度变化
    sendNextModel(); // 启动第一次训练



}

void StartMission::sendNextModel()
{
    QString Step1 = "Step1:中心服务器下发全局模型";
    m_interactionFromUI->sendTextToWebPage(Step1);
    qDebug() << Step1;

    if (m_remainingTrainTimes > 0) {
        qDebug() << "开始执行倒数第" << (m_remainingTrainTimes) << "次训练";
        m_server->sendModel(m_LastbroadcastModelPath);
        m_remainingTrainTimes--; // 递减计数器
    } else {
        qDebug() << "所有训练轮次已完成";
    }
}

QString StartMission::extractTimestampFromPath(const QString& filePath) {
    // 定义匹配时间戳的正则表达式
    // 格式：8位数字(yyyyMMdd) + 下划线 + 6位数字(hhmmss)
    QRegularExpression regex(R"((\d{8}_\d{6}))");
    QRegularExpressionMatch match = regex.match(filePath);

    if (match.hasMatch()) {
        return match.captured(1); // 返回第一个捕获组（完整时间戳）
    }

    return QString(); // 未匹配到返回空字符串
}

QString StartMission::getTestMsg(){

    return m_myStatServer->getLastMessage();

}

void StartMission::handleStatServerReceivedMessage(QString msg){

    qDebug() << "中心服务器读取到的状态信息为：" << msg;
    m_interactionFromUI->sendTextToWebPage(msg);
}
