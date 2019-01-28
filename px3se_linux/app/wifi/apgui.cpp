#include "apgui.h"
#include <QDebug>

#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <errno.h>

#include <stdlib.h>
#include <fcntl.h>
#include <dirent.h>
//#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
//#include <poll.h>
#include <stdbool.h>

#include <QThread>
#include <QDebug>

#include "wpamanager.h"

#define DBG false

#if DBG
#define DEBUG_INFO(M, ...) qDebug("DEBUG %d: " M, __LINE__, ##__VA_ARGS__)
#else
#define DEBUG_INFO(M, ...) do {} while (0)
#endif

#define DEBUG_ERR(M, ...) qDebug("DEBUG %d: " M, __LINE__, ##__VA_ARGS__)



static const char WPA_SUPPLICANT_CONF_DIR[]           = "/tmp/wpa_supplicant.conf";
static const char HOSTAPD_CONF_DIR[]	=	"/tmp/hostapd.conf";
int is_supplicant_running();


const bool console_run(const char *cmdline);

int get_pid(char *Name);


int creat_hostapd_file(const char* name, const char* password) {
    FILE* fp;
    fp = fopen(HOSTAPD_CONF_DIR, "wt+");

    if (fp != 0) {
        fputs("interface=wlan0\n", fp);
        fputs("driver=nl80211\n", fp);
        fputs("ssid=", fp);
        fputs(name, fp);
        fputs("\n", fp);
        fputs("channel=6\n", fp);
        fputs("hw_mode=g\n", fp);
        fputs("ieee80211n=1\n", fp);
        fputs("ht_capab=[SHORT-GI-20]\n", fp);
        fputs("ignore_broadcast_ssid=0\n", fp);
        fputs("auth_algs=1\n", fp);
        fputs("wpa=3\n", fp);
        fputs("wpa_passphrase=", fp);
        fputs(password, fp);
        fputs("\n", fp);
        fputs("wpa_key_mgmt=WPA-PSK\n", fp);
        fputs("wpa_pairwise=TKIP\n", fp);
        fputs("rsn_pairwise=CCMP", fp);

        fclose(fp);
        return 0;
    }
    return -1;
}

int is_hostapd_running()
{
    int ret;

    ret = get_pid("hostapd");
    qDebug()<<"is_hostapd_running:"<<ret;

    return ret;
}

int wifi_start_hostapd()
{
    if (is_hostapd_running()) {
        return 0;
    }
    qDebug()<<"wifi_start_hostapd---";
    console_run("ifconfig wlan0 up");
    console_run("ifconfig wlan0 192.168.100.1 netmask 255.255.255.0");
    console_run("echo 1 > /proc/sys/net/ipv4/ip_forward");
    console_run("iptables --flush");
    console_run("iptables --table nat --flush");
    console_run("iptables --delete-chain");
    console_run("iptables --table nat --delete-chain");
    console_run("iptables --table nat --append POSTROUTING --out-interface eth0 -j MASQUERADE");
    console_run("iptables --append FORWARD --in-interface wlan0 -j ACCEPT");
    console_run("/usr/sbin/hostapd /tmp/hostapd.conf -B");

    return 0;
}

int is_supplicant_running()
{
    int ret;

    ret = get_pid("wpa_supplicant");

    return ret;
}


int wifi_stop_hostapd()
{
    int pid;
    char *cmd = NULL;

    if (!is_hostapd_running()) {
        return 0;
    }
    qDebug()<<"wifi_stop_hostapd--";
    pid = get_pid("hostapd");
    asprintf(&cmd, "kill %d", pid);
    console_run(cmd);
    free(cmd);

    console_run("echo 0 > /proc/sys/net/ipv4/ip_forward");
    console_run("ifconfig wlan0 down");
    return 0;
}

bool	lanOldState = false;
bool	lanNewState = false;

void lanStateChanhe(bool state){
    //need to check wifi state
    if(state){
        console_run("ifconfig eth0 up");
        console_run("udhcpc -i eth0");
    }else{
        console_run("ifconfig eth0 down");
    }
}

void slot_checkLanConnection()
{
    char cmdbuf[1024] = {0};
    char cmdresult[1024] = {0};

    sprintf(cmdbuf, "cat /sys/class/net/eth0/carrier");
    FILE *pp = popen(cmdbuf, "r");
    if (!pp) {
        DEBUG_ERR("Running cmdline failed:cat /sys/class/net/eth0/carrier\n");
        return;
    }
    fgets(cmdresult, sizeof(cmdresult), pp);
    pclose(pp);

    if(strstr(cmdresult, "1"))
    {
        lanNewState = true;
    }else if(strstr(cmdresult, "0")){
        lanNewState = false;
    }else{
        console_run("ifconfig eth0 up");
    }
    if(lanOldState != lanNewState){
        if(lanNewState){
            //LanConnected
            lanStateChanhe(lanNewState);
        }else{
            //LanDisconnected
            lanStateChanhe(lanNewState);
        }
        lanOldState = 	lanNewState;
    }

}

void getIPAdress()
{
    console_run("udhcpc -i wlan0 &");
}

ApGui::ApGui(QObject *parent) : QObject(parent)
{
    this->set_ap_name("px3se_ap");
    this->set_ap_password("1234567890");
    this->_ap_switch=is_hostapd_running();
}

QString ApGui::ap_name(){
    return _ap_name;
}

void ApGui::set_ap_name(QString name){
    if(_ap_name!=name){
        this->_ap_name=name;
        emit sig_ap_name_changed();
    }
}

QString ApGui::ap_password(){
    return _ap_password;
}

void ApGui::set_ap_password(QString password){
    if(_ap_password!=password){
        this->_ap_password=password;
        emit sig_ap_password_changed();
    }
}

bool ApGui::ap_switch(){    
    return _ap_switch;
}

void ApGui::set_ap_switch(bool status){
    if(_ap_switch!=status){
        this->_ap_switch=status;
        emit sig_ap_switch_changed();
    }
}
int wifi_stop_supplicant();
int wifi_start_supplicant();
void ApGui::slot_ap_switch(bool newStatus){

    qDebug()<<"slot_ap_switch:"<<newStatus<<"ap name:"<<this->ap_name()<<"ap password:"<<this->ap_password();

    if(newStatus){
        wifi_stop_supplicant();
        creat_hostapd_file(this->ap_name().toLatin1().data(), this->ap_password().toLatin1().data());
        sleep(1);
        wifi_start_hostapd();

    }else{
        wifi_stop_hostapd();
        sleep(1);
        wifi_start_supplicant();
    }

    emit sig_ap_result(0);

}

void ApGui::slot_exit(){

}
