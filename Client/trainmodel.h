#ifndef TRAINMODEL_H
#define TRAINMODEL_H

#include<QObject>
//#include <Python.h>
#include <QProcess>
#include <QDebug>
#include "mymqttclient.h"
#include <QTimer>
#include <QDir>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QJsonObject>

class TrainModel:public QObject
{
    Q_OBJECT
public:
    //TrainModel();
    //~TrainModel();

    explicit TrainModel(QObject *parent = nullptr);
    void startTraining(const QString& modelPath);  // 启动单次训练

signals:
    void trainingFinished(bool success, const QString &message);  // 返回模型路径或错误信息

private slots:
    void onProcessFinished(int exitCode, QProcess::ExitStatus status);

private:
    QProcess *m_process;
    QString extractModelPath(const QString &output);  // 声明解析函数



};

#endif // TRAINMODEL_H
