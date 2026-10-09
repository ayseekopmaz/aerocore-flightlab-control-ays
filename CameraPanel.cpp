#include "CameraPanel.hpp"
#include <QLineEdit>
#include <QLabel>
#include <QPushButton>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QUrl>
#include <algorithm>
#include <cmath>
#include <cstdlib>
#ifdef AEROCORE_ENABLE_MULTIMEDIA
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QVideoSink>
#include <QVideoFrame>
#endif
CameraPanel::CameraPanel(QWidget*p):QWidget(p){
    setObjectName("cameraPanel");auto*layout=new QVBoxLayout(this);auto*top=new QHBoxLayout;
    auto*file=new QPushButton("Video dosyası seç");auto*open=new QPushButton("RTSP / HTTP aç");address_=new QLineEdit;address_->setObjectName("cameraUrlEdit");address_->setPlaceholderText("rtsp://... veya http(s)://... (isteğe bağlı gerçek akış)");offset_=new QDoubleSpinBox;offset_->setObjectName("videoTimeOffset");offset_->setRange(-3600,3600);offset_->setDecimals(1);offset_->setSuffix(" sn");offset_->setToolTip("Kayıt videosu ile telemetri arasında zaman farkı");
    top->addWidget(file);top->addWidget(address_,1);top->addWidget(open);top->addWidget(new QLabel("Video ofseti"));top->addWidget(offset_);layout->addLayout(top);
    status_=new QLabel("SYS 1 • Kamera kaynağı bekleniyor. Gazebo kamera akışı otomatik üretilmez.");status_->setObjectName("cameraStatus");status_->setWordWrap(true);layout->addWidget(status_);
#ifdef AEROCORE_ENABLE_MULTIMEDIA
    player_=new QMediaPlayer(this);video_=new QVideoWidget(this);video_->setObjectName("cameraVideo");video_->setMinimumSize(320,230);video_->setAspectRatioMode(Qt::KeepAspectRatio);video_->setStyleSheet("background:#0F2743;border-radius:12px;");player_->setVideoOutput(video_);layout->addWidget(video_,1);
    connect(video_->videoSink(),&QVideoSink::videoFrameChanged,this,[this](const QVideoFrame&f){if(f.isValid()){hasFrames_=true;status_->setText(QString("SYS %1 • GERÇEK VİDEO ÇERÇEVESİ • kaynak operatör tarafından seçildi").arg(system_));}});
    connect(player_,&QMediaPlayer::errorOccurred,this,[this](QMediaPlayer::Error,const QString&e){hasFrames_=false;status_->setText(QString("SYS %1 • Video hatası: %2").arg(system_).arg(e));});
#else
    auto*placeholder=new QLabel("Bu Qt kitinde MultimediaWidgets bulunmuyor. 3B telemetri sahnesi ve deneyler çalışır.");placeholder->setAlignment(Qt::AlignCenter);placeholder->setMinimumHeight(230);placeholder->setStyleSheet("background:#E7F0FB;color:#17365D;border-radius:12px;");layout->addWidget(placeholder,1);file->setEnabled(false);open->setEnabled(false);
#endif
    connect(file,&QPushButton::clicked,this,[this]{auto p=QFileDialog::getOpenFileName(this,"Drone kamerası / demo video",{},"Video (*.mp4 *.mov *.mkv *.avi);;Tüm dosyalar (*)");if(!p.isEmpty())openSource(QUrl::fromLocalFile(p));});
    connect(open,&QPushButton::clicked,this,[this]{QUrl url(address_->text().trimmed());auto protocol=url.scheme().toLower();if(!url.isValid()||!url.host().size()||(protocol!="rtsp"&&protocol!="http"&&protocol!="https")){status_->setText("Geçerli bir RTSP/HTTP video adresi girin.");return;}openSource(url);});
}
void CameraPanel::setSystem(int n){if(n<1||n>3||n==system_)return;system_=n;
#ifdef AEROCORE_ENABLE_MULTIMEDIA
    player_->stop();player_->setSource({});hasFrames_=false;
    if(sources_.contains(system_)){player_->setSource(sources_[system_]);player_->play();status_->setText(QString("SYS %1 • Kaynak yükleniyor…").arg(system_));}else status_->setText(QString("SYS %1 • Kamera kaynağı bekleniyor").arg(system_));
#else
    status_->setText(QString("SYS %1 • Qt MultimediaWidgets bulunmuyor").arg(system_));
#endif
    address_->clear();
}
void CameraPanel::openSource(const QUrl&u){sources_[system_]=u;
#ifdef AEROCORE_ENABLE_MULTIMEDIA
    player_->stop();player_->setSource(u);hasFrames_=false;status_->setText(QString("SYS %1 • Video yükleniyor…").arg(system_));player_->play();
#else
    Q_UNUSED(u)
#endif
}
void CameraPanel::setReplay(double t,bool playing,double speed,bool seek){
#ifdef AEROCORE_ENABLE_MULTIMEDIA
    if(!sources_.contains(system_)||!sources_[system_].isLocalFile())return;
    const qint64 ms=qint64(std::max(0.,t+offset_->value())*1000);
    if(player_->isSeekable()&&(seek||std::llabs(player_->position()-ms)>400))player_->setPosition(ms);
    player_->setPlaybackRate(speed);if(playing)player_->play();else player_->pause();
#else
    Q_UNUSED(t)Q_UNUSED(playing)Q_UNUSED(speed)Q_UNUSED(seek)
#endif
}
