#include <QCoreApplication>
#include"startmission.h"
#include <Python.h>

int main(int argc, char *argv[])
{
    QCoreApplication a(argc, argv);

    StartMission mission1;
    //mission1.SetSubcribe();

    //mission1.startTrainingCycle();

    return a.exec();
}
