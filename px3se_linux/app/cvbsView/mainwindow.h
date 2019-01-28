#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QStackedLayout>
#include <QStackedWidget>
#include <camerawidgets.h>
#include <base/basewindow.h>
#include <QThread>

class MainWindow : public baseWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    QStackedLayout *m_mainlyout;
    QStackedWidget *m_stackedWid;
    cameraWidgets *m_wid;
protected:
    void keyPressEvent(QKeyEvent *event);
private:
    bool mediaHasUpdate;

private:

    void initLayout();

private slots:
    void slot_appQuit();
    void slot_standby();
public slots:
    void slot_returnanimation();

};



#endif // MAINWINDOW_H
