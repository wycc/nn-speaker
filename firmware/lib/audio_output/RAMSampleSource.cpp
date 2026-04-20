#include "RAMSampleSource.h"

RAMSampleSource::RAMSampleSource()
{
    m_samples = nullptr;
    m_total_samples = 0;
    m_position = 0;
}

void RAMSampleSource::setBuffer(const int16_t *samples, int total_samples)
{
    m_samples = samples;
    m_total_samples = total_samples;
    m_position = 0;
}

int RAMSampleSource::getFrames(Frame_t *frames, int number_frames)
{
    if (!m_samples || m_position >= m_total_samples)
    {
        return 0;
    }

    int remaining = m_total_samples - m_position;
    int frames_to_copy = remaining < number_frames ? remaining : number_frames;
    for (int i = 0; i < frames_to_copy; i++)
    {
        int16_t s = m_samples[m_position++];
        frames[i].left = s;
        frames[i].right = s;
    }
    return frames_to_copy;
}

bool RAMSampleSource::available()
{
    return m_samples != nullptr && m_position < m_total_samples;
}
