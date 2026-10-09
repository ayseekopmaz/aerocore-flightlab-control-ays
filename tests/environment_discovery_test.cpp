#include "EnvironmentDiscovery.hpp"
#include <QCoreApplication>
#include <QString>

int main(int argc,char **argv){
    QCoreApplication app(argc,argv);
    auto utf16=[](const QString &s){
        QByteArray b("\xff\xfe",2);
        for(QChar c:s){auto n=c.unicode();b.append(char(n&255));b.append(char(n>>8));}
        return b;
    };
    const auto names=utf16("Ubuntu-22.04\r\nUbuntu-24.04\r\ndocker-desktop\r\n");
    if(EnvironmentDiscovery::chooseDistribution(names)!="Ubuntu-22.04")return 1;
    if(EnvironmentDiscovery::chooseDistribution(utf16("docker-desktop\r\nUbuntu-24.04\r\n"))!="Ubuntu-24.04")return 2;
    if(!EnvironmentDiscovery::chooseDistribution(utf16("docker-desktop\r\n")).isEmpty())return 3;
    if(EnvironmentDiscovery::chooseDistribution(QByteArray("Ubuntu-24.04\n"))!="Ubuntu-24.04")return 4;
    return 0;
}
