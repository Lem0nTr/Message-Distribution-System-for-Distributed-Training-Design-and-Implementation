#ifndef INTERACTIONFROMUI_H
#define INTERACTIONFROMUI_H

#include <QObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QDebug>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStringList>
#include <QRegularExpression>
#include <QWebSocket>
#include <QWebSocketServer>
#include <algorithm>
#include<QList>
#include<QMutex>
#include<QDir>
#include <QRegularExpression>
#include<QTimer>

class InteractionFromUI: public QTcpServer
{
    Q_OBJECT
public:
    explicit InteractionFromUI(QObject *parent = nullptr);

    //bool parseTrainingCommand(const QByteArray &postData, QString &command, int &times);
    void setTotalSteps(int total); // 新增公共接口

    void broadcastMessage(const QJsonObject &msg);

    void sendProgressUpdate();

    bool processTrainingCommand(const QString& params, int& times);

    bool parseUICommand(const QByteArray &postData, QString &command, QString &params);

    void onNewWebSocketConnection();

    void handleUpload(const QString& timestamp);

    bool validateTimestamp(const QString& timestamp);

    QString findModelFile(const QString& dirPath);

    void sendFileToWebpage(QTcpSocket* clientSocket);

    void addHistoryEntry(const QString& timestamp);

    void sendTextToWebPage(const QString& message);
private:
    QString formatTimestamp(const QString& rawTimestamp);

public slots:
    void incrementProgress();
    void enableDownload();

signals:
    void UICommend(QString command ,QString param);
    void parsedTrainCommandFromUI(int trainTimes);
    void readyToSendFile(QString filepath);
    void progressUpdated(int current, int total);
    void trainingCompleted();

protected:
    void incomingConnection(qintptr socketDescriptor) override;

private slots:
    void readClientData();
    void handleDisconnected();

    void handleUICommand(const QString& cmdType ,const QString& parameters);

    void threadSafeIncrement() {
        QMutexLocker locker(&m_mutex);
        incrementProgress();
    }

private:
    QTcpSocket *clientSocket;
    QWebSocketServer *m_webSocketServer;
    QList<QWebSocket*> m_webSocketClients; // 修改为QList
    QString m_command;
    QString m_param;//命令參數
    int m_traintimes;//訓練次數

    int m_currentStep = 0;
    int m_totalSteps = 0;
    bool m_trainingFinished = false;

    QMutex m_mutex;

    QString m_currentModelPath; // 当前要下载的模型文件路径

    QTcpSocket* m_currentClientForDownload = nullptr;
};

#endif // INTERACTIONFROMUI_H
