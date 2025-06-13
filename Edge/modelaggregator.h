#ifndef MODELAGGREGATOR_H
#define MODELAGGREGATOR_H

#include <QObject>
#include <QMap>
#include <QString>

class ModelAggregator : public QObject {
    Q_OBJECT
public:
    explicit ModelAggregator(QObject *parent = nullptr);

    void addClientModel(const QString &clientId, const QString &modelPath);
    QString getAggregatedModelPath() const;

signals:
    void aggregationCompleted(const QString &aggregatedModelPath);
    void aggregationFailed(const QString &error);

private:
    QMap<QString, QMap<QString, QString>> m_roundModels;  // <RoundID, <ClientID, ModelPath>>
    QString m_aggregatedModelPath;
    //QMap<QString, QMap<QString, QString>> m_roundModels; // <RoundID, <ClientID, ModelPath>>
    QString m_currentRound; // 新增当前轮次跟踪
    void checkRoundCompletion(const QString &roundId);
    bool performAggregation(const QStringList &modelPaths, const QString &outputPath);
};

#endif // MODELAGGREGATOR_H
