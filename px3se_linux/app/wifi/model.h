#ifndef MODEL_H
#define MODEL_H

#include <QAbstractListModel>
#include <QVector>

#include "wpamanager.h"

class Model : public QAbstractListModel
{
    Q_OBJECT

    struct ModelItem {
        QString sSSID;
        int nSignal;
        QString sBSSID;
        QString sPSK;
        QString sNote;
    };
    enum {ssid=Qt::UserRole+1, signal, bssid, psk, note};

public:
    explicit Model(QObject *parent = 0);

    int rowCount(const QModelIndex & parent = QModelIndex()) const;
    QVariant data(const QModelIndex & index, int role = Qt::DisplayRole) const;
    QHash<int, QByteArray> roleNames() const;

signals:
    void statusChange(QString status);

public slots:
    void setItemData2(int index, QVariant value, QString role);
    void onNetWorkInsert(QString ssid,QString bssid,QString signal, QString flags);
    void onNetWorkSelect(QString ssid,QString bssid,QString psk, QString flags);
    void scan();
    void connectWifi();
    void disconnectWifi();
    void onNetWorkStatusChange(QString status);

private:
    QVector<ModelItem> v;
    int current_id = -1;
    QString current_ssid;
    QString current_psk;
    QString current_status;
    wpaManager* wpa;

private:
    void CreateDefaultModel(void);
    void clear();
};

#endif // MODEL_H
