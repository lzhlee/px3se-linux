#include "model.h"

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


struct netWork   // Wifi信息结构体
{
    QString SSID;
    QString BSSID;
    QString frequence;
    QString signal;
    QString flags;
};

static const char WPA_SUPPLICANT_CONF_DIR[]           = "/tmp/wpa_supplicant.conf";
static const char HOSTAPD_CONF_DIR[]	=	"/tmp/hostapd.conf";
int is_supplicant_running();


int creat_supplicant_file()
{
    FILE* fp;
    fp = fopen(WPA_SUPPLICANT_CONF_DIR, "wt+");

    if (fp != 0) {
        fputs("ctrl_interface=/var/run/wpa_supplicant\n", fp);
        fputs("ap_scan=1\n", fp);
        fclose(fp);
        return 0;
    }
    return -1;
}


const bool console_run(const char *cmdline) {
    DEBUG_INFO("cmdline = %s\n",cmdline);

#if 0
    FILE *fp = popen(cmdline, "r");
    if (!fp) {
        DEBUG_ERR("Running cmdline failed: %s\n", cmdline);
        return false;
    }

    pclose(fp);
#else
    int ret;
    ret = system(cmdline);
    if(ret < 0){
        DEBUG_ERR("Running cmdline failed: %s\n", cmdline);
    }
#endif

    return true;
}


int wifi_start_supplicant()
{
    qDebug()<<"wifi_start_supplicant++";
    if (is_supplicant_running()) {
        return 0;
    }
    qDebug()<<"wifi_start_supplicant--";
    console_run("/usr/sbin/wpa_supplicant -Dnl80211 -iwlan0 -c /tmp/wpa_supplicant.conf &");

    return 0;
}

int get_pid(char *Name) {
    int len;
    char name[20] = {0};
    len = strlen(Name);
    strncpy(name,Name,len);
    name[len] ='\0';
    char cmdresult[256] = {0};
    char cmd[20] = {0};
    FILE *pFile = NULL;
    int  pid = 0;

    sprintf(cmd, "pidof %s", name);
    pFile = popen(cmd, "r");
    if (pFile != NULL)  {
        while (fgets(cmdresult, sizeof(cmdresult), pFile)) {
            pid = atoi(cmdresult);
            DEBUG_INFO("--- %s pid = %d ---\n",name,pid);
            break;
        }
    }
    pclose(pFile);
    return pid;
}

int wifi_stop_supplicant()
{
    int pid;
    char *cmd = NULL;
    qDebug()<<"wifi_stop_supplicant++";
    /* Check whether supplicant already stopped */
    if (!is_supplicant_running()) {
          return 0;
    }
    qDebug()<<"wifi_stop_supplicant--";
    pid = get_pid("wpa_supplicant");
    asprintf(&cmd, "kill %d", pid);
    console_run(cmd);
    free(cmd);

    return 0;
}

int is_udhcp_running()
{
    int ret;

    ret = get_pid("udhcpc");

    return ret;
}

int stop_udhcp()
{
    int pid;
    char *cmd = NULL;
    qDebug()<<"stop_udhcp++";
    /* Check whether already stopped */
    if (!is_udhcp_running()) {
          return 0;
    }
    qDebug()<<"stop_udhcp--";
    pid = get_pid("udhcpc");
    asprintf(&cmd, "kill %d", pid);
    console_run(cmd);
    free(cmd);

    return 0;
}

Model::Model(QObject *parent) :
    QAbstractListModel(parent)
{
    CreateDefaultModel();
}

void Model::CreateDefaultModel(void)
{
//    ModelItem itm;
//    itm.sSSID = "OpenWrt";
//    itm.nSignal = 34;
//    itm.sBSSID = "bssid";
//    itm.sPSK = "313568493";
//    itm.sNote="flags";
//    v.push_back(itm);
    if (access(WPA_SUPPLICANT_CONF_DIR, F_OK) < 0) {
            creat_supplicant_file();
    }
    wifi_start_supplicant();
    wpa = wpaManager::getInstance(this);
    connect(wpa, SIGNAL(insertNetwork(QString,QString,QString,QString)),SLOT(onNetWorkInsert(QString,QString,QString,QString)));
    connect(wpa, SIGNAL(statusChange(QString)),SLOT(onNetWorkStatusChange(QString)));
}

int Model::rowCount(const QModelIndex &parent) const
{
    return v.size();
}

QVariant Model::data(const QModelIndex &index, int role) const
{
    int i = index.row();
    if (i < 0 || i >= v.size()) {
        return QVariant();
    }

    if (role == ssid) {
        return v[i].sSSID;
    }
    if (role==signal) {
        return v[i].nSignal;
    }
    if (role==bssid) {
        return v[i].sBSSID;
    }
    if (role==psk) {
        return v[i].sPSK;
    }
    if (role==note) {
        return v[i].sNote;
    }
    if (role==Qt::DisplayRole) {
        return v[i].sSSID + " ("+ QString::number(v[i].nSignal) +")";
    }
}

QHash<int, QByteArray> Model::roleNames() const
{
    QHash<int, QByteArray> r =QAbstractListModel::roleNames();
    r[ssid]="ssid";
    r[signal]="signal";
    r[bssid]="bssid";
    r[psk]="psk";
    r[note]="note";
    return r;
}

void Model::setItemData2(int index, QVariant value, QString role)
{
    if (index >=0 && index <= v.size()) {
        QVector<int> roles;

        if(role=="ssid") {
            v[index].sSSID = value.toString();
            roles.push_back(ssid);
        }
        if(role=="signal") {
            v[index].nSignal = value.toInt();
            roles.push_back(signal);
        }
        if(role=="bssid") {
            v[index].sBSSID = value.toString();
            roles.push_back(bssid);
        }
        if(role=="psk") {
            v[index].sPSK = value.toString();
            roles.push_back(psk);
            current_psk=v[index].sPSK ;
        }
        if(role=="note") {
            v[index].sNote = value.toString();
            roles.push_back(note);
        }

        roles.push_back(Qt::DisplayRole);
        emit dataChanged(createIndex(0,0), createIndex(v.size()-1,0), roles);
    }
}


void Model::clear(){

    beginRemoveRows(QModelIndex(), 0, v.size());
        //清空动态数组
        v.clear();
    endRemoveRows();

    qDebug()<<"clear"<<v.size();

}


void Model::scan(){

    wpa->openCtrlConnection("wlan0");
    wpa->scan();
    wpa->updateScanResult();

    clear();
}
void Model::onNetWorkInsert(QString ssid,QString bssid, QString signal, QString flags){
    qDebug()<<"onNetWorkInsert:"<<ssid;
    beginInsertRows(QModelIndex(), rowCount(), rowCount());
    ModelItem itm;
    itm.sSSID = ssid;
    itm.nSignal = 34;
    itm.sBSSID = bssid;
    itm.sPSK = "";
    itm.sNote=flags;
    v.push_back(itm);
    endInsertRows();
}


void Model::onNetWorkSelect(QString ssid,QString bssid,QString psk, QString flags){
    qDebug()<<"onNetWorkSelect:"<<ssid;
    current_ssid=ssid;
    current_psk=psk;
}

void Model::connectWifi(){

    qDebug()<<"connectWifi";

    if(wpa){
        char reply[10], cmd[256];
        size_t reply_len;

        memset(reply, 0, sizeof(reply));
                reply_len = sizeof(reply) - 1;
        int ret = wpa->ctrlRequest("ADD_NETWORK", reply, &reply_len);
        if(ret <0){
            qDebug()<<"add_network faild";
            return;
        }
        if (reply[0] == 'F') {
            qDebug()<<"Failed to add network to wpa_supplicant\n";
            return;
        }
        current_id = atoi(reply);

        wpa->setNetworkParam(current_id, "ssid", current_ssid.toLocal8Bit().constData(),
                                     true);

        char set_cmd[256] = { 0 };
        snprintf(set_cmd, sizeof(set_cmd), "SET_NETWORK %d psk \"%s\"", current_id,current_psk.toLocal8Bit().constData());
        ret =wpa->ctrlRequest(set_cmd, reply, &reply_len);
        qDebug()<<set_cmd <<"ret:"<<ret;
    }

    size_t reply_len;
    char reply[10]= { 0 };
    char cmd[256] = { 0 };

    snprintf(cmd, sizeof(cmd), "ENABLE_NETWORK %d", current_id);

    int ret =wpa->ctrlRequest(cmd, reply, &reply_len);
    qDebug()<<cmd <<"ret:"<<ret <<"reply:"<<reply;
}

void Model::disconnectWifi(){
    qDebug()<<"disconnectWifi";
    size_t reply_len;
    char reply[10]= { 0 };
    char cmd[256] = { 0 };

    snprintf(cmd, sizeof(cmd), "DISABLE_NETWORK %d", current_id);

    int ret =wpa->ctrlRequest(cmd, reply, &reply_len);
    qDebug()<<cmd <<"ret:"<<ret <<"reply:"<<reply;

    snprintf(cmd, sizeof(cmd), "REMOVE_NETWORK %d", current_id);
    ret =wpa->ctrlRequest(cmd, reply, &reply_len);
    qDebug()<<cmd <<"ret:"<<ret <<"reply:"<<reply;
    current_id= 0;
}
void Model::onNetWorkStatusChange(QString status){
    if(current_status!=status){
        current_status=status;
        emit statusChange(status);
        if(current_status=="Completed"){
            console_run("udhcpc -i wlan0 &");
        }else if(current_status=="Inactive"){
            stop_udhcp();
        }
    }
}
