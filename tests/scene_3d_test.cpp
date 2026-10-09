#include "Scene3DView.hpp"
#include "MavlinkReadOnly.hpp"
#include <QApplication>
#include <QImage>
#include <cmath>
#include <memory>

// A UI regression test for all three vehicles: it checks the actual rendered
// output, rather than only that the telemetry objects exist in memory.
int main(int argc,char **argv){
    QApplication app(argc,argv);
    auto state=std::make_shared<FlightState>();
    const double now=sitl::monoSeconds();
    for(int sys=1;sys<=3;++sys){
        auto &v=state->vehicles[sys];
        v.lat=47.397742+sys*.000075;
        v.lon=8.545594+(sys-2)*.000075;
        v.alt=12+sys*5;
        v.yaw=15*sys;
        v.posAt=v.heartbeatAt=now;
        v.trail.append({v.lon-.00003,v.lat-.00003});
        v.trail.append({v.lon,v.lat});
    }
    Scene3DView scene(state);
    scene.resize(1280,780);
    scene.show();
    app.processEvents();
    const QImage image=scene.grab().toImage().convertToFormat(QImage::Format_RGB32);
    if(qEnvironmentVariableIsSet("AEROCORE_CAPTURE"))image.save(qEnvironmentVariable("AEROCORE_CAPTURE"));
    constexpr QRgb colors[]={0x00ec4899,0x00ef4444,0x00facc15};
    for(QRgb color:colors){
        int pixels=0;
        for(int y=0;y<image.height();++y)
            for(int x=0;x<image.width();++x)
                if((image.pixel(x,y)&0xffffff)==color)++pixels;
        if(pixels<10)return 1;
    }
    state->vehicles.clear();
    scene.update();app.processEvents();
    const QImage cleared=scene.grab().toImage().convertToFormat(QImage::Format_RGB32);
    if(image==cleared)return 2;
    return 0;
}
