#include "LabChart.hpp"
#include <QPainter>
#include <QPainterPath>
#include <cmath>
#include <algorithm>
void LabChart::paintEvent(QPaintEvent*){
    QPainter p(this);p.setRenderHint(QPainter::Antialiasing);p.fillRect(rect(),Qt::white);QRectF left(55,45,width()*.46-70,height()-100),right(width()*.5+50,45,width()*.5-85,height()-100);
    p.setPen(QColor("#17365D"));p.drawText(55,23,"Planlanan / gerçekleşen rota • Doğu / Kuzey (m)");p.drawText(int(width()*.5+50),23,"Yatay hata (m) / ölçüm zamanı (sn)");
    if(results_.isEmpty()){p.drawText(rect(),Qt::AlignCenter,"Test tamamlandığında rota ve hata grafikleri burada görünür.");return;}
    double xmin=-1,xmax=1,ymin=-1,ymax=1,tmax=1,emax=1;
    for(auto&r:results_){for(auto s:r.samples){xmin=std::min(xmin,s.east);xmax=std::max(xmax,s.east);ymin=std::min(ymin,s.north);ymax=std::max(ymax,s.north);tmax=std::max(tmax,s.t);emax=std::max(emax,s.error);}for(auto q:r.spec.route){xmin=std::min(xmin,q.east);xmax=std::max(xmax,q.east);ymin=std::min(ymin,q.north);ymax=std::max(ymax,q.north);}}
    double margin=std::max(xmax-xmin,ymax-ymin)*.08;xmin-=margin;xmax+=margin;ymin-=margin;ymax+=margin;
    auto xy=[&](double x,double y){return QPointF(left.left()+(x-xmin)/(xmax-xmin)*left.width(),left.bottom()-(y-ymin)/(ymax-ymin)*left.height());};
    auto te=[&](double t,double e){return QPointF(right.left()+t/tmax*right.width(),right.bottom()-e/emax*right.height());};
    for(auto box:{left,right}){p.setPen(QPen(QColor("#E2E8F0"),1));for(int i=0;i<=4;++i){p.drawLine(QPointF(box.left(),box.top()+i*box.height()/4),QPointF(box.right(),box.top()+i*box.height()/4));p.drawLine(QPointF(box.left()+i*box.width()/4,box.top()),QPointF(box.left()+i*box.width()/4,box.bottom()));}p.setPen(QColor("#64748B"));p.drawRect(box);}
    p.drawText(left.left(),left.bottom()+18,QString::number(xmin,'f',1));p.drawText(left.right()-35,left.bottom()+18,QString::number(xmax,'f',1));p.drawText(left.left()-50,left.top()+5,QString::number(ymax,'f',1));p.drawText(left.left()-50,left.bottom(),QString::number(ymin,'f',1));p.drawText(right.left()-35,right.top()+5,QString::number(emax,'f',1));p.drawText(right.left(),right.bottom()+18,"0");p.drawText(right.right()-35,right.bottom()+18,QString::number(tmax,'f',1));
    const QVector<QColor> colors={QColor("#EC4899"),QColor("#EF4444"),QColor("#D97706"),QColor("#17365D")};int index=0;
    for(auto&r:results_){QPainterPath planned;planned.moveTo(xy(0,0));for(auto q:r.spec.route)planned.lineTo(xy(q.east,q.north));p.setPen(QPen(colors[r.metrics.contains("systemId")?std::clamp(r.metrics["systemId"].toInt()-1,0,2):index%4],1,Qt::DashLine));p.drawPath(planned);QPainterPath track,error;bool trackOpen=false,errorOpen=false;
        for(auto s:r.samples){if(!s.measured){trackOpen=errorOpen=false;continue;}auto a=xy(s.east,s.north),b=te(s.t,s.error);if(!trackOpen)track.moveTo(a);else track.lineTo(a);if(!errorOpen)error.moveTo(b);else error.lineTo(b);trackOpen=errorOpen=true;}
        p.setPen(QPen(colors[r.metrics.contains("systemId")?std::clamp(r.metrics["systemId"].toInt()-1,0,2):index%4],2));p.drawPath(track);p.drawPath(error);p.drawText(55+index*200,height()-15,r.spec.versionTag.left(16)+" / "+r.status);if(++index>=4)break;
    }
}
