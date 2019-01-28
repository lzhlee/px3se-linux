#ifndef RKCIFCAMERA_H
#define RKCIFCAMERA_H

#include "base/basewidget.h"
#include <QApplication>
#include <QDebug>
#include <QThread>
#include <QWidget>

#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
#include <stdlib.h>

extern void dump_time(char *label);

class RkCifCamera : public QThread
{
	Q_OBJECT
public:
	RkCifCamera(QObject *parent = 0);
	~RkCifCamera();

	void setChannel(int channel);
	int start();
	int stop();

protected:
	void run();
private:
	bool mRunning;
	int mChannel;
};
#endif
