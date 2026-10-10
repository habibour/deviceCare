# Test evidence

Automated with `wokwi-cli` (headless Wokwi simulation of the STM32F103 firmware
`src/build/Debug/voltage_protector_2.elf`). Re-run everything with:

```bash
cd src && cmake --preset Debug && cmake --build --preset Debug && cd ..
export WOKWI_CLI_TOKEN=<token from https://wokwi.com/dashboard/ci>
test_evidence/run_all.sh
```

Pots: `pot1` = voltage (position x 330 V), `pot2` = current (position x 20 A).
Limits at defaults: OV 250 V, UV 170 V, OC 10 A. Relay = LED on PB0.

| ID | Test | Stimulus | Expected | Result | Evidence |
|---|---|---|---|---|---|
| 01 | Normal operation | 231 V, 5 A | `MONITORING / NONE`, relay on | PASS | `logs/01_normal.log`, `screenshots/01_relay_on_normal.png` |
| 02 | Overvoltage | 280 V | `DISCONNECTED / OVERVOLTAGE`, relay off | PASS | `logs/02_overvoltage.log`, `screenshots/02_relay_off_overvoltage.png` |
| 03 | Undervoltage | 132 V | `DISCONNECTED / UNDERVOLTAGE`, relay off | PASS | `logs/03_undervoltage.log` |
| 04 | Overcurrent | 15 A | `DISCONNECTED / OVERCURRENT`, relay off | PASS | `logs/04_overcurrent.log` |
| 05 | Cooldown | OV, then back to 231 V | `COOLDOWN` (relay off) then `MONITORING` (relay on) | PASS | `logs/05_cooldown.log`, `timing/05_cooldown.vcd` |
| 06 | Manual reset button (PB12) | press while faulted | `COOLDOWN` | PASS | `logs/06_reset_button.log` |
| 07 | UART commands (SET_OV/UV/OC, HELP, STATUS, errors) | serial input | `[OK]` / `[ERR]` replies | **NOT VERIFIED in simulator** (serial RX not delivered by Wokwi; needs real hardware) | see note below |
| 08 | UART setting takes effect | `SET_OV:240`, 248 V | fault at lower limit | **NOT VERIFIED in simulator** (same cause as 07) | see note below |
| 09 | Response time | OV injected at t = 5010 ms | PB0 low within one 100 ms sample | PASS, about 34 ms | `timing/09_timing.vcd` (PB0 falls at 5043.97 ms) |
| 10 | On-device UI | SEL, UP, UP on OV | LCD: `SET OVERVOLTAGE 260V` | PASS | `screenshots/10_lcd_view.png`, `screenshots/10_lcd_after_sel_up_up.png` |

## Settings change: on-device buttons verified, UART not verifiable in the simulator

The requirement to change the protection settings on the device is met and
verified in simulation by test 10: SEL selects OV/UV/OC, UP/DOWN step the value
(5 V / 0.5 A), and the LCD and the periodic `[STATUS]` line show the new limit.

The UART path (tests 07/08) could not be verified in Wokwi:

- Firmware TX works (banner, `[STATUS]` lines).
- Bytes written to the simulated serial port never reach USART1 RX. Tried:
  `wokwi-cli` scenario `write-serial`, `wokwi-cli --interactive`, and the VS Code
  extension's RFC2217 server (`rfc2217ServerPort = 4000`, pyserial miniterm).
  All of `HELP`, `STATUS`, `SET_OV:260`, `SET_UV`, `SET_OC` and an unknown
  command got no reply, and `OV` stayed at 250 V.
- Inspection of the running simulation over gdb after sending a command:
  USART1 `CR1 = 0x202C` (UE, RE, TE, RXNEIE set), `ring_head = ring_tail = 0`,
  i.e. no byte was ever received. The USART1 vector points to
  `USART1_IRQHandler` -> `uart_comm_rx_isr`.
- Changing PA10 from floating input to pull-up input made no difference.
- With bytes injected into the ring buffer via gdb and the CPU single-stepped,
  `uart_comm_process` consumed them and started the reply, so the parser path
  is exercised; free-running injection was inconclusive.

Conclusion: the simulator does not deliver serial input to this STM32 model. The
firmware RX path was reviewed but not shown working end to end.

**Pending:** repeat 07/08 on real hardware with a USB-serial adapter on
PA9 (TX) / PA10 (RX), 115200 8N1, and add the capture here.

## Observations

- Relay output goes high about 1.4 ms after reset (`fsm_init`), before the first
  ADC sample at 100 ms. If the supply is already faulted at power-up the load is
  connected for up to ~100 ms.
- Test 09 timing is simulated time; the injection instant has a few ms of
  uncertainty from scenario scheduling.
