#include <iostream>
#include <time.h>
#include <fcntl.h>


using namespace std;

extern int rk_camera_start(void);
extern int rk_camera_stop(void);

int main(int argc, const char* argv[]) {
    rk_camera_start();
    getchar();
    printf("get char to rk_camera_stop\n");
    rk_camera_stop();
    printf("rk_camera_stop succeed\n");
    return 0;
}

