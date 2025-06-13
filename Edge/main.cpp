#include <QCoreApplication>
#include"startmission.h"


int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    StartMission mission1;
    //mission1.testPublish();
    //作爲客戶端連接中心服務器
    int number1 = 11000;
    //QString str1 = QString::number(number1);
    QString  IP = "192.168.44.129";
    mission1.setCenterServerParams(IP , number1);
    // mission1.testSendStatus();

    // int number2 = 12000;
    // QString str2 = QString::number(number2);
    // mission1.SetServerPort(str2);

    return a.exec();
}
