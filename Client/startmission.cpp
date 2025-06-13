#include "startmission.h"



StartMission::StartMission() {

    m_mymqttclient = new MyMqttClient;
    m_trainer = new TrainModel;

    // 连接信号槽
    connect(m_trainer, &TrainModel::trainingFinished,
            this, &StartMission::onTrainingFinished);

    //訓練成功之後發送模型給邊緣服務器
    connect(m_mymqttclient, &MyMqttClient::publishModelSingal,this, &StartMission::onModelPublished);

    connect(m_mymqttclient, &MyMqttClient::ModelSaved ,this , &StartMission::saveModelPath);

    //便於調試，當這個信號觸發，訓練一次
    //connect(m_mymqttclient, &MyMqttClient::startTrainingRequested, m_trainer, &TrainModel::startTraining);

}

StartMission::~StartMission(){

    delete m_mymqttclient;
    delete m_trainer;


}

void StartMission::SetSubcribe(QString &topic){

    m_mymqttclient->subscribe(topic);

}

void StartMission::startTrainingCycle()
{
    // 启动首次训练（后续由Timer触发）
    m_trainer->startTraining(m_saveFromEdgeModel);
    qDebug() << "开始执行startTrain";

}



void StartMission::onTrainingFinished(bool success, const QString &message) {
    if (success) {
        qDebug() << "[Success] Model ready at:" << message;
        m_mymqttclient->publishModel(message);  // 触发模型上传
    } else {
        qCritical() << "[Failed]" << message;
        // 带延迟的重试（避免立即重试导致雪崩）
        QTimer::singleShot(5000, this, [this]() {
            qDebug() << "Retrying training...";
            m_trainer->startTraining(m_saveFromEdgeModel);
        });
    }
}

void StartMission::onModelPublished(bool success)
{
    if (success) {
        qDebug() << "[StartMission] Model published to edge server!";
    } else {
        qCritical() << "[StartMission] Model upload failed! Retrying...";
        QTimer::singleShot(5000, [this]() {  // 5秒后重试
            m_mymqttclient->publishModel(m_currentModelPath);
        });
    }
}

void StartMission::retryTraining() {
    qDebug() << "Retrying training...";
    m_trainer->startTraining(m_saveFromEdgeModel);
}

void StartMission::saveModelPath(const QString &modelpath){
    qDebug() << "边缘服務器接收的模型路徑成功保存!";
    m_saveFromEdgeModel = modelpath;

    startTrainingCycle();
}
