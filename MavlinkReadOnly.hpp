#pragma once

#include <array>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <memory>
#include <optional>
#include <span>
#include <stop_token>
#include <string>
#include <thread>
#include <variant>

namespace sitl {
using MonoClock = std::chrono::steady_clock;
inline double monoSeconds() { return std::chrono::duration<double>(MonoClock::now().time_since_epoch()).count(); }
enum class PacketKind { Heartbeat, Attitude, Position, System, Battery, CommandAck, ExtendedState, StatusText, Parameter, Gps, Actuators };
struct TelemetryPacketData {
    PacketKind kind{};
    std::uint8_t system=0, component=0, sequence=0;
    double receivedAt=0;
    std::array<float,32> actuators{};std::uint32_t activeOutputs=0;
    std::uint32_t customMode=0,bootMs=0;
    std::string paramName;float paramValue=0;std::uint32_t paramBits=0;std::uint8_t paramType=0,fixType=0;
    float roll=0,pitch=0,yaw=0;
    double latitude=0,longitude=0;
    float altitudeM=0,relativeAltitudeM=0,voltageV=0,currentA=0;
    std::int8_t remainingPercent=-1;
    bool voltageValid=false,currentValid=false;
    std::uint8_t vehicleType=0,autopilot=0,baseMode=0,landedState=0;
    std::uint16_t command=0; std::uint8_t result=0,ackTargetSystem=0,ackTargetComponent=0,severity=0;
    std::string text;
};
template<class T> concept TelemetryPacket = requires(T p) {
    { p.receivedAt } -> std::convertible_to<double>;
    { p.sequence } -> std::convertible_to<std::uint8_t>;
};
static_assert(TelemetryPacket<TelemetryPacketData>);
struct ParserStats { std::uint64_t valid=0, badCrc=0, unsupported=0, signedRejected=0, malformed=0; };
// Bounded common-message subset; command encoder is in SimulationProtocol.hpp.
// Signed messages are REJECTED: this implementation does not authenticate them.
class MavlinkParser {
public:
    std::optional<TelemetryPacketData> feed(std::uint8_t byte, double now);
    const ParserStats &stats() const { return stats_; }
    void reset() { used_=expected_=0; }
private:
    std::array<std::uint8_t,280> bytes_{};
    std::size_t used_=0,expected_=0;
    ParserStats stats_{};
    std::optional<TelemetryPacketData> decode(double now);
};
}
