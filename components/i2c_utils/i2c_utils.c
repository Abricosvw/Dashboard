#include <stdio.h>
#include "esp_log.h"
#include "driver/i2c.h"
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "i2c_utils.h"
#include "sdkconfig.h"

static const char *TAG = "I2C_UTILS";

// This component will use the same I2C port as the main application.
// We assume I2C_NUM_0 is the one configured in main.c
#define I2C_PORT_FOR_TOOLS I2C_NUM_0

/* i2cget specific arguments */
static struct {
    struct arg_int *chip_address;
    struct arg_int *register_address;
    struct arg_end *end;
} i2cget_args;

/* i2cset specific arguments */
static struct {
    struct arg_int *chip_address;
    struct arg_int *register_address;
    struct arg_int *data;
    struct arg_end *end;
} i2cset_args;

static int i2c_scan_cmd(int argc, char **argv)
{
    printf("     0  1  2  3  4  5  6  7  8  9  a  b  c  d  e  f\r\n");
    for (int i = 0; i < 128; i += 16) {
        printf("%02x: ", i);
        for (int j = 0; j < 16; j++) {
            fflush(stdout);
            uint8_t address = i + j;
            if (address < 0x03 || address > 0x77) {
                printf("-- ");
                continue;
            }
            i2c_cmd_handle_t cmd = i2c_cmd_link_create();
            i2c_master_start(cmd);
            i2c_master_write_byte(cmd, (address << 1) | I2C_MASTER_WRITE, true);
            i2c_master_stop(cmd);
            esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_FOR_TOOLS, cmd, pdMS_TO_TICKS(50));
            i2c_cmd_link_delete(cmd);
            if (ret == ESP_OK) {
                printf("%02x ", address);
            } else {
                printf("-- ");
            }
        }
        printf("\r\n");
    }
    return 0;
}

static int i2c_get_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&i2cget_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cget_args.end, argv[0]);
        return 1;
    }

    uint8_t chip_addr = i2cget_args.chip_address->ival[0];
    uint8_t reg_addr = i2cget_args.register_address->ival[0];
    uint8_t data = 0;

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (chip_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (chip_addr << 1) | I2C_MASTER_READ, true);
    i2c_master_read_byte(cmd, &data, I2C_MASTER_NACK);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_FOR_TOOLS, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        printf("Read from 0x%02x at reg 0x%02x: 0x%02x\n", chip_addr, reg_addr, data);
    } else {
        printf("Failed to read from 0x%02x at reg 0x%02x. Error: %s\n", chip_addr, reg_addr, esp_err_to_name(ret));
    }
    return 0;
}

static int i2c_set_cmd(int argc, char **argv)
{
    int nerrors = arg_parse(argc, argv, (void **)&i2cset_args);
    if (nerrors != 0) {
        arg_print_errors(stderr, i2cset_args.end, argv[0]);
        return 1;
    }

    uint8_t chip_addr = i2cset_args.chip_address->ival[0];
    uint8_t reg_addr = i2cset_args.register_address->ival[0];
    uint8_t data = i2cset_args.data->ival[0];

    i2c_cmd_handle_t cmd = i2c_cmd_link_create();
    i2c_master_start(cmd);
    i2c_master_write_byte(cmd, (chip_addr << 1) | I2C_MASTER_WRITE, true);
    i2c_master_write_byte(cmd, reg_addr, true);
    i2c_master_write_byte(cmd, data, true);
    i2c_master_stop(cmd);
    esp_err_t ret = i2c_master_cmd_begin(I2C_PORT_FOR_TOOLS, cmd, pdMS_TO_TICKS(1000));
    i2c_cmd_link_delete(cmd);

    if (ret == ESP_OK) {
        printf("Wrote 0x%02x to 0x%02x at reg 0x%02x\n", data, chip_addr, reg_addr);
    } else {
        printf("Failed to write to 0x%02x at reg 0x%02x. Error: %s\n", chip_addr, reg_addr, esp_err_to_name(ret));
    }
    return 0;
}

void i2c_utils_register_commands(void)
{
    const esp_console_cmd_t i2c_scan_cmd_config = {
        .command = "i2cscan",
        .help = "Scan I2C bus for devices",
        .hint = NULL,
        .func = &i2c_scan_cmd,
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2c_scan_cmd_config));

    i2cget_args.chip_address = arg_int1(NULL, NULL, "<chip_addr>", "I2C address of the device");
    i2cget_args.register_address = arg_int1(NULL, NULL, "<reg_addr>", "Register address to read from");
    i2cget_args.end = arg_end(2);
    const esp_console_cmd_t i2c_get_cmd_config = {
        .command = "i2cget",
        .help = "Read from an I2C device register",
        .hint = NULL,
        .func = &i2c_get_cmd,
        .argtable = &i2cget_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2c_get_cmd_config));

    i2cset_args.chip_address = arg_int1(NULL, NULL, "<chip_addr>", "I2C address of the device");
    i2cset_args.register_address = arg_int1(NULL, NULL, "<reg_addr>", "Register address to write to");
    i2cset_args.data = arg_int1(NULL, NULL, "<data>", "Data to write");
    i2cset_args.end = arg_end(3);
    const esp_console_cmd_t i2c_set_cmd_config = {
        .command = "i2cset",
        .help = "Write to an I2C device register",
        .hint = NULL,
        .func = &i2c_set_cmd,
        .argtable = &i2cset_args
    };
    ESP_ERROR_CHECK(esp_console_cmd_register(&i2c_set_cmd_config));

    ESP_LOGI(TAG, "Registered I2C utility commands");
}