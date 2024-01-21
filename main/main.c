// Copyright (c) 2023 Michael Heijmans
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.
//
// Description:
// This is the main entry point for the esp-idf-wav-player project.
// It will setup the sdcard and i2s and play a wav file.
//
// The wav file must be 16bit, 44.1kHz, stereo and have a 44 byte header.

#include "configuration.h"  // basic sysetm includes and pin setup
#include "sdcard.h"         // sdcard intialization
#include "driver/i2s_std.h" // i2s setup

// defines
#define REBOOT_WAIT 5000            // reboot after 5 seconds
#define AUDIO_BUFFER 2048           // buffer size for reading the wav file and sending to i2s

// constants
const char *TAG = "esp-idf-wav-player";

// handles
i2s_chan_handle_t tx_handle;

esp_err_t i2s_setup(void)
{
  // setup a standard config and the channel
  i2s_chan_config_t chan_cfg = I2S_CHANNEL_DEFAULT_CONFIG(I2S_NUM_AUTO, I2S_ROLE_MASTER);
  ESP_ERROR_CHECK(i2s_new_channel(&chan_cfg, &tx_handle, NULL));

  // setup the i2s config
  i2s_std_config_t std_cfg = {
      .clk_cfg = I2S_STD_CLK_DEFAULT_CONFIG(22050),                                                    // the wav file sample rate
      .slot_cfg = I2S_STD_PHILIPS_SLOT_DEFAULT_CONFIG(I2S_DATA_BIT_WIDTH_16BIT, I2S_SLOT_MODE_MONO), // the wav faile bit and channel config
      .gpio_cfg = {
          // refer to configuration.h for pin setup
          .mclk = I2S_SCLK_PIN,
          .bclk = I2S_BLK_PIN,
          .ws = I2S_WS_PIN,
          .dout = I2S_DATA_OUT_PIN,
          .din = I2S_DATA_IN_PIN,
          .invert_flags = {
              .mclk_inv = false,
              .bclk_inv = false,
              .ws_inv = false,
          },
      },
  };
  std_cfg.slot_cfg.slot_mask = I2S_STD_SLOT_RIGHT;
  return i2s_channel_init_std_mode(tx_handle, &std_cfg);
}

esp_err_t play_wav(char *fp)
{
  FILE *fh = fopen(fp, "rb");
  if (fh == NULL)
  {
    ESP_LOGE(TAG, "Failed to open file");
    return ESP_ERR_INVALID_ARG;
  }

  // skip the header...
  fseek(fh, 44, SEEK_SET);

  // create a writer buffer
  int16_t *buf = calloc(AUDIO_BUFFER, sizeof(int16_t));
  size_t bytes_read = 0;
  size_t bytes_written = 0;

  bytes_read = fread(buf, sizeof(int16_t), AUDIO_BUFFER, fh);

  i2s_channel_enable(tx_handle);

  while (bytes_read > 0)
  {
    // write the buffer to the i2s
    i2s_channel_write(tx_handle, buf, bytes_read * sizeof(int16_t), &bytes_written, portMAX_DELAY);
    bytes_read = fread(buf, sizeof(int16_t), AUDIO_BUFFER, fh);
    ESP_LOGV(TAG, "Bytes read: %d", bytes_read);
  }

  i2s_channel_disable(tx_handle);
  free(buf);

  return ESP_OK;
}

void app_main(void)
{
  ESP_LOGI(TAG, "Starting up");

  // basic info
  (void)print_system_info();

  // sdcard init
  ESP_ERROR_CHECK(init_sdcard());
  (void)print_sdcard_info();

  // setup i2s
  ESP_LOGI(TAG, "Setting up i2s");
  ESP_ERROR_CHECK(i2s_setup());

  // play the wav file
  ESP_LOGI(TAG, "Playing wav file");
  ESP_ERROR_CHECK(play_wav(WAV_FILE));

  // that'll do pig... that'll do
  i2s_del_channel(tx_handle); // delete the channel

  // reboot
  ESP_LOGI(TAG, "Rebooting in %d ms...", REBOOT_WAIT);
  vTaskDelay(REBOOT_WAIT / portTICK_PERIOD_MS);
  esp_restart();
}