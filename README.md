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
5. Press **F1 → "Wokwi: Request a new License"** (one-time, free, private OK):
   confirm the browser tab, click **GET YOUR LICENSE**, close the tab.

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
table (`IR_TABLE`) using the Sharp datasheet curve. Replace those table values
with your own DMM calibration points for the TA1 report.
## Flashing the real board

On Windows: `.\flash.cmd` (no args) lists the ports. Then `.\flash.cmd COM5`
(new bootloader) or `.\flash.cmd COM5 oldbootloader` (clone Nanos). The board
build defines `-DON_BOARD` so the sketch uses `VREF_MV_BOARD` instead of the
simulator's 5000 mV: measure AREF with the DMM and set `VREF_MV_BOARD` in the
sketch before flashing. The boot banner prints which build is running.

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
- D3 and L follow `src_cm` only (the sensor picked by `SENSOR_SOURCE`). `IR_cm`
  and `US_cm` are always printed, but only one of them drives the LEDs. The two
  LED rules are independent: below 16 cm, D3 is at 100% **and** L blinks
  (TA1: "flash L when the obstacle is outside [d1; d2]").

| `src_cm` (cm) | D3 brightness | L (D13/PB5) |
|---|---|---|
| below 16 | 100% | blinks, T = 1.5 s |
| 16 | 100% | solid ON |
| 17 to 48 | dims linearly as distance grows | solid ON |
| 49 | 0% | solid ON |
| above 49, or 999 (no reading) | 0% | blinks, T = 1.5 s |

- `IR_cm` never shows less than 10: the IR table starts at 10 cm, so anything
  closer is clamped to 10.
- UART 9600 8N1: `t_ms;IR_adc;IR_mV;IR_cm;US_cm;src_cm;PWM;L;US_us` — same lines
  must appear in the sim and on the real board. The printed `PWM` column is the
  *logical* duty (100% = 255); the pin output is inverted because D3 is
  active-low. `US_us` is the echo pulse width in microseconds (table 1 "time").

## Test A: simulator only (2 min)

No board needed — prove the workflow and the team can share code.

1. **Everyone:** Pull the repo, press F1 → Wokwi: Start Simulator in VS Code.
2. **Observe:** Serial monitor shows `TA1 READY`, `simulator build | VREF_MV=5000`, then the data rows every 0.5 s.
3. **Change the knob:** Click the potentiometer (`ir`), drag or press arrow keys to set a new value. Watch `IR_adc`, `IR_mV`, `IR_cm` change. When `src_cm` is 16–49, L is solid ON; outside that range it blinks.
4. **Change the US distance:** Click the HC-SR04, drag the `distance` slider. Watch `US_cm` and `US_us` change (the echo time in microseconds).
5. **Push a trivial change** (add a comment anywhere in the sketch), rebuild (`./build.sh` or `build.cmd`), re-run the sim. The group sees your commit on GitHub; pulling it gets everyone the same binary.

## Test B: real board (5 min)

One person flashes the board while the others watch the serial output.

**Before you start:** open the sketch and set `VREF_MV_BOARD` to your DMM reading of the AREF pin voltage (typically ~5000 mV if RV1 is at max, or 3.3 V if the course default is 3.3 V). Save. The simulator ignores this value.

1. **Install the CH340/CH341 USB-serial driver** if Windows doesn't recognize the Nano: download [CH341SER.EXE](https://wch-ic.com/downloads/CH341SER_EXE.html), run it, click Install. Unplug and replug the Nano.
2. **Find the port:** open a terminal (PowerShell), `cd` into the repo folder, type `.\flash.cmd` (no args). It prints `Port Protocol Type Board Name FQBN Core` and lists every serial device. The Nano typically shows as `COMx Serial Port (USB) Unknown`. Write down the port (e.g. `COM5`).
3. **Flash:** type `.\flash.cmd COM5`. It compiles with `-DON_BOARD`, uploads the hex, then opens the serial monitor. You'll see `TA1 READY`, `board build | VREF_MV=...`, then data rows. Press Ctrl+C to exit the monitor.
4. **Move the IR sensor:** slide the obstacle from 10 cm to 80 cm. Watch the `IR_mV` and `IR_cm` columns. At 16 cm, D3 is fully bright and L switches from blinking to solid ON. At 49 cm, D3 goes dark and L starts blinking again. The two LEDs are independent below 16 cm: D3 stays full ON, L blinks.
5. **Compare sim vs board:** the columns should match at the same obstacle distance — ADC counts and mV are identical for the same AREF voltage, and the PWM/L logic is deterministic. If L blinks in the sim at 10 cm but is solid on the board, check that you rebuilt after the latest pull.

## Adding a part to the simulator (1 min)

When you need another sensor, servo, or display in the Wokwi diagram:

1. **Web editor (easiest):** Open [wokwi.com/projects/new/arduino-nano](https://wokwi.com/projects/new/arduino-nano), click the blue **+** button at the top, pick a part. Drag it, wire it (click one pin, then the target pin), then copy the new `parts` and `connections` entries from that web project's `diagram.json` (click it in the left panel) into your local `diagram.json`. Save. F1 → Wokwi: Start Simulator loads the new diagram.
2. **Text edit (full control):** Open `diagram.json` in VS Code. Add an object to the `"parts"` array (copy an existing one as a template, give it a unique `"id"` and `"type"` — see [Wokwi parts docs](https://docs.wokwi.com/)). Add wire entries to `"connections"`: each is `["sourceId:pinName", "targetId:pinName", "color", []]`. For example, to wire a servo PWM line to D9 and power it: `["servo1:PWM", "nano:9", "orange", []]`, `["servo1:V+", "nano:5V", "red", []]`, `["servo1:GND", "nano:GND.1", "black", []]`. Run `wokwi-cli lint` (needs `npm install -g wokwi-cli`) to catch typos.
3. **VS Code diagram editor (Hobby+ plan only):** click `diagram.json`, use the visual tools to add/drag/wire parts. The free Community plan can view the diagram but not edit it visually — you edit the JSON directly instead.

**Pins:** Hover over a pin in the web sim to see its name (e.g. `nano:A0`, `nano:13`, `nano:GND.1`). `$serialMonitor:RX` and `$serialMonitor:TX` are special: they let you wire a software serial port to the serial monitor (not needed for the Nano — it auto-wires the hardware UART).

**Example:** to add a second ultrasonic sensor on D10 (TRIG) and D11 (ECHO), insert into `"parts"`:
```json
{ "type": "wokwi-hc-sr04", "id": "us2", "top": 100, "left": 400, "attrs": { "distance": "50" } }
```
and into `"connections"`:
```json
["us2:TRIG", "nano:10", "orange", []],
["us2:ECHO", "nano:11", "yellow", []],
["us2:VCC", "nano:5V", "red", []],
["us2:GND", "nano:GND.2", "black", []]
```
then update the sketch to read pin 11 with `pulseIn()`.

**Gotcha:** If you add a part and the sim refuses to start, run `wokwi-cli lint .` (after `npm install -g wokwi-cli`) — it catches duplicate IDs, bad pin names, and missing parts.
