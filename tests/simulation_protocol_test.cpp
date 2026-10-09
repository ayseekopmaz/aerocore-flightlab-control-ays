#include "SimulationProtocol.hpp"
#include "MavlinkReadOnly.hpp"
#include "Fixtures.hpp"
#include <stdexcept>
#include <iostream>
void require(bool ok,const char *message){if(!ok)throw std::runtime_error(message);}
std::optional<sitl::TelemetryPacketData> parse(sitl::MavlinkParser &parser,const std::vector<std::uint8_t>& bytes){
    std::optional<sitl::TelemetryPacketData> packet;
    for(auto byte:bytes){if(auto p=parser.feed(byte,10))packet=p;}
    return packet;
}
int main(){
    sitl::MavlinkParser parser;
    auto heartbeat=parse(parser,sitl::gcsHeartbeat(42));
    require(heartbeat&&heartbeat->system==255&&heartbeat->component==190&&heartbeat->vehicleType==6&&heartbeat->autopilot==8,"GCS identity and heartbeat");
    auto command=sitl::commandLong(400,3,1,{1,0,0,0,0,0,0},43);
    require(command.size()==45&&command[7]==76&&command[38]==0x90&&command[39]==1&&command[40]==3&&command[41]==1&&command[42]==0,"COMMAND_LONG wire target/ID/confirmation");
    require(command[10]==0&&command[11]==0&&command[12]==0x80&&command[13]==0x3F,"ARM float is little endian");
    std::vector<std::uint8_t> payload(10);
    payload[0]=0x90;payload[1]=1;payload[2]=2;payload[8]=255;payload[9]=190;
    auto ack=parse(parser,fixtures::frame(77,143,payload));
    require(ack&&ack->kind==sitl::PacketKind::CommandAck&&ack->command==400&&ack->result==2&&ack->ackTargetSystem==255&&ack->ackTargetComponent==190,"command denial and recipient decoded");
    auto ground=parse(parser,fixtures::frame(245,130,{0,1}));
    require(ground&&ground->landedState==1,"landed state decoded");
    std::vector<std::uint8_t> text(51);text[0]=4;text[1]='N';text[2]='o';
    auto status=parse(parser,fixtures::frame(253,83,text,true));
    require(status&&status->text=="No"&&status->severity==4,"preflight status text decoded");
    auto px4=parse(parser,fixtures::frame(0,50,{0,0,0,0,2,12,128,4,3}));
    require(px4&&px4->autopilot==12&&(px4->baseMode&128),"PX4 armed heartbeat decoded");
    auto go=sitl::reposition(3,47.4,8.54,505,3,1);
    require(go.size()==47&&go[7]==75&&go[38]==192&&go[40]==3&&go[41]==1&&go[42]==0,"COMMAND_INT frame / target / command");
    auto read=sitl::paramRead(3,"SYS_FAILURE_EN",1);require(read[7]==20&&read[10]==255&&read[11]==255&&read[12]==3,"param read -1 index and target");
    auto set=sitl::paramSet(3,"SYS_FAILURE_EN",1,6,1);require(set[10]==1&&set[11]==0&&set[32]==6,"PX4 int32 bytewise parameter encoding");
    std::vector<std::uint8_t> param(25);param[0]=1;param[24]=6;std::string name="SYS_FAILURE_EN";std::copy(name.begin(),name.end(),param.begin()+8);
    auto decoded=parse(parser,fixtures::frame(22,220,param));require(decoded&&decoded->paramName==name&&decoded->paramType==6&&decoded->paramValue==1,"int32 parameter readback decoded");
    std::vector<std::uint8_t> outputs(140);outputs[8]=15;sitl::put32(outputs,12,std::bit_cast<std::uint32_t>(.5f));
    auto motors=parse(parser,fixtures::frame(375,251,outputs));require(motors&&motors->activeOutputs==15&&motors->actuators[0]==.5f,"actuator output telemetry decoded");
    std::cout<<"Simulation protocol checks passed\n";
}
