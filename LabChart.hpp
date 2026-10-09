#pragma once
#include "LabTypes.hpp"
#include <QWidget>
class LabChart final:public QWidget {
public:
    explicit LabChart(QWidget* p=nullptr):QWidget(p){setMinimumSize(500,200);}
    void setResults(const QVector<LabResult>&r){results_=r;update();}
protected:
    void paintEvent(QPaintEvent*)override;
private:QVector<LabResult> results_;
};
