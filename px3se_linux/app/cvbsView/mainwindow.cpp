#include "mainwindow.h"
#include "global_value.h"

#include <QDebug>

static struct sigaction siga;
static cameraWidgets *g_wid;

static void multi_handler(int sig, siginfo_t *siginfo, void *context) {
    // get pid of sender,
    pid_t sender_pid = siginfo->si_pid;
	cameraWidgets *wid = g_wid;

    if(sig == SIGINT) {
        printf("INT, from [%d]\n", (int)sender_pid);
        if (wid)
            emit(wid->m_topWid->m_btnreturn->clicked());
    } else if(sig == SIGQUIT) {
        printf("Quit, bye, from [%d]\n", (int)sender_pid);
        exit(0);
    } else if(sig == SIGTERM) {
        printf("TERM, from [%d]\n", (int)sender_pid);
        if (wid)
            emit(wid->m_topWid->m_btnreturn->clicked());
    }

    return;
}

int signal_setting() {
    // print pid
    printf("process [%d] started.\n", (int)getpid());

    // prepare sigaction
    siga.sa_sigaction = *multi_handler;
    siga.sa_flags |= SA_SIGINFO; // get detail info

    // change signal action,
    if (sigaction(SIGINT, &siga, NULL) != 0) {
        printf("error sigaction()");
        return errno;
    }
    if (sigaction(SIGQUIT, &siga, NULL) != 0) {
        printf("error sigaction()");
        return errno;
    }
    if (sigaction(SIGTERM, &siga, NULL) != 0) {
        printf("error sigaction()");
        return errno;
    }

    // use "ctrl + c" to send SIGINT, and "ctrl + \" to send SIGQUIT,
    return 0;
}


MainWindow::MainWindow(QWidget *parent) :baseWindow(parent),mediaHasUpdate(false)

{
    mainwid=this;
    initLayout();

    connect(m_wid->m_topWid->m_btnmini,SIGNAL(clicked(bool)),this,SLOT(showMinimized()));
    connect(m_wid->m_topWid->m_btnexit,SIGNAL(clicked(bool)),this,SLOT(slot_appQuit()));
    connect(m_wid->m_topWid->m_btnreturn,SIGNAL(clicked(bool)),this,SLOT(slot_returnanimation()));

    //m_wid->openCamera();
    g_wid = m_wid;
    //signal_setting();
}

MainWindow::~MainWindow()
{
    qDebug() << "~MainWindow";
}

void MainWindow::initLayout(){
   QVBoxLayout *mainLayout = new QVBoxLayout;
    m_wid = new cameraWidgets(this);
    mainLayout->addWidget(m_wid);
    setLayout(mainLayout);
    mainLayout->setContentsMargins(0,0,0,0);
}
void MainWindow::slot_appQuit()
{
    this->close();
}
void MainWindow::slot_returnanimation()
{
    qDebug() << "closeCameraApp";
    printf("slot_returnanimation\n");
    m_wid->closeCamera();
    this->close();
    qDebug() << "closed";
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
    qDebug()<<"key value is:"<<event->key();
    switch(event->key())
    {
    // update musicPlayer and videoPlayer's volume by Key
    case Qt::Key_VolumeDown:
        QWidget::keyPressEvent(event);
        break;
    case Qt::Key_VolumeUp:
        QWidget::keyPressEvent(event);
        break;
    case Qt::Key_PowerOff:   // when key_power enter
        QTimer::singleShot(100, this, SLOT(slot_standby()));
        break;
    default:
        break;
    }
}
void MainWindow::slot_standby()
{
}
