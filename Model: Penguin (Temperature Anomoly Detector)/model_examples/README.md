# Penguin model examples

This directory contains small, dependency-free Python examples that explain
Penguin's two on-device models:

- [`autoencoder_example.py`](autoencoder_example.py) demonstrates the
  16–8–4–8–16 autoencoder and its reconstruction error.
- [`predictor_example.py`](predictor_example.py) demonstrates the eight-input
  linear predictor and its newest-sample prediction error.

These programs are educational references. They use readable Python lists and
ordinary floating-point arithmetic so every operation is visible. Production
firmware uses fixed-size C structures and static workspaces in [`../ai`](../ai).
The examples illustrate the same model roles and learning rules, but they are
not intended to produce bit-for-bit identical weights or scores.

## Why Penguin uses two models

The models observe different failure modes:

| Model | Input | Question | Error |
|---|---|---|---|
| Autoencoder | All 16 normalized temperatures | Does the complete recent shape resemble learned normal shapes? | Mean squared reconstruction error |
| Linear predictor | First 8 normalized temperatures | Does the newest value agree with the earlier portion of the window? | Absolute prediction error |

Penguin averages the two errors:

```text
score = (reconstruction_error + prediction_error) / 2
```

The autoencoder can notice an unfamiliar pattern distributed across the full
window. The predictor reacts directly when the newest measurement is
inconsistent with the earlier measurements. Combining them gives the MCU two
independent views while keeping memory and computation small.

## Shared preprocessing

Both models receive a 16-sample window in chronological order. Each window is
centered and scaled independently:

```text
center = mean(window)
scale  = max(standard_deviation(window), 0.05)
normalized[i] = (window[i] - center) / scale
```

Per-window normalization makes the models respond primarily to shape rather
than absolute room temperature. A gradual day-long temperature change can
remain normal if its short-window shape resembles the trained behavior.

The production service also checks the raw window range. A range no larger
than the P3T1755's `0.0625 °C` conversion step is treated as quantization and
cannot generate an anomaly. That system-level guard is intentionally separate
from the model examples.

## Autoencoder walkthrough

The autoencoder compresses 16 values to four latent values and expands them
back to 16:

```text
16 inputs → 8 ReLU → 4 ReLU → 8 ReLU → 16 linear outputs
```

Training presents a normal window as both input and desired output. Standard
backpropagation adjusts 356 parameters to reduce mean squared reconstruction
error. A familiar shape should later reconstruct with low error; an unfamiliar
jump, slope, or oscillation should reconstruct less accurately.

Production learning rate: `0.003`.

## Predictor walkthrough

The predictor is a single linear neuron:

```text
prediction = bias + sum(weight[i] * input[i]), i = 0..7
```

The target is normalized sample 15, the newest value. Online gradient updates
move the prediction toward the target:

```text
error = actual - prediction
bias += learning_rate * error
weight[i] += learning_rate * error * input[i]
```

Production learning rate: `0.01`. The model contains eight weights and one
bias, so its parameter storage is only 36 bytes with 32-bit floats.

## Baseline and guarded adaptation

Production Penguin trains both models during the selected 32–256 sample
baseline. It learns watch and anomaly thresholds from the mean and standard
deviation of the combined baseline score. After baseline training, it adapts
only when a window scores below the watch threshold. Watch and anomaly windows
are scored but never learned as normal.

This is important: online learning without that guard could absorb a sustained
fault and gradually stop reporting it.

## Run the examples

Python 3.10 or newer is sufficient; no packages are required:

```sh
python3 autoencoder_example.py
python3 predictor_example.py
```

On Windows, use `py` if that is how Python is installed:

```powershell
py autoencoder_example.py
py predictor_example.py
```

Each program trains on deterministic gentle temperature patterns, then prints
scores for a normal window and a window containing a sudden final change. The
exact numbers are less important than the expected relationship: the changed
window should have the larger error.

## Production sources

- [`../ai/baby_temp_ae.c`](../ai/baby_temp_ae.c) — autoencoder forward pass
- [`../ai/baby_temp_predictor.c`](../ai/baby_temp_predictor.c) — predictor
- [`../ai/baby_temp_service.c`](../ai/baby_temp_service.c) — normalization,
  training, threshold learning, ensemble scoring, and guarded adaptation
- [`../firmware/main.c`](../firmware/main.c) — sensor, USB, buttons, LEDs, RTC,
  and flash-logger integration
