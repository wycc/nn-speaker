#ifndef _application_h_
#define _applicaiton_h_

#include "state_machine/States.h"

class I2SSampler;
class I2SOutput;
class State;
class IndicatorLight;
class Speaker;
class IntentProcessor;

class Application
{
private:
    enum AppStateKind
    {
        STATE_DETECT,
        STATE_RECOGNISE
    };

public:
    enum AppPhase
    {
        PHASE_DETECT,
        PHASE_WAIT_BEFORE_RECORD,
        PHASE_RECORD,
        PHASE_WAIT_BEFORE_PLAYBACK,
        PHASE_PLAYBACK
    };

private:

    State *m_detect_wake_word_state;
    State *m_recognise_command_state;
    State *m_current_state;
    I2SSampler *m_sample_provider;
    Speaker *m_speaker;
    IndicatorLight *m_indicator_light;
    AppStateKind m_current_state_kind;
    AppPhase m_phase;
    unsigned long m_phase_start_time;

    int16_t *m_recorded_audio_buffer;
    int m_recorded_audio_capacity_samples;
    int m_recorded_audio_samples;
    int m_record_last_audio_position;

public:
    Application(I2SSampler *sample_provider, IntentProcessor *intent_processor, Speaker *speaker, IndicatorLight *indicator_light);
    ~Application();
    bool run();
    bool isInRecogniseState() const;
    AppPhase getPhase() const;
};

#endif
