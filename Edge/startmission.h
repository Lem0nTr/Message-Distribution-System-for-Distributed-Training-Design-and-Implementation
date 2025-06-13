#ifndef STARTMISSION_H
#define STARTMISSION_H

#include <QObject>
#include "myedgeclient.h"
#include "myedgeserver.h"
#include "modelaggregator.h"
#include "myedgestatclient.h"

class StartMission: public QObject
{
    Q_OBJECT
public:
    explicit StartMission(QObject *parent = nullptr);
    ~StartMission();

    // 设置中心服务器连接参数
    void setCenterServerParams(const QString &ip, quint16 port);

    // 设置边缘服务器参数
    // void setEdgeServerParams(const QString &hostname, quint16 port,
    //                          const QString &username, const QString &password);

    //設置要連接的服務器端口和ip地址
    //void SetClientPara(QString IP ,QString Port);

    // 开始任务流程
    void start();

    void testPublish();

    void testSendStatus();

signals:
    void aggregationCompleted(const QString &modelPath);
    void modelSentToCenter(bool success);
    void aggregationFailed(const QString &error); // 新增失败信号

private slots:
    void handleNewModelReceived(const QString &clientId, const QString &modelPath);
    void handleAggregationResult(const QString &aggregatedModelPath);
    void handleModelSendResult(bool success);
    void handleNewModelFromCenter(const QString &modelPath);

private:
    MyEdgeClient *m_edgeClient;      // 与中心服务器通信
    MyEdgeServer *m_edgeServer;      // 接收客户端模型
    ModelAggregator *m_aggregator;   // 模型聚合
    MyEdgeStatClient * m_edgeStatClient;//状态客户端

    QString m_currentAggregatedModel; // 当前聚合模型路径
    QString m_currentRoundId; // 新增成员变量
    // MyEdgeClient * m_MyEdgeClient;//向中心服務器發送數據
    // MyEdgeServer * m_MyEdgeServer;



    QString m_IP;
    QString m_Port;

};

#endif // STARTMISSION_H
