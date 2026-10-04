# FRDM-MCXC162 temperature telemetry

This firmware reads the onboard NXP P3T1755 temperature sensor and emits
newline-delimited JSON over the MCU-Link virtual COM port at 115200 baud,
8 data bits, no parity and 1 stop bit. It also provides:

- RTC time synchronized from the dashboard when USB serial connects.
- SW2 stop and SW3 record controls with 30 ms debounce.
- Red RGB LED while stopped, green while recording, and blue on AI anomalies.
- A CRC-protected, 1,024-record circular FIFO in the final two flash sectors.
- Dashboard-controlled record intervals from 1 second through 1 hour.
- A persistent, dashboard-controlled baseline-training length from 32 to 256 samples.
- Flash playback and confirmed logger-memory reset commands.

## Protocol

Samples have Celsius and Fahrenheit rounded to two decimal places:

```json
{"type":"sample","epoch":1790956800,"temp_c":23.50,"temp_f":74.30,"recording":true,"synced":true,"record_ms":1000,"log_count":42}
```

The board publishes a current reading every 200 ms while recording. Each
sample includes the RTC epoch time. Flash logging begins only after the RTC has
a valid time, preventing undated records.

Commands accepted from the dashboard are:

```text
TIME <unix-seconds>
RATE <1000-3600000 milliseconds>
TRAIN <32-256 samples>
AIFILTER <0|1>
AIANOMALY <0|1>
RECORD <0|1>
PLAY
CLEAR
STATUS
```

`MCXC162_flash_logger.ld` reserves `0x0000C000-0x0000FFFF` exclusively for
logger data. The application is linked into the first 48 KB and the build
fails if code grows into the logger partition.

## Build

From the MCUXpresso SDK workspace root:

```sh
export ARMGCC_DIR="$HOME/.mcuxpressotools/arm-gnu-toolchain-14.2.rel1-darwin-x86_64-arm-none-eabi"
export PATH="$HOME/.mcuxpressotools/.mcux-venv-3.12/bin:$ARMGCC_DIR/bin:$PATH"
west build -b frdmmcxc162 \
  /absolute/path/to/EmbeddedX_V2_0/projects/manufacturers/NXP/FRDM/MCXC162/firmware \
  --toolchain armgcc --config debug -p always \
  -d /absolute/path/to/EmbeddedX_V2_0/projects/manufacturers/NXP/FRDM/MCXC162/firmware/build
```

Changing `TRAIN` resets both in-RAM models and begins a new baseline. Changing
`AIFILTER` also resets training because it changes the model input, while
re-enabling `AIANOMALY` begins a fresh baseline. Record interval, training
length, AI filter, and AI anomaly enable share a versioned retained RTC-domain
settings word and are restored after reset while that domain remains powered.

The `.elf` and `.bin` outputs are placed in the selected build directory.
Building does not flash the board. Flashing and physical validation are
separate, explicit hardware steps.

## Verification

Recorded build and hardware results change with the implementation. Do not
infer current flash/RAM use or physical validation from this document; run the
build, inspect its memory report, and validate the resulting image on the
target board.
