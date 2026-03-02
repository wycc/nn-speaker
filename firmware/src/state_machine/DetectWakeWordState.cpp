#include <Arduino.h>
#include "I2SSampler.h"
#include "AudioProcessor.h"
#include "NeuralNetwork.h"
#include "RingBuffer.h"
#include "DetectWakeWordState.h"
#include "config.h"

#define WINDOW_SIZE 320
#define STEP_SIZE 160
#define POOLING_SIZE 6
#define AUDIO_LENGTH 16000

#if INFER_DIAG
namespace
{
const int kInferDiagRows = 99;
const int kInferDiagCols = 43;
const char kAsciiLevels[] = " .:-=+*#%@";

void dumpSpectrumAscii(const float *spectrum, uint32_t infer_index, uint32_t timestamp_ms)
{
    if (!spectrum)
    {
        return;
    }

    const int total = kInferDiagRows * kInferDiagCols;
    float min_value = spectrum[0];
    float max_value = spectrum[0];

    for (int i = 1; i < total; ++i)
    {
        const float v = spectrum[i];
        if (v < min_value)
        {
            min_value = v;
        }
        if (v > max_value)
        {
            max_value = v;
        }
    }

    const int level_count = sizeof(kAsciiLevels) - 1;
    const float range = max_value - min_value;
    const float inv_range = (range > 1e-12f) ? (1.0f / range) : 0.0f;

    Serial.printf("[INFER_DIAG] spectrum dump begin idx=%u ts=%ums min=%.6f max=%.6f\n",
                  infer_index, timestamp_ms, min_value, max_value);

    char line[kInferDiagCols + 1];
    line[kInferDiagCols] = '\0';

    for (int r = 0; r < kInferDiagRows; ++r)
    {
        const int row_base = r * kInferDiagCols;
        for (int c = 0; c < kInferDiagCols; ++c)
        {
            const float v = spectrum[row_base + c];
            const float normalized = (range > 1e-12f) ? ((v - min_value) * inv_range) : 0.0f;
            int idx = static_cast<int>(normalized * (level_count - 1));
            if (idx < 0)
            {
                idx = 0;
            }
            if (idx >= level_count)
            {
                idx = level_count - 1;
            }
            line[c] = kAsciiLevels[idx];
        }
        Serial.println(line);
    }

    Serial.println("[INFER_DIAG] spectrum dump end ----------------------------------------");
}
} // namespace
#endif

DetectWakeWordState::DetectWakeWordState(I2SSampler *sample_provider)
{
    // save the sample provider for use later
    m_sample_provider = sample_provider;
    // some stats on performance
    m_average_detect_time = 0;
    m_average_encode_time = 0;

    m_number_of_runs = 0;
    m_nn = NULL;
    m_audio_processor = NULL;
#if INFER_DIAG
    m_infer_index = 0;
#endif
}
void DetectWakeWordState::enterState()
{
    // create our audio processor
    m_audio_processor = new AudioProcessor(AUDIO_LENGTH, WINDOW_SIZE, STEP_SIZE, POOLING_SIZE);
    Serial.println("Created audio processor");

    // recreate the neural network only while this state is active
    // so the tensor arena can be released before HTTPS/TLS is started
    m_nn = new NeuralNetwork();
    Serial.println("Created Neural Net");

    m_number_of_detections = 0;
}
bool DetectWakeWordState::run()
{
    if (!m_nn || !m_audio_processor)
    {
        return false;
    }

    // time how long this takes for stats
    long start = millis();
    // get access to the samples that have been read in
    RingBufferAccessor *reader = m_sample_provider->getRingBufferReader();
    // rewind by 1 second
    reader->rewind(16000);
    // get hold of the input buffer for the neural network so we can feed it data
    float *input_buffer = m_nn->getInputBuffer();
    // process the samples to get the spectrogram
    m_audio_processor->get_spectrogram(reader, input_buffer);
#if INFER_DIAG
    dumpSpectrumAscii(input_buffer, m_infer_index, millis());
    ++m_infer_index;
#endif
    // finished with the sample reader
    long end = millis();
    m_average_encode_time = (end - start) * 0.1 + m_average_encode_time * 0.9;
    start = end;
    delete reader;
    // get the prediction for the spectrogram
    float output = m_nn->predict();
    end = millis();
    // compute the stats
    m_average_detect_time = (end - start) * 0.1 + m_average_detect_time * 0.9;
    m_number_of_runs++;
    // log out some timing info
    if (m_number_of_runs == 100)
    {
        m_number_of_runs = 0;
        Serial.printf("Average detection time %.fms %.fms\n", m_average_detect_time,m_average_encode_time);
    }
    // use quite a high threshold to prevent false positives
    if (output > 0.95)
    {
        m_number_of_detections++;
        if (m_number_of_detections > 1)
        {
            m_number_of_detections = 0;
            uint32_t free_ram = esp_get_free_heap_size();
            Serial.printf("Free ram after DetectWakeWord cleanup %d\n", free_ram);

            // detected the wake word in several runs, move to the next state
            Serial.printf("P(%.2f): Here I am, brain the size of a planet...\n", output);
            
            return true;
        }
    }
    // nothing detected stay in the current state
    return false;
}
void DetectWakeWordState::exitState()
{
    // Release wake-word detection resources to maximize free heap for TLS.
    uint32_t free_ram = esp_get_free_heap_size();
    Serial.printf("Free ram before DetectWakeWord cleanup %d\n", free_ram);

    delete m_audio_processor;
    m_audio_processor = NULL;

    delete m_nn;
    m_nn = NULL;

    free_ram = esp_get_free_heap_size();
    Serial.printf("Free ram after DetectWakeWord cleanup %d\n", free_ram);
}
