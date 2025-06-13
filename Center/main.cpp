#include <QCoreApplication>
#include<QTcpServer>
#include<QTcpSocket>
#include<QByteArray>
#include"myserver.h"
#include"startmission.h"





int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    StartMission mission1;
    int number = 11000;
    QString str = QString::number(number);
    mission1.setServerPort(str);

    // QString msg = mission1.getTestMsg();
    // qDebug() << msg ;

    return a.exec();
}
