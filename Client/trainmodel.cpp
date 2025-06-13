#include "trainmodel.h"



TrainModel::TrainModel(QObject *parent) : QObject(parent)
{
    m_process = new QProcess(this);
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
            this, &TrainModel::onProcessFinished);
}

void TrainModel::startTraining(const QString& modelPath)
{
    QString pythonPath = "/home/stl/anaconda3/envs/yolov8/bin/python";
    QString scriptPath = "/home/stl/Yolo/ultralytics-main/train02.py";
    QString outputDir = "/home/stl/qt/qtdata/Client02/models";

    QDir().mkpath(outputDir);

    // 添加模型路径参数
    m_process->start(pythonPath, {
                                     scriptPath,
                                     "--model-path",
                                     modelPath,  // 传入的模型路径参数
                                     "--output-dir",
                                     outputDir
                                 });

    if (!m_process->waitForStarted()) {
        qDebug() << "Failed to start process:" << m_process->errorString();
    }
}


void TrainModel::onProcessFinished(int exitCode, QProcess::ExitStatus status) {
    QString output = m_process->readAllStandardOutput();
    QString error = m_process->readAllStandardError();

    qDebug() << "Process stdout (last 200 chars):" << output.right(200);
    qDebug() << "Process stderr:" << error;

    QString modelPath = extractModelPath(output);
    if (!modelPath.isEmpty() && QFile::exists(modelPath)) {
        qDebug() << "Training succeeded. Model path:" << modelPath;
        emit trainingFinished(true, modelPath);
    } else {
        QString errorMsg = "Training failed. ";
        if (!error.isEmpty()) errorMsg += "Error: " + error;
        qCritical() << errorMsg;
        emit trainingFinished(false, errorMsg);
    }
}


QString TrainModel::extractModelPath(const QString &output) {
    // 移除所有控制字符（包括\r和颜色代码）
    QString cleanOutput;
    for (const QChar &c : output) {
        if (c == '\n' || (c >= ' ' && c <= '~')) {
            cleanOutput.append(c);
        }
    }

    // 提取最后一个完整的JSON对象
    int jsonStart = cleanOutput.lastIndexOf("{\"");
    int jsonEnd = cleanOutput.lastIndexOf("}") + 1;

    if (jsonStart == -1 || jsonEnd <= jsonStart) {
        qWarning() << "No valid JSON found in:" << cleanOutput;
        return "";
    }

    QString jsonStr = cleanOutput.mid(jsonStart, jsonEnd - jsonStart);
    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(jsonStr.toUtf8(), &error);

    if (error.error != QJsonParseError::NoError) {
        qWarning() << "JSON parse error:" << error.errorString()
        << "in:" << jsonStr;
        return "";
    }

    QJsonObject obj = doc.object();
    if (obj["status"].toString() == "success") {
        return obj["model_path"].toString();
    }

    return "";
}
