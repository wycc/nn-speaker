#include <Arduino.h>
#include "I2SSampler.h"
#include "RingBuffer.h"
#include "Application.h"
#include "state_machine/DetectWakeWordState.h"
#include "state_machine/RecogniseCommandState.h"
#include "IndicatorLight.h"
#include "Speaker.h"
#include "IntentProcessor.h"

#define WAIT_MS 1000
#define RECORD_MS 3000
#define SAMPLE_RATE 16000
#define RECORD_SAMPLES ((RECORD_MS * SAMPLE_RATE) / 1000)

Application::Application(I2SSampler *sample_provider, IntentProcessor *intent_processor, Speaker *speaker, IndicatorLight *indicator_light)
{
    m_sample_provider = sample_provider;
    // detect wake word state - waits for the wake word to be detected
    m_detect_wake_word_state = new DetectWakeWordState(sample_provider);
    // command recongiser - streams audio to the server for recognition
    m_recognise_command_state = new RecogniseCommandState(sample_provider, indicator_light, speaker, intent_processor);
    // start off in the detecting wakeword state
    m_current_state = m_detect_wake_word_state;
    m_current_state_kind = STATE_DETECT;
    m_phase = PHASE_DETECT;
    m_phase_start_time = 0;
    m_current_state->enterState();
    m_speaker = speaker;
    m_indicator_light = indicator_light;

    m_recorded_audio_capacity_samples = RECORD_SAMPLES;
    m_recorded_audio_buffer = new int16_t[m_recorded_audio_capacity_samples];
    m_recorded_audio_samples = 0;
    m_record_last_audio_position = -1;

}

Application::~Application()
{
    delete m_detect_wake_word_state;
    delete m_recognise_command_state;
    delete[] m_recorded_audio_buffer;
}

// process the next batch of samples
bool Application::run()
{
    unsigned long now = millis();

    if (m_phase == PHASE_DETECT)
    {
        bool state_done = m_current_state->run();
        if (state_done)
        {
            if (m_indicator_light)
            {
                m_indicator_light->setState(ON);
                vTaskDelay(100 / portTICK_PERIOD_MS);
                m_indicator_light->setState(OFF);
            }

            m_current_state->exitState();
            m_current_state = m_recognise_command_state;
            m_current_state_kind = STATE_RECOGNISE;
            m_current_state->enterState();

            m_phase = PHASE_WAIT_BEFORE_RECORD;
            m_phase_start_time = now;
            m_recorded_audio_samples = 0;
            m_record_last_audio_position = -1;
        }
        vTaskDelay(10);
        return state_done;
    }

    if (m_phase == PHASE_WAIT_BEFORE_RECORD)
    {
        if (now - m_phase_start_time >= WAIT_MS)
        {
            m_phase = PHASE_RECORD;
            m_phase_start_time = now;
            m_recorded_audio_samples = 0;
            m_record_last_audio_position = -1;
        }
        vTaskDelay(10);
        return false;
    }

    if (m_phase == PHASE_RECORD)
    {
        if (m_record_last_audio_position == -1)
        {
            m_record_last_audio_position = m_sample_provider->getCurrentWritePosition();
        }

        int audio_position = m_sample_provider->getCurrentWritePosition();
        int ring_buffer_size = m_sample_provider->getRingBufferSize();
        int sample_count = (audio_position - m_record_last_audio_position + ring_buffer_size) % ring_buffer_size;

        if (sample_count > 0 && m_recorded_audio_samples < m_recorded_audio_capacity_samples)
        {
            RingBufferAccessor *reader = m_sample_provider->getRingBufferReader();
            reader->setIndex(m_record_last_audio_position);

            int copy_count = sample_count;
            if (copy_count > (m_recorded_audio_capacity_samples - m_recorded_audio_samples))
            {
                copy_count = m_recorded_audio_capacity_samples - m_recorded_audio_samples;
            }

            for (int i = 0; i < copy_count; i++)
            {
                m_recorded_audio_buffer[m_recorded_audio_samples++] = reader->getCurrentSample();
                reader->moveToNextSample();
            }

            m_record_last_audio_position = (m_record_last_audio_position + sample_count + ring_buffer_size) % ring_buffer_size;
            delete reader;
        }

        if (m_recorded_audio_samples >= m_recorded_audio_capacity_samples)
        {
            m_phase = PHASE_WAIT_BEFORE_PLAYBACK;
            m_phase_start_time = now;
        }
        vTaskDelay(10);
        return false;
    }

    if (m_phase == PHASE_WAIT_BEFORE_PLAYBACK)
    {
        if (now - m_phase_start_time >= WAIT_MS)
        {
            m_speaker->playRecordedAudio(m_recorded_audio_buffer, m_recorded_audio_samples);
            m_phase = PHASE_PLAYBACK;
        }
        vTaskDelay(10);
        return false;
    }

    if (m_phase == PHASE_PLAYBACK)
    {
        if (!m_speaker->isPlaying())
        {
            m_current_state->exitState();
            m_current_state = m_detect_wake_word_state;
            m_current_state_kind = STATE_DETECT;
            m_current_state->enterState();
            m_phase = PHASE_DETECT;
            m_phase_start_time = now;
            return true;
        }
        vTaskDelay(10);
        return false;
    }

    vTaskDelay(10);
    return false;
}

bool Application::isInRecogniseState() const
{
    return m_current_state_kind == STATE_RECOGNISE;
}

Application::AppPhase Application::getPhase() const
{
    return m_phase;
}
