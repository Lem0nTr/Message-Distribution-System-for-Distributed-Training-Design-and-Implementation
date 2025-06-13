#ifndef STARTMISSION_H
#define STARTMISSION_H
#include"myserver.h"
#include "modelaggregator.h"
#include<QStringList>
#include <QDir>
#include<QCoreApplication>
#include"interactionfromui.h"
#include<QMetaObject>
#include <QRegularExpression>
#include"mystatserver.h"

class StartMission: public QObject
{
    Q_OBJECT
public:
    explicit StartMission(QObject *parent = nullptr);
    ~StartMission();

    QString getArchiveBasePath() const;

    //設置端口
    void setServerPort(QString Port);

    void testSendMessage();

    //void startTrainingRound();
    void setInitModel(const QString &path) { m_initModelPath = path; }
    QString extractTimestampFromPath(const QString& filePath);//解析时间戳的函数，提取时间戳


    //void initialize(const QString &initModelPath);
    void setSendEnabled(bool enable) { m_sendEnabled = enable; }

    void StartAggregation(const QString &aggModelPath);

    QString getTestMsg();
private slots:
    void sendNextModel();
    void handleModelReceived(const QString& modelPath);
private:
    bool m_sendEnabled = true;
    //int m_iteration = 0;
    QString m_currentModelPath;//記錄模型路徑


private slots:


    //void handleAggregationResult(const QString &aggModelPath);

    void handleAggregationCompleted(const QString &aggModelPath);

    void handleAggregationFailed(const QString &errorMessage);

    void handleStartTrain(const int traintimes);

    void handleStatServerReceivedMessage(QString msg);
public slots:
    //void handleNewModels(const QString &modelPaths);

signals:
    void roundStarted(int round);
    void roundCompleted(const QString &modelPath);
    //void aggregationFailed(const QString &error);
    void modelStored(const QString &StoredModelPath);

private:
    MyServer * m_server;
    ModelAggregator *m_aggregator;
    InteractionFromUI *m_interactionFromUI;
    MyStatServer * m_myStatServer;

    QString m_initModelPath;
    int m_iteration = 0;
    QString m_outputBaseDir = "./aggregated_models";

    QString m_ModelParameter;//模型參數
    QString m_DataSet;//數據集
    QString m_Model;//模型
    QString m_currentBaseModel;
    int m_currentRound = 1;

    int m_remainingTrainTimes = 0; // 用于跟踪剩余训练次数
    QString m_LastbroadcastModelPath = "/home/stl/qt/qtdata/Central_Server/yolov8n.pt";//上一次發送的模型，初始化爲全局模型
    QString m_CurrentbroadcastModelPath;//本次剛聚合完成的模型路徑
    bool m_CurrentEpochCompleted;

    QString m_DecomposedTimestamp;//从项目目录中拆解出的时间戳

};

#endif // STARTMISSION_H
