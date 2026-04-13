#include "audio_test.h"

#include <string.h>
#include <esp_heap_caps.h>
#include <esp_system.h>

#include "config.h"

namespace {

constexpr size_t kIoChunkBytes = 1024;

i2s_config_t makeMicConfig() {
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_RX);
  config.sample_rate = AUDIO_SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_LEFT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 4;
  config.dma_buf_len = 512;
  config.use_apll = false;
  config.tx_desc_auto_clear = false;
  config.fixed_mclk = 0;
  return config;
}

i2s_pin_config_t makeMicPins() {
  i2s_pin_config_t pins = {};
  pins.bck_io_num = INMP441_SCK;
  pins.ws_io_num = INMP441_WS;
  pins.data_out_num = I2S_PIN_NO_CHANGE;
  pins.data_in_num = INMP441_SD;
  return pins;
}

i2s_config_t makeSpeakerConfig() {
  i2s_config_t config = {};
  config.mode = static_cast<i2s_mode_t>(I2S_MODE_MASTER | I2S_MODE_TX);
  config.sample_rate = AUDIO_SAMPLE_RATE;
  config.bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT;
  config.channel_format = I2S_CHANNEL_FMT_ONLY_RIGHT;
  config.communication_format = I2S_COMM_FORMAT_STAND_I2S;
  config.intr_alloc_flags = ESP_INTR_FLAG_LEVEL1;
  config.dma_buf_count = 6;
  config.dma_buf_len = 512;
  config.use_apll = false;
  config.tx_desc_auto_clear = true;
  config.fixed_mclk = 0;
  return config;
}

i2s_pin_config_t makeSpeakerPins() {
  i2s_pin_config_t pins = {};
  pins.bck_io_num = MAX98357_BCLK;
  pins.ws_io_num = MAX98357_LRC;
  pins.data_out_num = MAX98357_DIN;
  pins.data_in_num = I2S_PIN_NO_CHANGE;
  return pins;
}

void applyGain(int16_t *samples, size_t sampleCount) {
  for (size_t i = 0; i < sampleCount; ++i) {
    const int32_t amplified = static_cast<int32_t>(samples[i]) * MIC_GAIN;
    if (amplified > INT16_MAX) {
      samples[i] = INT16_MAX;
    } else if (amplified < INT16_MIN) {
      samples[i] = INT16_MIN;
    } else {
      samples[i] = static_cast<int16_t>(amplified);
    }
  }
}

}  // namespace

bool initAudioHardware() {
  i2s_driver_uninstall(MIC_I2S_PORT);
  i2s_driver_uninstall(SPEAKER_I2S_PORT);
  delay(50);

  i2s_config_t micConfig = makeMicConfig();
  i2s_pin_config_t micPins = makeMicPins();
  esp_err_t err = i2s_driver_install(MIC_I2S_PORT, &micConfig, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("Mic I2S init failed: %s\n", esp_err_to_name(err));
    return false;
  }

  err = i2s_set_pin(MIC_I2S_PORT, &micPins);
  if (err != ESP_OK) {
    Serial.printf("Mic pin config failed: %s\n", esp_err_to_name(err));
    i2s_driver_uninstall(MIC_I2S_PORT);
    return false;
  }

  i2s_config_t speakerConfig = makeSpeakerConfig();
  i2s_pin_config_t speakerPins = makeSpeakerPins();
  err = i2s_driver_install(SPEAKER_I2S_PORT, &speakerConfig, 0, nullptr);
  if (err != ESP_OK) {
    Serial.printf("Speaker I2S init failed: %s\n", esp_err_to_name(err));
    i2s_driver_uninstall(MIC_I2S_PORT);
    return false;
  }

  err = i2s_set_pin(SPEAKER_I2S_PORT, &speakerPins);
  if (err != ESP_OK) {
    Serial.printf("Speaker pin config failed: %s\n", esp_err_to_name(err));
    i2s_driver_uninstall(MIC_I2S_PORT);
    i2s_driver_uninstall(SPEAKER_I2S_PORT);
    return false;
  }

  i2s_zero_dma_buffer(MIC_I2S_PORT);
  i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
  return true;
}

void shutdownAudioHardware() {
  i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
  i2s_driver_uninstall(MIC_I2S_PORT);
  i2s_driver_uninstall(SPEAKER_I2S_PORT);
}

int16_t *allocateRecordingBuffer(size_t sampleCount) {
  const size_t bytes = sampleCount * sizeof(int16_t);
  int16_t *buffer = static_cast<int16_t *>(
      heap_caps_malloc(bytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  if (buffer == nullptr) {
    buffer = static_cast<int16_t *>(malloc(bytes));
  }

  if (buffer != nullptr) {
    memset(buffer, 0, bytes);
  }

  return buffer;
}

void freeRecordingBuffer(int16_t *buffer) {
  free(buffer);
}

AudioStats analyzeAudio(const int16_t *buffer, size_t sampleCount) {
  AudioStats stats;
  if (buffer == nullptr || sampleCount == 0) {
    return stats;
  }

  uint64_t totalMagnitude = 0;
  for (size_t i = 0; i < sampleCount; ++i) {
    const int32_t sample = buffer[i];
    const uint32_t magnitude =
        static_cast<uint32_t>(sample < 0 ? -sample : sample);
    totalMagnitude += magnitude;
    if (magnitude > stats.peak) {
      stats.peak = magnitude;
    }
    if (sample == INT16_MAX || sample == INT16_MIN) {
      stats.clipped = true;
    }
  }

  stats.average = static_cast<uint32_t>(totalMagnitude / sampleCount);
  return stats;
}

bool recordAudio(int16_t *buffer, size_t sampleCount, AudioStats *stats) {
  if (buffer == nullptr || sampleCount == 0) {
    return false;
  }

  const size_t totalBytes = sampleCount * sizeof(int16_t);
  memset(buffer, 0, totalBytes);
  i2s_zero_dma_buffer(MIC_I2S_PORT);

  size_t offset = 0;
  while (offset < totalBytes) {
    size_t bytesRead = 0;
    const size_t bytesToRead = min(kIoChunkBytes, totalBytes - offset);
    esp_err_t err = i2s_read(
        MIC_I2S_PORT,
        reinterpret_cast<uint8_t *>(buffer) + offset,
        bytesToRead,
        &bytesRead,
        pdMS_TO_TICKS(1000));

    if (err != ESP_OK || bytesRead == 0) {
      Serial.printf("Record failed, I2S read error: %s\n", esp_err_to_name(err));
      return false;
    }

    applyGain(
        reinterpret_cast<int16_t *>(reinterpret_cast<uint8_t *>(buffer) + offset),
        bytesRead / sizeof(int16_t));
    offset += bytesRead;
  }

  if (stats != nullptr) {
    *stats = analyzeAudio(buffer, sampleCount);
  }
  return true;
}

bool playAudio(const int16_t *buffer, size_t sampleCount) {
  if (buffer == nullptr || sampleCount == 0) {
    return false;
  }

  const uint8_t *raw = reinterpret_cast<const uint8_t *>(buffer);
  const size_t totalBytes = sampleCount * sizeof(int16_t);
  size_t sent = 0;

  i2s_zero_dma_buffer(SPEAKER_I2S_PORT);

  while (sent < totalBytes) {
    size_t bytesWritten = 0;
    const size_t bytesToWrite = min(kIoChunkBytes, totalBytes - sent);
    esp_err_t err = i2s_write(
        SPEAKER_I2S_PORT,
        raw + sent,
        bytesToWrite,
        &bytesWritten,
        pdMS_TO_TICKS(1000));

    if (err != ESP_OK || bytesWritten == 0) {
      Serial.printf("Playback failed, I2S write error: %s\n", esp_err_to_name(err));
      return false;
    }

    sent += bytesWritten;
  }

  delay(50);
  i2s_zero_dma_buffer(SPEAKER_I2S_PORT);
  return true;
}

