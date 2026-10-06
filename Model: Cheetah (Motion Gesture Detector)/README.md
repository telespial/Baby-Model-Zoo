# Model: Cheetah - EdgeAI Motion and Gesture Detector

Baby Cheetah is a proposed tiny on-device accelerometer model for recognizing short motion patterns and gestures. It is intended for the MCXN236 EdgeAI demonstration, where motion inference shares one Cortex-M33 with the display and other specialist models.

> **Status:** project definition and documentation scaffold. A trained model, verified sensor configuration, measured accuracy, and production firmware adapter are not yet included.

## Purpose

Cheetah converts a short three-axis acceleration window into a motion class and confidence score. The initial candidate vocabulary is:

| Class | Meaning |
| --- | --- |
| `still` | no meaningful motion |
| `tilt_left` / `tilt_right` | sustained orientation change |
| `tilt_forward` / `tilt_back` | sustained orientation change on the second axis |
| `shake` | repeated alternating motion |
| `tap` | short impact-like event |
| `flick` | brief directional acceleration |
| `impact` | larger abrupt acceleration event |

Axis names, orientation, ranges, and class boundaries must be verified against the physical board and dataset. The names alone do not define a sensor coordinate system.

## How Cheetah works

```text
Three-axis accelerometer
          │
          ▼
Timestamped sample window
          │
          ▼
Calibration, gravity, and motion features
          │
          ▼
Tiny temporal classifier
          │
          ▼
Gesture class and confidence
```

The capture layer preserves axis order and timing. Preprocessing can remove bias, normalize the range, and derive magnitude or axis-specific features. A compact model then recognizes patterns across the window. The application layer performs event gating so one gesture does not produce multiple actions.

## Candidate model architecture

The first benchmark should compare:

- a feature-based MLP using axis statistics, differences, magnitude, and peak timing;
- a small 1-D convolutional model over the calibrated X/Y/Z sequence;
- a deterministic baseline for stillness, tilt, and impact.

A temporal model is useful only if it improves held-out gesture recognition enough to justify its RAM, flash, and latency cost. The final window duration, output data rate, feature dimensions, quantization, and architecture must be measured and recorded.

## Training data and labels

Collect repeated examples from multiple users and board orientations. Every recording should preserve the sensor configuration, axis order, range, output data rate, timestamp behavior, and gesture protocol. Include:

- no-motion and ordinary handling negatives;
- slow tilts in each documented direction;
- shake, tap, flick, and impact at varied strengths;
- gesture timing variation;
- incidental movements and false-positive scenarios;
- sensor saturation and dropped-sample cases.

The test split should hold out users and sessions where possible. Report a confusion matrix, per-class recall, false-trigger rate during ordinary handling, and detection latency.

## Runtime decision flow

1. Read timestamped samples through the verified sensor path.
2. Check sample validity and preserve the documented axis order.
3. Fill a bounded rolling window.
4. Apply the exact calibration and preprocessing used in training.
5. Run the model when the window and cadence requirements are met.
6. Apply confidence, refractory, and debounce rules.
7. Publish the accepted event and confidence to the application supervisor.

Long-running states such as tilt should use a state policy distinct from one-shot events such as tap or flick.

## Outputs

The proposed output record is:

```text
motion_class      : enum
confidence        : fixed-point or float score
sequence_number   : monotonically increasing window ID
captured_at_ms    : application timestamp
duration_ms       : source-window duration
valid             : sensor/model validity flag
```

The exact ABI and numeric representation belong in the eventual firmware contract.

## Deployment requirements

- verified accelerometer identity, address, bus route, and configuration;
- documented axis order, range, calibration, and output data rate;
- deterministic timestamping and dropped-sample behavior;
- fixed-size sample, feature, and inference buffers;
- model artifact with recorded quantization and class ordering;
- host-versus-target golden-vector test;
- measured inference latency, memory, and false-trigger behavior.

This README does not assert an I2C address, pin, bus instance, sensor range, or sample rate.

## Limitations

Cheetah recognizes patterns represented in its training data; it does not understand intent or guarantee detection of every physical event. Mounting orientation, attachment method, vibration, user technique, and sensor saturation can change the signal. Safety-critical impact or motion detection requires an independently validated sensing path.

## Related project

The MCXN236 integration scaffold is maintained in the [EmbeddedX project](https://github.com/telespial/EmbeddedX_V2_0/tree/main/projects/manufacturers/NXP/FRDM/MCXN236/models/baby-cheetah-motion).
