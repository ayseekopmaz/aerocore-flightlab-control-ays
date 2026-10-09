#include "MavlinkReadOnly.hpp"
#include <bit>
#include <algorithm>
#include <cmath>
namespace sitl {
namespace {
void crcByte(std::uint16_t &crc,std::uint8_t byte) {
    std::uint8_t t=byte ^ static_cast<std::uint8_t>(crc);
    t^=static_cast<std::uint8_t>(t<<4);
    crc=(crc>>8) ^ (std::uint16_t(t)<<8) ^ (std::uint16_t(t)<<3) ^ (t>>4);
}
std::uint16_t u16(const std::uint8_t *p) {return p[0]|(std::uint16_t(p[1])<<8);}
std::uint32_t u32(const std::uint8_t *p) {return u16(p)|(std::uint32_t(u16(p+2))<<16);}
float f32(const std::uint8_t *p) {return std::bit_cast<float>(u32(p));}
std::int16_t i16(const std::uint8_t *p) {return std::bit_cast<std::int16_t>(u16(p));}
std::int32_t i32(const std::uint8_t *p) {return std::bit_cast<std::int32_t>(u32(p));}
}
std::optional<TelemetryPacketData> MavlinkParser::feed(std::uint8_t byte,double now) {
    if(used_==0 && byte!=0xFD && byte!=0xFE)return std::nullopt;
    if(used_>=bytes_.size()){++stats_.malformed;reset();return std::nullopt;}
    bytes_[used_++]=byte;
    const bool v2=bytes_[0]==0xFD;
    if(used_==2)expected_=std::size_t(bytes_[1])+(v2?12:8);
    if(v2 && used_==3){
        if(bytes_[2]&~std::uint8_t(1)){++stats_.malformed;reset();return std::nullopt;}
        if(bytes_[2]&1)expected_+=13;
    }
    if(expected_ && used_==expected_){auto result=decode(now);reset();return result;}
    return std::nullopt;
}
std::optional<TelemetryPacketData> MavlinkParser::decode(double now) {
    const bool v2=bytes_[0]==0xFD;
    const auto header=v2?10u:6u;
    const std::uint32_t id=v2 ? bytes_[7]|(std::uint32_t(bytes_[8])<<8)|(std::uint32_t(bytes_[9])<<16) : bytes_[5];
    std::uint8_t extra=0;std::size_t minV1=0,maxV2=0;PacketKind kind{};
    switch(id){
    case 375:extra=251;minV1=140;kind=PacketKind::Actuators;break;
    case 22:extra=220;minV1=25;kind=PacketKind::Parameter;break;
    case 24:extra=24;minV1=30;kind=PacketKind::Gps;break;
    case 0:extra=50;minV1=9;kind=PacketKind::Heartbeat;break;
    case 30:extra=39;minV1=28;kind=PacketKind::Attitude;break;
    case 33:extra=104;minV1=28;kind=PacketKind::Position;break;
    case 1:extra=124;minV1=31;kind=PacketKind::System;break;
    case 77:extra=143;minV1=3;kind=PacketKind::CommandAck;break;
    case 245:extra=130;minV1=2;kind=PacketKind::ExtendedState;break;
    case 253:extra=83;minV1=51;kind=PacketKind::StatusText;break;
    case 147:extra=154;minV1=36;kind=PacketKind::Battery;break;
    default:++stats_.unsupported;return std::nullopt;
    }
    maxV2=id==24?52:id==147?54:id==77?10:id==253?54:minV1;
    if(v2 && bytes_[2]&1){++stats_.signedRejected;return std::nullopt;}
    const auto len=bytes_[1];
    if((!v2 && len!=minV1) || (v2 && (len==0 || len>maxV2))){++stats_.malformed;return std::nullopt;}
    std::uint16_t crc=0xFFFF;
    for(std::size_t i=1;i<header+len;++i)crcByte(crc,bytes_[i]);
    crcByte(crc,extra);
    if(crc!=u16(bytes_.data()+header+len)){++stats_.badCrc;return std::nullopt;}
    // MAVLink 2 truncates trailing zero payload bytes, so zero-fill first.
    std::array<std::uint8_t,255> payload{};
    std::copy_n(bytes_.begin()+header,len,payload.begin());const auto *p=payload.data();
    TelemetryPacketData r;r.kind=kind;r.receivedAt=now;
    r.sequence=bytes_[v2?4:2];r.system=bytes_[v2?5:3];r.component=bytes_[v2?6:4];
    switch(kind){
    case PacketKind::Actuators:r.activeOutputs=u32(p+8);for(int i=0;i<32;++i)r.actuators[i]=f32(p+12+i*4);break;
    case PacketKind::Parameter:r.paramBits=u32(p);r.paramType=p[24];r.paramValue=r.paramType==6?float(i32(p)):f32(p);for(int i=8;i<24&&p[i];++i)r.paramName.push_back(char(p[i]));break;
    case PacketKind::Gps:r.fixType=p[28];break;
    case PacketKind::Heartbeat:r.customMode=u32(p);r.vehicleType=p[4];r.autopilot=p[5];r.baseMode=p[6];break;
    case PacketKind::CommandAck:r.command=u16(p);r.result=p[2];r.ackTargetSystem=p[8];r.ackTargetComponent=p[9];break;
    case PacketKind::ExtendedState:r.landedState=p[1];break;
    case PacketKind::StatusText:r.severity=p[0];for(int i=1;i<=50&&p[i];++i)r.text.push_back(char(p[i]));break;
    case PacketKind::Attitude:
        r.roll=f32(p+4);r.pitch=f32(p+8);r.yaw=f32(p+12);
        if(!std::isfinite(r.roll)||!std::isfinite(r.pitch)||!std::isfinite(r.yaw)){++stats_.malformed;return std::nullopt;}break;
    case PacketKind::Position:
        r.bootMs=u32(p);r.latitude=i32(p+4)/1e7;r.longitude=i32(p+8)/1e7;
        r.altitudeM=i32(p+12)/1000.0f;r.relativeAltitudeM=i32(p+16)/1000.0f;
        if(std::abs(r.latitude)>90||std::abs(r.longitude)>180){++stats_.malformed;return std::nullopt;}break;
    case PacketKind::System:
        r.voltageValid=u16(p+14)!=65535;r.voltageV=u16(p+14)/1000.0f;
        r.currentValid=i16(p+16)!=-1;r.currentA=i16(p+16)/100.0f;r.remainingPercent=std::bit_cast<std::int8_t>(p[30]);break;
    case PacketKind::Battery: {
        std::uint32_t sum=0;bool any=false;
        for(int i=0;i<10;++i){auto v=u16(p+10+2*i);if(v!=65535){sum+=v;any=true;}}
        // Extension cell voltages use 0 for unsupported; include supplied values.
        for(int i=0;i<4;++i){auto v=u16(p+41+2*i);if(v!=0 && v!=65535){sum+=v;any=true;}}
        r.voltageValid=any;r.voltageV=sum/1000.0f;
        r.currentValid=i16(p+30)!=-1;r.currentA=i16(p+30)/100.0f;r.remainingPercent=std::bit_cast<std::int8_t>(p[35]);break;
    }}
    ++stats_.valid;return r;
}
}
