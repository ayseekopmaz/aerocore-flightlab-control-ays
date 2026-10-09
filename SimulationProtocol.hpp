#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>
namespace sitl {
// MAVLink 2 little-endian wire layout. Source GCS system=255, component=190.
inline void crcAccumulate(std::uint16_t &c,std::uint8_t b){
    std::uint8_t t=b^std::uint8_t(c);t^=std::uint8_t(t<<4);
    c=(c>>8)^(std::uint16_t(t)<<8)^(std::uint16_t(t)<<3)^(t>>4);
}
inline std::vector<std::uint8_t> encode(std::uint32_t id,std::uint8_t extra,
    const std::vector<std::uint8_t>& payload,std::uint8_t sequence){
    std::vector<std::uint8_t> b={0xFD,std::uint8_t(payload.size()),0,0,sequence,255,190,
        std::uint8_t(id),std::uint8_t(id>>8),std::uint8_t(id>>16)};
    b.insert(b.end(),payload.begin(),payload.end());
    std::uint16_t c=65535;for(std::size_t i=1;i<b.size();++i)crcAccumulate(c,b[i]);
    crcAccumulate(c,extra);b.push_back(std::uint8_t(c));b.push_back(std::uint8_t(c>>8));return b;
}
inline std::vector<std::uint8_t> gcsHeartbeat(std::uint8_t sequence){
    return encode(0,50,{0,0,0,0,6,8,0,4,3},sequence);
}
inline std::vector<std::uint8_t> commandLong(std::uint16_t command,std::uint8_t targetSystem,
    std::uint8_t targetComponent,const std::array<float,7>& params,std::uint8_t sequence){
    std::vector<std::uint8_t> p(33);
    for(std::size_t k=0;k<7;++k){auto v=std::bit_cast<std::uint32_t>(params[k]);
        for(int j=0;j<4;++j)p[k*4+j]=std::uint8_t(v>>(8*j));}
    p[28]=std::uint8_t(command);p[29]=std::uint8_t(command>>8);
    p[30]=targetSystem;p[31]=targetComponent;p[32]=0;
    return encode(76,152,p,sequence);
}
inline void put32(std::vector<std::uint8_t>& p,int offset,std::uint32_t v){for(int j=0;j<4;++j)p[offset+j]=std::uint8_t(v>>(8*j));}
inline std::vector<std::uint8_t> reposition(std::uint8_t sys,double lat,double lon,float amsl,float speed,std::uint8_t sequence){
    std::vector<std::uint8_t> p(35);put32(p,0,std::bit_cast<std::uint32_t>(speed));put32(p,4,std::bit_cast<std::uint32_t>(1.f));
    put32(p,12,0x7fc00000);put32(p,16,std::uint32_t(std::int32_t(std::llround(lat*1e7))));put32(p,20,std::uint32_t(std::int32_t(std::llround(lon*1e7))));
    put32(p,24,std::bit_cast<std::uint32_t>(amsl));p[28]=192;p[29]=0;p[30]=sys;p[31]=1;p[32]=0; // MAV_FRAME_GLOBAL / AMSL
    return encode(75,158,p,sequence);
}
inline std::vector<std::uint8_t> paramRead(std::uint8_t sys,const std::string& name,std::uint8_t sequence){
    std::vector<std::uint8_t> p(20);p[0]=255;p[1]=255;p[2]=sys;p[3]=1;std::copy_n(name.begin(),std::min<std::size_t>(16,name.size()),p.begin()+4);return encode(20,214,p,sequence);
}
inline std::vector<std::uint8_t> paramSet(std::uint8_t sys,const std::string& name,float value,std::uint8_t type,std::uint8_t sequence){
    std::vector<std::uint8_t> p(23);put32(p,0,type==6?std::uint32_t(std::int32_t(value)):std::bit_cast<std::uint32_t>(value));p[4]=sys;p[5]=1;
    std::copy_n(name.begin(),std::min<std::size_t>(16,name.size()),p.begin()+6);p[22]=type;return encode(23,168,p,sequence);
}

}
