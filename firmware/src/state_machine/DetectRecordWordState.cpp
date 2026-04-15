// for hw3 modified: implementation of DetectRecordWordState
// Flow:
//   Phase 1 WAITING_START: run NN continuously; when start-word confirmed (1×)
//     → LED stays ON (長開) → print "Starting recording" → arm AudioRecorder
//   Phase 2 RECORDING: show ASCII Spectrum Visualizer; run NN; when end-word confirmed (1×)
//     → stop recorder → LED blink 1× → print "End recording" → play back audio
//   → return true to Application (go back to DetectWakeWordState / sleep)
#include <Arduino.h>
#include <math.h>
#include "I2SSampler.h"
#include "AudioProcessor.h"
#include "NeuralNetwork.h"
#include "AudioRecorder.h"
#include "RingBuffer.h"
#include "IndicatorLight.h"
#include "Speaker.h"
#include "DetectRecordWordState.h"
#include "model_recordword.h"   // declares converted_model_recordword_tflite[]

#define WINDOW_SIZE  320
#define STEP_SIZE    160
#define POOLING_SIZE 6
#define AUDIO_LENGTH 16000

// Derived spectrogram dimensions (must match AudioProcessor internals)
// fft_size = next power of 2 >= WINDOW_SIZE (320) = 512
// energy_size = 512/2+1 = 257
// pooled_energy_size = ceil(257/6) = 43
// num_frames = (AUDIO_LENGTH - WINDOW_SIZE) / STEP_SIZE = 98
#define SPEC_FFT_SIZE     512
#define SPEC_ENERGY_SIZE  (SPEC_FFT_SIZE / 2 + 1)                              // 257
#define SPEC_NUM_BINS     ((int)ceilf((float)SPEC_ENERGY_SIZE / POOLING_SIZE)) // 43
#define SPEC_NUM_FRAMES   ((AUDIO_LENGTH - WINDOW_SIZE) / STEP_SIZE)           // 98

// ─── ASCII Spectrum Visualizer ────────────────────────────────────────────────
// Copied from reference project (nn-speaker/firmware) DetectWakeWordState.cpp,
// with absolute normalization applied (floor=-5.0, ceiling=2.0) so that
// silence stays quiet-looking instead of stretching to fill all 9 levels.
//
// Pipeline:
//   spec[98][43]  → average time axis → bin_energy[43]
//   → map each bin to 0–8 level (absolute scale)
//   → scale 43 bins to 99 display columns
//   → print one line with rainbow ANSI colors

static void printSpectrum(float *spec)
{
    static const char  chars[]    = ".:-=+*#%@";
    static const int   num_chars  = 9;
    static const int   line_width = 99;

    // rainbow order: violet (quiet) → bright white (loud)
    static const char *colors[] = {
        "\033[35m",       // . violet
        "\033[34m",       // : indigo/blue
        "\033[36m",       // - blue/cyan
        "\033[32m",       // = green
        "\033[33m",       // + yellow
        "\033[38;5;214m", // * orange  (256-color)
        "\033[31m",       // # red
        "\033[91m",       // % bright red
        "\033[97m",       // @ bright white (loud)
    };
    static const char *reset = "\033[0m";

    // Step 1: collapse time axis — average 98 frames → 43 bin energies
    float bin_energy[SPEC_NUM_BINS];
    for (int bin = 0; bin < SPEC_NUM_BINS; bin++)
    {
        float sum = 0;
        for (int frame = 0; frame < SPEC_NUM_FRAMES; frame++)
            sum += spec[frame * SPEC_NUM_BINS + bin];
        bin_energy[bin] = sum / SPEC_NUM_FRAMES;
    }

    // Step 2: absolute normalization (floor=-5.0, ceiling=2.0 in log10 scale)
    // This keeps silence as low-level dots; voice energy shows as bright chars.
    static const float FLOOR   = -5.0f;
    static const float CEILING =  2.0f;

    // Step 3: render 99 characters with rainbow colors into one buffer
    char line[line_width * 16 + 8];
    int pos = 0;
    for (int i = 0; i < line_width; i++)
    {
        int bin   = i * SPEC_NUM_BINS / line_width;
        float val = bin_energy[bin];
        int level = (int)((val - FLOOR) / (CEILING - FLOOR) * (num_chars - 1));
        if (level < 0)          level = 0;
        if (level >= num_chars) level = num_chars - 1;
        const char *col = colors[level];
        while (*col) line[pos++] = *col++;
        line[pos++] = chars[level];
    }
    const char *r = reset;
    while (*r) line[pos++] = *r++;
    line[pos++] = '\n';
    line[pos]   = '\0';
    Serial.print(line);
}
// ─────────────────────────────────────────────────────────────────────────────

// ─── Constructor ─────────────────────────────────────────────────────────────

DetectRecordWordState::DetectRecordWordState(I2SSampler    *sampler,
                                             AudioRecorder *recorder,
                                             IndicatorLight *light,
                                             Speaker        *speaker)
    : m_sample_provider(sampler),
      m_recorder(recorder),
      m_indicator_light(light),
      m_speaker(speaker),
      m_nn(nullptr),
      m_audio_processor(nullptr),
      m_phase(WAITING_START),
      m_detection_count(0)
{
}

// ─── enterState ──────────────────────────────────────────────────────────────

void DetectRecordWordState::enterState()
{
    m_phase           = WAITING_START;
    m_detection_count = 0;

    m_audio_processor = new AudioProcessor(AUDIO_LENGTH, WINDOW_SIZE, STEP_SIZE, POOLING_SIZE);
    m_nn              = new NeuralNetwork(converted_model_recordword_tflite);

    Serial.println("[RecordWord] Ready — say the start-record word...");
}

// ─── run ─────────────────────────────────────────────────────────────────────

bool DetectRecordWordState::run()
{
    if (!m_nn || !m_audio_processor) return false;

    // ── get spectrogram (shared by both phases) ───────────────────────────────
    RingBufferAccessor *reader = m_sample_provider->getRingBufferReader();
    reader->rewind(AUDIO_LENGTH);
    m_audio_processor->get_spectrogram(reader, m_nn->getInputBuffer());
    delete reader;

    // ── Phase 1: waiting for start-record word ────────────────────────────────
    if (m_phase == WAITING_START)
    {
        float output = m_nn->predict();

        if (output > 0.95f)
        {
            m_detection_count++;
            // 2 consecutive confirmations to avoid false positives from room noise
            if (m_detection_count > 1)
            {
                m_detection_count = 0;

                m_indicator_light->setState(ON);  // for hw3 modified: 長開 LED
                Serial.println("[RecordWord] Starting recording");
                m_recorder->startRecording();
                m_phase = RECORDING;
            }
        }
        else
        {
            m_detection_count = 0;
        }
    }

    // ── Phase 2: recording ────────────────────────────────────────────────────
    else
    {
        // ── ASCII Spectrum Visualizer ─────────────────────────────────────────
        // Called every inference cycle (~100 ms).
        // Prints one 99-char line showing frequency energy with rainbow colors.
        // Adapted from reference project (nn-speaker/firmware) with absolute
        // normalization so silence stays quiet-looking.
        printSpectrum(m_nn->getInputBuffer());

        // ── End-word detection ────────────────────────────────────────────────
        float output = m_nn->predict();

        if (output > 0.95f)
        {
            m_detection_count++;
            // 2 consecutive confirmations to avoid false positives
            if (m_detection_count > 1)
            {
                m_detection_count = 0;

                m_recorder->stopRecording();
                Serial.printf("[RecordWord] End recording — %d samples (%.2f s)\n",
                              m_recorder->getSampleCount(),
                              (float)m_recorder->getSampleCount() / RECORD_SAMPLE_RATE);

                m_indicator_light->setState(OFF);  // for hw3 modified: 停長開
                m_speaker->playRecording(m_recorder->getSamples(),
                                         m_recorder->getSampleCount());

                return true;
            }
        }
        else
        {
            m_detection_count = 0;
        }

        // ── Auto-stop when buffer is full ─────────────────────────────────────
        if (!m_recorder->isRecording())
        {
            Serial.println("[RecordWord] Buffer full — auto end recording");
            m_indicator_light->setState(OFF);  // for hw3 modified: 停長開
            m_speaker->playRecording(m_recorder->getSamples(),
                                     m_recorder->getSampleCount());
            return true;
        }
    }

    return false;
}

// ─── exitState ───────────────────────────────────────────────────────────────

void DetectRecordWordState::exitState()
{
    if (m_recorder->isRecording())
        m_recorder->stopRecording();

    delete m_audio_processor;
    m_audio_processor = nullptr;

    delete m_nn;
    m_nn = nullptr;

    Serial.printf("[RecordWord] exitState — free heap %u\n", esp_get_free_heap_size());
}
