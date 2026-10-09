#include "MavlinkReadOnly.hpp"
#include "Fixtures.hpp"
#include <cmath>
#include <stdexcept>
#include <iostream>
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
std::optional<sitl::TelemetryPacketData> feed(sitl::MavlinkParser &parser,const std::vector<std::uint8_t>& bytes){
    std::optional<sitl::TelemetryPacketData> result;
    for(auto byte:bytes)if(auto packet=parser.feed(byte,10))result=packet;
    return result;
}
int main(){
    sitl::MavlinkParser parser;
    std::vector<std::uint8_t> position(28);
    fixtures::put32(position,4,473977420);
    fixtures::put32(position,8,85455940);
    fixtures::put32(position,16,12500);
    auto bytes=fixtures::frame(33,104,position,true);
    auto p=feed(parser,bytes);
    require(p&&p->kind==sitl::PacketKind::Position,"position accepted");
    require(std::abs(p->latitude-47.397742)<1e-8&&p->relativeAltitudeM==12.5f,"position units");
    bytes.back()^=1;
    require(!feed(parser,bytes)&&parser.stats().badCrc==1,"bad CRC rejected");
    std::vector<std::uint8_t> attitude(28);
    fixtures::putFloat(attitude,4,0.25f);
    p=feed(parser,fixtures::frame(30,39,attitude,true));
    require(p&&p->roll==0.25f&&p->pitch==0,"truncated attitude zero filled");
    std::vector<std::uint8_t> heartbeat(9);
    auto signedFrame=fixtures::frame(0,50,heartbeat);
    signedFrame[2]=1; signedFrame.resize(signedFrame.size()+13);
    require(!feed(parser,signedFrame)&&parser.stats().signedRejected==1,"unauthenticated signed frame rejected");
    auto valid=fixtures::frame(0,50,heartbeat);
    valid[5]=3;
    std::uint16_t crc=65535;
    for(std::size_t i=1;i<valid.size()-2;++i)fixtures::crc(crc,valid[i]);
    fixtures::crc(crc,50);valid[valid.size()-2]=std::uint8_t(crc);valid.back()=std::uint8_t(crc>>8);
    p=feed(parser,valid);
    require(p&&p->system==3,"vehicle identity preserved after rejected packet");
    std::cout<<"MAVLink read-only checks passed\n";
}
