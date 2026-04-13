#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <driver/i2s.h>

constexpr gpio_num_t TEST_BUTTON_PIN = GPIO_NUM_2;
constexpr gpio_num_t STATUS_LED_PIN = GPIO_NUM_21;

constexpr i2s_port_t MIC_I2S_PORT = I2S_NUM_0;
constexpr int INMP441_WS = 5;
constexpr int INMP441_SCK = 4;
constexpr int INMP441_SD = 6;

constexpr i2s_port_t SPEAKER_I2S_PORT = I2S_NUM_1;
constexpr int MAX98357_LRC = 16;
constexpr int MAX98357_BCLK = 15;
constexpr int MAX98357_DIN = 7;

constexpr uint32_t AUDIO_SAMPLE_RATE = 16000;
constexpr size_t RECORD_SECONDS = 5;
constexpr size_t RECORD_SAMPLE_COUNT = AUDIO_SAMPLE_RATE * RECORD_SECONDS;
constexpr size_t RECORD_BUFFER_BYTES = RECORD_SAMPLE_COUNT * sizeof(int16_t);

constexpr int MIC_GAIN = 4;
constexpr uint32_t BUTTON_DEBOUNCE_MS = 30;
constexpr uint32_t POST_TEST_DELAY_MS = 250;

#endif
