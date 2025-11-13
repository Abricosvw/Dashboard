/**
 * ESP32-S3-Touch-LCD-7 ОФИЦИАЛЬНАЯ КОНФИГУРАЦИЯ ПИНОВ
 * Источник: Официальная документация Espressif/Waveshare 
 * Обновлено: 3 августа 2025
 * Статус: ✅ ПРОВЕРЕНО И ПОДТВЕРЖДЕНО
 */

#ifndef OFFICIAL_PINOUT_CONFIG_H
#define OFFICIAL_PINOUT_CONFIG_H

/* ========================================================================
   I2C MASTER CONFIGURATION - ОФИЦИАЛЬНЫЕ ДАННЫЕ
   ======================================================================== */
#define I2C_MASTER_SCL_IO      9        /*!< GPIO для I2C clock */
#define I2C_MASTER_SDA_IO      8        /*!< GPIO для I2C data */
#define I2C_MASTER_NUM         0        /*!< I2C port номер */
#define I2C_MASTER_FREQ_HZ     400000   /*!< I2C частота 400kHz */
#define I2C_MASTER_TX_BUF_DISABLE 0     /*!< I2C master TX buffer отключен */
#define I2C_MASTER_RX_BUF_DISABLE 0     /*!< I2C master RX buffer отключен */
#define I2C_MASTER_TIMEOUT_MS  1000     /*!< Таймаут I2C операций */

/* ========================================================================
   GPIO RESET CONFIGURATION
   ======================================================================== */
#define GPIO_INPUT_IO_4        4        /*!< GPIO для управления сбросом touch */
#define GPIO_INPUT_PIN_SEL     (1ULL<<GPIO_INPUT_IO_4)

/* ========================================================================
   LCD ST7262 RGB DISPLAY CONFIGURATION (800x480)
   ======================================================================== */
#define EXAMPLE_LCD_PIXEL_CLOCK_HZ    (18 * 1000 * 1000)  /*!< 18MHz pixel clock */
#define EXAMPLE_LCD_BK_LIGHT_ON_LEVEL  1                   /*!< Подсветка активный уровень */
#define EXAMPLE_LCD_BK_LIGHT_OFF_LEVEL !EXAMPLE_LCD_BK_LIGHT_ON_LEVEL
#define EXAMPLE_PIN_NUM_BK_LIGHT      -1                   /*!< Backlight не используется */

/* LCD Control Signals */
#define EXAMPLE_PIN_NUM_HSYNC         46    /*!< Horizontal Sync */
#define EXAMPLE_PIN_NUM_VSYNC         3     /*!< Vertical Sync */
#define EXAMPLE_PIN_NUM_DE            5     /*!< Data Enable */
#define EXAMPLE_PIN_NUM_PCLK          7     /*!< Pixel Clock */
#define EXAMPLE_PIN_NUM_DISP_EN       -1    /*!< Display Enable не используется */

/* RGB565 Data Pins (16-bit) */
#define EXAMPLE_PIN_NUM_DATA0         14    /*!< B3 (Blue bit 3) */
#define EXAMPLE_PIN_NUM_DATA1         38    /*!< B4 (Blue bit 4) */
#define EXAMPLE_PIN_NUM_DATA2         18    /*!< B5 (Blue bit 5) */
#define EXAMPLE_PIN_NUM_DATA3         17    /*!< B6 (Blue bit 6) */
#define EXAMPLE_PIN_NUM_DATA4         10    /*!< B7 (Blue bit 7) */
#define EXAMPLE_PIN_NUM_DATA5         39    /*!< G2 (Green bit 2) */
#define EXAMPLE_PIN_NUM_DATA6         0     /*!< G3 (Green bit 3) */
#define EXAMPLE_PIN_NUM_DATA7         45    /*!< G4 (Green bit 4) */
#define EXAMPLE_PIN_NUM_DATA8         48    /*!< G5 (Green bit 5) */
#define EXAMPLE_PIN_NUM_DATA9         47    /*!< G6 (Green bit 6) */
#define EXAMPLE_PIN_NUM_DATA10        21    /*!< G7 (Green bit 7) */
#define EXAMPLE_PIN_NUM_DATA11        1     /*!< R3 (Red bit 3) */
#define EXAMPLE_PIN_NUM_DATA12        2     /*!< R4 (Red bit 4) */
#define EXAMPLE_PIN_NUM_DATA13        42    /*!< R5 (Red bit 5) */
#define EXAMPLE_PIN_NUM_DATA14        41    /*!< R6 (Red bit 6) */
#define EXAMPLE_PIN_NUM_DATA15        40    /*!< R7 (Red bit 7) */

/* LCD Resolution */
#define EXAMPLE_LCD_H_RES             800   /*!< Горизонтальное разрешение */
#define EXAMPLE_LCD_V_RES             480   /*!< Вертикальное разрешение */

/* Frame Buffer Configuration */
#if CONFIG_EXAMPLE_DOUBLE_FB
    #define EXAMPLE_LCD_NUM_FB        2     /*!< Двойная буферизация */
#else
    #define EXAMPLE_LCD_NUM_FB        1     /*!< Одинарная буферизация */
#endif

/* ========================================================================
   TOUCH CONTROLLER GT911 CONFIGURATION
   ======================================================================== */
#define TOUCH_I2C_ADDRESS_8BIT        0x38  /*!< GT911 I2C адрес (8-bit формат) */
#define TOUCH_RESET_GPIO              4     /*!< GPIO для аппаратного сброса */

/* ========================================================================
   GPIO EXPANDER CH422G CONFIGURATION  
   ======================================================================== */
#define CH422G_I2C_ADDRESS            0x24  /*!< CH422G 7-bit I2C адрес */

/* ========================================================================
   LVGL CONFIGURATION
   ======================================================================== */
#define EXAMPLE_LVGL_TICK_PERIOD_MS   2     /*!< LVGL тик каждые 2ms */
#define EXAMPLE_LVGL_TASK_MAX_DELAY_MS 500  /*!< Максимальная задержка задачи */
#define EXAMPLE_LVGL_TASK_MIN_DELAY_MS 1    /*!< Минимальная задержка задачи */
#define EXAMPLE_LVGL_TASK_STACK_SIZE  (4 * 1024) /*!< 4KB стек для LVGL задачи */
#define EXAMPLE_LVGL_TASK_PRIORITY    2     /*!< Приоритет задачи */

/* ========================================================================
   LCD TIMING PARAMETERS (ПРОВЕРЕНО С ОФИЦИАЛЬНОЙ ДОКУМЕНТАЦИЕЙ)
   ======================================================================== */
#define LCD_HSYNC_BACK_PORCH          8     /*!< Горизонтальная задняя пауза */
#define LCD_HSYNC_FRONT_PORCH         8     /*!< Горизонтальная передняя пауза */
#define LCD_HSYNC_PULSE_WIDTH         4     /*!< Ширина импульса HSYNC */
#define LCD_VSYNC_BACK_PORCH          16    /*!< Вертикальная задняя пауза */
#define LCD_VSYNC_FRONT_PORCH         16    /*!< Вертикальная передняя пауза */
#define LCD_VSYNC_PULSE_WIDTH         4     /*!< Ширина импульса VSYNC */

/* ========================================================================
   CAN INTERFACE CONFIGURATION (ТРЕБУЕТ УТОЧНЕНИЯ)
   ======================================================================== */
/* ВНИМАНИЕ: Пины для TJA1051 CAN transceiver требуют проверки в схеме */
#define CAN_TX_GPIO                   43    /* Требует подтверждения */
#define CAN_RX_GPIO                   44    /* Требует подтверждения */

#endif /* OFFICIAL_PINOUT_CONFIG_H */