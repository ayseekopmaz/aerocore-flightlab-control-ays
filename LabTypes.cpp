#include "LabTypes.hpp"
#include <QRegularExpression>
#include <cmath>
namespace {
QJsonArray points(const QVector<aero::Point>&p){QJsonArray a;for(auto q:p)a.append(QJsonArray{q.east,q.north});return a;}
bool finite(double n,double lo,double hi){return std::isfinite(n)&&n>=lo&&n<=hi;}
}
QString labKindName(const QString &k){static const QMap<QString,QString> names={{"hover","Havada sabit kalma"},{"route","Rota takibi"},{"takeoff","Kalkış ve iniş"},{"wind","Rüzgâr dayanımı"},{"payload","Yük ve ağırlık merkezi"},{"battery","Batarya ve dayanıklılık"},{"sensor","GPS / sensör arızası"},{"link","Bağlantı kesintisi"},{"controller","Kontrolcü karşılaştırması"},{"regression","Yazılım regresyonu"}};return names.value(k);}
LabSpec labPreset(const QString&k){LabSpec s;s.kind=k;s.name=labKindName(k);if(k=="route"||k=="controller"||k=="regression")s.route={{10,0},{10,10},{0,10},{0,0}};if(k=="wind")s.events={{10,8,3,90,"wind"}};if(k=="payload")s.payload=.2;if(k=="sensor")s.events={{10,5,4,1,"sensor"}};if(k=="link"){s.events={{10,5,100,0,"link"}};s.expectedMode="Return";}if(k=="battery"){s.duration=60;s.batteryDrain=45;s.batteryMin=10;s.expectedMode="Return";}return s;}
QJsonObject LabSpec::profile()const{return {{"payload",payload},{"cgX",cgX},{"cgY",cgY},{"cgZ",cgZ},{"gpsNoise",gpsNoise}};}
QJsonObject LabSpec::json()const{QJsonObject p;for(auto it=parameters.begin();it!=parameters.end();++it)p[it.key()]=it.value();QJsonArray e;for(auto x:events)e.append(QJsonObject{{"at",x.at},{"duration",x.duration},{"a",x.a},{"b",x.b},{"type",x.type}});return {{"schema",1},{"name",name},{"kind",kind},{"vehicle",vehicle},{"versionTag",versionTag},{"altitude",altitude},{"duration",duration},{"speed",speed},{"dwell",dwell},{"maxError",maxError},{"maxAltError",maxAltError},{"maxTilt",maxTilt},{"maxTime",maxTime},{"minCoverage",minCoverage},{"windSpeed",windSpeed},{"windDirection",windDirection},{"payload",payload},{"cgX",cgX},{"cgY",cgY},{"cgZ",cgZ},{"gpsNoise",gpsNoise},{"batteryDrain",batteryDrain},{"batteryMin",batteryMin},{"energyRequired",energyRequired},{"batteryModel",batteryModel},{"expectedMode",expectedMode},{"parameters",p},{"route",points(route)},{"events",e}};}
QString LabSpec::validate()const{
    if(name.trimmed().isEmpty()||name.size()>160||labKindName(kind).isEmpty()||vehicle!="x500")return "Geçersiz test adı / türü / araç. Bu sürüm x500 destekler.";
    const auto bounds=QVector<std::tuple<double,double,double>>{{altitude,2,30},{duration,5,1800},{speed,.2,10},{dwell,0,60},{maxError,.1,100},{maxAltError,.1,30},{maxTilt,1,85},{maxTime,30,3600},{minCoverage,.1,1},{windSpeed,0,15},{windDirection,0,360},{payload,0,2},{cgX,-.5,.5},{cgY,-.5,.5},{cgZ,-.5,.5},{gpsNoise,0,10},{batteryDrain,0,3600},{batteryMin,0,100}};
    for(auto [n,lo,hi]:bounds)if(!finite(n,lo,hi))return "Sayısal değer sınır dışında / sonlu değil.";
    if(maxTime<duration+20)return "Toplam zaman aşımı, ölçüm süresinden en az 20 sn uzun olmalı.";
    if(route.size()>100||events.size()>100||parameters.size()>30)return "Senaryo boyut sınırı aşıldı.";
    for(auto p:route)if(!finite(p.east,-500,500)||!finite(p.north,-500,500))return "Rota noktası başlangıçtan en fazla ±500 m olabilir.";
    if((kind=="route"||kind=="controller"||kind=="regression")&&route.isEmpty())return "Bu test için en az bir rota noktası gerekli.";
    double end=-1;for(auto e:events){if(!finite(e.at,0,duration-1)||!finite(e.duration,.1,duration-e.at)||!std::isfinite(e.a)||!std::isfinite(e.b))return "Olay zamanı / süresi geçersiz.";if(e.at<end)return "Olaylar zaman sırasıyla ve çakışmadan tanımlanmalı.";end=e.at+e.duration;
        if(e.type=="wind"){if(!finite(e.a,0,15)||!finite(e.b,0,360))return "Rüzgâr: 0–15 m/s ve 0–360° gerekir.";}
        else if(e.type=="link"){if(!finite(e.a,0,100)||!finite(e.b,0,2000))return "Bağlantı: kayıp %0–100, gecikme 0–2000 ms gerekir.";}
        else if(e.type=="sensor"){if(!finite(e.a,0,5)||std::floor(e.a)!=e.a||!(e.b==1||e.b==2))return "Sensör: gyro=0, accel=1, mag=2, baro=3, GPS=4, mesafe=5; off=1/stuck=2.";}
        else return "Bilinmeyen olay türü.";
    }
    QRegularExpression nameRx("^[A-Z][A-Z0-9_]{0,15}$");for(auto it=parameters.begin();it!=parameters.end();++it)if(!nameRx.match(it.key()).hasMatch()||!std::isfinite(it.value())||std::abs(it.value())>1e6)return "Parametre adı / değeri geçersiz.";
    if(!expectedMode.isEmpty()&&expectedMode!="Return"&&expectedMode!="Land"&&expectedMode!="Hold")return "Beklenen mod Return / Land / Hold olabilir.";
    return {};
}
std::optional<LabSpec> LabSpec::fromJson(const QJsonObject&o,QString&error){
    LabSpec s;if(o["schema"].toInt()!=1){error="Desteklenmeyen senaryo şeması.";return {};}
    const QStringList strings={"name","kind","vehicle","versionTag","batteryModel","expectedMode"};for(auto key:strings)if(!o[key].isString()){error="Eksik metin: "+key;return {};}
    s.name=o["name"].toString();s.kind=o["kind"].toString();s.vehicle=o["vehicle"].toString();s.versionTag=o["versionTag"].toString();s.batteryModel=o["batteryModel"].toString();s.expectedMode=o["expectedMode"].toString();
#define NUMBER(x) if(!o[#x].isDouble()){error="Eksik / geçersiz sayı: " #x;return {};}s.x=o[#x].toDouble();
    NUMBER(altitude) NUMBER(duration) NUMBER(speed) NUMBER(dwell) NUMBER(maxError) NUMBER(maxAltError) NUMBER(maxTilt) NUMBER(maxTime) NUMBER(minCoverage) NUMBER(windSpeed) NUMBER(windDirection) NUMBER(payload) NUMBER(cgX) NUMBER(cgY) NUMBER(cgZ) NUMBER(gpsNoise) NUMBER(batteryDrain) NUMBER(batteryMin)
#undef NUMBER
    if(!o["energyRequired"].isBool()||!o["route"].isArray()||!o["events"].isArray()||!o["parameters"].isObject()){error="Senaryo koleksiyonları eksik.";return {};}s.energyRequired=o["energyRequired"].toBool();
    for(auto v:o["route"].toArray()){auto a=v.toArray();if(a.size()!=2||!a[0].isDouble()||!a[1].isDouble()){error="Geçersiz rota.";return {};}s.route.append({a[0].toDouble(),a[1].toDouble()});}
    for(auto v:o["events"].toArray()){auto e=v.toObject();for(auto key:{"at","duration","a","b"})if(!e[key].isDouble()){error="Geçersiz olay.";return {};}if(!e["type"].isString()){error="Olay türü eksik.";return {};}s.events.append({e["at"].toDouble(),e["duration"].toDouble(),e["a"].toDouble(),e["b"].toDouble(),e["type"].toString()});}
    auto p=o["parameters"].toObject();for(auto it=p.begin();it!=p.end();++it){if(!it.value().isDouble()){error="Geçersiz parametre.";return {};}s.parameters[it.key()]=it.value().toDouble();}
    error=s.validate();if(!error.isEmpty())return {};return s;
}
QJsonObject LabResult::json()const{QJsonArray data;for(auto s:samples)data.append(QJsonArray{s.t,s.boot,s.east,s.north,s.alt,s.error,s.altError,s.roll,s.pitch,s.yaw,s.mode,s.battery,s.measured});return {{"resultSchema",1},{"id",id},{"status",status},{"reason",reason},{"started",started},{"firmware",firmware},{"profileHash",profileHash},{"spec",spec.json()},{"metrics",metrics},{"eventLog",eventLog},{"originLat",originLat},{"originLon",originLon},{"samples",data}};}
std::optional<LabResult> LabResult::fromJson(const QJsonObject&o,QString&error){if(o["resultSchema"].toInt()!=1){error="Desteklenmeyen sonuç şeması.";return {};}auto spec=LabSpec::fromJson(o["spec"].toObject(),error);if(!spec)return {};LabResult r;r.spec=*spec;r.id=o["id"].toString();r.status=o["status"].toString();r.reason=o["reason"].toString();r.started=o["started"].toString();r.firmware=o["firmware"].toString();r.profileHash=o["profileHash"].toString();r.metrics=o["metrics"].toObject();r.eventLog=o["eventLog"].toArray();r.originLat=o["originLat"].toDouble();r.originLon=o["originLon"].toDouble();if(!finite(r.originLat,-85,85)||!finite(r.originLon,-180,180)||o["samples"].toArray().size()>40000){error="Sonuç sınırı aşıldı.";return {};}
    for(auto v:o["samples"].toArray()){auto a=v.toArray();if(a.size()!=13||!a[12].isBool()){error="Geçersiz örnek.";return {};}for(int i=0;i<12;++i)if(!a[i].isDouble()||!std::isfinite(a[i].toDouble())){error="Geçersiz örnek sayısı.";return {};}r.samples.append({a[0].toDouble(),a[1].toDouble(),a[2].toDouble(),a[3].toDouble(),a[4].toDouble(),a[5].toDouble(),a[6].toDouble(),a[7].toDouble(),a[8].toDouble(),a[9].toDouble(),a[10].toInt(),a[11].toInt(),a[12].toBool()});}return r;
}
