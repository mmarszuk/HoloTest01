#include <QFileDialog>
#include <QMessageBox>
#include <QPicture>

#include "mainwindow.h"
#include "ui_mainwindow.h"

#include "ipp/latest/include/ipp.h"


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


void MainWindow::on_pB_Open_clicked()
{
    const QString imagePath = QFileDialog::getOpenFileName(this,"Open image",QString(),"Images (*.png *.xpm *.jpg)");
    if( imagePath.isNull() ) {
        return;
    }
    QImage tmpImage( imagePath );
    if( imagePath.isNull() ) {
        QMessageBox::warning(this,"Warning",QString("The system can not open file [%1].").arg(imagePath) );
        return;
    }
    image = tmpImage.convertToFormat(QImage::Format_Grayscale8);
    if( image.isNull() ) {
        QMessageBox::warning(this,"Warning",QString("The system can convert to the grayscale.").arg(imagePath) );
        return;
    }
    ui->lbl_Origin->setPixmap( QPixmap::fromImage(image) );
}



void MainWindow::on_pB_Exit_clicked()
{
    qApp->exit(0);
}


void MainWindow::on_pB_Compute_clicked()
{

}

