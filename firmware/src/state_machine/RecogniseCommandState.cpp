#include <Arduino.h>
#include <SPIFFS.h>
#include "I2SSampler.h"
#include "RingBuffer.h"
#include "RecogniseCommandState.h"
#include "IndicatorLight.h"
#include "Speaker.h"

#define RECORD_SECONDS 3
#define SAMPLE_RATE 16000
#define BITS_PER_SAMPLE 16
#define NUM_CHANNELS 1
#define TOTAL_SAMPLES (RECORD_SECONDS * SAMPLE_RATE)
#define DATA_BYTES (TOTAL_SAMPLES * (BITS_PER_SAMPLE / 8))
#define WAV_HEADER_SIZE 44

static void writeWavHeader(File &file, uint32_t data_bytes)
{
    uint32_t chunk_size = 36 + data_bytes;
    uint32_t byte_rate = SAMPLE_RATE * NUM_CHANNELS * (BITS_PER_SAMPLE / 8);
    uint16_t block_align = NUM_CHANNELS * (BITS_PER_SAMPLE / 8);

    file.write((const uint8_t *)"RIFF", 4);
    file.write((const uint8_t *)&chunk_size, 4);
    file.write((const uint8_t *)"WAVE", 4);
    file.write((const uint8_t *)"fmt ", 4);
    uint32_t subchunk1_size = 16;
    file.write((const uint8_t *)&subchunk1_size, 4);
    uint16_t audio_format = 1;
    file.write((const uint8_t *)&audio_format, 2);
    uint16_t num_channels = NUM_CHANNELS;
    file.write((const uint8_t *)&num_channels, 2);
    uint32_t sample_rate = SAMPLE_RATE;
    file.write((const uint8_t *)&sample_rate, 4);
    file.write((const uint8_t *)&byte_rate, 4);
    file.write((const uint8_t *)&block_align, 2);
    uint16_t bits_per_sample = BITS_PER_SAMPLE;
    file.write((const uint8_t *)&bits_per_sample, 2);
    file.write((const uint8_t *)"data", 4);
    file.write((const uint8_t *)&data_bytes, 4);
}

RecogniseCommandState::RecogniseCommandState(I2SSampler *sample_provider, IndicatorLight *indicator_light, Speaker *speaker)
{
    m_sample_provider = sample_provider;
    m_indicator_light = indicator_light;
    m_speaker = speaker;
}

void RecogniseCommandState::enterState()
{
    m_indicator_light->setState(ON);

    m_start_time = millis();
    m_elapsed_time = 0;
    m_last_audio_position = -1;

    m_speaker->playReady();
    Serial.println("Recording audio to SPIFFS...");
}

bool RecogniseCommandState::run()
{
    if (m_last_audio_position == -1)
    {
        // start 1 second behind current write position
        m_last_audio_position = m_sample_provider->getCurrentWritePosition() - SAMPLE_RATE;
    }

    unsigned long current_time = millis();
    m_elapsed_time += current_time - m_start_time;
    m_start_time = current_time;

    if (m_elapsed_time < (RECORD_SECONDS * 1000))
    {
        return false;
    }

    // 3 seconds elapsed - collect audio from ring buffer and save to SPIFFS
    Serial.println("3 seconds elapsed - saving WAV to SPIFFS");

    File wav_file = SPIFFS.open("/recording.wav", FILE_WRITE);
    if (!wav_file)
    {
        Serial.println("Failed to open /recording.wav for writing");
        m_indicator_light->setState(OFF);
        return true;
    }

    // write placeholder header - will overwrite with correct sizes after data
    uint8_t header[WAV_HEADER_SIZE] = {0};
    wav_file.write(header, WAV_HEADER_SIZE);

    // collect TOTAL_SAMPLES from ring buffer starting at m_last_audio_position
    RingBufferAccessor *reader = m_sample_provider->getRingBufferReader();
    reader->setIndex(m_last_audio_position);

    int16_t sample_buffer[256];
    int remaining = TOTAL_SAMPLES;
    uint32_t bytes_written = 0;

    while (remaining > 0)
    {
        int batch = (remaining < 256) ? remaining : 256;
        for (int i = 0; i < batch; i++)
        {
            sample_buffer[i] = reader->getCurrentSample();
            reader->moveToNextSample();
        }
        wav_file.write((const uint8_t *)sample_buffer, batch * sizeof(int16_t));
        bytes_written += batch * sizeof(int16_t);
        remaining -= batch;
    }
    delete reader;

    // go back and write correct WAV header
    wav_file.seek(0);
    writeWavHeader(wav_file, bytes_written);
    wav_file.close();

    Serial.printf("Saved /recording.wav: %u bytes of audio + 44 byte header\n", bytes_written);
    uint32_t free_ram = esp_get_free_heap_size();
    Serial.printf("Free RAM after save: %u\n", free_ram);

    // play back the recording through the speaker
    Serial.println("Playing back recording...");
    m_speaker->playRecording("/recording.wav");

    m_indicator_light->setState(OFF);
    return true;
}

void RecogniseCommandState::exitState()
{
    // nothing to clean up
}
