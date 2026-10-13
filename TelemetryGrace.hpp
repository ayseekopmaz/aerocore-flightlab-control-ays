#pragma once
#include <optional>
namespace aero {
struct TelemetryGrace {
    std::optional<double> staleAt;
    // Keep strict freshness checks; allow recovery without issuing new motion commands.
    bool expired(double now,bool fresh){if(fresh){staleAt.reset();return false;}if(!staleAt)staleAt=now;return now-*staleAt>=3.;}
};
}
