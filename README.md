# ESP-FC Configurator

Native Qt configurator for ESP-FC flight-controller firmware.

This project is intentionally separate from the firmware repository. It starts with a Linux-first Qt 6 desktop application and targets the ESP32-S3 ESP-FC setup first.

## Current Scope

- USB serial and TCP transports.
- ESP32-S3 native USB identification (`303A:8167`).
- MSP v1 request/response framing for status and future structured config.
- CLI console for ESP-FC text commands (`#`, `dump`, `get`, `set`, `save`, `reboot`, `status`, `stats`).
- Placeholder tabs for the planned setup, receiver, motors, PID, filters, rates, and sensors workflows.

## Planned MVP

- Dashboard with firmware, sensors, arming flags, rates, and CPU load.
- Setup wizard for ESP32-S3 + BMI160 + SBUS/CRSF + PWM motors.
- Receiver provider setting through `MSP_RX_CONFIG` / `MSP_SET_RX_CONFIG` so CRSF can be selected even though ESP-FC CLI does not expose it.
- Motor order diagram for ESP-FC `QUADX` (`RR`, `FR`, `RL`, `FL`).
- PID, filters, rates/expo/limiters, modes, Wi-Fi/TCP, and CLI dump import/export.
- Safety gates for motor tests and arming-risk operations.

## Build

Requirements:

- CMake 3.21+
- Qt 6.4+ with `Core`, `Widgets`, `SerialPort`, `Network`, and `OpenGLWidgets`
- C++20 compiler

```sh
cmake -S . -B build
cmake --build build
```

## ESP-FC Connection Notes

USB serial uses the ESP32-S3 native CDC device. For the current firmware board definition, the expected USB IDs are:

```text
VID: 0x303A
PID: 0x8167
```

TCP works when ESP-FC Wi-Fi configuration mode is active:

```text
tcp://192.168.4.1:1111
```

Wi-Fi config mode disables arming in the firmware and requires a reboot before flight.
