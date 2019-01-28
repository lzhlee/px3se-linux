#include <fcntl.h>
#include<sys/types.h>
#include<sys/stat.h>
#include "rkCifCamera.h"

extern int rk_camera_init(void);
extern int rk_camera_deinit(void);
extern int rk_camera_start(void);
extern int rk_camera_stop(void);
extern int rk_camera_change(int, int, int);
/*channel start from 0*/
extern void rk_camera_set_ad_channel(int channel);
extern int rk_camera_get_ad_channel(void);

RkCifCamera::RkCifCamera(QObject *parent)
	: QThread(parent),mChannel(0)
{
    rk_camera_init();
}

RkCifCamera::~RkCifCamera()
{
	qDebug() << "~RkCifCamera\n";
    rk_camera_deinit();
	QThread::wait();
}

void RkCifCamera::setChannel(int channel)
{
	mChannel = channel;
}

int RkCifCamera::stop()
{
	qDebug() << "RkCifCamera::stop()\n";
	mRunning = false;
	return rk_camera_stop();
}

int RkCifCamera::start()
{
	qDebug() << "RkCifCamera start\n";
	mRunning = true;
	QThread::start();
	rk_camera_set_ad_channel(mChannel);
	qDebug() << "rk camera set channel " << mChannel;
    return rk_camera_start();
}

void RkCifCamera::run()
{
	int res;
	int pipe_fd;
	char buffer[PIPE_BUF + 1];  
	const char *fifo_name = "/tmp/cvbsView_fifo";
	int width;
	int height;
	fd_set rset;
	struct timeval timeout;

	qDebug() <<  "thread run read fifo " << fifo_name;
    memset(buffer, '\0', sizeof(buffer));
	if(access(fifo_name, F_OK) == -1)
	{
		res = mkfifo(fifo_name, 0777);
        if(res != 0)
		{
			qDebug() <<  "Could not create fifo " << fifo_name;
			exit(EXIT_FAILURE);
		}
	}
 
	qDebug() <<  "open fifo " << fifo_name;
    pipe_fd = open(fifo_name, O_RDWR);  
	if(pipe_fd < 0) {
		qDebug() <<  "Could open fifo " << fifo_name;
		return;
	}

	qDebug() <<  "open fifo fd " << pipe_fd;
	while (mRunning) {
		FD_SET(pipe_fd, &rset);
		timeout.tv_sec = 0;
		timeout.tv_usec = 100*1000;
		res = select(pipe_fd + 1, &rset, NULL, NULL, &timeout);
		if (res == 0) {
			//qDebug() << "select timeout";
			continue;
		}

		if (FD_ISSET(pipe_fd, &rset)) { 
			qDebug() <<  "start read fifo " << fifo_name;
			res = read(pipe_fd, buffer, PIPE_BUF);  
			if (res > 0) {
				buffer[res] = 0;
				qDebug() << "buffer = " <<  buffer;
				sscanf(buffer, "RESOLUTION=%dx%d",&width, &height);
				if (width > 0 && height > 0)
					rk_camera_change(width, height, 30);
			}
		}
	}
	close(pipe_fd);
}
