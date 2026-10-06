# Model: Cat - EdgeAI Ambient Light State Detector

Baby Cat is a proposed tiny on-device model for classifying ambient-light conditions and short light transitions. It is intended for the MCXN236 EdgeAI demonstration and can provide context for display brightness, visual behavior, or application state.

> **Status:** project definition and documentation scaffold. A trained model, calibrated sensor contract, measured accuracy, and production firmware adapter are not yet included.

## Purpose

Cat converts a short history of light measurements into a stable state and confidence score. The initial candidate vocabulary is:

| Class | Meaning |
| --- | --- |
| `dark` | very low measured illumination |
| `dim` | low illumination |
| `normal` | ordinary operating illumination |
| `bright` | high illumination |
| `rapid_change` | illumination changed quickly |
| `shadow_event` | a short localized decrease or obstruction pattern |

Thresholds and class boundaries must be learned from calibrated data. The names do not imply universal lux limits.

## How Cat works

```text
Verified light measurement
          │
          ▼
Calibration and validity check
          │
          ▼
Short time-history buffer
          │
          ▼
Level, slope, variation, and change features
          │
          ▼
Tiny classifier or regressor
          │
          ▼
Light state, score, and confidence
```

The input path first converts raw readings into a documented physical or normalized unit. A bounded history captures both the current level and its temporal behavior. Deterministic features can include robust level, slope, range, variance, and recent differences. The model then combines those features into a state estimate.

## Candidate model architecture

Cat should begin with a simple baseline before a neural model is selected:

1. calibrated rule thresholds for a transparent reference;
2. a small MLP or logistic classifier over the time-history features;
3. only if needed, a tiny 1-D temporal model for transition shape.

This progression makes it possible to measure whether a learned model improves over deterministic logic while keeping flash, RAM, and latency bounded. The final input window, feature dimensions, quantization, and architecture must come from reproducible experiments.

## Training data and labels

Data should cover the full expected illumination range and transitions rather than only a few static rooms. Record sensor readings together with the intended application state and environmental context. Include:

- gradual dawn and dusk changes;
- fast lamp or display changes;
- partial occlusion and moving shadows;
- sensor noise and saturation;
- repeated measurements at each operating level;
- transitions that should remain in the previous state.

Labels need a documented hysteresis and transition policy. A state that changes every sample is not useful to the display supervisor, even if its per-sample classification accuracy looks high.

## Runtime decision flow

1. Read a measurement through the verified sensor interface.
2. Reject invalid, saturated, or unavailable samples.
3. Apply the recorded calibration and append the value to the bounded history.
4. Compute the exact features used during training.
5. Run the model at the selected cadence.
6. Apply confidence, hysteresis, and minimum-duration rules.
7. Publish the stable state to the application supervisor.

The model should not directly change display hardware until the application policy has accepted the state transition.

## Outputs

The proposed output record is:

```text
light_state       : enum
score             : normalized level or transition score
confidence        : fixed-point or float score
sequence_number   : monotonically increasing sample/window ID
captured_at_ms    : application timestamp
valid             : sensor/model validity flag
```

The exact ABI and numeric representation belong in the eventual firmware contract.

## Deployment requirements

- verified sensor interface, calibration, and measurement units;
- documented sample cadence and history length;
- fixed feature and model memory;
- model artifact with recorded quantization and class ordering;
- host-versus-target golden-vector test;
- measured latency and update cadence;
- defined behavior for saturation, missing samples, and sensor disconnect.

This README does not assert an ADC channel, bus route, lux threshold, board pin, or sensor part number.

## Limitations

Cat estimates the light conditions observed at its sensor. It does not measure every location in a room, identify the cause of a change, or replace a calibrated lighting instrument. Similar readings can represent different physical situations, and transitions may be ambiguous without application context.

## Related project

The MCXN236 integration scaffold is maintained in the [EmbeddedX project](https://github.com/telespial/EmbeddedX_V2_0/tree/main/projects/manufacturers/NXP/FRDM/MCXN236/models/baby-cat-light-state).
