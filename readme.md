# deviceCare - Voltage Protector (STM32F103 Blue Pill)

Microcontroller-based protector that disconnects a load (relay) on overvoltage,
undervoltage or overcurrent, with settings changeable on the device or from a computer.

## Repository layout

| Path | Content |
|---|---|
| `src/` | Firmware (STM32 HAL, CMake project) |
| `project_files/` | Wokwi simulation (`diagram.json`, `wokwi.toml`) |
| `schematics/` | Schematic / block diagram of the real product |
| `test_evidence/` | Screenshots, logs, timing captures |
| `design_document.pdf` | Design document |

## Features

- Fault detection: overvoltage (default 250 V), undervoltage (170 V), overcurrent (10 A)
- Relay output on PB0 disconnects the load on any fault
- State machine: MONITORING -> DISCONNECTED -> COOLDOWN (3 s) -> MONITORING
- Manual reset button (PB12)
- Settings on the device: SEL / UP / DOWN buttons (PB13 / PB14 / PB15) and a 16x2 I2C LCD (PB6 SCL, PB7 SDA)
- Settings from a computer over UART (USART1, PA9 TX / PA10 RX, 115200 8N1)
- Status line printed every second

## Serial commands

End each command with Enter (CR or LF).

| Command | Effect |
|---|---|
| `SET_OV:<V>` | Overvoltage limit |
| `SET_UV:<V>` | Undervoltage limit |
| `SET_OC:<A>` | Overcurrent limit |
| `STATUS` | Print status now |
| `RESET` | Same as the reset button |
| `HELP` | List commands |

Limits are kept in RAM only and return to defaults after a restart.
OV must be above UV and at most 330 V; OC must be above 0 and at most 20 A.

## On-device UI

SEL cycles: view -> set OV -> set UV -> set OC -> view. UP / DOWN change the
value (5 V steps for OV/UV, 0.5 A for OC). Returns to the view after 10 s idle.

## Build

Requirements: `arm-none-eabi-gcc`, CMake >= 3.22, Ninja.

```bash
cd src
cmake --preset Debug
cmake --build --preset Debug
```

Output: `src/build/Debug/voltage_protector_2.elf`

## Run the simulation (Wokwi for VS Code)

1. Build the firmware (above).
2. Install the Wokwi Simulator VS Code extension and request a license
   (`F1` -> *Wokwi: Request a Free License*).
3. Open the `project_files/` folder in VS Code.
4. `F1` -> *Wokwi: Start Simulator*. The serial monitor is wired to USART1.

Simulation inputs: the two potentiometers stand in for the voltage sensor
(0-100 % = 0-330 V, PA0) and the current sensor (0-100 % = 0-20 A, PA1).
The red LED on PB0 represents the relay (lit = load connected).

## Limitations

- Fault checking is polled in the main loop every 100 ms, not interrupt-driven.
- Settings are not stored in flash.
- No hardware watchdog.
