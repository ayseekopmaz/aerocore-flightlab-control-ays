#pragma once
#include <array>
#include <bit>
#include <cstdint>
#include <vector>
namespace fixtures {
inline void crc(std::uint16_t &c,std::uint8_t b){std::uint8_t t=b^std::uint8_t(c);t^=std::uint8_t(t<<4);c=(c>>8)^(std::uint16_t(t)<<8)^(std::uint16_t(t)<<3)^(t>>4);}
inline std::vector<std::uint8_t> frame(std::uint32_t id,std::uint8_t extra,std::vector<std::uint8_t> p,bool trim=false){
    if(trim)while(p.size()>1 && p.back()==0)p.pop_back();
    std::vector<std::uint8_t> b={0xFD,std::uint8_t(p.size()),0,0,7,1,1,std::uint8_t(id),std::uint8_t(id>>8),std::uint8_t(id>>16)};
    b.insert(b.end(),p.begin(),p.end());std::uint16_t c=65535;for(std::size_t i=1;i<b.size();++i)crc(c,b[i]);crc(c,extra);b.push_back(std::uint8_t(c));b.push_back(std::uint8_t(c>>8));return b;
}
inline void put32(std::vector<std::uint8_t> &p,int offset,std::uint32_t v){for(int i=0;i<4;++i)p[std::size_t(offset+i)]=std::uint8_t(v>>(8*i));}
inline void putFloat(std::vector<std::uint8_t> &p,int offset,float v){put32(p,offset,std::bit_cast<std::uint32_t>(v));}
}
