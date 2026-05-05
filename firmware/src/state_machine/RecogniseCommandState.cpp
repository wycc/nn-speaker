#include <Arduino.h>
#include "I2SSampler.h"
#include "RingBuffer.h"
#include "RecogniseCommandState.h"
#include "IndicatorLight.h"
#include "Speaker.h"

#define RECORD_SECONDS 3
#define SAMPLE_RATE 16000
#define TOTAL_SAMPLES (RECORD_SECONDS * SAMPLE_RATE)

RecogniseCommandState::RecogniseCommandState(I2SSampler *sample_provider, IndicatorLight *indicator_light, Speaker *speaker)
{
    m_sample_provider = sample_provider;
    m_indicator_light = indicator_light;
    m_speaker = speaker;
    m_audio_buffer = NULL;
}

void RecogniseCommandState::enterState()
{
    m_indicator_light->setState(ON);
    m_start_time = millis();
    m_elapsed_time = 0;
    m_last_audio_position = -1;
    m_speaker->playReady();
    Serial.println("Recording audio to RAM...");
}

bool RecogniseCommandState::run()
{
    if (m_last_audio_position == -1)
    {
        m_last_audio_position = m_sample_provider->getCurrentWritePosition() - SAMPLE_RATE;
    }

    unsigned long current_time = millis();
    m_elapsed_time += current_time - m_start_time;
    m_start_time = current_time;

    if (m_elapsed_time < (RECORD_SECONDS * 1000))
    {
        return false;
    }

    Serial.println("3 seconds elapsed - saving to RAM");

    // allocate buffer in PSRAM
    if (m_audio_buffer)
    {
        free(m_audio_buffer);
    }
    m_audio_buffer = (int16_t *)ps_malloc(TOTAL_SAMPLES * sizeof(int16_t));
    if (!m_audio_buffer)
    {
        Serial.println("Failed to allocate RAM buffer");
        m_indicator_light->setState(OFF);
        return true;
    }

    // copy samples from ring buffer to RAM
    RingBufferAccessor *reader = m_sample_provider->getRingBufferReader();
    reader->setIndex(m_last_audio_position);
    for (int i = 0; i < TOTAL_SAMPLES; i++)
    {
        m_audio_buffer[i] = reader->getCurrentSample();
        reader->moveToNextSample();
    }
    delete reader;

    Serial.printf("Saved %d samples to RAM\n", TOTAL_SAMPLES);
    uint32_t free_ram = esp_get_free_heap_size();
    Serial.printf("Free RAM after save: %u\n", free_ram);

    // play back from RAM
    Serial.println("Playing back from RAM...");
    m_speaker->playBuffer(m_audio_buffer, TOTAL_SAMPLES);

    m_indicator_light->setState(OFF);
    return true;
}

void RecogniseCommandState::exitState()
{
    // keep m_audio_buffer alive for playback (freed on next recording)
}
