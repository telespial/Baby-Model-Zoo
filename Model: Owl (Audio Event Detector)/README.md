# Model: Owl - EdgeAI Audio Event Detector

Baby Owl is a proposed tiny on-device audio model for recognizing a small vocabulary of acoustic events. It is intended for the MCXN236 EdgeAI demonstration, where one Cortex-M33 shares time between the display application and several specialist models.

> **Status:** project definition and documentation scaffold. A trained model, fixed tensor contract, measured accuracy, and production firmware adapter are not yet included.

## Purpose

Owl turns short windows from a digital microphone into an event class and confidence score without sending audio to the cloud. The initial demonstration vocabulary is:

| Class | Meaning |
| --- | --- |
| `ambient` | background or no target event |
| `voice_activity` | speech-like energy is present |
| `wake_word` | a selected wake-word pattern, once a dataset exists |
| `clap` | short broadband impulse |
| `knock` | one or more impact-like impulses |
| `loud_transient` | loud event not assigned to another class |

The class names are a starting contract, not a claim that the model has been trained to recognize them.

## How Owl works

```text
Digital microphone
       │
       ▼
PCM capture ring buffer
       │
       ▼
Frame windowing and optional pre-emphasis
       │
       ▼
Log-energy / spectral features
       │
       ▼
Tiny classifier
       │
       ▼
Class, confidence, and timestamp
```

The capture layer collects a bounded audio frame. Deterministic preprocessing converts the waveform into a compact feature tensor. The classifier then estimates the event probabilities. A confidence gate and temporal debounce should be applied by the application so a single uncertain frame does not repeatedly trigger an action.

The final sample rate, frame length, hop length, feature type, tensor dimensions, quantization, and classifier architecture must be selected from the training experiment and verified against the target microphone path. They are deliberately not guessed here.

## Candidate model architecture

The first implementation should compare a small dense classifier against a compact 2-D or 1-D convolutional classifier. A practical starting point is a quantized model operating on a time-frequency feature image:

```text
feature frames × frequency bands
             │
             ▼
small convolution or dense encoder
             │
             ▼
class logits
             │
             ▼
softmax / calibrated confidence
```

The architecture is not final until the feature dimensions, RAM use, flash size, latency, and validation split are measured on the MCXN236 build.

## Training data and labels

Training data should contain representative recordings for every class, including microphone placement, room noise, distance, loudness, and board enclosure conditions. The `ambient` class needs substantially varied negative examples so the model does not treat one quiet room as the definition of silence.

Recommended dataset partitions are speaker- and-session-independent where applicable:

- training set for fitting weights;
- validation set for architecture and threshold selection;
- held-out test set for the final report;
- noise and interference set for robustness checks.

Wake-word data requires separate speakers and negative phrases. Clap, knock, and transient labels need explicit event boundaries or a documented window-labeling rule.

## Runtime decision flow

1. Capture a bounded frame from the verified microphone driver.
2. Apply the exact preprocessing used during training.
3. Run the quantized model with fixed-size buffers.
4. Reject or hold results below the confidence threshold.
5. Debounce repeated detections over a short history.
6. Publish the class, confidence, and capture timestamp to the MCXN236 supervisor.

The supervisor should treat audio inference as event-driven work and avoid starving display deadlines or other model windows.

## Outputs

The proposed output record is:

```text
audio_class       : enum
confidence        : fixed-point or float score
sequence_number   : monotonically increasing frame ID
captured_at_ms    : application timestamp
valid             : preprocessing/inference validity flag
```

The exact ABI and numeric representation belong in the eventual firmware contract.

## Deployment requirements

- verified microphone interface and pin/peripheral routing;
- fixed sample and frame timing;
- bounded capture, feature, and inference memory;
- model artifact with recorded input scale/zero point and output ordering;
- host-versus-target golden-vector test;
- measured inference latency and power impact;
- behavior for clipping, dropped frames, and invalid capture.

No specific MCXN236 peripheral, DMA channel, sample rate, or pin is asserted by this README.

## Limitations

Owl is not a general speech recognizer, security boundary, or reliable voice-authentication system. Acoustic classification can be affected by placement, reverberation, competing sounds, microphone saturation, and unseen speakers. A production application must provide a safe fallback when the model is uncertain or the capture path fails.

## Related project

The MCXN236 integration scaffold is maintained in the [EmbeddedX project](https://github.com/telespial/EmbeddedX_V2_0/tree/main/projects/manufacturers/NXP/FRDM/MCXN236/models/baby-owl-audio-event).
