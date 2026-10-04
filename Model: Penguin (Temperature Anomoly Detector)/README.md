# Model: Penguin — Temperature Anomaly Detector

Penguin is a compact, adaptive temperature-anomaly detector designed to run entirely on a resource-constrained microcontroller. The current reference implementation runs on the NXP FRDM-MCXC162 development board and observes its onboard P3T1755 temperature sensor.

The model combines two independent views of the same temperature history:

1. A neural-network **autoencoder** asks whether the *shape* of the latest temperature window resembles the behavior learned during baseline training.
2. An adaptive **linear predictor** asks whether the newest reading agrees with what the earlier portion of that window predicts.

Their errors are combined into a single anomaly score. A detected anomaly is reported over USB and flashes the board's blue LED. All sampling, normalization, training, inference, scoring, and classification happen on the MCU; no cloud connection is required.

> **Important:** Penguin is an experimental environmental anomaly detector. It is not a medical device, a clinical infant monitor, or a substitute for an independently validated absolute-temperature alarm.

## Project goals

- Detect sudden or structurally unusual changes in a temperature stream.
- Learn the local environment on the MCU instead of requiring a cloud-trained model.
- Operate without dynamic memory allocation.
- Keep computation and model storage small enough for a constrained Cortex-M target.
- Keep anomaly inference independent of the flash-recording interval.
- Expose understandable component errors rather than only a binary alarm.

## Reference system

| Item | Reference implementation |
|---|---|
| Target | NXP FRDM-MCXC162 |
| Temperature sensor | Onboard P3T1755 |
| Temperature sampling | Nominally every 200 ms while recording |
| Analysis window | 16 samples, approximately 3.2 seconds |
| Baseline period | Adjustable from 32 to 256 samples; default 128 |
| Model 1 | 16–8–4–8–16 autoencoder |
| Model 2 | Eight-input linear predictor |
| Combined score | Mean of reconstruction and prediction errors |
| Alert output | USB telemetry plus a 500 ms blue LED indication |
| Memory policy | Fixed-size static buffers; no model heap allocation |

## Install and run the reference application

The runnable firmware and browser dashboard are maintained in the [EmbeddedX repository](https://github.com/telespial/EmbeddedX_V2_0/tree/main/projects/manufacturers/NXP/FRDM/MCXC162). The dashboard runs locally on macOS, Windows, and Linux and sends no temperature data to a cloud service.

### Requirements

- An FRDM-MCXC162 programmed with the reference firmware.
- A data-capable USB cable attached to the MCU-Link/debug USB connector.
- Node.js 24.
- A current desktop Google Chrome or Microsoft Edge browser. Firefox and Safari do not currently expose the Web Serial API required by the dashboard.
- Git, unless the repository is downloaded as a ZIP file.

### macOS installation

1. Install Node.js 24 from [nodejs.org](https://nodejs.org/en/download).
2. Install Chrome or Edge.
3. Open Terminal and run:

   ```sh
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   cd EmbeddedX_V2_0
   node --version
   ```

The Node version should begin with `v24`. A current macOS installation normally recognizes the MCU-Link serial interface without an additional driver.

### Windows installation

1. Install Node.js 24 from [nodejs.org](https://nodejs.org/en/download) and allow the installer to add Node to `PATH`.
2. Install Chrome or Edge and, if needed, [Git for Windows](https://git-scm.com/download/win).
3. Open PowerShell and run:

   ```powershell
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   Set-Location EmbeddedX_V2_0
   node --version
   ```

The Node version should begin with `v24`. Allow Windows to finish installing the board's USB serial device after first attachment.

### Linux installation

1. Install Node.js 24 and a current Chrome or Edge build.
2. Clone the repository:

   ```sh
   git clone https://github.com/telespial/EmbeddedX_V2_0.git
   cd EmbeddedX_V2_0
   node --version
   ```

3. Ensure that your account can access USB serial devices. On distributions using the `dialout` group:

   ```sh
   sudo usermod -aG dialout "$USER"
   ```

   Log out completely and sign in again. If the board's `/dev/ttyACM*` device belongs to another group, use the group configured by that distribution. Do not run the browser or dashboard as root.

### Start the dashboard

From the EmbeddedX repository root on any supported operating system, run:

```sh
node scripts/serve-dashboard.mjs
```

The equivalent npm command is:

```sh
npm run dashboard
```

The local server binds to `127.0.0.1` and opens <http://localhost:4173>. If the browser does not open automatically, enter that address manually in Chrome or Edge. Keep the terminal open while using the application; press `Ctrl+C` to stop it.

### Connect the device

1. Attach the board through its MCU-Link/debug USB connector.
2. Close IDE serial terminals, other dashboard tabs, and any program using the board's serial port.
3. Select **Connect board** in the dashboard.
4. Choose the MCU-Link serial port in the browser's device chooser.
5. Confirm that the upper-right status changes to **Board connected · RTC synced**.

The dashboard synchronizes the RTC from the computer after every successful connection. Browser security requires a user click before a serial-device chooser can open.

### SW2, SW3, and LEDs

The firmware boots stopped:

- **SW3** starts recording, live graph telemetry, and Penguin inference.
- **SW2** stops recording and live sampling without erasing history.
- **Red steady** means recording is stopped.
- **Green steady** means recording is active.
- **Green off for 25 ms** marks a successful flash record. This blink follows the selected record interval.
- **Blue for 500 ms** indicates a Penguin anomaly. Continued anomalies restart the timer.

The dashboard's start/pause button mirrors SW3 and SW2.

### Configure and use the application

- Unlock the record-interval slider with **Lock: Off**, choose a value from one second to one hour, and optionally relock it. The MCU retains the setting and supplies it to the dashboard at connection time.
- Choose a **Baseline training** length from 32 to 256 samples. At the 200 ms inference cadence, this is approximately 6.4 to 51.2 seconds. Changing it resets both in-RAM models and begins a new baseline. The choice is retained on the device across reset while the RTC power domain remains available.
- Keep the sensor in representative normal conditions while the baseline is collected.
- Enable **AI Filter** to apply a three-sample moving average to model input. Displayed and logged measurements remain raw.
- Use **Playback** to load flash history, the mouse wheel to zoom, dragging to scroll, **Show all** to reset the view, and **Export CSV** to download the displayed data.
- **Reset logger** permanently erases temperature history after confirmation; it does not clear the retained interval or training selection.

The full operational and troubleshooting guide is also available in the [FRDM-MCXC162 project README](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/README.md).

## Signal and inference pipeline

```mermaid
flowchart LR
    S[P3T1755 temperature] --> F{AI filter enabled?}
    F -->|No| R[Raw sample]
    F -->|Yes| M[3-sample moving average]
    R --> W[16-sample rolling window]
    M --> W
    W --> N[Per-window normalization]
    N --> A[Autoencoder]
    N --> P[Linear predictor]
    A --> RE[Reconstruction error]
    P --> PE[Prediction error]
    RE --> C[Average the two errors]
    PE --> C
    C --> T{Adaptive thresholds}
    T -->|Below watch| OK[Ready]
    T -->|Watch threshold| WATCH[Watch]
    T -->|Anomaly threshold| ALERT[Anomaly + blue LED]
```

The optional AI filter affects only the samples supplied to the models. The temperature displayed by the dashboard and written to the flash logger remains the raw sensor measurement.

## Input window and normalization

Penguin stores the latest 16 samples in a ring buffer. Once the buffer is full, it puts them in chronological order and normalizes that window:

```text
center = mean(window)
scale  = standard deviation(window)
input[i] = (window[i] - center) / scale
```

The scale has a lower bound of `0.05` to avoid division by a very small number when temperature is nearly constant.

Window-by-window normalization makes the models sensitive to pattern shape—jumps, slopes, oscillations, and other changes—while reducing sensitivity to the absolute room temperature. This is useful for behavioral anomaly detection, but it also means Penguin is not an absolute high-temperature or low-temperature safety alarm.

## Model 1: neural autoencoder

### Purpose

The autoencoder learns how a normal 16-sample temperature pattern looks. It compresses the normalized window into a small latent representation and attempts to reconstruct the original window. A pattern unlike the learned baseline is harder to reconstruct and therefore produces a larger error.

### Architecture

```text
16 normalized temperatures
          ↓
8-unit encoder layer, ReLU
          ↓
4-unit latent layer, ReLU
          ↓
8-unit decoder layer, ReLU
          ↓
16-value linear reconstruction
```

The network contains 356 single-precision parameters:

| Layer | Weights | Biases | Activation |
|---|---:|---:|---|
| 16 → 8 | 128 | 8 | ReLU |
| 8 → 4 | 32 | 4 | ReLU |
| 4 → 8 | 32 | 8 | ReLU |
| 8 → 16 | 128 | 16 | Linear |
| **Total** | **320** | **36** | **356 floats / 1,424 bytes** |

The forward-pass workspace is also statically allocated. It holds the intermediate 8-, 4-, and 8-unit layers plus the 16 reconstructed values.

### Reconstruction error

The autoencoder contribution is mean squared error across all 16 normalized values:

```text
reconstruction_error = Σ(input[i] - reconstruction[i])² / 16
```

This error captures disagreement across the entire recent pattern rather than looking only at the newest reading.

### On-device training

Weights start from deterministic pseudo-random values generated from seed `7`, making initialization reproducible. During baseline learning, the network performs online backpropagation with a learning rate of `0.003` on each eligible window.

After baseline training, the autoencoder continues guarded adaptation only when the current combined anomaly score is below the watch threshold. Suspect and anomalous windows are scored but are not used as training targets. This guard reduces the chance that a new anomaly will immediately be learned as normal.

## Model 2: adaptive linear predictor

### Purpose

The predictor provides a second kind of evidence. Instead of reconstructing the entire window, it estimates the newest normalized temperature from earlier samples. A sudden change that violates this learned relationship produces a large prediction error.

### Architecture

The model is intentionally small:

```text
prediction = bias + Σ(weight[i] × input[i]), i = 0…7
```

It has eight single-precision weights and one bias: nine floats, or 36 bytes of parameters.

In the current implementation, the predictor consumes positions `0` through `7` of the normalized 16-sample chronological window and is trained against position `15`, the newest sample. In other words, it uses the first half of the current window to estimate its final value.

### Prediction error

Its contribution to the anomaly score is absolute error:

```text
prediction_error = abs(actual_newest_value - predicted_value)
```

Absolute error keeps the contribution non-negative and treats unexpected upward and downward changes equally.

### On-device training

The predictor uses online gradient updates with a learning rate of `0.01`:

```text
error = actual - prediction
bias += learning_rate × error
weight[i] += learning_rate × error × input[i]
```

It follows the same lifecycle as the autoencoder: initial baseline training followed by guarded adaptation only for windows considered normal.

## Ensemble anomaly decision

The two model outputs are given equal weight:

```text
anomaly_score = (reconstruction_error + prediction_error) / 2
```

Using both models lets Penguin respond to two different failure modes:

- The autoencoder detects an unusual overall window shape.
- The predictor detects disagreement between earlier history and the newest reading.

Neither component alone determines the state. Classification is based on the combined score and thresholds learned during the baseline period.

### Adaptive thresholds

During the selected baseline-training period, Penguin maintains the running mean and sample variance of the combined error. It then calculates:

```text
watch_threshold   = max(0.08, mean_error + 2 × standard_deviation)
anomaly_threshold = max(0.20, mean_error + 3 × standard_deviation)
```

The fixed minimums prevent an extremely quiet baseline from producing thresholds that are too close to zero.

| State | Meaning |
|---|---|
| `untrained` | Models have been initialized but have not received data. |
| `collecting` | Fewer than 16 samples are available, so no complete window exists. |
| `training` | The baseline is being learned for the selected 32–256 sample period. |
| `ready` | The score is below the watch threshold. |
| `watch` | The score is at or above the watch threshold but below the anomaly threshold. |
| `anomaly` | The score is at or above the anomaly threshold. |
| `error` | Reserved for model-service errors. |

## Runtime behavior

The MCU performs temperature sampling and AI processing at a nominal 200 ms cadence while recording is active. This inference cadence is separate from the configurable flash-recording interval. For example, a five-second logger interval still allows the models to analyze approximately 25 sensor samples between stored records.

When the combined model state is `anomaly`:

- USB sample telemetry reports `ai_state: "anomaly"`.
- The telemetry includes reconstruction error, prediction error, and combined score.
- The blue LED is switched on for 500 ms.
- Additional anomalous samples restart the blue LED timer.

The green LED continues to represent recording status, and the red LED represents stopped recording. Those indicators are independent of the blue anomaly alert.

## AI input filter

The optional filter is a three-sample moving average implemented on the MCU:

```text
filtered_temperature = mean(latest three raw sensor samples)
```

At a nominal 200 ms sample cadence, it smooths roughly the latest 600 ms of measurements. Filtering can reduce false alerts caused by single-sample noise, but it can also reduce the magnitude and delay the detection of a genuine abrupt change. The filter setting therefore changes the model input, not merely the dashboard display.

## Training and persistence lifecycle

The model parameters live in MCU RAM. Serialization and checksum routines exist for packaging the autoencoder, predictor, and training-step count, but the current production firmware does not call them to save or restore a trained model. The selected baseline length is retained separately as a device setting.

Consequences:

- Stopping and restarting recording without resetting the MCU retains the learned model.
- Resetting or power-cycling the MCU initializes new deterministic weights and repeats baseline training.
- Flash-recorded temperature history survives independently of the in-RAM model state.

A future persistence implementation should store model data in a dedicated, wear-managed region and validate its version, size, and checksum before restoration.

## Telemetry fields

The reference firmware emits newline-delimited JSON over USB serial at 115200 baud. AI-related sample fields include:

| Field | Description |
|---|---|
| `ai_filter` | Whether the MCU-side three-sample filter is enabled. |
| `ai_state` | Current lifecycle or classification state. |
| `ai_error` | Autoencoder reconstruction mean squared error. |
| `ai_prediction_error` | Linear predictor absolute error. |
| `ai_score` | Equal-weight mean of the two errors. |

## Source implementation

The current reference source is maintained in the EmbeddedX repository:

- [Model service and ensemble logic](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/ai/baby_temp_service.c)
- [Autoencoder implementation](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/ai/baby_temp_ae.c)
- [Linear predictor implementation](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/ai/baby_temp_predictor.c)
- [MCXC162 firmware integration](https://github.com/telespial/EmbeddedX_V2_0/blob/main/projects/manufacturers/NXP/FRDM/MCXC162/firmware/main.c)
- [Host-side model tests](https://github.com/telespial/EmbeddedX_V2_0/tree/main/projects/manufacturers/NXP/FRDM/MCXC162/ai)

## Validation strategy

The model package includes host-side tests for:

- Autoencoder forward propagation and reconstruction error.
- Predictor output and absolute prediction error.
- Service training and status reporting.
- A large synthetic temperature excursion producing an anomaly.
- Model serialization, restoration, and checksum rejection.

Hardware validation should additionally measure real sensor cadence, baseline behavior, anomaly response latency, false-alert rate, and blue LED timing. Host tests do not replace validation on the target board.

## Known limitations

- Per-window normalization emphasizes pattern changes rather than absolute temperature limits.
- The selected first 32–256 samples form the baseline; a disturbance during startup can influence learned behavior.
- Training data comes from the current device and environment, not from a diverse population dataset.
- The three-sample filter trades some response speed for noise reduction.
- The current firmware does not persist trained parameters across reset or complete power loss.
- Detection quality has not been clinically validated.
- Thresholds and learning rates require empirical characterization across sensors, boards, and environments.

For safety-related use, pair Penguin with independent deterministic high/low limits, sensor fault detection, watchdog behavior, and a validated alert path.

## License

This project is released under the repository's [MIT License](../LICENSE).

Copyright © 2026 Richard Haberkern.
