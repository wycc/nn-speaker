#ifndef __ram_sample_source_h__
#define __ram_sample_source_h__

#include "SampleSource.h"

class RAMSampleSource : public SampleSource
{
private:
    int16_t *m_buffer;
    int m_total_samples;
    int m_position;

public:
    RAMSampleSource(int16_t *buffer, int total_samples)
        : m_buffer(buffer), m_total_samples(total_samples), m_position(0) {}

    int getFrames(Frame_t *frames, int number_frames)
    {
        int frames_read = 0;
        for (int i = 0; i < number_frames && m_position < m_total_samples; i++)
        {
            frames[i].left = frames[i].right = m_buffer[m_position++];
            frames_read++;
        }
        return frames_read;
    }

    bool available() { return m_position < m_total_samples; }
    void reset() { m_position = 0; }
};

#endif
