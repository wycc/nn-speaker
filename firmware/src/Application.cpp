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
    m_speaker = speaker;
    m_indicator_light = indicator_light;
    m_recognise_start_time = 0;

    // waiting for wake word: LED OFF
    m_indicator_light->setState(OFF);
    m_current_state->enterState();
}

void Application::run()
{
    bool state_done = m_current_state->run();

    // 5-second LED timeout: force exit from recognise state if stuck
    if (m_current_state == m_recognise_command_state && m_recognise_start_time > 0)
    {
        if (millis() - m_recognise_start_time > 5000)
        {
            Serial.println("LED timeout: forcing exit from recognise state");
            state_done = true;
        }
    }

    if (state_done)
    {
        m_current_state->exitState();
        if (m_current_state == m_detect_wake_word_state)
        {
            // wake word detected: LED ON immediately
            m_indicator_light->setState(ON);
            m_current_state = m_recognise_command_state;
            m_recognise_start_time = millis();
            m_speaker->playOK();
        }
        else
        {
            // recognition finished: LED OFF
            m_indicator_light->setState(OFF);
            m_current_state = m_detect_wake_word_state;
            m_recognise_start_time = 0;
        }
        m_current_state->enterState();
    }
    vTaskDelay(10);
}
