#include <Arduino.h>
#include "Application.h"
#include "state_machine/DetectWakeWordState.h"
#include "state_machine/RecogniseCommandState.h"
#include "state_machine/DetectRecordWordState.h"  // for hw3 modified
#include "AudioRecorder.h"                         // for hw3 modified
#include "IndicatorLight.h"
#include "Speaker.h"
#include "IntentProcessor.h"

Application::Application(I2SSampler *sample_provider, IntentProcessor *intent_processor, Speaker *speaker, IndicatorLight *indicator_light)
{
    // detect wake word state - waits for the wake word to be detected
    m_detect_wake_word_state = new DetectWakeWordState(sample_provider);
    // command recongiser - streams audio to the server for recognition
    m_recognise_command_state = new RecogniseCommandState(sample_provider, indicator_light, speaker, intent_processor);
    // for hw3 modified: audio recorder (PSRAM buffer) and record-word state
    m_recorder = new AudioRecorder(sample_provider);
    m_detect_record_word_state = new DetectRecordWordState(sample_provider, m_recorder, indicator_light, speaker);
    // start off in the detecting wakeword state
    m_current_state = m_detect_wake_word_state;
    m_current_state->enterState();
    m_speaker = speaker;
    // for hw3 modified: store indicator_light so run() can control LED
    m_indicator_light = indicator_light;
}

// process the next batch of samples
void Application::run()
{
    bool state_done = m_current_state->run();
    if (state_done)
    {
        // for hw3 modified: blink once only — LED does NOT stay ON after wake word
        // LED will be turned ON in DetectRecordWordState when start-word is confirmed
        if (m_current_state == m_detect_wake_word_state)
        {
            m_indicator_light->blinkBlue(1);    // 藍色閃一下，之後不長開
        }

        m_current_state->exitState();
        // switch to the next state - very simple state machine so we just go to the other state...
        if (m_current_state == m_detect_wake_word_state)
        {
            // for hw3 modified: go to record-word state instead of command recogniser
            m_current_state = m_detect_record_word_state;
            // m_current_state = m_recognise_command_state;  // for hw3 modified: commented out
            m_speaker->playOK();
        }
        else
        {
            // for hw3 modified: safety net — ensure LED is OFF whenever we
            // return to the waiting state, even if a state
            // failed to turn it off (e.g. early-exit on connection error)
            m_indicator_light->setState(OFF);
            m_current_state = m_detect_wake_word_state;
        }
        m_current_state->enterState();
    }
    vTaskDelay(10);
}
