# Model: Penguin - Temperature Anomaly Detector Demo
## FRDM MCXC162 Development Board

**Penguin learns the normal temperature behavior around an FRDM-MCXC162 and alerts when that behavior changes.**

It runs entirely on the microcontroller. No cloud connection is required for sampling, training, inference, scoring, or alerting.

Penguin combines two small models:

- An **autoencoder** recognizes unusual shapes in recent temperature history.
- A **linear predictor** recognizes a newest reading that does not agree with earlier readings.

When their combined score crosses the learned anomaly threshold, the board flashes its blue LED and reports the anomaly to the browser dashboard.

> [!CAUTION]
> Penguin is an experimental environmental anomaly detector. It is not a medical device, a clinical temperature monitor, or a replacement for an independently validated high/low temperature alarm.

## Concept and implementation of the EdgeAI models and code by: Richard Haberkern

## Contents

- [How Penguin works](#how-penguin-works)
- [Model 1: autoencoder](#model-1-autoencoder)
- [Model 2: linear predictor](#model-2-linear-predictor)
- [Training and anomaly decisions](#training-and-anomaly-decisions)
- [Install and run](#install-and-run)
- [Board controls and LEDs](#board-controls-and-leds)
- [Dashboard controls](#dashboard-controls)
- [Technical reference](#technical-reference)
- [Limitations](#limitations)

## At a glance

- **Board:** NXP FRDM-MCXC162
- **Sensor:** onboard P3T1755
- **Live sampling:** approximately every 200 ms while recording
- **Model input:** latest 16 samples, representing about 3.2 seconds
- **Baseline:** adjustable from 32 to 256 samples; default 128
- **Alert:** USB telemetry and a 500 ms blue LED indication
- **Memory:** fixed-size static buffers with no model heap allocation

## How Penguin works

```text
P3T1755 temperature sensor
           │
           ▼
Optional 3-sample AI filter
           │
           ▼
16-sample rolling window
           │
           ▼
Normalize the window
           │
      ┌────┴────┐
      ▼         ▼
 Autoencoder  Predictor
      │         │
      ▼         ▼
Reconstruction Prediction
    error         error
      └────┬────┘
           ▼
 Average both errors
           │
           ▼
Ready  /  Watch  /  Anomaly
                         │
                         ▼
                  Blue LED + USB alert
```

The optional filter affects only the input sent to the models. The dashboard and flash logger continue to use the raw temperature measurement.

### The 16-sample input window

Penguin keeps the latest 16 readings in chronological order. At the normal 200 ms sampling cadence, the window covers approximately 3.2 seconds.

Each window is normalized before it reaches either model:

```text
center   = mean(window)
scale    = standard deviation(window)
input[i] = (window[i] - center) / scale
```

The minimum scale is `0.05`, which prevents division by a very small value when the temperature is nearly constant.

Normalization makes Penguin sensitive to the *shape* of a change—such as a jump, slope, or oscillation—rather than simply reacting to the absolute room temperature.

The P3T1755 reports temperature in `0.0625 °C` increments. A complete window spanning no more than one of those increments is treated as sensor quantization and cannot raise an anomaly. Larger excursions continue through both models normally. This prevents a flat training baseline followed by a single normal conversion-count change from becoming a false alert.

## Model 1: autoencoder

### What it asks

> Does the complete recent temperature pattern look normal?

The autoencoder learns to reproduce normal 16-sample temperature windows. If an incoming pattern resembles its baseline, reconstruction is accurate. If the pattern is unfamiliar, reconstruction becomes less accurate.

### Network shape

```text
16 normalized samples
         │
         ▼
  8-unit encoder
         │
         ▼
4-unit compressed representation
         │
         ▼
  8-unit decoder
         │
         ▼
16 reconstructed samples
```

The three internal layers use ReLU activation. The final reconstruction layer is linear.

### What it measures

The model calculates mean squared error across the complete window:

```text
reconstruction error = Σ(input[i] - reconstruction[i])² / 16
```

A low error means the shape is familiar. A high error means the recent pattern differs from what the model learned.

### How it learns

- Weights begin from reproducible pseudo-random values generated from seed `7`.
- Baseline learning uses online backpropagation.
- The learning rate is `0.003`.
- After baseline training, adaptation continues only for samples below the watch threshold.
- Watch and anomaly samples are never used as self-training targets.

That final guard helps prevent a new anomaly from immediately becoming learned as normal.

<details>
<summary><strong>Autoencoder parameter details</strong></summary>

| Layer | Weights | Biases | Activation |
|---|---:|---:|---|
| 16 → 8 | 128 | 8 | ReLU |
| 8 → 4 | 32 | 4 | ReLU |
| 4 → 8 | 32 | 8 | ReLU |
| 8 → 16 | 128 | 16 | Linear |
| **Total** | **320** | **36** | **356 floats** |

The parameters occupy 1,424 bytes as single-precision floats. The fixed forward-pass workspace stores the 8-, 4-, and 8-unit internal layers plus the 16 reconstructed values.

</details>

## Model 2: linear predictor

### What it asks

> Is the newest temperature consistent with the earlier part of this window?

The predictor uses the first eight normalized values in the 16-sample window to estimate the newest value at position 15.

```text
First 8 values ──► weighted sum + bias ──► predicted newest value
                                                    │
Actual newest value ────────────────────────────────┘
                                                    │
                                                    ▼
                                            Absolute error
```

### What it measures

```text
prediction       = bias + Σ(weight[i] × input[i]), i = 0…7
prediction error = abs(actual newest value - prediction)
```

The model has only eight weights and one bias: nine floats, or 36 bytes of parameters.

### How it learns

The predictor adapts online with a learning rate of `0.01`:

```text
error     = actual - prediction
bias     += learning_rate × error
weight[i] += learning_rate × error × input[i]
```

Like the autoencoder, it trains during baseline collection and later adapts only when the combined score remains below the watch threshold.

## Training and anomaly decisions

### Adjustable baseline

The dashboard's **Baseline training** slider selects 32–256 samples:

- **32 samples:** approximately 6.4 seconds
- **64 samples:** approximately 12.8 seconds
- **128 samples:** approximately 25.6 seconds and the default
- **256 samples:** approximately 51.2 seconds

Intermediate settings of 96, 160, 192, and 224 samples are also available.

Changing the setting resets both models and begins a new baseline. It does **not** change the fixed 16-sample model input.

Use a shorter baseline for faster startup. Use a longer baseline when normal conditions naturally vary. Keep the sensor in representative normal conditions while the baseline is collected.

### One combined score

The models contribute equally:

```text
anomaly score = (reconstruction error + prediction error) / 2
```

- The autoencoder contributes evidence about the complete pattern.
- The predictor contributes evidence about the newest reading.

### Learned thresholds

During baseline collection, Penguin measures the mean and variation of its combined error:

```text
watch threshold   = max(0.08, mean error + 2 × standard deviation)
anomaly threshold = max(0.20, mean error + 3 × standard deviation)
```

The minimum values prevent an extremely quiet baseline from producing thresholds too close to zero.

The resulting states are:

- **Collecting:** fewer than 16 samples are available.
- **Training:** Penguin is learning its selected baseline.
- **Ready:** score is below the watch threshold.
- **Watch:** score is elevated but below the anomaly threshold.
- **Anomaly:** score meets or exceeds the anomaly threshold.

## Install and run

The runnable firmware and dashboard are maintained in the [EmbeddedX FRDM-MCXC162 project](https://github.com/telespial/EmbeddedX_V2_0/tree/main/projects/manufacturers/NXP/FRDM/MCXC162).

### Requirements

- FRDM-MCXC162 programmed with the reference firmware
- Data-capable USB cable connected to the MCU-Link/debug USB connector
- Node.js 24
- Current desktop Google Chrome or Microsoft Edge
- Git, unless you download the repository as a ZIP file

> [!NOTE]
> Firefox and Safari do not currently provide the Web Serial API required by this dashboard.

### 1. Install the application

<details open>
<summary><strong>macOS</strong></summary>

1. Install Node.js 24 from [nodejs.org](https://nodejs.org/en/download).
2. Install Chrome or Edge.
3. Open Terminal:

   ```sh
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   cd EmbeddedX_V2_0
   node --version
   ```

The version should begin with `v24`. macOS normally recognizes MCU-Link serial without another driver.

</details>

<details>
<summary><strong>Windows</strong></summary>

1. Install Node.js 24 from [nodejs.org](https://nodejs.org/en/download) and allow it to be added to `PATH`.
2. Install Chrome or Edge and, if necessary, [Git for Windows](https://git-scm.com/download/win).
3. Open PowerShell:

   ```powershell
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   Set-Location EmbeddedX_V2_0
   node --version
   ```

The version should begin with `v24`. Let Windows finish installing the USB serial device after first attaching the board.

</details>

<details>
<summary><strong>Linux</strong></summary>

1. Install Node.js 24 and a current Chrome or Edge build.
2. Open a terminal:

   ```sh
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   cd EmbeddedX_V2_0
   node --version
   ```

3. If your distribution assigns serial devices to `dialout`, grant your user access:

   ```sh
   sudo usermod -aG dialout "$USER"
   ```

Log out completely and sign in again. Some distributions use another group; check the ownership of the board's `/dev/ttyACM*` device. Do not run the browser or dashboard as root.

</details>

### 2. Start the dashboard

From the EmbeddedX repository root on macOS, Windows, or Linux:

```sh
node scripts/serve-dashboard.mjs
```

The server opens the default browser at <http://localhost:4173>. If it does not open automatically, enter that address manually in Chrome or Edge.

Keep the terminal open while using Penguin. Press `Ctrl+C` to stop the server.

### 3. Connect the board

1. Attach the MCU-Link/debug USB connector with a data-capable cable.
2. Close IDE serial terminals, other dashboard tabs, and anything else using the board's serial port.
3. Select **Connect board**.
4. Choose the MCU-Link serial port.
5. Wait for **Board connected · RTC synced** in the upper-right corner.

The browser requires a deliberate click before it may request access to a serial device. The dashboard synchronizes the board RTC after every successful connection.

## Board controls and LEDs

The firmware always boots with recording stopped.

### Buttons

- **SW3 starts recording.** It also starts live graph samples and Penguin inference.
- **SW2 stops recording.** Existing flash history is not erased.

The dashboard's **Start recording** and **Pause recording** button mirrors SW3 and SW2.

### LEDs

- **Red, steady:** recording is stopped.
- **Green, steady:** recording is active.
- **Green off for 25 ms:** one temperature record was written to flash.
- **Blue for 500 ms:** Penguin detected an anomaly.

The green blink follows the selected record interval. Blue anomaly detection continues at the faster live sampling cadence and does not wait for a flash record.

## Dashboard controls

### Record interval

Controls how often the board stores a temperature in its 1,024-record circular logger. Set **Lock: Off** to change it and **Lock: On** to prevent accidental movement. It does not change graph or inference speed.

### Baseline training

Controls how many samples are used to establish normal behavior. Changing it resets both models and begins a new baseline. The selection is retained through MCU resets while the RTC power domain remains powered.

### AI controls

- **AI Filter:** applies a three-sample moving average before model inference.
- **AI Anomaly:** enables or disables browser handling of MCU anomaly states.
- **Anomaly Alert:** controls the large red graph alert.
- **Anomaly Alarm:** controls browser vibration where supported.

### Recorded data

- **Playback:** loads flash history onto the graph.
- **Show all:** resets zoom and scrolling.
- **Export CSV:** downloads the currently displayed samples.
- **Reset logger:** permanently erases temperature history after confirmation.
- **Mouse wheel:** zooms the graph.
- **Drag:** scrolls through recorded history.

## Technical reference

<details>
<summary><strong>Runtime and telemetry</strong></summary>

Penguin runs approximately every 200 ms while recording. The flash-record interval is independent; a five-second interval still allows about 25 inference samples between records.

The firmware emits newline-delimited JSON at 115200 baud. Important AI fields are:

- `ai_filter` — whether MCU-side filtering is enabled
- `ai_state` — collecting, training, ready, watch, or anomaly
- `ai_error` — autoencoder reconstruction error
- `ai_prediction_error` — predictor absolute error
- `ai_score` — combined score
- `training_samples` — selected baseline length
- `training_steps` — completed model updates

</details>

<details>
<summary><strong>Training and persistence lifecycle</strong></summary>

Model parameters live in RAM. Stopping and restarting recording without resetting the MCU retains the trained models. Resetting or power-cycling initializes deterministic weights and repeats baseline training.

Serialization and checksum code exists, but production firmware does not currently save or restore trained parameters. The baseline-length setting is retained separately in the RTC domain. Complete power-loss persistence of this setting is not claimed.

Flash-recorded temperature history is independent of model state.

</details>

<details>
<summary><strong>Source code and tests</strong></summary>

- [Model service and ensemble logic](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/ai/baby_temp_service.c)
- [Autoencoder](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/ai/baby_temp_ae.c)
- [Linear predictor](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/ai/baby_temp_predictor.c)
- [Firmware integration](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/firmware/main.c)
- [Host-side tests](https://github.com/telespial/EmbeddedX_V2_0/tree/main/projects/manufacturers/NXP/FRDM/MCXC162/ai)
- [Complete operating guide](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/README.md)

Host tests cover forward propagation, component errors, service training, synthetic anomaly response, and model serialization checks. Hardware testing is still required for sensor timing, false-alert rate, response latency, LED timing, and complete power-loss behavior.

</details>

## Troubleshooting

### The browser says Web Serial is unavailable

Use current desktop Chrome or Edge and open <http://localhost:4173>. Do not open `index.html` directly.

### The board does not appear

Confirm that the cable carries data and is connected to the MCU-Link/debug connector. Close MCUXpresso serial terminals and other dashboard tabs. On Linux, verify serial-device group permissions.

### The dashboard connects but the graph is empty

Recording is stopped at boot. Press SW3 or select **Start recording**. The green LED should turn on.

### Connection fails after selecting the board

Another application probably owns the serial port. Only one browser tab or serial application can connect at a time.

## Limitations

- Penguin recognizes changes from learned behavior; it does not enforce absolute safe temperature limits.
- A disturbance during baseline collection can influence what the models learn as normal.
- Training uses the current device and environment, not a broad population dataset.
- The AI filter can reduce sensor noise but may soften and delay a real abrupt change.
- Trained model parameters do not survive reset or complete power loss.
- Thresholds and learning rates still require characterization across boards and environments.
- Detection quality has not been clinically validated.

For safety-related use, add independent high/low limits, sensor-fault handling, watchdog behavior, and a validated alert path.

## License

Released under the repository's [MIT License](../LICENSE).

Copyright © 2026 Richard Haberkern.
