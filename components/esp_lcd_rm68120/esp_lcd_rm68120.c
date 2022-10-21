/*
 * SPDX-FileCopyrightText: 2021-2022 Espressif Systems (Shanghai) CO LTD
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <stdlib.h>
#include <sys/cdefs.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_panel_interface.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_commands.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "esp_check.h"

static const char *TAG = "rm68120";

static esp_err_t panel_rm68120_del(esp_lcd_panel_t *panel);
static esp_err_t panel_rm68120_reset(esp_lcd_panel_t *panel);
static esp_err_t panel_rm68120_init(esp_lcd_panel_t *panel);
static esp_err_t panel_rm68120_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data);
static esp_err_t panel_rm68120_invert_color(esp_lcd_panel_t *panel, bool invert_color_data);
static esp_err_t panel_rm68120_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y);
static esp_err_t panel_rm68120_swap_xy(esp_lcd_panel_t *panel, bool swap_axes);
static esp_err_t panel_rm68120_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap);
static esp_err_t panel_rm68120_disp_on_off(esp_lcd_panel_t *panel, bool off);

typedef struct {
    esp_lcd_panel_t base;
    esp_lcd_panel_io_handle_t io;
    int reset_gpio_num;
    bool reset_level;
    int x_gap;
    int y_gap;
    unsigned int bits_per_pixel;
    uint8_t madctl_val; // save current value of LCD_CMD_MADCTL register
    uint8_t colmod_cal; // save surrent value of LCD_CMD_COLMOD register
} rm68120_panel_t;

esp_err_t esp_lcd_new_panel_rm68120(const esp_lcd_panel_io_handle_t io, const esp_lcd_panel_dev_config_t *panel_dev_config, esp_lcd_panel_handle_t *ret_panel)
{
    esp_err_t ret = ESP_OK;
    rm68120_panel_t *rm68120 = NULL;
    ESP_GOTO_ON_FALSE(io && panel_dev_config && ret_panel, ESP_ERR_INVALID_ARG, err, TAG, "invalid argument");
    rm68120 = calloc(1, sizeof(rm68120_panel_t));
    ESP_GOTO_ON_FALSE(rm68120, ESP_ERR_NO_MEM, err, TAG, "no mem for rm68120 panel");

    if (panel_dev_config->reset_gpio_num >= 0) {
        gpio_config_t io_conf = {
            .mode = GPIO_MODE_OUTPUT,
            .pin_bit_mask = 1ULL << panel_dev_config->reset_gpio_num,
        };
        ESP_GOTO_ON_ERROR(gpio_config(&io_conf), err, TAG, "configure GPIO for RST line failed");
    }

    switch (panel_dev_config->color_space) {
    case ESP_LCD_COLOR_SPACE_RGB:
        rm68120->madctl_val = 0;
        break;
    case ESP_LCD_COLOR_SPACE_BGR:
        rm68120->madctl_val |= LCD_CMD_BGR_BIT;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported color space");
        break;
    }

    switch (panel_dev_config->bits_per_pixel) {
    case 16:
        rm68120->colmod_cal = 0x55;
        break;
    case 18:
        rm68120->colmod_cal = 0x66;
        break;
    default:
        ESP_GOTO_ON_FALSE(false, ESP_ERR_NOT_SUPPORTED, err, TAG, "unsupported pixel width");
        break;
    }

    rm68120->io = io;
    rm68120->bits_per_pixel = panel_dev_config->bits_per_pixel;
    rm68120->reset_gpio_num = panel_dev_config->reset_gpio_num;
    rm68120->reset_level = panel_dev_config->flags.reset_active_high;
    rm68120->base.del = panel_rm68120_del;
    rm68120->base.reset = panel_rm68120_reset;
    rm68120->base.init = panel_rm68120_init;
    rm68120->base.draw_bitmap = panel_rm68120_draw_bitmap;
    rm68120->base.invert_color = panel_rm68120_invert_color;
    rm68120->base.set_gap = panel_rm68120_set_gap;
    rm68120->base.mirror = panel_rm68120_mirror;
    rm68120->base.swap_xy = panel_rm68120_swap_xy;
#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    rm68120->base.disp_off = panel_rm68120_disp_on_off;
#else
    rm68120->base.disp_on_off = panel_rm68120_disp_on_off;
#endif
    *ret_panel = &(rm68120->base);
    ESP_LOGD(TAG, "new rm68120 panel @%p", rm68120);

    return ESP_OK;

err:
    if (rm68120) {
        if (panel_dev_config->reset_gpio_num >= 0) {
            gpio_reset_pin(panel_dev_config->reset_gpio_num);
        }
        free(rm68120);
    }
    return ret;
}

static esp_err_t panel_rm68120_del(esp_lcd_panel_t *panel)
{
    rm68120_panel_t *rm68120 = __containerof(panel, rm68120_panel_t, base);

    if (rm68120->reset_gpio_num >= 0) {
        gpio_reset_pin(rm68120->reset_gpio_num);
    }
    ESP_LOGD(TAG, "del rm68120 panel @%p", rm68120);
    free(rm68120);
    return ESP_OK;
}

static esp_err_t panel_rm68120_reset(esp_lcd_panel_t *panel)
{
    rm68120_panel_t *rm68120 = __containerof(panel, rm68120_panel_t, base);
    esp_lcd_panel_io_handle_t io = rm68120->io;

    // perform hardware reset
    if (rm68120->reset_gpio_num >= 0) {
        gpio_set_level(rm68120->reset_gpio_num, rm68120->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
        gpio_set_level(rm68120->reset_gpio_num, !rm68120->reset_level);
        vTaskDelay(pdMS_TO_TICKS(10));
    } else { // perform software reset
        esp_lcd_panel_io_tx_param(io, LCD_CMD_SWRESET, NULL, 0);
        vTaskDelay(pdMS_TO_TICKS(20)); // spec, wait at least 5ms before sending new command
    }

    return ESP_OK;
}

typedef struct {
    uint16_t cmd;
    uint8_t data[1];
    uint8_t data_bytes; // Length of data in above data array; 0xFF = end of cmds.
} lcd_init_cmd_t;

static const lcd_init_cmd_t vendor_specific_init[] = {
    /* Software reset */
	{0x0100, {0x00}, 0},

    /* Sleep out */
    {0x1100, {0x00}, 0},

    /* Memory Access Data Control - rotation */
    {0x3600, {0xA3}, 1},

    /* Pixel format - 16bit RGB656 */
    {0x3A00, {0x55}, 1},

    /* Display on */
    {0x2900, {0x00}, 0},

    /* End */
	{0, {0}, 0xff},
};

static esp_err_t panel_rm68120_init(esp_lcd_panel_t *panel)
{
    rm68120_panel_t *rm68120 = __containerof(panel, rm68120_panel_t, base);
    esp_lcd_panel_io_handle_t io = rm68120->io;

    // LCD goes into sleep mode and display will be turned off after power on reset, exit sleep mode first
    /*esp_lcd_panel_io_tx_param(io, LCD_CMD_SLPOUT, NULL, 0);
    vTaskDelay(pdMS_TO_TICKS(100));
    esp_lcd_panel_io_tx_param(io, LCD_CMD_MADCTL, (uint8_t[]) {
        rm68120->madctl_val,
    }, 1);
    esp_lcd_panel_io_tx_param(io, LCD_CMD_COLMOD, (uint8_t[]) {
        rm68120->colmod_cal,
    }, 1);*/

    // vendor specific initialization, it can be different between manufacturers
    // should consult the LCD supplier for initialization sequence code
    int cmd = 0;
    while (vendor_specific_init[cmd].data_bytes != 0xff) {
        esp_lcd_panel_io_tx_param(io, vendor_specific_init[cmd].cmd, vendor_specific_init[cmd].data, vendor_specific_init[cmd].data_bytes & 0x1F);
        cmd++;
    }

    return ESP_OK;
}

static esp_err_t panel_rm68120_draw_bitmap(esp_lcd_panel_t *panel, int x_start, int y_start, int x_end, int y_end, const void *color_data)
{
    rm68120_panel_t *rm68120 = __containerof(panel, rm68120_panel_t, base);
    assert((x_start < x_end) && (y_start < y_end) && "start position must be smaller than end position");
    esp_lcd_panel_io_handle_t io = rm68120->io;

    x_start += rm68120->x_gap;
    x_end += rm68120->x_gap;
    y_start += rm68120->y_gap;
    y_end += rm68120->y_gap;

    esp_lcd_panel_io_tx_param(io, 0x2A00,   (uint8_t[]){(x_start>>8)}, 1);
    esp_lcd_panel_io_tx_param(io, 0x2A00+1, (uint8_t[]){x_start}, 1);
    esp_lcd_panel_io_tx_param(io, 0x2A00+2, (uint8_t[]){((x_end-1)>>8)}, 1);
    esp_lcd_panel_io_tx_param(io, 0x2A00+3, (uint8_t[]){(x_end-1)}, 1);
    esp_lcd_panel_io_tx_param(io, 0x2B00,   (uint8_t[]){(y_start>>8)}, 1);
    esp_lcd_panel_io_tx_param(io, 0x2B00+1, (uint8_t[]){y_start}, 1);
    esp_lcd_panel_io_tx_param(io, 0x2B00+2, (uint8_t[]){((y_end-1)>>8)}, 1);
    esp_lcd_panel_io_tx_param(io, 0x2B00+3, (uint8_t[]){(y_end-1)}, 1);

    // transfer frame buffer
    size_t len = (x_end - x_start) * (y_end - y_start) * rm68120->bits_per_pixel / 8;
    esp_lcd_panel_io_tx_color(io, 0x2C00, color_data, len);

    return ESP_OK;
}

static esp_err_t panel_rm68120_invert_color(esp_lcd_panel_t *panel, bool invert_color_data)
{
    return ESP_OK;
}

static esp_err_t panel_rm68120_mirror(esp_lcd_panel_t *panel, bool mirror_x, bool mirror_y)
{
    return ESP_OK;
}

static esp_err_t panel_rm68120_swap_xy(esp_lcd_panel_t *panel, bool swap_axes)
{
    return ESP_OK;
}

static esp_err_t panel_rm68120_set_gap(esp_lcd_panel_t *panel, int x_gap, int y_gap)
{
    rm68120_panel_t *rm68120 = __containerof(panel, rm68120_panel_t, base);
    rm68120->x_gap = x_gap;
    rm68120->y_gap = y_gap;
    return ESP_OK;
}

static esp_err_t panel_rm68120_disp_on_off(esp_lcd_panel_t *panel, bool on_off)
{
    rm68120_panel_t *rm68120 = __containerof(panel, rm68120_panel_t, base);
    esp_lcd_panel_io_handle_t io = rm68120->io;
    int param = 0;

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(5, 0, 0)
    on_off = !on_off;
#endif

    if (on_off) {
        param = 0;
    } else {
        param = LCD_CMD_DISPOFF;
    }
    esp_lcd_panel_io_tx_param(io, 0x2900, NULL, 0);
    return ESP_OK;
}
