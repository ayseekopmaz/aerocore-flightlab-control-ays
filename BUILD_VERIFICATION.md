# Build verification — 0.6.0

C++20, GNU 13.3, Qt 6.4.2, CMake 3.28.3, Ninja — Linux.
Full application compiled successfully.
CTest: 12/12 test groups passed, 0 failed.

Groups: mavlink_readonly_test, simulation_protocol_test, experiment_core_test,
launcher_test, profile_test, fleet_test, lab_types_test,
experiment_runner_test, fleet_transport_test, fleet_comparison_test,
recording_test, ui_smoke_test.

Qt offscreen previews of fleet and replay pages inspected.
Three fake PX4/PTY processes and fake Gazebo service readiness exercised.
Comparison test verifies three vehicle runs, per-SYS profile parameters,
landing/disarm, parameter restoration and automatic JSON/HTML reports.
No actual Windows/MinGW/WSL/PX4/Gazebo execution was performed here.
Windows users must compile in their Qt Creator kit and verify the real session.

Reproduce with a normally installed Qt toolchain:

```sh
cmake -S . -B build -G Ninja -DBUILD_TESTING=ON
cmake --build build
ctest --test-dir build --output-on-failure
```
