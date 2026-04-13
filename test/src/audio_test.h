#ifndef AUDIO_TEST_H
#define AUDIO_TEST_H

#include <Arduino.h>

struct AudioStats {
  uint32_t peak = 0;
  uint32_t average = 0;
  bool clipped = false;
};

bool initAudioHardware();
void shutdownAudioHardware();

int16_t *allocateRecordingBuffer(size_t sampleCount);
void freeRecordingBuffer(int16_t *buffer);

bool recordAudio(int16_t *buffer, size_t sampleCount, AudioStats *stats);
bool playAudio(const int16_t *buffer, size_t sampleCount);
AudioStats analyzeAudio(const int16_t *buffer, size_t sampleCount);

#endif
