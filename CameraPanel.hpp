#pragma once
#include <QWidget>
#include <QMap>
#include <QUrl>
class QLineEdit;class QLabel;class QSpinBox;class QDoubleSpinBox;
#ifdef AEROCORE_ENABLE_MULTIMEDIA
class QMediaPlayer;class QVideoWidget;
#endif
// Selects an operator-provided file or network video for each SYS. The scene
// never substitutes model-rendered pixels for a real camera frame.
class CameraPanel final:public QWidget {
    Q_OBJECT
public:
    explicit CameraPanel(QWidget*parent=nullptr);
    void setSystem(int);
    void setReplay(double seconds,bool playing,double speed,bool seek=false);
private:
    QMap<int,QUrl> sources_;int system_=1;
    QLineEdit*address_;QLabel*status_;QDoubleSpinBox*offset_;
#ifdef AEROCORE_ENABLE_MULTIMEDIA
    QMediaPlayer*player_;QVideoWidget*video_;bool hasFrames_=false;
#endif
    void openSource(const QUrl&);
};
