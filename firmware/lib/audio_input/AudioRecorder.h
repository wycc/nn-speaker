// for hw3 modified: copied from reference project (nn-speaker/firmware)
// Captures audio from I2SSampler into a flat PSRAM buffer.
// Used by DetectRecordWordState to buffer audio between start-word and end-word detection.
#ifndef __audio_recorder_h__
#define __audio_recorder_h__

#include <Arduino.h>
#include "I2SSampler.h"

// ─── Recording parameters ────────────────────────────────────────────────────
// Sample rate must match the I2S mic config in main.cpp (16 kHz).
#define RECORD_SAMPLE_RATE    16000
// Maximum recording duration. 3 s × 16 000 samples/s = 48 000 int16_t values
// = 96 kB — fits easily in the 4 MB PSRAM on the ESP32-WROVER.
#define RECORD_MAX_SECONDS    3
#define RECORD_BUFFER_SAMPLES (RECORD_SAMPLE_RATE * RECORD_MAX_SECONDS)

/**
 * AudioRecorder
 *
 * Captures audio from a running I2SSampler into a flat int16_t buffer stored
 * in PSRAM.
 *
 * How it works
 * ────────────
 * AudioRecorder arms a hook directly inside I2SSampler::addSample().  Every
 * sample the I2S reader task produces is copied straight into the recording
 * buffer in the same call, in the same task context — no ring-buffer chasing,
 * no separate FreeRTOS task, no scheduling gaps or missed samples.
 *
 * Usage
 * ─────
 *   recorder.startRecording();   // arm the hook
 *   // … wait / do other work …
 *   recorder.stopRecording();    // disarm (or it stops automatically when full)
 *   // getSamples() / getSampleCount() now hold the captured audio
 */
class AudioRecorder
{
public:
    AudioRecorder(I2SSampler *sampler);
    ~AudioRecorder();

    // Arm the recording hook — capturing begins with the next I2S sample.
    void startRecording();

    // Disarm the recording hook early (also called automatically when full).
    void stopRecording();

    // True while the hook is still armed and copying samples.
    bool isRecording() const { return m_sampler->isRecording(); }

    // Pointer to the captured int16_t samples (valid after recording stops).
    const int16_t *getSamples() const { return m_buffer; }

    // Number of samples captured during the last recording.
    int getSampleCount() const { return m_sampler->recordedSamples(); }

private:
    I2SSampler *m_sampler;
    int16_t    *m_buffer;  // flat recording buffer in PSRAM (owned here)
};

#endif // __audio_recorder_h__
