#ifndef APGUI_H
#define APGUI_H

#include <QObject>

class ApGui : public QObject
{
    Q_OBJECT

    Q_PROPERTY(QString ap_name READ ap_name WRITE set_ap_name NOTIFY sig_ap_name_changed)

    Q_PROPERTY(QString ap_password READ ap_password WRITE set_ap_password  NOTIFY sig_ap_password_changed)

    Q_PROPERTY(bool ap_switch READ ap_switch WRITE set_ap_switch NOTIFY sig_ap_switch_changed)

public:
    explicit ApGui(QObject *parent = 0);

    QString ap_name();
    void set_ap_name(QString name);
    QString ap_password();
    void set_ap_password(QString password);
    bool ap_switch();
    void set_ap_switch(bool status);
signals:
    void sig_ap_name_changed();
    void sig_ap_password_changed();
    void sig_ap_switch_changed();
    void sig_ap_result(int result);
public slots:
    void slot_ap_switch(bool newStatus);
    void slot_exit();
private:
    QString _ap_name;
    QString _ap_password;
    bool _ap_switch;
};

#endif // APGUI_H
