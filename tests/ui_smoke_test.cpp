#include "sitlpanel.h"
#include "LabPanel.hpp"
#include "FleetPanel.hpp"
#include "SessionRecording.hpp"
#include "Scene3DView.hpp"
#include "CameraPanel.hpp"
#include <QSpinBox>
#include <QApplication>
#include <QComboBox>
#include <QTableWidget>
#include <QPushButton>
#include <QTabWidget>
#include <QDoubleSpinBox>
#include <QTimer>
#include <QDir>
#include <iostream>
int main(int argc,char**argv){QApplication app(argc,argv);QApplication::setStyle("Fusion");QCoreApplication::setApplicationName("AeroCoreUiTest");SitlPanel panel;panel.resize(1320,1080);panel.show();auto *lab=panel.findChild<LabPanel*>();if(!lab)return 2;auto *kinds=lab->findChild<QComboBox*>("kindCombo");if(!kinds||kinds->count()!=10)return 3;
    kinds->setCurrentIndex(kinds->findData("route"));lab->findChild<QPushButton*>("presetButton")->click();auto *table=lab->findChild<QTableWidget*>("routeTable");if(!table||table->rowCount()!=4)return 4;QString error;auto spec=lab->spec(error);if(!error.isEmpty()||spec.kind!="route"||spec.route.size()!=4)return 5;
    auto*fleet=panel.findChild<FleetPanel*>();auto*replay=panel.findChild<ReplayPanel*>();if(!fleet||!replay||fleet->count()!=3)return 6;auto profiles=fleet->profiles(error);if(!error.isEmpty()||profiles.size()!=3)return 7;
    auto*tabs=panel.findChild<QTabWidget*>("workspaceTabs");if(tabs->count()!=5)return 8;
    if(QString::fromLatin1(Scene3DView::staticMetaObject.className())!="Scene3DView")return 11;
    if(!tabs->widget(1)->findChild<Scene3DView*>()||!tabs->widget(1)->findChild<CameraPanel*>())return 9;
    if(!replay->findChild<Scene3DView*>()||!replay->findChild<CameraPanel*>())return 10;
    tabs->setCurrentIndex(qEnvironmentVariableIsSet("AEROCORE_TAB")?qEnvironmentVariableIntValue("AEROCORE_TAB"):1);
    QTimer::singleShot(150,[&]{if(qEnvironmentVariableIsSet("AEROCORE_CAPTURE")){panel.grab().save(qEnvironmentVariable("AEROCORE_CAPTURE"));}std::cout<<"Designer UI, preset binding, form values and ten test kinds verified\n";app.quit();});return app.exec();}
