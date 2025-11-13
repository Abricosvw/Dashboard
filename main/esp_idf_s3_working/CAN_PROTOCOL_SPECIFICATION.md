# CAN-bus Protocol Specification для ECU Dashboard

## 🚗 Обзор автомобильной CAN-шины

CAN (Controller Area Network) - это стандартный протокол связи в автомобильной промышленности, разработанный компанией Bosch. Он обеспечивает надежную передачу данных между электронными блоками управления (ECU) в реальном времени.

### Основные характеристики CAN:
- **Топология:** Шина с двумя проводами (CANH и CANL)
- **Скорость:** от 125 кбит/с до 1 Мбит/с
- **Максимальная длина:** до 1000м при 50 кбит/с, до 40м при 1 Мбит/с
- **Количество узлов:** до 110 на одной шине
- **Детекция ошибок:** CRC, ACK, форматирование, биты заполнения
- **Приоритизация:** Арбитраж на основе ID сообщения

---

## 📋 Стандартные CAN ID для ECU систем

### Engine Management (0x200-0x2FF):

```
┌──────────┬─────────────────────────────────────────┐
│ CAN ID   │ Description                             │
├──────────┼─────────────────────────────────────────┤
│ 0x200    │ Boost Control Data                      │
│ 0x201    │ Turbo Parameters                        │
│ 0x202    │ Intercooler Temperature                 │
│ 0x203    │ Fuel Rail Pressure                      │
│ 0x210    │ Ignition Timing                         │
│ 0x211    │ Fuel Injection Timing                   │
│ 0x220    │ Lambda Sensor Data                      │
│ 0x221    │ Exhaust Gas Temperature                 │
│ 0x230    │ Engine Protection Status                │
│ 0x240    │ Knock Detection                         │
│ 0x250    │ Variable Valve Timing                   │
└──────────┴─────────────────────────────────────────┘
```

### Vehicle Dynamics (0x300-0x3FF):

```
┌──────────┬─────────────────────────────────────────┐
│ CAN ID   │ Description                             │
├──────────┼─────────────────────────────────────────┤
│ 0x300    │ Wheel Speed Sensors                     │
│ 0x301    │ Steering Angle                          │
│ 0x302    │ Brake Pressure                          │
│ 0x303    │ Suspension Position                     │
│ 0x310    │ Accelerometer Data                      │
│ 0x311    │ Gyroscope Data                          │
│ 0x320    │ GPS Position                            │
│ 0x321    │ Vehicle Speed                           │
│ 0x330    │ Traction Control Status                 │
│ 0x340    │ ABS Status                              │
└──────────┴─────────────────────────────────────────┘
```

### Engine Status (0x380-0x3FF):

```
┌──────────┬─────────────────────────────────────────┐
│ CAN ID   │ Description                             │
├──────────┼─────────────────────────────────────────┤
│ 0x380    │ Primary Engine Data (RPM, MAP, TPS)     │
│ 0x381    │ Engine Temperatures                     │
│ 0x382    │ Oil Pressure & Temperature              │
│ 0x383    │ Fuel System Status                      │
│ 0x384    │ Cooling System                          │
│ 0x385    │ Intake Air Temperature                  │
│ 0x390    │ Battery Voltage                         │
│ 0x391    │ Alternator Status                       │
│ 0x3A0    │ Diagnostic Trouble Codes               │
└──────────┴─────────────────────────────────────────┘
```

### Transmission (0x440-0x4FF):

```
┌──────────┬─────────────────────────────────────────┐
│ CAN ID   │ Description                             │
├──────────┼─────────────────────────────────────────┤
│ 0x440    │ TCU Status & Gear Position              │
│ 0x441    │ Clutch Engagement                       │
│ 0x442    │ Transmission Temperature                │
│ 0x443    │ Hydraulic Pressure                      │
│ 0x450    │ Launch Control Status                   │
│ 0x451    │ Shift Strategy                          │
│ 0x460    │ Differential Status                     │
│ 0x470    │ Torque Vectoring                        │
└──────────┴─────────────────────────────────────────┘
```

---

## 📊 Детальная спецификация сообщений ECU Dashboard

### 1. Boost Control Data (ID: 0x200)

```
Message Structure (8 bytes):
┌──────┬──────┬─────────────────────────────────────────────┐
│Offset│ Size │ Parameter                                   │
├──────┼──────┼─────────────────────────────────────────────┤
│  0   │  1   │ Wastegate Position (0-100%)                 │
│  1   │  2   │ Target Boost Pressure (0-300 kPa)          │
│  3   │  2   │ Actual Boost Pressure (0-300 kPa)          │
│  5   │  1   │ Boost Error (-100 to +100%)                │
│  6   │  2   │ Wastegate Duty Cycle (0-100%)              │
└──────┴──────┴─────────────────────────────────────────────┘

Data Types:
- Wastegate Position: uint8_t (0=закрыт, 100=полностью открыт)
- Boost Pressures: uint16_t (в kPa, разрешение 0.1 kPa)
- Boost Error: int8_t (отклонение от заданного значения)
- Duty Cycle: uint16_t (PWM в процентах, разрешение 0.1%)

Frequency: 50Hz (каждые 20мс)
Priority: High (критично для управления турбонаддувом)

Example Frame:
0x200: [45 7D 00 6E 00 FB 2C 01]
       │  │   │   │   │   │  │  │
       │  │   │   │   │   │  │  └─ Duty Cycle MSB
       │  │   │   │   │   │  └─── Duty Cycle LSB  
       │  │   │   │   │   └────── Boost Error (-5%)
       │  │   │   │   └─────────── Actual Boost MSB
       │  │   │   └────────────── Actual Boost LSB
       │  │   └───────────────── Target Boost MSB
       │  └──────────────────── Target Boost LSB
       └─────────────────────── Wastegate Position (45%)
```

### 2. Primary Engine Data (ID: 0x380)

```
Message Structure (8 bytes):
┌──────┬──────┬─────────────────────────────────────────────┐
│Offset│ Size │ Parameter                                   │
├──────┼──────┼─────────────────────────────────────────────┤
│  0   │  2   │ Engine RPM (0-8000 RPM)                     │
│  2   │  2   │ MAP Pressure (0-300 kPa)                    │
│  4   │  1   │ TPS Position (0-100%)                       │
│  5   │  1   │ Engine Temperature (-40 to +150°C)          │
│  6   │  2   │ Fuel Pressure (0-800 kPa)                  │
└──────┴──────┴─────────────────────────────────────────────┘

Data Types:
- Engine RPM: uint16_t (разрешение 1 RPM)
- MAP Pressure: uint16_t (в kPa, разрешение 0.1 kPa)
- TPS Position: uint8_t (0=закрыт, 100=полностью открыт)
- Engine Temp: int8_t (в °C, смещение +40°C)
- Fuel Pressure: uint16_t (в kPa, разрешение 0.1 kPa)

Frequency: 100Hz (каждые 10мс)
Priority: Critical (основные параметры двигателя)

Example Frame:
0x380: [10 0E FA 00 54 6E 90 01]
       │   │  │   │  │  │  │   │
       │   │  │   │  │  │  │   └─ Fuel Pressure MSB
       │   │  │   │  │  │  └──── Fuel Pressure LSB
       │   │  │   │  │  └─────── Engine Temp (110-40=70°C)
       │   │  │   │  └────────── TPS Position (84%)
       │   │  │   └───────────── MAP Pressure MSB
       │   │  └──────────────── MAP Pressure LSB
       │   └─────────────────── Engine RPM MSB
       └────────────────────── Engine RPM LSB (3600 RPM)
```

### 3. TCU Status Data (ID: 0x440)

```
Message Structure (8 bytes):
┌──────┬──────┬─────────────────────────────────────────────┐
│Offset│ Size │ Parameter                                   │
├──────┼──────┼─────────────────────────────────────────────┤
│  0   │  2   │ Torque Request (0-500 Nm)                   │
│  2   │  1   │ Protection Status Flags                     │
│  3   │  1   │ Current Gear Position (0-8)                 │
│  4   │  2   │ Clutch Pressure (0-16 bar)                  │
│  6   │  1   │ TCU Temperature (-40 to +150°C)             │
│  7   │  1   │ Shift Strategy (0-7)                        │
└──────┴──────┴─────────────────────────────────────────────┘

Protection Status Flags (Byte 2):
Bit 0: Overheat Protection
Bit 1: Overspeed Protection  
Bit 2: Low Pressure Warning
Bit 3: Clutch Slip Detection
Bit 4: Launch Control Active
Bit 5: Limp Mode Active
Bit 6: Diagnostic Mode
Bit 7: System Error

Gear Position Values:
0 = Neutral, 1-6 = Forward gears, 7 = Reverse, 8 = Park

Frequency: 50Hz (каждые 20мс)
Priority: High (критично для управления трансмиссией)

Example Frame:
0x440: [F4 01 20 03 64 00 5A 02]
       │   │  │  │  │   │  │  │
       │   │  │  │  │   │  │  └─ Shift Strategy (Sport)
       │   │  │  │  │   │  └──── TCU Temperature (90-40=50°C)
       │   │  │  │  │   └─────── Clutch Pressure MSB
       │   │  │  │  └─────────── Clutch Pressure LSB
       │   │  │  └────────────── Current Gear (3rd)
       │   │  └───────────────── Protection Status (0x20=Limp Mode)
       │   └─────────────────── Torque Request MSB
       └────────────────────── Torque Request LSB (500 Nm)
```

---

## 🔧 Настройка CAN контроллера ESP32-S3

### Hardware Configuration:

```c
// CAN GPIO pins
#define CAN_TX_GPIO    GPIO_NUM_20
#define CAN_RX_GPIO    GPIO_NUM_19
#define CAN_STBY_GPIO  GPIO_NUM_45  // Standby control (optional)

// CAN timing for 500 kbps
#define CAN_TIMING_CONFIG_500KBITS() {  \
    .brp = 8,                           \
    .tseg_1 = 15,                       \
    .tseg_2 = 4,                        \
    .sjw = 3,                           \
    .triple_sampling = false            \
}
```

### Software Initialization:

```c
#include "driver/twai.h"

esp_err_t init_can_bus(void)
{
    // Configure TWAI general settings
    twai_general_config_t g_config = {
        .mode = TWAI_MODE_NORMAL,
        .tx_io = CAN_TX_GPIO,
        .rx_io = CAN_RX_GPIO,
        .clkout_io = TWAI_IO_UNUSED,
        .bus_off_io = TWAI_IO_UNUSED,
        .tx_queue_len = 10,
        .rx_queue_len = 10,
        .alerts_enabled = TWAI_ALERT_TX_IDLE | TWAI_ALERT_TX_SUCCESS | 
                         TWAI_ALERT_RX_DATA | TWAI_ALERT_ERR_PASS | 
                         TWAI_ALERT_BUS_ERROR,
        .clkout_divider = 0,
        .intr_flags = ESP_INTR_FLAG_LEVEL1
    };

    // Configure TWAI timing for 500 kbps
    twai_timing_config_t t_config = CAN_TIMING_CONFIG_500KBITS();

    // Configure TWAI filter to accept ECU messages
    twai_filter_config_t f_config = {
        .acceptance_code = 0x200,     // Accept 0x200-0x4FF range
        .acceptance_mask = 0x600,     // Mask for ECU message range
        .single_filter = true
    };

    // Install TWAI driver
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_LOGI("CAN", "TWAI driver installed");

    // Start TWAI driver
    ESP_ERROR_CHECK(twai_start());
    ESP_LOGI("CAN", "TWAI driver started");

    return ESP_OK;
}
```

### Message Receiving:

```c
typedef struct {
    uint16_t engine_rpm;
    uint16_t map_pressure;
    uint8_t tps_position;
    uint8_t engine_temp;
    uint16_t fuel_pressure;
} engine_data_t;

typedef struct {
    uint8_t wastegate_position;
    uint16_t target_boost;
    uint16_t actual_boost;
    int8_t boost_error;
    uint16_t duty_cycle;
} boost_data_t;

typedef struct {
    uint16_t torque_request;
    uint8_t protection_status;
    uint8_t gear_position;
    uint16_t clutch_pressure;
    uint8_t tcu_temperature;
    uint8_t shift_strategy;
} tcu_data_t;

void process_can_message(twai_message_t *message)
{
    switch (message->identifier) {
        case 0x380: {  // Engine Data
            engine_data_t *data = (engine_data_t*)message->data;
            
            // Update engine RPM (big-endian to little-endian)
            uint16_t rpm = (message->data[0] << 8) | message->data[1];
            
            // Update MAP pressure 
            uint16_t map = (message->data[2] << 8) | message->data[3];
            
            // Update TPS position
            uint8_t tps = message->data[4];
            
            // Update gauge values
            update_engine_gauges(rpm, map, tps);
            break;
        }
        
        case 0x200: {  // Boost Control
            uint8_t wastegate = message->data[0];
            uint16_t target_boost = (message->data[1] << 8) | message->data[2];
            uint16_t actual_boost = (message->data[3] << 8) | message->data[4];
            
            update_boost_gauges(wastegate, target_boost, actual_boost);
            break;
        }
        
        case 0x440: {  // TCU Status
            uint16_t torque = (message->data[0] << 8) | message->data[1];
            uint8_t protection = message->data[2];
            uint8_t gear = message->data[3];
            
            update_tcu_status(torque, protection, gear);
            break;
        }
        
        default:
            ESP_LOGW("CAN", "Unknown message ID: 0x%03X", message->identifier);
            break;
    }
}

void can_receive_task(void *pvParameters)
{
    twai_message_t message;
    
    while (1) {
        if (twai_receive(&message, pdMS_TO_TICKS(100)) == ESP_OK) {
            // Process received message
            process_can_message(&message);
            
            // Log received data (debug only)
            ESP_LOGD("CAN", "ID:0x%03X DLC:%d Data:", 
                    message.identifier, message.data_length_code);
        } else {
            // No data received within timeout
            ESP_LOGW("CAN", "No CAN data received - check connection");
            
            // Update display to show "NO DATA" after 1 second timeout
            set_gauges_no_data_state();
        }
        
        vTaskDelay(pdMS_TO_TICKS(10));  // 100Hz polling rate
    }
}
```

---

## 🔍 Diagnostic и отладка CAN

### CAN Bus Health Monitoring:

```c
void monitor_can_bus_health(void)
{
    twai_status_info_t status;
    ESP_ERROR_CHECK(twai_get_status_info(&status));
    
    ESP_LOGI("CAN", "Bus State: %s", 
            (status.state == TWAI_STATE_RUNNING) ? "RUNNING" :
            (status.state == TWAI_STATE_BUS_OFF) ? "BUS_OFF" :
            (status.state == TWAI_STATE_STOPPED) ? "STOPPED" : "RECOVERING");
    
    ESP_LOGI("CAN", "TX Queue: %d/%d, RX Queue: %d/%d", 
            status.msgs_to_tx, 10, status.msgs_to_rx, 10);
    
    ESP_LOGI("CAN", "TX Errors: %d, RX Errors: %d", 
            status.tx_error_counter, status.rx_error_counter);
    
    // Check for bus-off condition
    if (status.state == TWAI_STATE_BUS_OFF) {
        ESP_LOGE("CAN", "Bus-off detected! Initiating recovery...");
        twai_initiate_recovery();
    }
}
```

### CAN Frame Logger:

```c
void log_can_frame(twai_message_t *message, bool transmitted)
{
    char direction = transmitted ? 'T' : 'R';
    char data_str[25] = {0};
    
    // Format data bytes as hex string
    for (int i = 0; i < message->data_length_code; i++) {
        sprintf(data_str + (i * 3), "%02X ", message->data[i]);
    }
    
    ESP_LOGI("CAN", "[%c] ID:0x%03X DLC:%d Data:[%s]", 
            direction, message->identifier, 
            message->data_length_code, data_str);
}
```

### Error Handling:

```c
void handle_can_errors(uint32_t alerts)
{
    if (alerts & TWAI_ALERT_ERR_PASS) {
        ESP_LOGW("CAN", "Error passive state entered");
    }
    
    if (alerts & TWAI_ALERT_BUS_ERROR) {
        ESP_LOGE("CAN", "Bus error detected");
        
        // Get detailed error information
        twai_status_info_t status;
        twai_get_status_info(&status);
        
        if (status.tx_error_counter > 96) {
            ESP_LOGE("CAN", "High TX error count: %d", status.tx_error_counter);
        }
        
        if (status.rx_error_counter > 96) {
            ESP_LOGE("CAN", "High RX error count: %d", status.rx_error_counter);
        }
    }
    
    if (alerts & TWAI_ALERT_TX_FAILED) {
        ESP_LOGE("CAN", "Message transmission failed");
    }
    
    if (alerts & TWAI_ALERT_RX_QUEUE_FULL) {
        ESP_LOGW("CAN", "RX queue full - messages lost");
    }
}
```

---

## 📈 Performance Optimization

### Efficient Message Processing:

```c
// Use lookup table for faster message processing
typedef void (*can_handler_t)(uint8_t *data);

typedef struct {
    uint32_t id;
    can_handler_t handler;
    uint32_t last_received;
    uint32_t timeout_ms;
} can_message_config_t;

static can_message_config_t can_messages[] = {
    {0x200, handle_boost_data,  0, 50},   // 50ms timeout
    {0x380, handle_engine_data, 0, 20},   // 20ms timeout  
    {0x440, handle_tcu_data,    0, 50},   // 50ms timeout
};

void process_can_message_optimized(twai_message_t *message)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    // Find handler using binary search for large arrays
    for (int i = 0; i < sizeof(can_messages)/sizeof(can_messages[0]); i++) {
        if (can_messages[i].id == message->identifier) {
            can_messages[i].handler(message->data);
            can_messages[i].last_received = now;
            return;
        }
    }
}

void check_can_timeouts(void)
{
    uint32_t now = xTaskGetTickCount() * portTICK_PERIOD_MS;
    
    for (int i = 0; i < sizeof(can_messages)/sizeof(can_messages[0]); i++) {
        if ((now - can_messages[i].last_received) > can_messages[i].timeout_ms) {
            ESP_LOGW("CAN", "Timeout for message 0x%03X", can_messages[i].id);
            // Set corresponding gauge to error state
        }
    }
}
```

---

## 🛡️ CAN Security Considerations

### Message Validation:

```c
bool validate_engine_data(uint8_t *data)
{
    uint16_t rpm = (data[0] << 8) | data[1];
    uint16_t map = (data[2] << 8) | data[3];
    uint8_t tps = data[4];
    
    // Sanity checks
    if (rpm > 8000) return false;           // RPM too high
    if (map > 3000) return false;           // MAP pressure impossible
    if (tps > 100) return false;            // TPS percentage invalid
    
    // Cross-validation
    if (rpm > 1000 && tps == 0) return false;  // RPM high but no throttle
    if (rpm < 500 && map > 150) return false;  // Low RPM but high MAP
    
    return true;
}
```

### Rate Limiting:

```c
#define MAX_MESSAGE_RATE_HZ 200
static uint32_t last_message_time[0x800] = {0};  // Array for all possible IDs

bool check_message_rate(uint32_t id)
{
    uint32_t now = esp_timer_get_time() / 1000;  // Get time in ms
    uint32_t min_interval = 1000 / MAX_MESSAGE_RATE_HZ;
    
    if ((now - last_message_time[id]) < min_interval) {
        ESP_LOGW("CAN", "Message 0x%03X rate too high", id);
        return false;
    }
    
    last_message_time[id] = now;
    return true;
}
```

---

**Файл:** CAN_PROTOCOL_SPECIFICATION.md  
**Версия:** 1.0  
**Дата:** 24 июля 2025  
**Стандарт:** ISO 11898-1 (CAN 2.0B)  
**Совместимость:** OBD-II, SAE J1939