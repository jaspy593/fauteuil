#include "mainwindow.h"
#include <QDebug>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    socket.connectToHost("192.168.11.210",5000);

    connect(&socket,&QTcpSocket::connected,this,[&]()
            {
                qDebug()<<"Connexion réussie";

                socket.write("reculer\n");
            });

    connect(&socket,&QTcpSocket::errorOccurred,this,[&](QAbstractSocket::SocketError)
            {
                qDebug()<<socket.errorString();
            });
}
MainWindow::~MainWindow()
{
    delete ui;
}
