# FRDM-MCXC162 temperature recorder and Penguin anomaly detector

This project turns an NXP FRDM-MCXC162 board into a USB-connected temperature recorder with an RTC, circular flash history, browser dashboard, and the on-device Penguin anomaly detector. Temperature is measured by the board's P3T1755 sensor and shown in Celsius and Fahrenheit.

> **Public mirror:** Commits to this project on EmbeddedX `main` are mirrored to
> `models/penguin-temperature-anomaly-detector` in the Baby Model Zoo by
> `.github/workflows/sync-penguin-model.yml`. The model-zoo landing `README.md`
> is preserved; this document is published there as `EMBEDDEDX_PROJECT_README.md`.

The dashboard runs locally on macOS, Windows, and Linux. It communicates directly with the MCU-Link virtual serial port through the browser's Web Serial API; temperature data is not sent to a cloud service.

## What you need

- An FRDM-MCXC162 running the firmware in [`firmware`](firmware).
- A data-capable USB cable connected to the board's MCU-Link/debug USB connector.
- A current desktop version of Google Chrome or Microsoft Edge. Firefox and Safari do not currently provide the Web Serial API used by this dashboard.
- Node.js 24, matching this repository's supported Node version.
- Git if you want to clone the repository instead of downloading a ZIP file.

Only the local dashboard server requires Node.js. The dashboard itself has no additional runtime packages or cloud dependency.

## Install on macOS

1. Install Node.js 24 from [nodejs.org](https://nodejs.org/en/download).
2. Install [Google Chrome](https://www.google.com/chrome/) or [Microsoft Edge](https://www.microsoft.com/edge/download).
3. Open Terminal and clone the repository:

   ```sh
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   cd EmbeddedX_V2_0/projects/manufacturers/NXP/FRDM/MCXC162
   npm install
   ```

The installer verifies Node.js, the dashboard, firmware sources, production AI
sources, and educational examples. It does not flash the board or install
system-wide software.

No serial driver is normally required for the MCU-Link USB serial interface on a current macOS installation.

## Install on Windows

1. Install Node.js 24 from [nodejs.org](https://nodejs.org/en/download). Allow the installer to add Node to `PATH`.
2. Install [Google Chrome](https://www.google.com/chrome/) or [Microsoft Edge](https://www.microsoft.com/edge/download).
3. Install [Git for Windows](https://git-scm.com/download/win) if Git is not already available.
4. Open PowerShell and clone the repository:

   ```powershell
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   Set-Location "EmbeddedX_V2_0\projects\manufacturers\NXP\FRDM\MCXC162"
   npm install
   ```

The installer performs the same local validation on Windows and never programs
the attached board automatically.

Windows should install the standard USB serial device automatically when the board is attached. Let device installation finish before opening the dashboard.

## Install on Linux

1. Install Node.js 24 from [nodejs.org](https://nodejs.org/en/download) or through a trusted distribution-specific Node.js 24 package.
2. Install a current Google Chrome or Microsoft Edge build.
3. Clone the repository:

   ```sh
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   cd EmbeddedX_V2_0/projects/manufacturers/NXP/FRDM/MCXC162
   npm install
   ```

4. Ensure your user can access USB serial devices. On distributions that assign serial ports to the `dialout` group, run:

   ```sh
   sudo usermod -aG dialout "$USER"
   ```

   Log out completely and sign in again before continuing. Some distributions use a different serial-device group; check the ownership of the board's `/dev/ttyACM*` device if `dialout` does not apply.

Do not run Chrome, Edge, or the dashboard server as root.

## What the installer does

Running `npm install` invokes Penguin's zero-dependency installer. It:

- requires Node.js 24 or newer;
- confirms the dashboard, firmware, AI, and example files are present;
- checks the dashboard JavaScript syntax;
- reports the detected operating system, architecture, Node version, and
  project path; and
- prints the next commands.

It does **not** install USB drivers, download an MCU SDK, build or flash
firmware, change serial permissions, or modify global system settings. Run the
installer again at any time with `npm run setup`, or run validation explicitly
with `npm run verify`.

## Start the dashboard

From the MCXC162 project folder, the same command works on macOS, Windows, and
Linux:

```sh
npm start
```

The server binds only to `127.0.0.1`, prints the dashboard address, and opens the default browser at:

<http://localhost:4173>

If the browser does not open automatically, copy that address into Chrome or Edge. Keep the terminal window open while using the dashboard. Press `Ctrl+C` in the terminal to stop the server.

To prevent automatic browser opening, set `PENGUIN_DASHBOARD_OPEN=0` before
starting the server. To use another port, set `PENGUIN_DASHBOARD_PORT` to the
desired port number.

## Connect the board

1. Connect the board's MCU-Link/debug USB connector to the computer with a data-capable cable.
2. Close MCUXpresso serial terminals, other browser tabs, terminal programs, and any application that has the board's serial port open. Only one application can own the port at a time.
3. Start the local dashboard and open <http://localhost:4173> in Chrome or Edge.
4. Select **Connect board**.
5. In the browser device chooser, select the MCU-Link serial port associated with the FRDM-MCXC162, then select **Connect**.
6. The dashboard sends the computer's current Unix time to the board. The connection status should change to **Board connected · RTC synced**.

The browser requires a deliberate button click before it may request a serial port. A page cannot silently connect on startup.

## Recording with SW2 and SW3

The board always boots with recording stopped.

- **SW3 — Start recording:** begins 200 ms live temperature sampling, Penguin inference, dashboard telemetry, and interval-based flash recording.
- **SW2 — Stop recording:** stops live sampling and flash recording. The RTC and existing flash history remain intact.

The dashboard's **Start recording** and **Pause recording** button sends the same start/stop state changes over USB. Physical buttons continue to work while the dashboard is connected.

## LED status

| LED | Meaning |
|---|---|
| Red, steady | Recording is stopped. This is also the normal power-on state. |
| Green, steady | Recording is active. |
| Green off for 25 ms | A temperature record was successfully written to flash. The blink cadence follows the selected record interval. |
| Blue for 500 ms | Penguin classified the current temperature window as anomalous. Continued anomalies restart the blue timer. |

The blue anomaly indication is independent of the flash-recording interval because the model evaluates the temperature stream at the live sampling cadence.

## Dashboard controls

### Record interval

The record interval controls how often a timestamped temperature is stored in the board's 1,024-record circular flash logger. It does not control the 200 ms live graph or inference cadence.

Available settings range from one second to one hour. Set **Lock: Off** before moving the interval slider; set **Lock: On** to prevent accidental changes. The selected interval is stored on the device and restored after a board reset. Connecting the dashboard reads the board value rather than replacing it with a browser default.

### Baseline training

The **Baseline training** slider selects how many live samples Penguin uses for its initial baseline:

| Samples | Approximate time at 200 ms/sample |
|---:|---:|
| 32 | 6.4 seconds |
| 64 | 12.8 seconds |
| 96 | 19.2 seconds |
| 128 | 25.6 seconds |
| 160 | 32.0 seconds |
| 192 | 38.4 seconds |
| 224 | 44.8 seconds |
| 256 | 51.2 seconds |

Set the training-window **Lock: Off** before moving the slider; set **Lock: On** to prevent accidental retraining. Changing the unlocked slider immediately resets both in-RAM Penguin models and starts a new baseline using the selected sample count. The selected count persists on the device across reset. It does not change the autoencoder's fixed 16-sample input architecture.

Use a shorter baseline for quicker startup and a longer baseline when the normal environment contains more variation. Keep the board in representative normal conditions during baseline collection.

### AI controls

- **AI Filter:** enables or disables the MCU's three-sample moving average before samples enter the two models. Raw temperatures are still displayed and logged. The setting is retained on the board; changing it resets model training because the input distribution changes.
- **AI Anomaly:** enables or disables MCU anomaly inference and the blue LED. The setting is retained on the board. Re-enabling it starts a fresh model baseline.
- **Anomaly Alert:** enables the large red dashboard alert over the graph. This browser-only preference is retained in browser storage.
- **Anomaly Alarm:** enables browser vibration where the browser and computer support it. This browser-only preference is retained in browser storage.

The interval and training-window **Lock** states are browser-only preferences. Recording deliberately remains stopped after every MCU boot and is never automatically restored as active.

### History controls

- **Playback:** replaces the graph with all valid records currently in the board's flash FIFO.
- **Show all:** resets graph zoom and scrolling.
- **Export CSV:** downloads the currently loaded live or playback samples.
- **Reset logger:** permanently erases recorded temperature history after confirmation. It does not erase the selected record interval or baseline-training setting.
- Mouse wheel over the graph: zoom in or out.
- Drag the graph: scroll through the visible time range.

## Normal operating sequence

1. Attach the board and start the dashboard server.
2. Open the dashboard and connect the MCU-Link serial port.
3. Confirm that the dashboard reports RTC synchronization.
4. Choose the record interval and baseline-training length if the existing device values are not suitable.
5. Place the sensor in representative normal conditions while baseline training runs.
6. Press SW3 or **Start recording**.
7. Observe live temperature, AI state, flash count, and LED feedback.
8. Press SW2 or **Pause recording** before disconnecting if you want recording stopped.
9. Use **Playback** and **Export CSV** to retrieve stored history.

## Educational model examples

The [`model_examples`](model_examples/) folder contains dependency-free,
heavily commented Python implementations of Penguin's 16–8–4–8–16
autoencoder and eight-input linear predictor. Its
[`README`](model_examples/README.md) explains preprocessing, training,
ensemble scoring, guarded adaptation, and the relationship between the
educational code and the production C implementation in [`ai`](ai/).

## Firmware protocol

The dashboard uses newline-delimited JSON at 115200 baud, 8 data bits, no parity, and one stop bit. Commands sent to the MCU are:

```text
TIME <unix-seconds>
RATE <milliseconds>
TRAIN <32-256 samples>
AIFILTER <0|1>
AIANOMALY <0|1>
RECORD <0|1>
PLAY
CLEAR
STATUS
```

`TRAIN` resets the current in-RAM model state when the selected sample count changes. `AIFILTER` resets training when its value changes, and re-enabling `AIANOMALY` begins a new baseline. `RATE`, `TRAIN`, `AIFILTER`, and `AIANOMALY` update the retained board settings.

## Troubleshooting

### Web Serial unavailable

Use a current desktop Chrome or Edge window and open the dashboard from `http://localhost:4173`. Opening `index.html` directly, using an unsupported browser, or using a non-secure remote page can prevent Web Serial access.

### The board does not appear in the chooser

- Confirm the cable carries data, not only power.
- Use the MCU-Link/debug USB connector.
- Disconnect and reconnect the cable.
- Close MCUXpresso and other serial applications.
- On Linux, confirm serial-device group permissions and sign in again after a group change.

### The board appears but connection fails

Another process probably owns the serial port. Close other dashboard tabs, serial monitors, and IDE terminals, then try again. Only one browser tab should connect to the board.

### Dashboard connects but no graph appears

Recording is stopped at boot. Press SW3 or select **Start recording**. The green LED should turn on and live samples should begin appearing.

### RTC is not synchronized

Disconnect in the dashboard, reconnect, and allow the page to send the current computer time. The RTC continues from its last value while its power domain remains available, but the dashboard resynchronizes it at every successful connection.

### Port 4173 is already in use

Stop the other dashboard process or choose another port. For example:

```sh
PENGUIN_DASHBOARD_PORT=4174 npm start
```

In Windows PowerShell:

```powershell
$env:PENGUIN_DASHBOARD_PORT = "4174"
npm start
```

## Building and flashing firmware

The dashboard requires the matching firmware in [`firmware`](firmware). Firmware building uses the NXP MCUXpresso SDK, its `west` environment, and an Arm GNU embedded toolchain. Detailed build commands and the serial protocol are documented in the [firmware README](firmware/README.md).

Building does not automatically flash the board. Flashing is a separate hardware action using MCUXpresso for VS Code or NXP LinkServer. Confirm the selected probe and target before programming.

## Safety and limitations

Penguin detects deviations from a learned local pattern. It is not an absolute high/low temperature alarm and has not been clinically validated. Do not use it as the sole protection mechanism for people, animals, property, or safety-critical equipment.

## Copyright

Copyright © 2026 Richard Haberkern.
