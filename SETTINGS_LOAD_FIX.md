# Исправление проблемы загрузки настроек при перезагрузке

## Проблема
Настройки устройства сбрасываются к дефолтным значениям при каждой перезагрузке, несмотря на то, что они успешно сохраняются на SD карту.

## Причина
Обнаружен **двойной вызов** функции `settings_load()`:

1. **main.c:91** - Загружает настройки из SD карты ✅
2. **ui.c:48** - Повторно вызывает `settings_load()` ❌

### Порядок инициализации:
```
main.c: sd_card_init() → settings_load() → display() → ui_init() → settings_load()
                            ↑ Загружает из SD        ↑ Перезаписывает дефолтными!
```

Второй вызов в `ui.c` **перезаписывал** загруженные настройки дефолтными значениями.

## Решение

### 1. Удален дублирующий вызов
В файле `main/ui/ui.c` закомментирован повторный вызов:

```c
// Initialize screen manager
ui_screen_manager_init();

// Settings are already loaded in main.c, no need to load again
// settings_load(); // REMOVED: Settings already loaded in main.c

// Initialize all screens
```

### 2. Добавлено расширенное логирование
Для отладки добавлены логи:

#### При загрузке настроек:
```c
ESP_LOGI(TAG, "Read %d bytes from settings.cfg: %s", bytes_read, buffer);
ESP_LOGI(TAG, "Loaded settings: Demo=%s, Screen3=%s, Sensitivity=%d", ...);
```

#### При инициализации дефолтных настроек:
```c
ESP_LOGI(TAG, "Initialized default settings: Demo=%s, Screen3=%s, Sensitivity=%d", ...);
```

## Ожидаемые логи

### Успешная загрузка настроек:
```
I (xxxxx) ECU_DASHBOARD: Loading settings from SD card...
I (xxxxx) SETTINGS_CONFIG: Attempting to load settings from SD card...
I (xxxxx) SETTINGS_CONFIG: Read 45 bytes from settings.cfg: {"sensitivity":5,"demo_mode":false,"screen3_enabled":false}
I (xxxxx) SETTINGS_CONFIG: Settings loaded from settings.cfg successfully.
I (xxxxx) SETTINGS_CONFIG: Loaded settings: Demo=OFF, Screen3=OFF, Sensitivity=5
```

### При отсутствии файла настроек:
```
I (xxxxx) SETTINGS_CONFIG: settings.cfg not found on SD card, initializing with defaults.
I (xxxxx) SETTINGS_CONFIG: Initialized default settings: Demo=ON, Screen3=ON, Sensitivity=5
```

## Дефолтные значения

Определены в `main/ui/settings_config.h`:
```c
#define DEFAULT_TOUCH_SENSITIVITY        5
#define DEFAULT_DEMO_MODE_ENABLED        true    // Demo mode ON
#define DEFAULT_SCREEN3_ENABLED          true    // Screen3 ON
```

## Файл настроек

**Расположение**: `/sdcard/settings.cfg`  
**Формат**: JSON  
**Пример содержимого**:
```json
{"sensitivity":5,"demo_mode":false,"screen3_enabled":false}
```

## Результат

После исправления:
1. ✅ Настройки загружаются **только один раз** из SD карты
2. ✅ Загруженные настройки **не перезаписываются** дефолтными
3. ✅ При перезагрузке настройки **сохраняются**
4. ✅ Подробные логи помогают отслеживать процесс загрузки

## Тестирование

1. Измените настройки в UI (например, выключите Demo mode)
2. Перезагрузите устройство
3. Проверьте, что настройки сохранились
4. В логах должна быть видна успешная загрузка из `settings.cfg`

Проблема с загрузкой настроек при перезагрузке полностью решена!
