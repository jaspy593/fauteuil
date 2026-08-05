#include "mainwindow.h"
#include <QDebug>
#include <QElapsedTimer>

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    socket.connectToHost("192.168.11.114",5000);

    connect(&socket,&QTcpSocket::connected,this,[&]()
            {
                qDebug()<<"Connexion réussie";

                socket.write("reculer\n");
            });

    connect(&socket,&QTcpSocket::errorOccurred,this,[&](QAbstractSocket::SocketError)
            {
                qDebug()<<socket.errorString();
            });
    timer.start();
    connect(&socket, &QTcpSocket::readyRead, this, [&]()
            {    
                QByteArray data = socket.readAll();

                qDebug() << data;
                tampon += data;
                QList<QByteArray> lignes = tampon.split('\n');
                quint64 n = timer.elapsed();
                double seconde = n / 1000.0;
                c += lignes.size() - 1;
                tampon = lignes.last();
                if(seconde >= 1)
                {
                  qDebug() << c;
                  c=0;
                  timer.restart();
                }
            });
}
MainWindow::~MainWindow()
{
    delete ui;
}
