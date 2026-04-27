#include <Arduino.h>
#include "Application.h"
#include "state_machine/DetectWakeWordState.h"
#include "state_machine/RecogniseCommandState.h"
#include "IndicatorLight.h"
#include "Speaker.h"
#include "IntentProcessor.h"

Application::Application(I2SSampler *sample_provider, IntentProcessor *intent_processor, Speaker *speaker, IndicatorLight *indicator_light)
{
    m_detect_wake_word_state = new DetectWakeWordState(sample_provider);
    m_recognise_command_state = new RecogniseCommandState(sample_provider, indicator_light, speaker, intent_processor);

    m_current_state = m_detect_wake_word_state;
    m_current_state->enterState();

    m_speaker = speaker;
    m_indicator_light = indicator_light;
    m_indicator_light->off();
}

void Application::run()
{
    bool state_done = m_current_state->run();

    if (state_done)
    {
        m_current_state->exitState();

        if (m_current_state == m_detect_wake_word_state)
        {
            // Wake word detected: LED ON immediately
            m_indicator_light->on();

            m_current_state = m_recognise_command_state;
            m_speaker->playOK();
        }
        else
        {
            // Recognition finished: LED OFF
            m_indicator_light->off();

            m_current_state = m_detect_wake_word_state;
        }

        m_current_state->enterState();
    }

    vTaskDelay(10);
}
