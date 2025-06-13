#ifndef STARTMISSION_H
#define STARTMISSION_H

#include<QObject>
#include<QDebug>
//#include"QtMqtt/qmqttclient.h"
#include"mymqttclient.h"
#include"trainmodel.h"


//StartMission 协调类，将训练（TrainerController）和模型发布（MyMqttClient）解耦
class StartMission:public QObject
{
    Q_OBJECT
public:
    StartMission();
    ~StartMission();

    void SetSubcribe(QString &topic);



    void startTrainingCycle();
private slots:
    void onTrainingFinished(bool success, const QString &message);
    void onModelPublished(bool success);

private slots:
    void retryTraining();  // 添加这行
    void saveModelPath(const QString &modelpath);
private:
    MyMqttClient * m_mymqttclient;
    TrainModel *m_trainer;
    QString m_currentModelPath;  // 当前训练的模型路径
    QString m_saveFromEdgeModel;
};

#endif // STARTMISSION_H
