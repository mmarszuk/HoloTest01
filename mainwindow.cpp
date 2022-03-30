
#include <QFileDialog>
#include <QMessageBox>
#include <QPicture>

#include "mainwindow.h"
#include "ui_mainwindow.h"


MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    ui->splitterH->setSizes( {400,400} );
    ui->splitterV->setSizes( {300,100} );
    load("C:\\Users\\m\\Documents\\Hologram Phase\\Hologramy\\image_0000.bmp");
}


void MainWindow::load(const QString imagePath)
{
    QImage tmpImage( imagePath );
    if( imagePath.isNull() ) {
        QMessageBox::warning(this,"Warning",QString("The system can not open file [%1].").arg(imagePath) );
        return;
    }
    inpImage = tmpImage.convertToFormat(QImage::Format_Grayscale8);
    if( inpImage.isNull() ) {
        QMessageBox::warning(this,"Warning",QString("The system can convert to the grayscale.").arg(imagePath) );
        return;
    }
    ui->lbl_Origin->setPixmap( QPixmap::fromImage(inpImage) );
    outData.resize( inpImage.width() * inpImage.height() );

    fft2DPhase.test( inpImage.bits(), inpImage.width(), inpImage.height(), inpImage.bytesPerLine(), outData.data(), 40, 60 );

    QImage outImage( outData.data(), inpImage.width(), inpImage.height(),  inpImage.width(), QImage::Format_Grayscale8 );
    ui->lbl_Output->setPixmap( QPixmap::fromImage(outImage) );

}



MainWindow::~MainWindow()
{
    delete ui;
}


void MainWindow::on_pB_Open_clicked()
{
    const QString imagePath = QFileDialog::getOpenFileName(this,"Open image",QString(),"Images (*.png *.xpm *.jpg *.bmp)");
    if( imagePath.isNull() ) {
        return;
    }
    load( imagePath );
}



void MainWindow::on_pB_Exit_clicked()
{
    qApp->exit(0);
}


void MainWindow::on_pB_Compute_clicked()
{

}

