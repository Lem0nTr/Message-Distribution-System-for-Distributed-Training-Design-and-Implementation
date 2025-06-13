// modelaggregator.cpp
#include "modelaggregator.h"
#include <QDir>
#include <QDateTime>
#include <QProcess>
#include <QDebug>


ModelAggregator::ModelAggregator(QObject *parent) : QObject(parent) {
    QDir dir("aggregated_models");
    if (!dir.exists()) {
        dir.mkpath(".");
    }
}



void ModelAggregator::addClientModel(const QString &clientId, const QString &modelPath) {
    // 正确解析路径结构：received_models/round_20250330_170520/client01/model.pt
    QFileInfo info(modelPath);

    // 获取模型文件所在目录（示例：client01/）
    QDir modelDir = info.dir();

    // 上溯到轮次目录（round_20250330_170520）
    if (!modelDir.cdUp()) {
        qWarning() << "无法上溯到轮次目录：" << modelDir.path();
        return;
    }

    QString roundId = modelDir.dirName();
    qDebug() << "[聚合器] 客户端:" << clientId
             << " | 轮次ID:" << roundId
             << " | 完整路径:" << modelPath;

    m_roundModels[roundId][clientId] = modelPath;
    checkRoundCompletion(roundId);
}


void ModelAggregator::checkRoundCompletion(const QString &roundId)
{

    qDebug() << "检查轮次:" << roundId
             << "现有客户端:" << m_roundModels[roundId].keys();

    if (!m_roundModels.contains(roundId)) return;

    const auto &roundData = m_roundModels[roundId];
    const QStringList requiredClients = {"client01", "client02"}; // 需要这两个客户端的模型

    // 检查是否包含所有必要客户端
    bool hasAllClients = true;
    for (const auto &client : requiredClients) {
        if (!roundData.contains(client)) {
            hasAllClients = false;
            break;
        }
    }

    if (hasAllClients) {
        QStringList modelPaths = {
            roundData["client01"],
            roundData["client02"]
        };

        // 创建带时间戳的输出目录
        QString outputDir = QString("aggregated_models/%1").arg(roundId);
        QString outputPath = QString("%1/aggregated_model.pt").arg(outputDir);

        QDir().mkpath(outputDir);

        if (performAggregation(modelPaths, outputPath)) {
            emit aggregationCompleted(outputPath);
            m_roundModels.remove(roundId); // 清除已完成轮次
            qDebug() << "[聚合器] 轮次" << roundId << "聚合完成";
        }
    }
}

bool ModelAggregator::performAggregation(const QStringList &modelPaths, const QString &outputPath) {

    // 创建带时间戳的目录
    QDir outputDir(QFileInfo(outputPath).absolutePath());
    if (!outputDir.mkpath(".")) {
        emit aggregationFailed("无法创建聚合目录");
        return false;
    }

    // 验证输入
    if (modelPaths.size() < 2) {
        qCritical() << "至少需要2个模型进行聚合";
        emit aggregationFailed("至少需要2个模型进行聚合");
        return false;
    }

    // 准备输出目录
    QFileInfo outputInfo(outputPath);
    QDir().mkpath(outputInfo.absolutePath());

    // 执行聚合脚本
    const QString pythonScript = "/home/stl/Yolo/ultralytics-main/aggregate_models.py";
    // QStringList args = {
    //     "--model1", modelPaths[0],
    //     "--model2", modelPaths[1],
    //     "--output", outputPath
    // };

    // 修改Python脚本调用参数
    QStringList args = {
        "--model1", modelPaths[0],
        "--model2", modelPaths[1],
        "--output", outputPath,
        //"--timestamp", QDateTime::currentDateTime().toString(Qt::ISODate)
    };


    QProcess process;
    process.start("/home/stl/anaconda3/envs/yolov8/bin/python", QStringList() << pythonScript << args);

    // 处理执行结果
    if (!process.waitForFinished(60000)) {
        qDebug() << "聚合超时:" << process.errorString();
        emit aggregationFailed("聚合操作超时");
        return false;
    }

    if (process.exitCode() != 0) {
        qDebug() << "聚合失败:"
                 << "\n错误输出:" << process.readAllStandardError()
                 << "\n标准输出:" << process.readAllStandardOutput();
        emit aggregationFailed("聚合脚本执行失败");
        return false;
    }

    if (!QFileInfo::exists(outputPath)) {
        qDebug() << "输出文件未生成:" << outputPath;
        emit aggregationFailed("输出文件生成失败");
        return false;
    }

    return true;
}

QString ModelAggregator::getAggregatedModelPath() const {
    return m_aggregatedModelPath;
}
