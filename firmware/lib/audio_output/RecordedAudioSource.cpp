// for hw3 modified: copied from reference project (nn-speaker/firmware)
// Implements RecordedAudioSource — wraps a recorded int16_t buffer as a
// SampleSource so I2SOutput can play it back through the speaker.
#include "RecordedAudioSource.h"

// ─── Constructor ─────────────────────────────────────────────────────────────

RecordedAudioSource::RecordedAudioSource(const int16_t *samples, int sample_count)
    : m_samples(samples),
      m_sample_count(sample_count),
      m_pos(0)
{
    // Nothing to allocate — we only hold a pointer to the recorder's buffer.
    // The caller must ensure the buffer stays alive for the lifetime of this
    // object (i.e. until I2SOutput finishes calling getFrames).
}

// ─── SampleSource interface ──────────────────────────────────────────────────

/**
 * getFrames
 *
 * Called by i2sWriterTask in batches of 256 frames.
 *
 * Step-by-step:
 *  1. Iterate up to number_frames times (or until we run out of samples).
 *  2. For each frame, read one mono int16_t sample from the recorded buffer.
 *  3. Duplicate it to both the left and right channel of the Frame_t struct
 *     so the audio comes out of both speakers at equal volume.
 *  4. Advance the playback position and count the frame as filled.
 *  5. Return the number of frames actually written.
 */
int RecordedAudioSource::getFrames(Frame_t *frames, int number_frames)
{
    int frames_filled = 0;

    for (int i = 0; i < number_frames && m_pos < m_sample_count; i++)
    {
        // for hw3 modified: ×8 gain to make playback louder; clamp to int16 range
        int32_t raw    = (int32_t)m_samples[m_pos] * 8;
        if (raw >  32767) raw =  32767;
        if (raw < -32768) raw = -32768;
        int16_t sample = (int16_t)raw;

        // duplicate mono sample to both channels
        frames[i].left  = sample;
        frames[i].right = sample;

        m_pos++;
        frames_filled++;
    }

    return frames_filled;
}
