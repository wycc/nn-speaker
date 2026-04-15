#ifndef _application_h_
#define _applicaiton_h_

#include "state_machine/States.h"

class I2SSampler;
class I2SOutput;
class State;
class IndicatorLight;
class Speaker;
class IntentProcessor;
class AudioRecorder;  // for hw3 modified

class Application
{
private:
    State *m_detect_wake_word_state;
    State *m_recognise_command_state;
    State *m_current_state;
    Speaker *m_speaker;
    // for hw3 modified: store indicator_light so Application can control LED
    // directly at the moment of wake word detection
    IndicatorLight *m_indicator_light;
    // for hw3 modified: record-word state and its audio recorder
    State         *m_detect_record_word_state;
    AudioRecorder *m_recorder;

public:
    Application(I2SSampler *sample_provider, IntentProcessor *intent_processor, Speaker *speaker, IndicatorLight *indicator_light);
    ~Application();
    void run();
};

#endif