#pragma once
#include "LabTypes.hpp"
QString flightReportHtml(const QVector<LabResult>&);
bool saveFlightReportPdf(const QString&,const QVector<LabResult>&,QString&);
void attachIhaAnalysis(LabResult&,const QJsonObject&);
