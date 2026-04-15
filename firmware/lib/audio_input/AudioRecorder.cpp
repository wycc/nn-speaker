// for hw3 modified: copied from reference project (nn-speaker/firmware)
// Implements AudioRecorder — allocates PSRAM buffer once, arms/disarms
// the recording hook inside I2SSampler.
#include <esp_heap_caps.h>
#include "AudioRecorder.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

AudioRecorder::AudioRecorder(I2SSampler *sampler)
    : m_sampler(sampler), m_buffer(nullptr)
{
    // Allocate the flat recording buffer once at startup.
    //
    // We use PSRAM (SPIRAM) so that the scarce internal SRAM is kept free for
    // the TLS stack, the TFLite arena, and FreeRTOS task stacks.
    // PSRAM is accessed via the ESP32's cache, so reads/writes are transparent
    // to normal C code — no special API calls needed once it is allocated.
#ifdef BOARD_HAS_PSRAM
    m_buffer = (int16_t *)heap_caps_malloc(
        RECORD_BUFFER_SAMPLES * sizeof(int16_t), MALLOC_CAP_SPIRAM);
    Serial.printf("[Recorder] %d-byte buffer in PSRAM\n",
                  (int)(RECORD_BUFFER_SAMPLES * sizeof(int16_t)));
#else
    m_buffer = (int16_t *)malloc(RECORD_BUFFER_SAMPLES * sizeof(int16_t));
    Serial.printf("[Recorder] %d-byte buffer in internal RAM\n",
                  (int)(RECORD_BUFFER_SAMPLES * sizeof(int16_t)));
#endif

    if (!m_buffer)
    {
        Serial.println("[Recorder] ERROR: buffer allocation failed!");
    }
}

AudioRecorder::~AudioRecorder()
{
    if (m_buffer) free(m_buffer);
}

// ─── Public interface ─────────────────────────────────────────────────────────

void AudioRecorder::startRecording()
{
    if (m_sampler->isRecording())
    {
        Serial.println("[Recorder] already recording — ignoring");
        return;
    }
    if (!m_buffer)
    {
        Serial.println("[Recorder] no buffer — cannot record");
        return;
    }

    // Arm the hook inside I2SSampler.
    // From this point on, every int16_t sample that passes through
    // I2SSampler::addSample() is also written into m_buffer.
    // Recording stops automatically when RECORD_BUFFER_SAMPLES are filled,
    // or when stopRecording() is called.
    m_sampler->startRecording(m_buffer, RECORD_BUFFER_SAMPLES);

    Serial.printf("[Recorder] started — max %d s at %d Hz\n",
                  RECORD_MAX_SECONDS, RECORD_SAMPLE_RATE);
}

void AudioRecorder::stopRecording()
{
    // Disarm the hook. I2SSampler::addSample() will stop copying
    // on its very next call (the volatile flag is visible immediately).
    int captured = m_sampler->stopRecording();
    Serial.printf("[Recorder] stopped — %d samples (%.2f s)\n",
                  captured, (float)captured / RECORD_SAMPLE_RATE);
}
