# ESP32-S3-Touch-LCD-7 Hardware Wiring Guide

## 🔌 Подробные схемы подключения

### ESP32-S3-Touch-LCD-7 Pinout Map

```
                    ESP32-S3-WROOM-1-N16R8
                           ┌─────────────┐
                     3V3 ──┤1          40├── GND
                  NC/GPIO0 ──┤2          39├── GPIO39 (LCD_HSYNC)
                    GPIO1 ──┤3          38├── GPIO38 (NC)
                    GPIO2 ──┤4          37├── GPIO37 (NC)
                    GPIO3 ──┤5          36├── GPIO36 (NC)
                    GPIO4 ──┤6          35├── GPIO35 (NC)
                    GPIO5 ──┤7          34├── GPIO34 (NC)
                    GPIO6 ──┤8          33├── GPIO33 (NC)
                    GPIO7 ──┤9          32├── GPIO32 (NC)
                    GPIO8 ──┤10         31├── GPIO31 (NC)
                    GPIO9 ──┤11         30├── GPIO30 (NC)
                   GPIO10 ──┤12         29├── GPIO29 (NC)
                   GPIO11 ──┤13         28├── GPIO28 (NC)
                   GPIO12 ──┤14         27├── GPIO27 (NC)
                   GPIO13 ──┤15         26├── GPIO26 (NC)
                   GPIO14 ──┤16         25├── GPIO25 (NC)
                   GPIO15 ──┤17         24├── GPIO24 (NC)
                   GPIO16 ──┤18         23├── GPIO23 (NC)
                   GPIO17 ──┤19         22├── GPIO22 (NC)
                   GPIO18 ──┤20         21├── GPIO21 (NC)
                           └─────────────┘
                                    
             Additional pins (не показанные выше):
             GPIO19 (Touch_SDA), GPIO20 (Touch_SCL)
             GPIO40 (LCD_VSYNC), GPIO41 (LCD_DE), GPIO42 (LCD_PCLK)
             GPIO43 (CAN_TX), GPIO44 (CAN_RX), GPIO45 (CAN_STBY)
             GPIO46 (LCD_D6)
```

### Детальная схема RGB LCD подключения

```
ST7262 RGB TFT Controller Connection:
┌─────────────────────────────────────┐
│           ST7262 Controller         │
├─────────────────────────────────────┤
│ Pin   │ Signal   │ ESP32-S3 GPIO    │
├─────────────────────────────────────┤
│ D0    │ LCD_D0   │ GPIO15           │
│ D1    │ LCD_D1   │ GPIO16           │
│ D2    │ LCD_D2   │ GPIO17           │
│ D3    │ LCD_D3   │ GPIO18           │
│ D4    │ LCD_D4   │ GPIO8            │
│ D5    │ LCD_D5   │ GPIO3            │
│ D6    │ LCD_D6   │ GPIO46           │
│ D7    │ LCD_D7   │ GPIO9            │
│ D8    │ LCD_D8   │ GPIO1            │
│ D9    │ LCD_D9   │ GPIO5            │
│ D10   │ LCD_D10  │ GPIO6            │
│ D11   │ LCD_D11  │ GPIO7            │
│ D12   │ LCD_D12  │ GPIO10           │
│ D13   │ LCD_D13  │ GPIO11           │
│ D14   │ LCD_D14  │ GPIO12           │
│ D15   │ LCD_D15  │ GPIO14           │
├─────────────────────────────────────┤
│ HSYNC │ LCD_HSYNC│ GPIO39           │
│ VSYNC │ LCD_VSYNC│ GPIO40           │
│ DE    │ LCD_DE   │ GPIO41           │
│ PCLK  │ LCD_PCLK │ GPIO42           │
│ BL    │ LCD_BL   │ GPIO4            │
├─────────────────────────────────────┤
│ VCC   │ Power    │ 3.3V             │
│ GND   │ Ground   │ GND              │
└─────────────────────────────────────┘

Timing Configuration:
- Resolution: 800x480 pixels
- Pixel Clock: 14MHz (для стабильности)
- HSYNC: Back=40, Front=48, Pulse=48
- VSYNC: Back=13, Front=3, Pulse=3
```

### GT911 Touch Controller подключение

```
GT911 Capacitive Touch Controller:
┌─────────────────────────────────────┐
│            GT911 Touch             │
├─────────────────────────────────────┤
│ Pin     │ Signal     │ ESP32-S3     │
├─────────────────────────────────────┤
│ SDA     │ I2C Data   │ GPIO19       │
│ SCL     │ I2C Clock  │ GPIO20       │
│ RST     │ Reset      │ NC (pullup)  │
│ INT     │ Interrupt  │ NC (pullup)  │
│ VCC     │ Power      │ 3.3V         │
│ GND     │ Ground     │ GND          │
└─────────────────────────────────────┘

I2C Configuration:
- Address: 0x5D (primary) / 0x14 (secondary)
- Clock Speed: 400kHz
- Pull-up resistors: 4.7kΩ на SDA/SCL (встроенные)
- Reset/Interrupt: подтянуты к VCC через 10kΩ
```

### CAN-bus интерфейс (TJA1051)

```
TJA1051T High-Speed CAN Transceiver:
┌─────────────────────────────────────┐
│           TJA1051T CAN              │
├─────────────────────────────────────┤
│ Pin     │ Signal     │ Connection   │
├─────────────────────────────────────┤
│ TXD     │ CAN TX     │ GPIO43       │
│ RXD     │ CAN RX     │ GPIO44       │
│ STB     │ Standby    │ GPIO45 (opt) │
│ CANH    │ CAN High   │ CAN Bus H    │
│ CANL    │ CAN Low    │ CAN Bus L    │
│ VCC     │ Power      │ 3.3V или 5V  │
│ GND     │ Ground     │ GND          │
└─────────────────────────────────────┘

Automotive CAN Bus Connection:
- CANH: High line (2.5V-4.0V)
- CANL: Low line (1.0V-2.5V) 
- Termination: 120Ω resistors на концах шины
- Скорость: 500 кбит/с (стандарт OBD-II)
- Максимальная длина: до 40м при 500 кбит/с
```

## 🔧 Монтажная схема

### Физическое размещение компонентов:

```
         ESP32-S3-Touch-LCD-7 Board Layout
    ┌─────────────────────────────────────────┐
    │  ┌─────┐     ┌──────────────────┐       │
    │  │USB-C│     │   7" TFT LCD     │  ┌──┐ │
    │  │ PWR │     │    800x480       │  │SD│ │
    │  └─────┘     │   ST7262 RGB     │  │  │ │
    │              │                  │  └──┘ │
    │  ┌─────┐     │                  │       │
    │  │ESP32│     │     GT911        │  ┌──┐ │
    │  │ S3  │     │    Touch         │  │  │ │
    │  │WROOM│     │                  │  │  │ │
    │  └─────┘     └──────────────────┘  └──┘ │
    │                                         │
    │  ┌─────┐  ┌─────┐  ┌─────┐  ┌─────┐    │
    │  │CH343│  │TJA  │  │CH422│  │UART │    │
    │  │USB  │  │1051 │  │GPIO │  │TTL  │    │
    │  │UART │  │ CAN │  │ EXP │  │     │    │
    │  └─────┘  └─────┘  └─────┘  └─────┘    │
    └─────────────────────────────────────────┘
```

### Разъемы и коннекторы:

```
Main Board Connectors:
┌────────────────────────────────────────┐
│ Connector  │ Type      │ Purpose        │
├────────────────────────────────────────┤
│ J1         │ USB-C     │ Power/Program  │
│ J2         │ 40-pin    │ GPIO Expansion │
│ J3         │ 4-pin     │ I2C Extension  │
│ J4         │ 3-pin     │ CAN Interface  │
│ J5         │ 6-pin     │ UART Debug     │
│ J6         │ 2-pin     │ External Power │
│ J7         │ MicroSD   │ Storage        │
└────────────────────────────────────────┘

Power Input Options:
1. USB-C: 5V, 2A (для разработки)
2. External: 7-36V DC (для автомобиля)
3. Battery: 3.7V Li-Po (резервное питание)
```

## ⚡ Электрические характеристики

### Напряжения питания:

```
Power Supply Tree:
                    ┌── 5V ──── USB-C Input
External 12V ────┤
                    └── 5V ──── DC-DC Converter
                               │
                               ├── 3.3V ── LDO ── ESP32-S3 Core
                               │                ├── GT911 Touch
                               │                └── TJA1051 CAN
                               │
                               ├── 5V ─────── LCD Backlight
                               └── 3.3V ──── LCD Logic
```

### Токи потребления:

```
Current Consumption @ 5V:
┌─────────────────────────────────────┐
│ Component      │ Typical │ Maximum  │
├─────────────────────────────────────┤
│ ESP32-S3 Core  │ 180mA   │ 240mA    │
│ LCD Display    │ 200mA   │ 250mA    │
│ LCD Backlight  │ 300mA   │ 450mA    │
│ GT911 Touch    │ 8mA     │ 12mA     │
│ TJA1051 CAN    │ 15mA    │ 25mA     │
│ Support Logic  │ 25mA    │ 40mA     │
├─────────────────────────────────────┤
│ TOTAL          │ 728mA   │ 1017mA   │
└─────────────────────────────────────┘

Рекомендуемый блок питания: 5V 2A (10W)
```

## 🛡️ Защита и надежность

### Схема защиты питания:

```
Protection Circuit:
Input ──┬── TVS Diode (защита от всплесков)
        │
        ├── Fuse 2A (защита от перегрузки)
        │
        ├── Reverse Polarity Protection
        │
        └── LC Filter ── Output
```

### ESD защита:

```
ESD Protection Points:
- USB connector: Built-in TVS arrays
- CAN interface: External TVS diodes
- Touch panel: ESD protection diodes
- GPIO expansion: Series resistors + clamp diodes
- Programming interface: Isolation buffers
```

### Температурная защита:

```
Operating Temperature Ranges:
┌─────────────────────────────────────┐
│ Component      │ Range     │ Grade   │
├─────────────────────────────────────┤
│ ESP32-S3       │-40~+85°C  │ Industrial │
│ ST7262 LCD     │-20~+70°C  │ Commercial │
│ GT911 Touch    │-20~+70°C  │ Commercial │
│ TJA1051 CAN    │-40~+125°C │ Automotive │
├─────────────────────────────────────┤
│ System Overall │-20~+70°C  │ Automotive │
└─────────────────────────────────────┘
```

## 🔧 Mechanical Specifications

### Размеры платы:

```
Physical Dimensions:
┌─────────────────────────────────────┐
│ Length: 195mm (7.68 inches)        │
│ Width:  110mm (4.33 inches)        │
│ Height: 15mm  (0.59 inches)        │
│ Weight: 285g  (10.05 oz)           │
└─────────────────────────────────────┘

Mounting Holes:
- 4x M3 holes in corners
- Center distance: 185x100mm
- Hole diameter: 3.2mm (for M3 screws)
```

### Корпус и монтаж:

```
Recommended Enclosure:
┌─────────────────────────────────────┐
│ Material: ABS Plastic или Aluminum │
│ IP Rating: IP54 (dust/splash proof)│
│ Mounting: VESA 100x100 compatible  │
│ Ventilation: Side slots recommended│
│ Cable entries: IP67 connectors     │
└─────────────────────────────────────┘

Installation Options:
1. Dashboard mount (3M adhesive + screws)
2. DIN rail mount (с дополнительным адаптером)
3. Panel mount (врезной в приборную панель)
4. Portable mount (на стойке с присосками)
```

## 📋 Контрольный список подключения

### Pre-Installation Checklist:

```
☐ 1. Питание
    ☐ 5V 2A блок питания проверен
    ☐ Полярность подключения проверена
    ☐ Заземление корпуса выполнено

☐ 2. LCD Display
    ☐ Все 16 data pins подключены правильно
    ☐ HSYNC, VSYNC, DE, PCLK подключены
    ☐ Backlight control (GPIO4) подключен
    ☐ Питание 3.3V на LCD подано

☐ 3. Touch Panel
    ☐ I2C SDA (GPIO19) подключен
    ☐ I2C SCL (GPIO20) подключен
    ☐ Pull-up резисторы установлены
    ☐ Питание 3.3V подано

☐ 4. CAN Interface
    ☐ TXD (GPIO43) подключен к TJA1051
    ☐ RXD (GPIO44) подключен к TJA1051
    ☐ CANH/CANL подключены к автомобильной шине
    ☐ Терминаторы 120Ω установлены на концах шины

☐ 5. Programming Interface
    ☐ USB-C кабель для программирования готов
    ☐ ESP-IDF v5.3 установлена и настроена
    ☐ COM-порт определен в системе
```

### Post-Installation Test Procedure:

```
1. Power-On Test:
   ☐ Подача питания 5V
   ☐ Измерение тока потребления (должно быть ~700mA)
   ☐ Проверка выходных напряжений 3.3V
   ☐ Включение подсветки дисплея (GPIO4 HIGH)

2. Display Test:
   ☐ Инициализация LVGL
   ☐ Отображение тестового паттерна
   ☐ Проверка всех цветов RGB
   ☐ Тест анимации (плавность 60Hz)

3. Touch Test:
   ☐ I2C сканирование (должен найти 0x5D)
   ☐ Калибровка координат касания
   ☐ Тест multi-touch (до 5 точек)
   ☐ Проверка чувствительности

4. CAN Test:
   ☐ Инициализация TWAI контроллера
   ☐ Проверка сопротивления шины (60Ом между CANH-CANL)
   ☐ Передача тестового сообщения
   ☐ Прием сообщений от ECU

5. System Integration Test:
   ☐ Запуск полного ECU Dashboard
   ☐ Проверка анимации всех 6 датчиков
   ☐ Тест обновления данных в реальном времени
   ☐ Проверка цветовых индикаций состояний
```

---

**Файл:** HARDWARE_WIRING_GUIDE.md  
**Версия:** 1.0  
**Дата:** 24 июля 2025  
**Совместимость:** ESP32-S3-Touch-LCD-7, ESP-IDF v5.3