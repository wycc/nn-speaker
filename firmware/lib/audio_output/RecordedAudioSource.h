// for hw3 modified: copied from reference project (nn-speaker/firmware)
// A SampleSource that plays back a recorded int16_t buffer through I2SOutput.
// Used by Speaker::playRecording() after end-word is detected.
#ifndef __recorded_audio_source_h__
#define __recorded_audio_source_h__

#include "SampleSource.h"

/**
 * RecordedAudioSource
 *
 * A SampleSource that plays back a flat int16_t mono sample array through
 * the stereo I2SOutput driver.
 *
 * How it fits into the output pipeline
 * ─────────────────────────────────────
 * I2SOutput drives the codec in stereo (left + right = one Frame_t).
 * getFrames() is called by the i2sWriterTask in bursts of NUM_FRAMES_TO_SEND
 * (256 frames).  Each mono sample from the recording is duplicated to both
 * channels so the same audio plays from both speakers.
 *
 * available() returns false once all samples have been served, at which point
 * the i2sWriterTask automatically falls back to silence.
 */
class RecordedAudioSource : public SampleSource
{
public:
    // samples      — pointer to the int16_t buffer produced by AudioRecorder
    // sample_count — number of valid samples in that buffer
    RecordedAudioSource(const int16_t *samples, int sample_count);

    // Fill up to number_frames frames from the recorded buffer.
    // Returns the number of frames actually filled (may be less at end-of-data).
    int getFrames(Frame_t *frames, int number_frames) override;

    // False once every sample has been handed out.
    bool available() override { return m_pos < m_sample_count; }

private:
    const int16_t *m_samples;      // pointer to recorded data (not owned)
    int            m_sample_count; // total number of samples
    int            m_pos;          // current read position (advances each frame)
};

#endif // __recorded_audio_source_h__
