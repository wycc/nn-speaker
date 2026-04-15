#include "Speaker.h"
#include "I2SOutput.h"
#include "WAVFileReader.h"
#include "RecordedAudioSource.h" // for hw3 modified: needed for playRecording()

Speaker::Speaker(I2SOutput *i2s_output)
{
    m_i2s_output = i2s_output;
    m_ok = new WAVFileReader("/ok.wav");
    m_ready_ping = new WAVFileReader("/ready_ping.wav");
    m_cantdo = new WAVFileReader("/cantdo.wav");
    m_light_on = new WAVFileReader("/light_on.wav");
    m_light_off = new WAVFileReader("/light_off.wav");
    m_life = new WAVFileReader("/life.wav");
    m_jokes[0] = new WAVFileReader("/joke0.wav");
    m_jokes[1] = new WAVFileReader("/joke1.wav");
    m_jokes[2] = new WAVFileReader("/joke2.wav");
    m_jokes[3] = new WAVFileReader("/joke3.wav");
    m_jokes[4] = new WAVFileReader("/joke4.wav");
}

Speaker::~Speaker()
{
    delete m_ok;
    delete m_ready_ping;
    delete m_cantdo;
    delete m_light_on;
    delete m_light_off;
    delete m_life;
    delete m_jokes[0];
    delete m_jokes[1];
    delete m_jokes[2];
    delete m_jokes[3];
    delete m_jokes[4];
}

void Speaker::playOK()
{
    m_ok->reset();
    m_i2s_output->setSampleGenerator(m_ok);
}

void Speaker::playReady()
{
    m_ready_ping->reset();
    m_i2s_output->setSampleGenerator(m_ready_ping);
}

void Speaker::playCantDo()
{
    m_cantdo->reset();
    m_i2s_output->setSampleGenerator(m_cantdo);
}

void Speaker::playLightOn()
{
    m_light_on->reset();
    m_i2s_output->setSampleGenerator(m_light_on);
}

void Speaker::playLightOff()
{
    m_light_off->reset();
    m_i2s_output->setSampleGenerator(m_light_off);
}

void Speaker::playRandomJoke()
{
    int joke = random(5);
    m_i2s_output->setSampleGenerator(m_jokes[joke]);
}

void Speaker::playLife()
{
    m_life->reset();
    m_i2s_output->setSampleGenerator(m_life);
}

// for hw3 modified: play back recorded PCM audio ──────────────────────────────
// Step 1 — wrap the raw buffer in a SampleSource (no copy, just a pointer).
// Step 2 — hand it to I2SOutput; the i2sWriterTask calls getFrames() in a loop.
// Step 3 — wait for playback duration + 500 ms margin.
//           The +500 ms margin exists because setSampleGenerator() is async:
//           the i2sWriterTask may still be finishing a DMA buffer when we call
//           it, so the first getFrames() call could be delayed by up to one
//           buffer period.  Without margin the tail of the recording is cut off.
// Step 4 — detach source so the writer task stops using the pointer.
// Step 5 — give the writer task one buffer period (~20 ms) to finish any
//           in-progress getFrames() call before we free the object.
void Speaker::playRecording(const int16_t *samples, int sample_count)
{
    if (!samples || sample_count <= 0) return;

    RecordedAudioSource *source = new RecordedAudioSource(samples, sample_count);

    // Step 2
    m_i2s_output->setSampleGenerator(source);

    // Step 3: actual duration + safety margin
    uint32_t playback_ms = (uint32_t)(1000UL * sample_count / 16000) + 500;
    vTaskDelay(pdMS_TO_TICKS(playback_ms));

    // Step 4: detach before deleting so writer task never uses a freed pointer
    m_i2s_output->setSampleGenerator(nullptr);

    // Step 5: one DMA buffer period safety gap
    vTaskDelay(pdMS_TO_TICKS(20));

    delete source;
}
// ─────────────────────────────────────────────────────────────────────────────
