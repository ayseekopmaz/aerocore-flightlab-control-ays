# Verification

Custom IHA model: 6 Python tests pass (geometry/physics/camera, V2 changes, invalid-profile rejection, three-vehicle isolation, embedded Qt launcher-source preparation, numeric body dimensions/motor mass and camera toggle/resolution).
Existing launcher: 8 tests pass; existing fleet suite: 15 tests pass (includes profile regression tests).
All 76 original files retained. Qt UI test updated for the added sixth tab. Changes to existing sources are integration, launcher dispatch/model boot overrides and displayed model identity; legacy model generation remains available.
Qt/CMake development tools and Gazebo runtime are unavailable in this environment. C++ build, GUI launch, Gazebo SDF runtime load, camera frames and actual flight have NOT been verified. Build in your existing Qt kit and test in WSL.

Windows launch transport fix: 3 bootstrap tests pass, including 140 KB source/config over stdin, Unicode/spaces, stop command preservation, temporary file cleanup and malformed/EOF rejection. Six custom-model tests still pass. Windows/WSL/Qt execution remains unverified here.

Post-test energy estimate compiled and executed with g++ C++20: 60 s flat hover, tilt increase, no integration across stale gaps, invalid input rejection. Qt report rendering/compilation cannot be checked without Qt development libraries here.

Three-drone extension: independent runner/transport/state per system, readiness gating and common measurement epoch, per-system profile hash lookup, main manual-command gating, cancellation paths and IHA-only 3D style. Python custom model (6), bootstrap (3), fleet regressions (15) and portable energy test pass. New Qt coordinator/view code not compiled or run here; actual concurrent flight and visual QA remain unverified.

Aura-Relay: standalone RelayCore C++ test compiled/executed; consumption, ideal recharge, stale handover gate and non-100% coverage gaps checked. Python bootstrap/custom-model/fleet regressions pass. Qt coordinator/live graph/PDF and actual SITL handover NOT compiled or run here.

Wind formation: standalone FormationCore C++ geometry/threshold/savings test compiled and passed. Qt coordination and PDF rendering not compiled/run. Common world wind only, no wake/leader-local gust model, no calibrated current/range model.
