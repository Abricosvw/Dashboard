# 🚗 ECU Dashboard - ESP32 + React + LVGL

## 📊 Полная система мониторинга ECU

Это интегрированная система мониторинга ECU с тремя компонентами:
1. **ESP32 LVGL Dashboard** - Локальный дисплей с анимированными датчиками
2. **WiFi WebSocket Server** - Сервер на ESP32 для передачи данных
3. **React Web Client** - Веб-дашборд для удаленного мониторинга

---

## 🔧 Адаптированные компоненты

### ✅ **ESP32 LVGL UI (Завершено)**
- **5 анимированных датчиков:**
  - 🌀 MAP Pressure (100-250 kPa) - голубой
  - 🚰 Wastegate (0-100%) - зеленый  
  - 🎛️ TPS Position (0-100%) - золотой
  - 🚗 Engine RPM (0-7000 RPM) - оранжевый
  - ⚡ Target Boost (100-250 kPa) - золотой

- **TCU Status индикатор:**
  - 🟢 Зеленый LED + "OK" (RPM < 4500)
  - 🟡 Желтый LED + "WARNING" (RPM 4500-5500)
  - 🔴 Красный LED + "ERROR" (RPM > 5500)

### ✅ **WiFi WebSocket Server (Завершено)**
- **WiFi Access Point:** `ECU_Dashboard` / `12345678`
- **IP адрес:** `192.168.4.1`
- **WebSocket endpoint:** `ws://192.168.4.1/ws`
- **HTTP API endpoints:**
  - `GET /api/ecu-data` - JSON данные ECU
  - `GET /api/datastream` - Лог событий
  - `GET /` - Простой HTML интерфейс

### ✅ **CAN-bus интеграция (Завершено)**
- **CAN TX/RX пины:** GPIO43/GPIO44 (ESP32-S3 TXD0/RXD0)
- **Скорость:** 500 kbps
- **Поддерживаемые CAN ID:**
  - `0x201` - Engine RPM
  - `0x202` - MAP Pressure  
  - `0x203` - TPS Position
  - `0x204` - Wastegate Position
  - `0x205` - Target Boost
  - `0x206` - TCU Status

### ✅ **React Web Client (Завершено)**
- **Компоненты:** Адаптированы под ESP32 WebSocket
- **Автоподключение:** К ESP32 по IP
- **Реальное время:** WebSocket обновления каждые 200ms
- **Настройки:** IP конфигурация ESP32

---

## 🚀 Как использовать

### 1. **Прошивка ESP32**
```bash
# В папке Dashboard/
idf.py build
idf.py -p COM13 flash monitor
```

### 2. **Подключение к WiFi**
- ESP32 создает точку доступа: `ECU_Dashboard`
- Пароль: `12345678`
- Подключитесь с телефона/компьютера

### 3. **Веб-дашборд**
- Откройте браузер: `http://192.168.4.1/`
- Или запустите React клиент и подключитесь к ESP32

### 4. **React веб-клиент**
```bash
# В папке client dashboard/
npm install
npm run dev
# Откройте http://localhost:3000/esp32-dashboard
```

---

## 📁 Структура файлов

```
Dashboard/
├── main/
│   ├── include/
│   │   ├── ecu_data.h        # Типы данных ECU
│   │   ├── wifi_server.h     # WiFi WebSocket сервер
│   │   └── canbus.h          # CAN-bus интерфейс
│   ├── ui/
│   │   └── screens/
│   │       ├── ui_Screen1.c  # LVGL дашборд с датчиками
│   │       └── ui_Screen1.h
│   ├── ecu_data.c            # Реализация данных ECU
│   ├── wifi_server.c         # WiFi сервер
│   ├── canbus.c              # CAN-bus реализация
│   └── main.c                # Основной файл
│
└── client dashboard/
    ├── src/
    │   ├── hooks/
    │   │   └── useESP32WebSocket.ts    # ESP32 WebSocket хук
    │   ├── pages/
    │   │   └── ESP32Dashboard.tsx      # Веб-дашборд
    │   └── components/
    │       └── Gauge.tsx               # React датчики
```

---

## ⚙️ Конфигурация

### **ESP32 настройки**
В `main.c` измените:
```c
// Использовать реальные CAN данные вместо симуляции
static bool use_real_canbus = true;  // false для симуляции
```

### **WiFi настройки**
В `wifi_server.c`:
```c
static const char* DEFAULT_SSID = "Ваше_Имя_WiFi";
static const char* DEFAULT_PASSWORD = "ВашПароль";
```

### **CAN-bus пины**
В `canbus.h`:
```c
#define CAN_TX_PIN GPIO_NUM_43  // ESP32-S3 TXD0 пин
#define CAN_RX_PIN GPIO_NUM_44  // ESP32-S3 RXD0 пин
```

---

## 🔗 Endpoints API

### **WebSocket**
- **URL:** `ws://192.168.4.1/ws`
- **Данные:** JSON ECU данные каждые 200ms
- **Пинг/Понг:** для проверки соединения

### **HTTP REST API**
- **GET /api/ecu-data**
  ```json
  {
    "mapPressure": 180.5,
    "wastegatePosition": 45.2,
    "tpsPosition": 67.8,
    "engineRpm": 3250.0,
    "targetBoost": 200.0,
    "tcuProtectionActive": false,
    "tcuLimpMode": false,
    "torqueRequest": 85.5,
    "timestamp": 1634567890123
  }
  ```

- **GET /api/datastream**
  ```json
  [
    {
      "timestamp": 1634567890123,
      "message": "ECU Dashboard started",
      "type": "success"
    }
  ]
  ```

---

## 🎯 Возможности системы

### **Реального времени мониторинг**
- ⚡ Обновления каждые 100ms (LVGL)
- 🌐 WebSocket передача каждые 200ms
- 📊 Плавные анимации датчиков

### **Безопасность**
- 🚨 Автоматические предупреждения при превышении лимитов
- 🛡️ TCU protection monitoring
- 📋 Лог всех событий

### **Гибкость**
- 🔄 Переключение между симуляцией и реальными CAN данными
- 📱 Responsive веб-интерфейс
- ⚙️ Настраиваемые датчики и лимиты

---

## 🚨 Важные замечания

### **Распиновка оборудования**
⚠️ **ОБЯЗАТЕЛЬНО проверьте:**
- CAN TX/RX пины (GPIO43/GPIO44 для ESP32-S3)
- Распиновку дисплея
- Touch контроллер GT911 настройки

### **Безопасность**
- Используйте только для мониторинга
- НЕ изменяйте ECU параметры через эту систему
- Проверяйте все соединения перед использованием

### **Производительность**
- ESP32 поддерживает до 10 WebSocket клиентов
- Рекомендуется использовать ESP32-S3 для лучшей производительности

---

## 📞 Поддержка

Если есть вопросы по адаптации или настройке:
1. Проверьте лог ESP32 через `idf.py monitor`
2. Убедитесь в правильности распиновки
3. Проверьте WiFi подключение
4. Проверьте WebSocket соединение в браузере (F12 → Network)

**Успешной настройки ECU Dashboard! 🚗💨**
