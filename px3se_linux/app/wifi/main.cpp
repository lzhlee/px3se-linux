#include <QApplication>
#include <QQmlApplicationEngine>
#include <QtQml>
#include "model.h"
#include "apgui.h"
int main(int argc, char *argv[])
{
    qputenv("QT_IM_MODULE", QByteArray("qtvirtualkeyboard"));
    QApplication app(argc, argv);

    qmlRegisterType<ApGui>("ApGui", 1, 0, "ApGui");

    Model mc;
    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("modcpp", &mc);    
    engine.load(QUrl(QStringLiteral("qrc:/main.qml")));
    return app.exec();
}
