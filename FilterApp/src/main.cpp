#include "mainwindow.h"
#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    
    MainWindow w;
    w.setWindowTitle("Фильтрация сигналов от модели PID");
    w.resize(1200, 800);
    w.show();
    
    return a.exec();
}