#include "sd_card.h"
#include "driver/i2c_master.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "example";

// ✅ NEW: CH422G I2C Expander Constants (eliminates magic numbers)
#define CH422G_CFG_ADDR         0x24    // CH422G configuration register I2C address
#define CH422G_DATA_ADDR        0x38    // CH422G data register I2C address
#define CH422G_INIT_CFG         0x01    // CH422G initialization configuration value
#define CH422G_DEFAULT_STATE    0x2E    // Default EXIO state after display initialization
#define CH422G_SD_CS_BIT        0x10    // EXIO4 bit controls SD card CS (Chip Select)

sdmmc_card_t *card;
const char mount_point[] = MOUNT_POINT;

// By default, SD card frequency is initialized to SDMMC_FREQ_DEFAULT (20MHz)
// For setting a specific frequency, use host.max_freq_khz (range 400kHz - 20MHz for SDSPI)
// Example: for fixed frequency of 10MHz, use host.max_freq_khz = 10000;
sdmmc_host_t host = SDSPI_HOST_DEFAULT();

// CH422G helper state for EXIO outputs (EXIO4 controls SD CS on Waveshare board)
static SemaphoreHandle_t ch422g_mutex = NULL;
static i2c_master_dev_handle_t ch422g_dev_handle = NULL;
static i2c_master_dev_handle_t ch422g_cfg_handle = NULL;
static bool ch422g_state_initialized = false;
static uint8_t ch422g_exio_shadow = CH422G_DEFAULT_STATE; // ✅ FIXED: Using named constant

static esp_err_t ch422g_ensure_device(void)
{
    if (ch422g_mutex == NULL) {
        ch422g_mutex = xSemaphoreCreateMutex();
        if (ch422g_mutex == NULL) {
            ESP_LOGE(TAG, "Failed to create CH422G mutex");
            return ESP_ERR_NO_MEM;
        }
    }

    if (ch422g_dev_handle == NULL || ch422g_cfg_handle == NULL) {
        extern i2c_master_bus_handle_t display_get_i2c_bus_handle(void);
        i2c_master_bus_handle_t i2c_bus = display_get_i2c_bus_handle();
        if (i2c_bus == NULL) {
            ESP_LOGE(TAG, "I2C bus handle is NULL - cannot control CH422G");
            return ESP_FAIL;
        }

        if (ch422g_cfg_handle == NULL) {
            i2c_device_config_t cfg_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = CH422G_CFG_ADDR, // ✅ FIXED: Using named constant
                .scl_speed_hz = 100000,
            };

            esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &cfg_cfg, &ch422g_cfg_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add CH422G config device: %s", esp_err_to_name(ret));
                return ret;
            }
        }

        if (ch422g_dev_handle == NULL) {
            i2c_device_config_t data_cfg = {
                .dev_addr_length = I2C_ADDR_BIT_LEN_7,
                .device_address = CH422G_DATA_ADDR, // ✅ FIXED: Using named constant
                .scl_speed_hz = 100000,
            };

            esp_err_t ret = i2c_master_bus_add_device(i2c_bus, &data_cfg, &ch422g_dev_handle);
            if (ret != ESP_OK) {
                ESP_LOGE(TAG, "Failed to add CH422G data device: %s", esp_err_to_name(ret));
                return ret;
            }
        }
    }

    if (!ch422g_state_initialized) {
        uint8_t init_cfg = CH422G_INIT_CFG; // ✅ FIXED: Using named constant
        esp_err_t ret = i2c_master_transmit(ch422g_cfg_handle, &init_cfg, 1, 100 / portTICK_PERIOD_MS);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "Failed to configure CH422G direction register: %s", esp_err_to_name(ret));
        }

        uint8_t current_state = ch422g_exio_shadow;
        ret = i2c_master_receive(ch422g_dev_handle, &current_state, 1, 100 / portTICK_PERIOD_MS);
        if (ret == ESP_OK) {
            ch422g_exio_shadow = current_state;
            ESP_LOGD(TAG, "CH422G EXIO shadow initialised to 0x%02x", ch422g_exio_shadow);
        } else {
            ESP_LOGW(TAG, "Unable to read CH422G EXIO state (err=%s), using default 0x%02x", esp_err_to_name(ret), ch422g_exio_shadow);
        }

        ch422g_state_initialized = true;
    }

    return ESP_OK;
}

static esp_err_t ch422g_write_shadow(void)
{
    esp_err_t ret = i2c_master_transmit(ch422g_dev_handle, &ch422g_exio_shadow, 1, 100 / portTICK_PERIOD_MS);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to update CH422G EXIO state: %s", esp_err_to_name(ret));
    }
    return ret;
}

esp_err_t s_example_write_file(const char *path, char *data)
{
    // Select SD card (CS LOW) before file operations
    sd_cs_set_low();
    vTaskDelay(pdMS_TO_TICKS(2)); // ✅ OPTIMIZED: Reduced from 10ms to 2ms (CH422G I2C response)
    
    // Opening file
    ESP_LOGW(TAG, "Opening file %s", path);
    FILE *f = fopen(path, "w");
    if (f == NULL)
    {
        // Failed to open file for writing
        ESP_LOGW(TAG, "Failed to open file for writing");
        sd_cs_set_high(); // Deselect on error
        return ESP_FAIL;
    }
    // Write data to file
    fprintf(f, data);
    fclose(f);
    // File written
    ESP_LOGW(TAG, "File written");
    
    // ✅ OPTIMIZED: Reduced from 50ms to 5ms (FATFS operations on modern SD cards are fast)
    vTaskDelay(pdMS_TO_TICKS(5));
    sd_cs_set_high();

    return ESP_OK;
}

esp_err_t s_example_read_file(const char *path)
{
    // Select SD card (CS LOW) before file operations
    sd_cs_set_low();
    vTaskDelay(pdMS_TO_TICKS(2)); // ✅ OPTIMIZED: Reduced from 10ms to 2ms
    
    // Reading file
    ESP_LOGW(TAG, "Reading file %s", path);
    FILE *f = fopen(path, "r");
    if (f == NULL)
    {
        // Failed to open file for reading
        ESP_LOGW(TAG, "Failed to open file for reading");
        sd_cs_set_high(); // Deselect on error
        return ESP_FAIL;
    }
    // Read a line from the file
    char line[EXAMPLE_MAX_CHAR_SIZE];
    fgets(line, sizeof(line), f);
    fclose(f);

    // Strip newline
    char *pos = strchr(line, '\n');
    if (pos)
    {
        *pos = '\0';
    }
    // Read from file
    ESP_LOGW(TAG, "Read from file: '%s'", line);
    
    // ✅ OPTIMIZED: Reduced from 50ms to 5ms
    vTaskDelay(pdMS_TO_TICKS(5));
    sd_cs_set_high();

    return ESP_OK;
}

// Helper function to open file with CS control
FILE* sd_fopen_with_cs(const char *path, const char *mode)
{
    sd_cs_set_low();
    vTaskDelay(pdMS_TO_TICKS(2)); // ✅ OPTIMIZED: Reduced from 10ms to 2ms
    FILE *f = fopen(path, mode);
    if (f == NULL) {
        vTaskDelay(pdMS_TO_TICKS(2)); // ✅ OPTIMIZED: Reduced from 10ms to 2ms
        sd_cs_set_high(); // Deselect on error
    }
    return f;
}

// Helper function to close file with CS control
void sd_fclose_with_cs(FILE *f)
{
    if (f != NULL) {
        fclose(f);
    }
    // ✅ OPTIMIZED: Reduced from 50ms to 5ms
    vTaskDelay(pdMS_TO_TICKS(5));
    sd_cs_set_high();
}

// i2c_master_init() function removed - I2C initialization is now handled by display driver

esp_err_t waveshare_sd_card_init()
{
    esp_err_t ret;
    // Note: I2C is already initialized by the display driver, so we skip i2c_master_init()
    
    // Control CH422G to pull down the CS pin of the SD
    // Note: These commands are now handled by the display driver during its initialization
    // The display driver already sends the same commands to CH422G (0x24 and 0x38)
    ESP_LOGI(TAG, "CH422G control for SD card is handled by display driver");
    
    // Ensure SD_CS is properly controlled via CH422G
    ESP_LOGI(TAG, "Setting SD_CS to HIGH (deselect) via CH422G");
    sd_cs_set_high();  // Deselect SD card initially

    // Options for mounting the filesystem
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
#ifdef CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED
        .format_if_mount_failed = true, // If mount fails, format the card
#else
        .format_if_mount_failed = false, // If mount fails, do not format card
#endif
        .max_files = 5,                   // Maximum number of files
        .allocation_unit_size = 16 * 1024 // Set allocation unit size
    };

    // Initializing SD card
    ESP_LOGW(TAG, "Initializing SD card");

    // Configure SPI bus for SD card configuration
    spi_bus_config_t bus_cfg = {
        .mosi_io_num = PIN_NUM_MOSI, // Set MOSI pin
        .miso_io_num = PIN_NUM_MISO, // Set MISO pin
        .sclk_io_num = PIN_NUM_CLK,  // Set SCLK pin
        .quadwp_io_num = -1,         // Not used
        .quadhd_io_num = -1,         // Not used
        .max_transfer_sz = 4000,     // Maximum transfer size
    };
    // Initialize SPI bus
    ret = spi_bus_initialize(host.slot, &bus_cfg, SDSPI_DEFAULT_DMA);
    if (ret != ESP_OK)
    {
        // Failed to initialize bus
        ESP_LOGW(TAG, "Failed to initialize bus.");
        return ESP_FAIL;
    }

    // Configure SD card slot
    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_NUM_CS; // Set CS pin
    slot_config.host_id = host.slot;  // Set host ID

    // Lower the SPI clock for improved stability on shared buses
    host.max_freq_khz = 8000; // 8 MHz
    // Ensure SD_CS is LOW before mounting (select SD card)
    sd_cs_set_low();
    vTaskDelay(pdMS_TO_TICKS(10)); // Small delay for CH422G to respond
    
    // Mounting filesystem
    ESP_LOGW(TAG, "Mounting filesystem");
    ret = esp_vfs_fat_sdspi_mount(mount_point, &host, &slot_config, &mount_config, &card);

    if (ret != ESP_OK)
    {
        if (ret == ESP_FAIL)
        {
            // Failed to mount filesystem
            ESP_LOGW(TAG, "Failed to mount filesystem. "
                          "If you want the card to be formatted, set the CONFIG_EXAMPLE_FORMAT_IF_MOUNT_FAILED menuconfig option.");
        }
        else
        {
            // Failed to initialize the card
            ESP_LOGW(TAG, "Failed to initialize the card (%s). "
                          "Make sure SD card lines have pull-up resistors in place.",
                     esp_err_to_name(ret));
        }
        sd_cs_set_high();
        return ESP_FAIL;
    }

    // Filesystem mounted
    ESP_LOGW(TAG, "Filesystem mounted");
    sd_cs_set_high();
    return ESP_OK;
}

esp_err_t waveshare_sd_card_test()
{
    esp_err_t ret;

    // Card has been initialized, print its properties
    sdmmc_card_print_info(stdout, card);

    // Use POSIX and C standard library functions to work with files

    // First create a file
    const char *file_hello = MOUNT_POINT "/hello.txt";
    char data[EXAMPLE_MAX_CHAR_SIZE];
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Hello", card->cid.name);
    // Write data to file
    ret = s_example_write_file(file_hello, data);
    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    const char *file_foo = MOUNT_POINT "/foo.txt";

    // Check if destination file exists before renaming
    struct stat st;
    if (stat(file_foo, &st) == 0)
    {
        // Delete it if it exists
        unlink(file_foo);
    }

    // Rename original file
    ESP_LOGW(TAG, "Renaming file %s to %sv", file_hello, file_foo);
    if (rename(file_hello, file_foo) != 0)
    {
        // Rename failed
        ESP_LOGW(TAG, "Rename failed");
        return ESP_FAIL;
    }

    // Read renamed file
    ret = s_example_read_file(file_foo);
    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

       // Format FATFS
#ifdef CONFIG_EXAMPLE_FORMAT_SD_CARD
    ret = esp_vfs_fat_sdcard_format(mount_point, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to format FATFS (%s)", esp_err_to_name(ret));
        return;
    }

    if (stat(file_foo, &st) == 0) {
        ESP_LOGI(TAG, "file still exists");
        return;
    } else {
        ESP_LOGI(TAG, "file doesn't exist, formatting done");
    }
#endif // CONFIG_EXAMPLE_FORMAT_SD_CARD

    // Create a new file "nihao.txt" after formatting
    const char *file_nihao = MOUNT_POINT "/nihao.txt";
    memset(data, 0, EXAMPLE_MAX_CHAR_SIZE);                                     // Clear the data buffer
    snprintf(data, EXAMPLE_MAX_CHAR_SIZE, "%s %s!\n", "Nihao", card->cid.name); // Writing data
    ret = s_example_write_file(file_nihao, data);                               // Writing data to a file
    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Open and read the newly created file
    ret = s_example_read_file(file_nihao);
    if (ret != ESP_OK)
    {
        return ESP_FAIL;
    }

    // Keep SD card mounted for settings and other file operations
    // Do NOT unmount - card should remain available for settings save/load
    // Do NOT free SPI bus - SD card needs to remain active
    ESP_LOGI(TAG, "SD card test completed - keeping card mounted and SPI bus active for application use");
    sd_cs_set_high();
    
    return ESP_OK;
}

// CH422G SD_CS control functions implementation
esp_err_t sd_cs_set_high(void) {
    // Get I2C bus handle from display driver
    esp_err_t ret = ch422g_ensure_device();
    if (ret != ESP_OK) {
        return ret;
    }

    if (xSemaphoreTake(ch422g_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take CH422G mutex for SD_CS set HIGH");
        return ESP_ERR_TIMEOUT;
    }

    ch422g_exio_shadow |= CH422G_SD_CS_BIT; // ✅ FIXED: Using named constant
    ret = ch422g_write_shadow();

    xSemaphoreGive(ch422g_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SD_CS set HIGH (deselect) - CH422G EXIO4");
    }

    return ret;
}

esp_err_t sd_cs_set_low(void) {
    esp_err_t ret = ch422g_ensure_device();
    if (ret != ESP_OK) {
        return ret;
    }

    if (xSemaphoreTake(ch422g_mutex, pdMS_TO_TICKS(50)) != pdTRUE) {
        ESP_LOGE(TAG, "Failed to take CH422G mutex for SD_CS set LOW");
        return ESP_ERR_TIMEOUT;
    }

    ch422g_exio_shadow &= ~CH422G_SD_CS_BIT; // ✅ FIXED: Using named constant
    ret = ch422g_write_shadow();

    xSemaphoreGive(ch422g_mutex);

    if (ret == ESP_OK) {
        ESP_LOGI(TAG, "SD_CS set LOW (select) - CH422G EXIO4");
    }

    return ret;
}