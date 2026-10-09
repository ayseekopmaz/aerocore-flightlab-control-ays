#include "sitlpanel.h"
#include <QApplication>
#include <QPalette>
int main(int argc,char *argv[]){
    QApplication app(argc,argv);
    QApplication::setStyle("Fusion");QPalette light;
    light.setColor(QPalette::Window,QColor("#EAF2FB"));
    for(auto role:{QPalette::Base,QPalette::Button})light.setColor(role,Qt::white);
    light.setColor(QPalette::AlternateBase,QColor("#EFF6FF"));
    for(auto role:{QPalette::WindowText,QPalette::Text,QPalette::ButtonText})light.setColor(role,QColor("#17365D"));
    light.setColor(QPalette::Highlight,QColor("#2563EB"));light.setColor(QPalette::HighlightedText,Qt::white);
    app.setPalette(light);
    QCoreApplication::setApplicationName("AeroCoreSimulator");
    QCoreApplication::setOrganizationName("AeroCore");
    SitlPanel panel;
    panel.setWindowTitle("AeroCore — Uçuş Simülasyonu 0.7.2 • 3B Drone Laboratuvarı");
    panel.showMaximized();
    return app.exec();
}
