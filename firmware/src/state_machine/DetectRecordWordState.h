// for hw3 modified: new state — detects start/end record words via NN,
// buffers audio between them, and plays back through speaker when done.
#ifndef _detect_record_word_state_h_
#define _detect_record_word_state_h_

#include "States.h"

class I2SSampler;
class NeuralNetwork;
class AudioProcessor;
class AudioRecorder;
class IndicatorLight;
class Speaker;

class DetectRecordWordState : public State
{
public:
    DetectRecordWordState(I2SSampler    *sampler,
                          AudioRecorder *recorder,
                          IndicatorLight *light,
                          Speaker        *speaker);

    void enterState() override;
    bool run()        override;
    void exitState()  override;

private:
    // Two-phase recording flow:
    //   WAITING_START — NN listens for the start-record word
    //   RECORDING     — AudioRecorder captures audio; NN listens for end-record word
    enum Phase { WAITING_START, RECORDING };

    I2SSampler     *m_sample_provider;
    AudioRecorder  *m_recorder;
    IndicatorLight *m_indicator_light;
    Speaker        *m_speaker;

    NeuralNetwork  *m_nn;            // loaded with model_recordword in enterState()
    AudioProcessor *m_audio_processor;

    Phase m_phase;
    int   m_detection_count;        // for hw3 modified: 1 detection confirms word (was >1)
};

#endif // _detect_record_word_state_h_
