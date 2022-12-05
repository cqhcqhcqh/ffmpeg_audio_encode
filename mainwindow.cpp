#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "encodethread.h"
MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
}

MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_resampleButton_clicked()
{
    EncodeThread *thread = new EncodeThread(this);
    thread->start();
}

