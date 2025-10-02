# Исправление нестабильности SD карты

## Проблема
SD карта работает непостоянно - иногда сохраняет настройки успешно, иногда показывает "SD card is not initialized".

## Анализ логов

### Успешная работа:
```
I (13533) SD_CARD: File written
I (13533) SETTINGS_CONFIG: Settings saved to SD card successfully.
```

### Неуспешная работа:
```
E (13343) SETTINGS_CONFIG: SD card is not initialized! Cannot save settings.
E (14163) SETTINGS_CONFIG: SD card is not initialized! Cannot save settings.
```

## Причина
SD карта **периодически теряет инициализацию** из-за аппаратных проблем:
- Нестабильное питание
- Плохие контакты
- Помехи на SPI линиях
- Отсутствие pull-up резисторов

## Программное решение

### 1. Автоматическая переинициализация
При обнаружении неинициализированной SD карты система автоматически пытается её переинициализировать:

```c
if (!sd_card_is_initialized()) {
    ESP_LOGW(TAG, "SD card is not initialized! Attempting to reinitialize...");
    esp_err_t reinit_result = sd_card_init();
    if (reinit_result != ESP_OK) {
        ESP_LOGE(TAG, "Failed to reinitialize SD card!");
        return;
    } else {
        ESP_LOGI(TAG, "SD card reinitialized successfully");
    }
}
```

### 2. Улучшенная проверка состояния
Функция `sd_card_is_initialized()` теперь не только проверяет флаги, но и пытается реально обратиться к SD карте:

```c
DIR* test_dir = opendir(MOUNT_POINT);
if (test_dir == NULL) {
    ESP_LOGW(TAG, "SD card appears to be disconnected or unmounted");
    sd_card_initialized = false;  // Mark as uninitialized
    return false;
}
```

### 3. Безопасная переинициализация
При повторной инициализации система сначала корректно деинициализирует предыдущее состояние:

```c
if (sd_card_initialized) {
    ESP_LOGW(TAG, "SD card already initialized, deinitializing first...");
    sd_card_deinit();
    vTaskDelay(pdMS_TO_TICKS(100));  // Small delay
}
```

## Аппаратные рекомендации

### Обязательные улучшения:
1. **Pull-up резисторы 10кОм** на все SPI линии:
   - MISO (GPIO 13) → 3.3V
   - MOSI (GPIO 11) → 3.3V  
   - CLK (GPIO 12) → 3.3V
   - CS (GPIO 4) → 3.3V

2. **Стабильное питание**:
   - Отдельный LDO регулятор 3.3В для SD карты
   - Конденсаторы фильтрации: 100нФ + 10мкФ рядом с SD картой

3. **Качественные соединения**:
   - Минимальная длина проводов (< 10 см)
   - Экранированные кабели при необходимости
   - Качественная пайка контактов

### Проверка питания:
- Измерьте напряжение на SD карте осциллографом
- Убедитесь в отсутствии просадок при записи
- Проверьте стабильность при переключении экранов

## Ожидаемый результат

После внесения изменений в логах должно появляться:
```
W (xxxxx) SETTINGS_CONFIG: SD card is not initialized! Attempting to reinitialize...
I (xxxxx) SETTINGS_CONFIG: SD card reinitialized successfully
I (xxxxx) SETTINGS_CONFIG: Settings saved to SD card successfully.
```

Система станет **самовосстанавливающейся** - при потере соединения с SD картой она автоматически попытается переподключиться.

## Мониторинг

Следите за логами на предмет:
- Частоты переинициализации (не должно быть слишком часто)
- Успешности переинициализации
- Стабильности после аппаратных улучшений

Если переинициализация происходит очень часто - проблема определенно в аппаратной части.
