#pragma once

#include <QMainWindow>

QT_BEGIN_NAMESPACE
namespace Ui { class MainWindow; }
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT
private:
    QImage image;

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_pB_Open_clicked();
    void on_pB_Exit_clicked();

    void on_pB_Compute_clicked();

private:
    Ui::MainWindow *ui;
};

