#include "lineout.h"
#include "pins.h"
#include <driver/i2s_std.h>
#include <esp_rom_gpio.h>
#include <soc/gpio_sig_map.h>
#include <soc/gpio_periph.h>
#include <soc/io_mux_reg.h>

static i2s_chan_handle_t tx = nullptr;
static bool active = false;

bool lineoutStart() {
    if (active) return true;
    if (!tx) {
        i2s_chan_config_t ccfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_1, I2S_ROLE_SLAVE);
        ccfg.dma_desc_num = 8;
        ccfg.dma_frame_num = 240;
        ccfg.auto_clear = true; // underrun -> silence, not stale data
        if (i2s_new_channel(&ccfg, &tx, nullptr) != ESP_OK) {
            Serial.println("[lineout] i2s1 channel alloc failed");
            return false;
        }
        i2s_std_config_t std = {
            .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(44100), // nominal; slave follows external BCK/WS
            .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_STEREO),
            .gpio_cfg = {
                .mclk = I2S_GPIO_UNUSED,
                .bclk = I2S_GPIO_UNUSED, // routed manually below — the pins are I2S0 *outputs*
                .ws = I2S_GPIO_UNUSED,
                .dout = (gpio_num_t)PIN_I2S_LINE_DOUT,
                .din = I2S_GPIO_UNUSED,
                .invert_flags = {},
            },
        };
        if (i2s_channel_init_std_mode(tx, &std) != ESP_OK) {
            Serial.println("[lineout] i2s1 std init failed");
            return false;
        }
        // Clock I2S1 from the bus that I2S0 is driving: enable the input path on the
        // (output-driven) BCLK/WS pins and matrix them into I2S1's slave clock inputs.
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[PIN_I2S_EXT_BCK]);
        PIN_INPUT_ENABLE(GPIO_PIN_MUX_REG[PIN_I2S_EXT_WS]);
        esp_rom_gpio_connect_in_signal(PIN_I2S_EXT_BCK, I2S1O_BCK_IN_IDX, false);
        esp_rom_gpio_connect_in_signal(PIN_I2S_EXT_WS, I2S1O_WS_IN_IDX, false);
    }
    if (i2s_channel_enable(tx) != ESP_OK) {
        Serial.println("[lineout] i2s1 enable failed");
        return false;
    }
    active = true;
    Serial.printf("[lineout] fixed line-out on GPIO%d (I2S1 slave)\n", PIN_I2S_LINE_DOUT);
    return true;
}

void lineoutStop() {
    if (!active) return;
    active = false;
    i2s_channel_disable(tx);
    Serial.println("[lineout] disabled");
}

bool lineoutActive() { return active; }

void lineoutWrite(const int16_t *samples, size_t frames) {
    if (!active) return;
    size_t written = 0;
    // timeout 0: never stall the audio task — a full DMA queue just drops a chunk
    i2s_channel_write(tx, samples, frames * 2 * sizeof(int16_t), &written, 0);
}
