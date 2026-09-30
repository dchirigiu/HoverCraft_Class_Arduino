# hovercraft-firmware

ENGR 290 (F2026) hovercraft group repo. One physical board exists — everyone
develops and tests against the **Wokwi simulator**, the same compiled binary is
flashed to the real Arduino Nano when it's your turn.

## Layout

| Path | What it is |
|---|---|
| `sketch/hovercraft_ta1/` | The firmware (Technical Assignment 1: IR + ultrasonic, D3 PWM, L blink, UART) |
| `diagram.json` | The simulated circuit wiring (Wokwi) |
| `wokwi.toml` | Wokwi project config (points at the compiled firmware) |
| `build.sh` | Compile (WSL/Linux) — output lands in `build/` |
| `sim.sh` | Run headless simulation, checks serial output |
| `flash.cmd` | Windows: compile + upload to the real Nano + serial monitor |
| `.github/workflows/sim.yml` | GitHub Actions: compile + simulate on every push |

## Team setup (per laptop, ~10 min)

1. Install VS Code + the Wokwi extension (`wokwi.wokwi-vscode`) — free, private repos OK.
2. `git clone <repo-url>` and open the folder in VS Code.
3. Compile once: `./build.sh` (WSL) — needs [arduino-cli](https://arduino.github.io/arduino-cli/latest/installation/) + `arduino-cli core install arduino:avr`.
   (On Windows: install arduino-cli, then run `flash.cmd` to compile+flash.)
4. Press **F1 → "Wokwi: Request a new License"** (one-time, free): confirm the
   browser, click **GET YOUR LICENSE**, confirm twice.
5. Press **F1 → "Wokwi: Start Simulator"** — that's the virtual Nano with the sensors.

## Simulating

- **VS Code (interactive, recommended):** F1 → Wokwi: Start Simulator. Click the
  potentiometer (`ir`) to set the IR stand-in voltage, click HC-SR04 to set the
  obstacle distance. Serial monitor tab shows the UART output.
- **Headless:** `./sim.sh` (needs `WOKWI_CLI_TOKEN` env var — free token from
  wokwi.com → Dashboard → CI).
- **CI:** every push runs the sim in GitHub Actions using repo secret
  `WOKWI_CLI_TOKEN` (Settings → Secrets and variables → Actions). The gate
  (`--expect-text` in `sim.sh` and `sim.yml`) asserts the full deterministic
  data row `;190;928;24;100;24;194;1` — ADC 190 → 928 mV → 24 cm (inside range,
  so PWM 194 and L solid ON) with
  US at 100 cm. If you change the diagram defaults (pot `value`, HC-SR04
  `distance`) or `IR_TABLE`, update that expected string in both places.

## Sensor stand-ins (important)

Wokwi has an exact **HC-SR04** part. There is no Sharp GP2Y0A21 part, so the IR
sensor is emulated by a **potentiometer on A0** (`ir` in diagram.json) — turn the
knob to set the "distance". The firmware converts mV → cm with a piecewise-linear
table (`IR_TABLE` in the sketch) using the Sharp datasheet curve; replace the
table values with your own DMM calibration points for the report.

## Flashing the real board

On Windows: `flash.cmd COM5` (new bootloader) or `flash.cmd COM5 oldbootloader`
(older Nano clones). Find the COM port with `flash.cmd` (no args) → `board list`.

## Comparing sim vs board

1. Save the simulation's serial output: `./sim.sh` writes `build/serial.log`
   (or copy the serial monitor text from the VS Code sim).
2. Flash the board: `flash.cmd COM5`.
3. Record the board's UART: `powershell -ExecutionPolicy Bypass -File capture.ps1 COM5 60`
   → writes `board_serial.log`.
4. Compare the columns (`t_ms;IR_adc;IR_mV;IR_cm;US_cm;src_cm;PWM;L`) between the
   two logs. Raw counts should match closely for the same conditions; distances
   differ only because the physical obstacle position differs from the sim's
   potentiometer / HC-SR04 slider settings.

## Firmware behavior (TA1)

- `SENSOR_SOURCE` at the top of the sketch selects IR or US as the driver sensor.
- Pin map (from the course PCB's `init_290.c`): IR on **A0**; HC-SR04 **TRIG on
  D8** and **ECHO on D2** (D4–D7 are power-control outputs on the PCB — never
  wire the sensor there); D3 LED on **PB3 = Arduino pin 11, ACTIVE-LOW**
  (PWM `255 - duty`); "L" on **PB5 = D13**. Confirm the silkscreen labels on
  your board revision before flashing.
- D3 brightness: ≤16 cm → 100%, ≥49 cm → 0%, linear in between (integer `map`).
- L (D13/PB5) blinks with T = 1.5 s while the obstacle is outside [16; 49] cm.
- UART 9600 8N1: `t_ms;IR_adc;IR_mV;IR_cm;US_cm;src_cm;PWM;L` — same lines must
  appear in the sim and on the real board. The printed `PWM` column is the
  *logical* duty (100% = 255); the pin output is inverted because D3 is
  active-low.
