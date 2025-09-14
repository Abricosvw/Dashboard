# Исправление ошибки SPI Bus Already Initialized

## Проблема
При попытке переинициализации SD карты возникает ошибка:
```
E (9173) spi: spi_bus_initialize(800): SPI bus already initialized.
E (9183) SD_CARD: Failed to initialize bus.
E (9193) SETTINGS_CONFIG: Failed to reinitialize SD card! Error: ESP_ERR_INVALID_STATE
```

## Причина
При деинициализации SD карты SPI шина не освобождалась корректно, что приводило к конфликту при повторной инициализации.

## Решение

### 1. Улучшенная обработка SPI шины
Теперь система корректно обрабатывает случай, когда SPI шина уже инициализирована:

```c
ret = spi_bus_initialize(s_host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
if (ret != ESP_OK) {
    if (ret == ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "SPI bus already initialized, continuing...");
        // This is OK, bus is already initialized
    } else {
        ESP_LOGE(TAG, "Failed to initialize bus: %s", esp_err_to_name(ret));
        return ret;
    }
}
```

### 2. Разделение деинициализации
Создано две функции деинициализации:

#### `sd_card_deinit()` - для переинициализации
- Размонтирует файловую систему
- Удаляет мьютекс
- **НЕ освобождает SPI шину** (для повторного использования)

#### `sd_card_full_deinit()` - для полного завершения
- Выполняет обычную деинициализацию
- Дополнительно освобождает SPI шину
- Используется только при полном завершении работы

### 3. Увеличенная задержка
Увеличена задержка между деинициализацией и инициализацией с 100мс до 200мс для более надежной очистки ресурсов.

## Логика работы

### При переинициализации:
1. `sd_card_deinit()` - размонтирует FS, сохраняет SPI шину
2. Задержка 200мс
3. `sd_card_init()` - использует существующую SPI шину

### При полном завершении:
1. `sd_card_full_deinit()` - полная очистка включая SPI шину

## Ожидаемые логи

### Успешная переинициализация:
```
W (xxxxx) SETTINGS_CONFIG: SD card is not initialized! Attempting to reinitialize...
I (xxxxx) SD_CARD: Initializing SD card
W (xxxxx) SD_CARD: SPI bus already initialized, continuing...
I (xxxxx) SD_CARD: Filesystem mounted
I (xxxxx) SETTINGS_CONFIG: SD card reinitialized successfully
I (xxxxx) SETTINGS_CONFIG: Settings saved to SD card successfully.
```

### При первой инициализации:
```
I (xxxxx) SD_CARD: Initializing SD card
I (xxxxx) SD_CARD: Initializing SPI bus...
I (xxxxx) SD_CARD: Filesystem mounted
```

## Преимущества решения

1. **Стабильность**: Нет конфликтов SPI шины при переинициализации
2. **Эффективность**: SPI шина переиспользуется, не тратится время на пересоздание
3. **Надежность**: Корректная обработка всех состояний SPI шины
4. **Гибкость**: Два варианта деинициализации для разных сценариев

## Результат
Система теперь может стабильно переинициализировать SD карту при потере соединения без ошибок SPI шины.
