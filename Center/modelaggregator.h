// modelaggregator.h
#ifndef MODELAGGREGATOR_H
#define MODELAGGREGATOR_H

#include <QObject>
#include <QProcess>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>

class ModelAggregator : public QObject
{
    Q_OBJECT
public:
    explicit ModelAggregator(QObject *parent = nullptr);

    // void aggregate(const QStringList &modelPaths,
    //                const QString &initModelPath,
    //                int iteration,
    //                const QString &outputDir);

    QString latestAggregatedModel() const { return m_latestModel; }
    void startAggregation(const QString &edgeModelPath,
                          const QString &globalModelPath,
                          int currentRound);
signals:
    void aggregationCompleted(const QString &aggModelPath);
    void aggregationFailed(const QString &error);

private:
    const QString m_aggregationScript = "/home/stl/qt/qtdata/Central_Server/aggregate_models.py";
    QString generateOutputPath(int round) const;
    void runPythonScript(const QStringList &args);

    QString m_scriptPath = "/home/stl/qt/qtdata/Central_Server/aggregate_models.py";
    QString m_latestModel;
    int m_currentRound = 0;

};

#endif // MODELAGGREGATOR_H
