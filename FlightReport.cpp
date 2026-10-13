#include "FlightReport.hpp"
#include "EnergyEstimate.hpp"
#include <QTextDocument>
#include <QPdfWriter>
#include <QPageSize>
#include <QPageLayout>
#include <QSaveFile>
#include <QFont>
#include <algorithm>
namespace {
QString val(const QJsonObject &o,const QString &key,int decimals=2){return o.value(key).isDouble()?QString::number(o.value(key).toDouble(),'f',decimals):QString("—");}
QString row(const QString &a,const QString &b,const QString &c){return "<tr><td>"+a.toHtmlEscaped()+"</td><td>"+b.toHtmlEscaped()+"</td><td>"+c.toHtmlEscaped()+"</td></tr>";}
}
void attachIhaAnalysis(LabResult &r,const QJsonObject &hardware){
    if(hardware.isEmpty())return;r.metrics["ihaHardware"]=hardware;
    const auto e=hardware.value("reportEstimates").toObject();const auto battery=hardware.value("energyEstimate").toObject();
    const double current=battery.value("hoverCurrentA").toDouble(e.value("hoverCurrentA").toDouble());
    const double volts=battery.value("voltageV").toDouble(e.value("loadedVoltageV").toDouble());
    aero::FlightEnergyEstimate estimate{current,volts,e.value("capacityMah").toDouble(),battery.value("usableFraction").toDouble(.8)};
    if(!estimate.configValid())return;
    for(const auto &sample:r.samples)estimate.add(sample.t,sample.roll,sample.pitch,sample.measured);
    if(estimate.coveredS<=0)return;
    double measurement=r.metrics.value("measurementS").toDouble();double coverage=measurement>0?std::clamp(estimate.coveredS/measurement,0.,1.):0;
    r.metrics["estimatedEnergy"]=QJsonObject{{"source","Post-test hover-current + tilt approximation; NOT measured battery or motor current"},{"mah",estimate.mah},{"wh",estimate.wh},{"coveredS",estimate.coveredS},{"coverage",coverage},{"usableRemainingMah",estimate.usableRemainingMah()},{"hoverCurrentA",current},{"voltageV",volts},{"capacityMah",estimate.capacityMah},{"usableFraction",estimate.usableFraction},{"scope","Measurement segment only; takeoff/landing excluded; no sag/heating/acceleration model"}};
}
QString flightReportHtml(const QVector<LabResult>&results){
    QString html="<html><head><meta charset='utf-8'><style>body{font-family:Arial;color:#17365D;font-size:10pt}h1{font-size:20pt}h2{font-size:14pt}table{border-collapse:collapse;width:100%}td,th{border:1px solid #cbd5e1;padding:6px}th{background:#eff6ff}p{line-height:1.35}</style></head><body><h1>AeroCore • Deney sonuçları</h1><p>Görev tamamlanma durumu, Gazebo/PX4 uçuş ölçümleri ve tahmini enerji hesabı ayrı gösterilir. Simülasyon fiziksel uçuş doğrulaması değildir.</p>";
    for(const auto &r:results){
        const auto hardware=r.metrics.value("ihaHardware").toObject();const auto live=hardware.value("reportEstimates").toObject();const auto reference=hardware.value("referenceReport").toObject().isEmpty()?live:hardware.value("referenceReport").toObject();
        html+="<h2>"+r.spec.versionTag.toHtmlEscaped()+" — "+r.spec.name.toHtmlEscaped()+"</h2><p><b>Uçuş sonucu: "+r.status.toHtmlEscaped()+"</b><br>"+r.reason.toHtmlEscaped()+"<br>Başlangıç: "+r.started.toHtmlEscaped()+"<br>Run: "+r.id.toHtmlEscaped()+"<br>Model: "+r.spec.vehicle.toHtmlEscaped()+"</p>";
        html+="<table><tr><th>Özellik</th><th>Deney girdisi</th><th>AeroCore uçuş ölçümü</th></tr>";
        html+=row("Kalkış ağırlığı (g)",val(live,"takeoffMassG"),"Model girdisi; bağımsız ölçüm değil");html+=row("Aks açıklığı (mm)",val(live,"wheelbaseMm"),"Model girdisi");html+=row("Maksimum toplam itki (g)",val(live,"maxTotalThrustG"),"Model girdisi; üretici testi değil");
        html+=row("Ölçüm süresi (s)","Test hedefi: "+QString::number(r.spec.duration),val(r.metrics,"measurementS"));html+=row("Toplam deney süresi (s)","Kalkış + ölçüm + iniş",val(r.metrics,"durationS"));
        for(const auto &pair:{qMakePair(QString("Ortalama konum hatası (m)"),QString("meanErrorM")),qMakePair(QString("Maksimum konum hatası (m)"),QString("maxErrorM")),qMakePair(QString("Maksimum irtifa hatası (m)"),QString("maxAltErrorM")),qMakePair(QString("Maksimum yatış (derece)"),QString("maxTiltDeg"))})html+=row(pair.first,"—",val(r.metrics,pair.second));
        html+=row("Telemetri kapsamı (%)","—",r.metrics.contains("coverage")?QString::number(r.metrics["coverage"].toDouble()*100,'f',1):"—");html+="</table>";
        const auto windResult=r.metrics.value("windFormation").toObject();
        if(!windResult.isEmpty()){html+="<h3>Rüzgar Hamlesinde Dinamik Sürü</h3><table><tr><th>Ölçüt</th><th>Sonuç</th><th>Açıklama</th></tr>";html+=row("Koşu",windResult["trial"].toString(),"Sabit referans / adaptif");html+=row("Lider değişimi",windResult["adaptationConfirmed"].toBool()?"Doğrulandı":"Doğrulanmadı","%40 tahmini akım artışı tetikleme koşulu");html+=row("Maksimum formasyon hatası (m)",val(windResult,"maxFormationErrorM"),"Telemetri konumlarından");html+=row("Tahmini sürü tüketimi (mAh)",val(windResult,"fleetUsedMah"),"Akım/yatış yaklaşımı");html+=row("Tahmini tüketim farkı (%)",windResult["energySavingValid"].toBool()?val(windResult,"energySavingPct"):"Hesaplanamaz / doğrulanmadı","Geçerli sabit/adaptif koşul çifti gerekli; %18 sabit değil");html+="</table><p>Ortak dünya rüzgarı kullanılır; lider-local türbülans, rotor etkileşimi veya aerodinamik koruma modellenmez. Bu, formasyon kontrol deneyi ve tahmini tüketim karşılaştırmasıdır.</p>";}
        const auto relay=r.metrics.value("relayMission").toObject();
        if(!relay.isEmpty()){html+="<h3>Görev İhale Deneyi görev devri</h3><p>Kesintisizlik burada taze telemetriyle devriye bölgesinde araç bulunmasıdır; kamera görüşü/termal gözlem doğrulanmaz.</p><table><tr><th>Ölçüt</th><th>Sonuç</th><th>Kaynak</th></tr>";html+=row("Konumsal görev mevcudiyeti (%)",val(relay,"coveragePct"),"Görev kaydından hesaplandı");html+=row("Boşluk / en uzun boşluk (s)",val(relay,"gapS")+" / "+val(relay,"longestGapS"),"Telemetri ve alan koşulu");html+=row("Devir sayısı",val(relay,"handovers",0),"Devralma noktasında doğrulama");html+=row("Tahmini tüketim (mAh)",val(relay,"usedMah"),"Canlı askı-akımı/yatış yaklaşımı");html+=row("İdeal şarj (mAh)",val(relay,"chargedMah"),"Yerde sabit akım varsayımı");html+=row("Son tahmini SOC (%)",val(relay,"finalSocPct"),"PX4 batarya telemetrisi değildir");html+="</table><p>Şarj ve tüketim uçuş fiziğine bağlı değildir. Voltaj sabittir; motor akımı ölçülmez. 7/24 süreklilik bu sınırlı testle kanıtlanamaz.</p>";}
        const auto energy=r.metrics.value("estimatedEnergy").toObject();html+="<h3>Tahmini batarya hesabı</h3>";
        if(energy.isEmpty()&&relay.isEmpty()&&windResult.isEmpty())html+="<p>Hesap için akım/gerilim girdisi veya yeterli uçuş örneği bulunamadı.</p>";
        else if(!energy.isEmpty()){html+="<p><b>Elektriksel ölçüm değildir.</b> Girilen askı akımı ve uçuş açılarından, sabit irtifa varsayımıyla hesaplanır. Kalkış/iniş tüketimini, gerilim düşmesini, ısınmayı ve ivmelenmeyi içermez.</p><table><tr><th>Değer</th><th>Tahmin</th><th>Kapsam</th></tr>";html+=row("Tüketim (mAh)",val(energy,"mah"),"Taze örneklerin kapsadığı ölçüm bölümü");html+=row("Enerji (Wh)",val(energy,"wh"),"Sabit gerilim varsayımı");html+=row("Hesaplanan süre (s)",val(energy,"coveredS"),"Telemetri boşlukları eklenmez");html+=row("Kullanılabilir kapasite bakiyesi (mAh)",val(energy,"usableRemainingMah"),"Yalnız kapsanan bölüm sonrası; tüm uçuş bakiyesi değil");html+="</table><p>Batarya tüm göreve yeter sonucu bu hesapla doğrulanamaz. Uçuş PASS/FAIL sonucu elektriksel yeterlilik onayı değildir.</p>";}
        html+="<p><b>Model varsayımları:</b> yaklaşık X geometri ve atalet; k·omega² itki modeli; ideal konum sensörü. Aynı senaryo/ortam/ayarlarla yapılan sonuçlar karşılaştırılmalıdır.</p><p>Fizik profili SHA256: "+r.profileHash.toHtmlEscaped()+"<br>PX4 SHA256: "+r.firmware.toHtmlEscaped()+"</p>";
    }
    return html+"</body></html>";
}
bool saveFlightReportPdf(const QString &path,const QVector<LabResult>&results,QString &error){
    if(results.isEmpty()){error="PDF için en az bir uçuş sonucu gerekli.";return false;}
    QSaveFile file(path);if(!file.open(QIODevice::WriteOnly)){error=file.errorString();return false;}
    {QPdfWriter writer(&file);writer.setTitle("AeroCore IHA Maker Uçuş Analizi");writer.setPageSize(QPageSize(QPageSize::A4));writer.setResolution(96);writer.setPageMargins(QMarginsF(15,15,15,15),QPageLayout::Millimeter);QTextDocument document;document.setDefaultFont(QFont("Arial",10));document.setHtml(flightReportHtml(results));document.print(&writer);}
    if(file.size()<100){error="PDF çıktısı oluşturulamadı.";file.cancelWriting();return false;}
    if(!file.commit()){error=file.errorString();return false;}return true;
}
