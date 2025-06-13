// modelaggregator.cpp
#include "modelaggregator.h"
#include <QDebug>



ModelAggregator::ModelAggregator(QObject *parent) : QObject(parent) {}

void ModelAggregator::startAggregation(const QString &edgeModelPath,
                                       const QString &globalModelPath,
                                       int currentRound)
{
    m_currentRound = currentRound;
    QString outputPath = generateOutputPath(currentRound); // 生成带时间戳的路径

    // 构建Python脚本参数
    QStringList args = {
        m_scriptPath,
        "--edge-model", edgeModelPath,    // 边缘服务器聚合后的模型
        "--global-model", globalModelPath,// 上一次分发的全局模型
        "--output", outputPath + "/aggregated_model.pt", // 固定输出文件名
        "--round", QString::number(currentRound)
    };

    // 确保输出目录存在
    QDir().mkpath(outputPath);

    // 执行Python聚合脚本
    runPythonScript(args);
}

QString ModelAggregator::generateOutputPath(int round) const
{
    // 生成路径格式: aggregatefinalmodel/yyyyMMdd_hhmmss/round_001
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_hhmmss");
    return QString("aggregatefinalmodel/%1/round_%2")
        .arg(timestamp)
        .arg(round, 3, 10, QLatin1Char('0')); // 轮次补零到3位
}

void ModelAggregator::runPythonScript(const QStringList &args)
{
    QProcess *process = new QProcess(this);
    connect(process, &QProcess::finished,
            [=](int exitCode, QProcess::ExitStatus status){
                if (exitCode == 0) {
                    m_latestModel = args.at(5); // 获取output路径
                    emit aggregationCompleted(m_latestModel);
                } else {
                    emit aggregationFailed(QString("聚合失败 (Code %1):\n%2")
                                               .arg(exitCode)
                                               .arg(QString::fromLocal8Bit(process->readAllStandardError())));
                }
                process->deleteLater();
            });

    // 启动Python进程（注意路径中的空格需处理）
    process->start("/home/stl/anaconda3/envs/yolov8/bin/python", args);
}
