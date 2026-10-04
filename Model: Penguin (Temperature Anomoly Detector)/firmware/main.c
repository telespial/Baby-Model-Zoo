/*
 * FRDM-MCXC162 temperature recorder
 *
 * Newline-delimited JSON is transmitted over MCU-Link VCOM at 115200 8-N-1.
 * Incoming commands are: TIME <epoch>, RATE <ms>, TRAIN <samples>,
 * AIFILTER <0|1>, AIANOMALY <0|1>, RECORD <0|1>, PLAY, CLEAR, and STATUS.
 *
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "app.h"
#include "baby_temp_service.h"
#include "board.h"
#include "flash_logger.h"
#include "fsl_clock.h"
#include "fsl_debug_console.h"
#include "fsl_gpio.h"
#include "fsl_lpi2c.h"
#include "fsl_lpuart.h"
#include "fsl_p3t1755.h"
#include "fsl_rtc.h"
#include "pin_mux.h"

#define RTC_BASE                    RTC0
#define I2C_BAUDRATE_HZ             100000U
#define I2C_TIMEOUT_ITERATIONS      100000000U
#define TELEMETRY_PERIOD_MS         200U
#define DEFAULT_RECORD_PERIOD_MS    1000U
#define MIN_RECORD_PERIOD_MS        1000U
#define MAX_RECORD_PERIOD_MS        3600000U
#define MIN_VALID_EPOCH             946684800U
#define LOOP_TICK_US                10000U
#define LOOP_TICK_MS                (LOOP_TICK_US / 1000U)
#define RX_BUFFER_SIZE              128U
#define COMMAND_BUFFER_SIZE         48U
#define BUTTON_DEBOUNCE_TICKS       3U
#define LEGACY_INTERVAL_MAGIC       0xA1000000U
#define LEGACY_SETTINGS_MAGIC       0xA2000000U
#define LEGACY_SETTINGS_MASK        0xFF000000U
#define SETTINGS_RETENTION_MAGIC    0xB0000000U
#define SETTINGS_RETENTION_MASK     0xF0000000U
#define SETTINGS_RECORD_TICK_MS     200U
#define SETTINGS_RECORD_TICK_MASK   0x00007FFFU
#define SETTINGS_TRAINING_SHIFT     15U
#define SETTINGS_TRAINING_MASK      0x00FF8000U
#define SETTINGS_AI_FILTER_MASK     0x01000000U
#define SETTINGS_AI_ANOMALY_MASK    0x02000000U

typedef struct
{
    bool candidatePressed;
    bool stablePressed;
    uint8_t stableTicks;
} button_state_t;

static volatile status_t s_completionStatus;
static volatile bool s_masterCompletionFlag;
static lpi2c_master_handle_t s_i2cMasterHandle;
static p3t1755_handle_t s_sensorHandle;
static volatile char s_rxBuffer[RX_BUFFER_SIZE];
static volatile uint16_t s_rxHead;
static volatile uint16_t s_rxTail;
static bool s_recording;
static bool s_aiFilterEnabled;
static bool s_aiAnomalyEnabled = true;
static double s_aiFilterSamples[3];
static uint8_t s_aiFilterCount;
static uint8_t s_aiFilterHead;
static bool s_timeSynced;
static uint32_t s_recordPeriodMs = DEFAULT_RECORD_PERIOD_MS;
static uint32_t s_trainingSamples = BABY_TEMP_TRAINING_SAMPLES_DEFAULT;
static uint32_t s_lastRecordEpoch;
static uint32_t s_blueFlashMs;

static void retain_device_settings(void)
{
    uint32_t recordTicks = s_recordPeriodMs / SETTINGS_RECORD_TICK_MS;
    RTC_BASE->TAR = SETTINGS_RETENTION_MAGIC |
                    ((s_trainingSamples << SETTINGS_TRAINING_SHIFT) & SETTINGS_TRAINING_MASK) |
                    (recordTicks & SETTINGS_RECORD_TICK_MASK) |
                    (s_aiFilterEnabled ? SETTINGS_AI_FILTER_MASK : 0U) |
                    (s_aiAnomalyEnabled ? SETTINGS_AI_ANOMALY_MASK : 0U);
}

static void restore_device_settings(void)
{
    uint32_t retained = RTC_BASE->TAR;
    uint32_t legacyMagic = retained & LEGACY_SETTINGS_MASK;
    if ((retained & SETTINGS_RETENTION_MASK) == SETTINGS_RETENTION_MAGIC)
    {
        uint32_t recordPeriodMs = (retained & SETTINGS_RECORD_TICK_MASK) * SETTINGS_RECORD_TICK_MS;
        uint32_t trainingSamples = (retained & SETTINGS_TRAINING_MASK) >> SETTINGS_TRAINING_SHIFT;
        if ((recordPeriodMs >= MIN_RECORD_PERIOD_MS) && (recordPeriodMs <= MAX_RECORD_PERIOD_MS))
        {
            s_recordPeriodMs = recordPeriodMs;
        }
        if ((trainingSamples >= BABY_TEMP_TRAINING_SAMPLES_MIN) &&
            (trainingSamples <= BABY_TEMP_TRAINING_SAMPLES_MAX))
        {
            s_trainingSamples = trainingSamples;
        }
        s_aiFilterEnabled = (retained & SETTINGS_AI_FILTER_MASK) != 0U;
        s_aiAnomalyEnabled = (retained & SETTINGS_AI_ANOMALY_MASK) != 0U;
    }
    else if (legacyMagic == LEGACY_SETTINGS_MAGIC)
    {
        uint32_t recordPeriodMs = (retained & SETTINGS_RECORD_TICK_MASK) * SETTINGS_RECORD_TICK_MS;
        uint32_t trainingSamples = (retained & SETTINGS_TRAINING_MASK) >> SETTINGS_TRAINING_SHIFT;
        if ((recordPeriodMs >= MIN_RECORD_PERIOD_MS) && (recordPeriodMs <= MAX_RECORD_PERIOD_MS))
        {
            s_recordPeriodMs = recordPeriodMs;
        }
        if ((trainingSamples >= BABY_TEMP_TRAINING_SAMPLES_MIN) &&
            (trainingSamples <= BABY_TEMP_TRAINING_SAMPLES_MAX))
        {
            s_trainingSamples = trainingSamples;
        }
    }
    else if (legacyMagic == LEGACY_INTERVAL_MAGIC)
    {
        uint32_t recordPeriodMs = retained & 0x00FFFFFFU;
        if ((recordPeriodMs >= MIN_RECORD_PERIOD_MS) && (recordPeriodMs <= MAX_RECORD_PERIOD_MS))
        {
            s_recordPeriodMs = recordPeriodMs;
        }
    }
}

static void i2c_master_callback(LPI2C_Type *base,
                                lpi2c_master_handle_t *handle,
                                status_t status,
                                void *userData)
{
    (void)base;
    (void)handle;
    (void)userData;
    if (status == kStatus_Success)
    {
        s_masterCompletionFlag = true;
    }
    s_completionStatus = status;
}

static status_t wait_for_i2c_completion(void)
{
    uint32_t timeout = 0U;
    while (!s_masterCompletionFlag)
    {
        timeout++;
        if ((s_completionStatus != kStatus_Success) || (timeout >= I2C_TIMEOUT_ITERATIONS))
        {
            break;
        }
    }
    return timeout >= I2C_TIMEOUT_ITERATIONS ? kStatus_Timeout : s_completionStatus;
}

status_t i2c_WriteSensor(uint8_t deviceAddress, uint32_t regAddress, uint8_t *regData, size_t dataSize)
{
    lpi2c_master_transfer_t transfer = {0};
    status_t result;
    transfer.slaveAddress = deviceAddress;
    transfer.direction = kLPI2C_Write;
    transfer.subaddress = regAddress;
    transfer.subaddressSize = 1U;
    transfer.data = regData;
    transfer.dataSize = dataSize;
    transfer.flags = kLPI2C_TransferDefaultFlag;
    s_masterCompletionFlag = false;
    s_completionStatus = kStatus_Success;
    result = LPI2C_MasterTransferNonBlocking(
        BOARD_TEMP_SENSOR_I2C_BASEADDR, &s_i2cMasterHandle, &transfer);
    return result == kStatus_Success ? wait_for_i2c_completion() : result;
}

status_t i2c_ReadSensor(uint8_t deviceAddress, uint32_t regAddress, uint8_t *regData, size_t dataSize)
{
    lpi2c_master_transfer_t transfer = {0};
    status_t result;
    transfer.slaveAddress = deviceAddress;
    transfer.direction = kLPI2C_Read;
    transfer.subaddress = regAddress;
    transfer.subaddressSize = 1U;
    transfer.data = regData;
    transfer.dataSize = dataSize;
    transfer.flags = kLPI2C_TransferDefaultFlag;
    s_masterCompletionFlag = false;
    s_completionStatus = kStatus_Success;
    result = LPI2C_MasterTransferNonBlocking(
        BOARD_TEMP_SENSOR_I2C_BASEADDR, &s_i2cMasterHandle, &transfer);
    return result == kStatus_Success ? wait_for_i2c_completion() : result;
}

void BOARD_UART_IRQ_HANDLER(void)
{
    uint32_t status;
    /* The board debug console may run with the receive FIFO disabled.  In
       that mode RXCOUNT stays zero even though RDRF is asserted, so drain by
       the receive-data flag instead of relying on FIFO status. */
    while (((status = LPUART_GetStatusFlags(LPUART0)) &
            (uint32_t)kLPUART_RxDataRegFullFlag) != 0U)
    {
        uint16_t next = (uint16_t)((s_rxHead + 1U) % RX_BUFFER_SIZE);
        char received = (char)LPUART_ReadByte(LPUART0);
        if (next != s_rxTail)
        {
            s_rxBuffer[s_rxHead] = received;
            s_rxHead = next;
        }
    }
    if ((status & (uint32_t)kLPUART_RxOverrunFlag) != 0U)
    {
        LPUART_ClearStatusFlags(LPUART0, (uint32_t)kLPUART_RxOverrunFlag);
    }
    SDK_ISR_EXIT_BARRIER;
}

static bool serial_get_char(char *received)
{
    if (s_rxTail == s_rxHead)
    {
        return false;
    }
    *received = s_rxBuffer[s_rxTail];
    s_rxTail = (uint16_t)((s_rxTail + 1U) % RX_BUFFER_SIZE);
    return true;
}

static void set_indicator(bool recording)
{
    s_blueFlashMs = 0U;
    LED_BLUE_OFF();
    if (recording)
    {
        LED_RED_OFF();
        LED_GREEN_ON();
    }
    else
    {
        LED_GREEN_OFF();
        LED_RED_ON();
    }
}

static const char *ai_state_name(baby_temp_state_t state)
{
    switch (state) {
        case BABY_TEMP_TRAINING: return "training";
        case BABY_TEMP_READY: return "ready";
        case BABY_TEMP_WATCH: return "watch";
        case BABY_TEMP_ANOMALY: return "anomaly";
        case BABY_TEMP_COLLECTING: return "collecting";
        default: return "untrained";
    }
}

static void set_recording(bool recording, const char *source)
{
    s_recording = recording;
    if (recording) s_lastRecordEpoch = RTC_BASE->TSR;
    set_indicator(recording);
    PRINTF("{\"type\":\"state\",\"recording\":%s,\"source\":\"%s\",\"epoch\":%u}\r\n",
           recording ? "true" : "false", source, RTC_BASE->TSR);
}

static bool button_changed(button_state_t *state, bool pressed)
{
    if (pressed != state->candidatePressed)
    {
        state->candidatePressed = pressed;
        state->stableTicks = 0U;
        return false;
    }
    if (state->stableTicks < BUTTON_DEBOUNCE_TICKS)
    {
        state->stableTicks++;
    }
    if ((state->stableTicks == BUTTON_DEBOUNCE_TICKS) && (state->stablePressed != pressed))
    {
        state->stablePressed = pressed;
        return true;
    }
    return false;
}

static void poll_buttons(void)
{
    static button_state_t sw2;
    static button_state_t sw3;
    static bool initialized;
    bool sw2Pressed = GPIO_PinRead(BOARD_SW2_GPIO, BOARD_SW2_GPIO_PIN) != 0U;
    bool sw3Pressed = GPIO_PinRead(BOARD_SW3_GPIO, BOARD_SW3_GPIO_PIN) == 0U;
    if (!initialized)
    {
        sw2.candidatePressed = sw2.stablePressed = sw2Pressed;
        sw3.candidatePressed = sw3.stablePressed = sw3Pressed;
        initialized = true;
        return;
    }
    if (button_changed(&sw2, sw2Pressed) && sw2Pressed)
    {
        set_recording(false, "sw2");
    }
    if (button_changed(&sw3, sw3Pressed) && sw3Pressed)
    {
        set_recording(true, "sw3");
    }
}

static bool parse_unsigned(const char *text, uint32_t *value)
{
    char *end = NULL;
    unsigned long parsed = strtoul(text, &end, 10);
    if ((end == text) || (*end != '\0') || (parsed > UINT32_MAX))
    {
        return false;
    }
    *value = (uint32_t)parsed;
    return true;
}

static void playback_visitor(const flash_logger_sample_t *sample, void *context)
{
    double temperatureC = (double)sample->temperatureCentiC / 100.0;
    double temperatureF = (temperatureC * 1.8) + 32.0;
    (void)context;
    PRINTF("{\"type\":\"log\",\"sequence\":%u,\"epoch\":%u,\"temp_c\":%.2f,\"temp_f\":%.2f}\r\n",
           sample->sequence, sample->epochSeconds, temperatureC, temperatureF);
}

static void publish_state(void)
{
    PRINTF("{\"type\":\"status\",\"recording\":%s,\"synced\":%s,\"epoch\":%u,"
           "\"record_ms\":%u,\"training_samples\":%u,\"log_count\":%u,\"log_capacity\":%u,"
           "\"ai_filter\":%s,\"ai_anomaly\":%s}\r\n",
           s_recording ? "true" : "false", s_timeSynced ? "true" : "false",
           RTC_BASE->TSR, s_recordPeriodMs, s_trainingSamples, FlashLogger_Count(), FLASH_LOGGER_CAPACITY,
           s_aiFilterEnabled ? "true" : "false", s_aiAnomalyEnabled ? "true" : "false");
}

static float ai_input_temperature(double rawTemperatureC)
{
    s_aiFilterSamples[s_aiFilterHead] = rawTemperatureC;
    s_aiFilterHead = (uint8_t)((s_aiFilterHead + 1U) % 3U);
    if (s_aiFilterCount < 3U) s_aiFilterCount++;
    if (!s_aiFilterEnabled) return (float)rawTemperatureC;
    double sum = 0.0;
    for (uint8_t i = 0U; i < s_aiFilterCount; ++i) sum += s_aiFilterSamples[i];
    return (float)(sum / (double)s_aiFilterCount);
}

static void process_command(const char *command)
{
    uint32_t value;
    if (strncmp(command, "TIME ", 5U) == 0)
    {
        if (!parse_unsigned(command + 5, &value) || (value < MIN_VALID_EPOCH))
        {
            PRINTF("{\"type\":\"error\",\"message\":\"invalid_time\"}\r\n");
            return;
        }
        RTC_StopTimer(RTC_BASE);
        RTC_BASE->TSR = value;
        RTC_StartTimer(RTC_BASE);
        s_timeSynced = true;
        s_lastRecordEpoch = RTC_BASE->TSR;
        PRINTF("{\"type\":\"time\",\"synced\":true,\"epoch\":%u}\r\n", RTC_BASE->TSR);
    }
    else if (strncmp(command, "RATE ", 5U) == 0)
    {
        if (!parse_unsigned(command + 5, &value))
        {
            PRINTF("{\"type\":\"error\",\"message\":\"invalid_rate\"}\r\n");
            return;
        }
        if (value < MIN_RECORD_PERIOD_MS)
        {
            value = MIN_RECORD_PERIOD_MS;
        }
        else if (value > MAX_RECORD_PERIOD_MS)
        {
            value = MAX_RECORD_PERIOD_MS;
        }
        value -= value % SETTINGS_RECORD_TICK_MS;
        s_recordPeriodMs = value;
        s_lastRecordEpoch = RTC_BASE->TSR;
        retain_device_settings();
        PRINTF("{\"type\":\"rate\",\"record_ms\":%u}\r\n", s_recordPeriodMs);
    }
    else if (strncmp(command, "TRAIN ", 6U) == 0)
    {
        uint32_t previousTrainingSamples = s_trainingSamples;
        if (!parse_unsigned(command + 6, &value) ||
            !baby_temp_service_set_training_samples((unsigned)value))
        {
            PRINTF("{\"type\":\"error\",\"message\":\"invalid_training_samples\"}\r\n");
            return;
        }
        s_trainingSamples = value;
        retain_device_settings();
        PRINTF("{\"type\":\"training\",\"training_samples\":%u,\"model_reset\":%s}\r\n",
               s_trainingSamples, previousTrainingSamples != s_trainingSamples ? "true" : "false");
    }
    else if (strncmp(command, "AIFILTER ", 9U) == 0)
    {
        if (!parse_unsigned(command + 9, &value) || value > 1U)
        {
            PRINTF("{\"type\":\"error\",\"message\":\"invalid_ai_filter\"}\r\n");
            return;
        }
        bool wasEnabled = s_aiFilterEnabled;
        s_aiFilterEnabled = value != 0U;
        if (wasEnabled != s_aiFilterEnabled)
        {
            memset(s_aiFilterSamples, 0, sizeof(s_aiFilterSamples));
            s_aiFilterCount = 0U;
            s_aiFilterHead = 0U;
            baby_temp_service_reset();
        }
        retain_device_settings();
        PRINTF("{\"type\":\"ai_filter\",\"ai_filter\":%s,\"model_reset\":%s}\r\n",
               s_aiFilterEnabled ? "true" : "false",
               wasEnabled != s_aiFilterEnabled ? "true" : "false");
    }
    else if (strncmp(command, "AIANOMALY ", 10U) == 0)
    {
        if (!parse_unsigned(command + 10, &value) || value > 1U)
        {
            PRINTF("{\"type\":\"error\",\"message\":\"invalid_ai_anomaly\"}\r\n");
            return;
        }
        bool wasEnabled = s_aiAnomalyEnabled;
        s_aiAnomalyEnabled = value != 0U;
        if (!s_aiAnomalyEnabled)
        {
            s_blueFlashMs = 0U;
            LED_BLUE_OFF();
        }
        else if (!wasEnabled)
        {
            memset(s_aiFilterSamples, 0, sizeof(s_aiFilterSamples));
            s_aiFilterCount = 0U;
            s_aiFilterHead = 0U;
            baby_temp_service_reset();
        }
        retain_device_settings();
        PRINTF("{\"type\":\"ai_anomaly\",\"ai_anomaly\":%s,\"model_reset\":%s}\r\n",
               s_aiAnomalyEnabled ? "true" : "false",
               s_aiAnomalyEnabled && !wasEnabled ? "true" : "false");
    }
    else if (strcmp(command, "RECORD 1") == 0)
    {
        set_recording(true, "web");
    }
    else if (strcmp(command, "RECORD 0") == 0)
    {
        set_recording(false, "web");
    }
    else if (strcmp(command, "PLAY") == 0)
    {
        PRINTF("{\"type\":\"playback_start\",\"log_count\":%u}\r\n", FlashLogger_Count());
        FlashLogger_ForEach(playback_visitor, NULL);
        PRINTF("{\"type\":\"playback_end\",\"log_count\":%u}\r\n", FlashLogger_Count());
    }
    else if (strcmp(command, "CLEAR") == 0)
    {
        status_t result = FlashLogger_Clear();
        PRINTF("{\"type\":\"clear\",\"success\":%s,\"log_count\":%u}\r\n",
               result == kStatus_Success ? "true" : "false", FlashLogger_Count());
    }
    else if (strcmp(command, "STATUS") == 0)
    {
        publish_state();
    }
    else
    {
        PRINTF("{\"type\":\"error\",\"message\":\"unknown_command\"}\r\n");
    }
}

static void poll_serial_commands(void)
{
    static char command[COMMAND_BUFFER_SIZE];
    static size_t length;
    char received;
    while (serial_get_char(&received))
    {
        if ((received == '\r') || (received == '\n'))
        {
            if (length > 0U)
            {
                command[length] = '\0';
                process_command(command);
                length = 0U;
            }
        }
        else if ((received >= ' ') && (received <= '~'))
        {
            if (length < (COMMAND_BUFFER_SIZE - 1U))
            {
                command[length++] = received;
            }
            else
            {
                length = 0U;
                PRINTF("{\"type\":\"error\",\"message\":\"command_too_long\"}\r\n");
            }
        }
    }
}

static status_t read_temperature(double *temperatureC)
{
    status_t result = P3T1755_ReadTemperature(&s_sensorHandle, temperatureC);
    if (result != kStatus_Success)
    {
        PRINTF("{\"type\":\"error\",\"message\":\"sensor_read_failed\",\"status\":%d}\r\n", result);
    }
    return result;
}

static void publish_temperature(double temperatureC)
{
    double temperatureF = (temperatureC * 1.8) + 32.0;
    baby_temp_status_t aiStatus;
    (void)baby_temp_service_get_status(&aiStatus);
    PRINTF("{\"type\":\"sample\",\"epoch\":%u,\"temp_c\":%.2f,\"temp_f\":%.2f,"
           "\"recording\":%s,\"synced\":%s,\"record_ms\":%u,\"log_count\":%u,"
           "\"training_samples\":%u,\"training_steps\":%u,\"ai_filter\":%s,\"ai_anomaly\":%s,"
           "\"ai_state\":\"%s\","
           "\"ai_error\":%.5f,\"ai_prediction_error\":%.5f,\"ai_score\":%.5f}\r\n",
           RTC_BASE->TSR, temperatureC, temperatureF, s_recording ? "true" : "false",
           s_timeSynced ? "true" : "false",
           s_recordPeriodMs, FlashLogger_Count(), s_trainingSamples, aiStatus.trainingSteps,
           s_aiFilterEnabled ? "true" : "false", s_aiAnomalyEnabled ? "true" : "false",
           s_aiAnomalyEnabled ? ai_state_name(aiStatus.state) : "disabled",
           s_aiAnomalyEnabled ? (double)aiStatus.reconstructionError : 0.0,
           s_aiAnomalyEnabled ? (double)aiStatus.predictionError : 0.0,
           s_aiAnomalyEnabled ? (double)aiStatus.anomalyScore : 0.0);
}

static void initialize_peripherals(void)
{
    lpi2c_master_config_t masterConfig;
    p3t1755_config_t sensorConfig;
    rtc_config_t rtcConfig;
    BOARD_InitHardware();
    BOARD_InitLEDsPins();
    BOARD_InitBUTTONsPins();
    LED_RED_INIT(LOGIC_LED_OFF);
    LED_GREEN_INIT(LOGIC_LED_OFF);
    LED_BLUE_INIT(LOGIC_LED_OFF);
    set_indicator(s_recording);

    (void)CLOCK_SetupFRO16KClocking(kCLOCK_Clk16kToSysAndCore);
    RTC_GetDefaultConfig(&rtcConfig);
    RTC_Init(RTC_BASE, &rtcConfig);
    restore_device_settings();
    retain_device_settings();
    RTC_StartTimer(RTC_BASE);
    s_timeSynced = RTC_BASE->TSR >= MIN_VALID_EPOCH;

    LPI2C_MasterGetDefaultConfig(&masterConfig);
    masterConfig.baudRate_Hz = I2C_BAUDRATE_HZ;
    LPI2C_MasterInit(BOARD_TEMP_SENSOR_I2C_BASEADDR, &masterConfig, I2C_CLOCK_FREQUENCY);
    LPI2C_MasterTransferCreateHandle(
        BOARD_TEMP_SENSOR_I2C_BASEADDR, &s_i2cMasterHandle, i2c_master_callback, NULL);
    sensorConfig.writeTransfer = i2c_WriteSensor;
    sensorConfig.readTransfer = i2c_ReadSensor;
    sensorConfig.sensorAddress = SENSOR_SLAVE_ADDR;
    P3T1755_Init(&s_sensorHandle, &sensorConfig);
    baby_temp_service_init();
    (void)baby_temp_service_set_training_samples((unsigned)s_trainingSamples);

    LPUART_EnableInterrupts(LPUART0,
                           (uint32_t)kLPUART_RxDataRegFullInterruptEnable |
                               (uint32_t)kLPUART_RxOverrunInterruptEnable);
    EnableIRQ(BOARD_UART_IRQ);
}

int main(void)
{
    uint32_t telemetryElapsedMs = TELEMETRY_PERIOD_MS;
    initialize_peripherals();
    if (FlashLogger_Init() != kStatus_Success)
    {
        PRINTF("{\"type\":\"error\",\"message\":\"flash_logger_init_failed\"}\r\n");
    }
    PRINTF("{\"type\":\"hello\",\"board\":\"FRDM-MCXC162\",\"sensor\":\"P3T1755\","
           "\"baud\":115200,\"telemetry_ms\":%u,\"record_ms\":%u,\"training_samples\":%u,"
           "\"ai_filter\":%s,\"ai_anomaly\":%s,\"log_capacity\":%u}\r\n",
           TELEMETRY_PERIOD_MS, s_recordPeriodMs, s_trainingSamples,
           s_aiFilterEnabled ? "true" : "false", s_aiAnomalyEnabled ? "true" : "false",
           FLASH_LOGGER_CAPACITY);
    publish_state();

    while (true)
    {
        double temperatureC = 0.0;
        poll_serial_commands();
        poll_buttons();
        if (s_recording && (telemetryElapsedMs >= TELEMETRY_PERIOD_MS))
        {
            if (read_temperature(&temperatureC) == kStatus_Success)
            {
                baby_temp_status_t aiStatus;
                if (s_aiAnomalyEnabled)
                {
                    baby_temp_service_add_sample(ai_input_temperature(temperatureC));
                }
                (void)baby_temp_service_get_status(&aiStatus);
                if (s_aiAnomalyEnabled && (aiStatus.state == BABY_TEMP_ANOMALY)) {
                    s_blueFlashMs = 500U;
                    LED_BLUE_ON();
                }
                publish_temperature(temperatureC);
                uint32_t recordPeriodSeconds = (s_recordPeriodMs + 999U) / 1000U;
                if (s_timeSynced && ((RTC_BASE->TSR - s_lastRecordEpoch) >= recordPeriodSeconds))
                {
                    int16_t centiC = (int16_t)((temperatureC * 100.0) + (temperatureC >= 0.0 ? 0.5 : -0.5));
                    if (FlashLogger_Append(RTC_BASE->TSR, centiC) != kStatus_Success)
                    {
                        PRINTF("{\"type\":\"error\",\"message\":\"flash_log_write_failed\"}\r\n");
                    }
                    else
                    {
                        LED_GREEN_OFF();
                        SDK_DelayAtLeastUs(25000U, CLOCK_GetCoreSysClkFreq());
                        if (s_recording) LED_GREEN_ON();
                    }
                    s_lastRecordEpoch = RTC_BASE->TSR;
                }
            }
            telemetryElapsedMs = 0U;
        }
        SDK_DelayAtLeastUs(LOOP_TICK_US, CLOCK_GetCoreSysClkFreq());
        if (s_blueFlashMs > LOOP_TICK_MS) {
            s_blueFlashMs -= LOOP_TICK_MS;
        } else if (s_blueFlashMs != 0U) {
            s_blueFlashMs = 0U;
            LED_BLUE_OFF();
        }
        if (s_recording)
        {
            telemetryElapsedMs += LOOP_TICK_MS;
        }
    }
}
