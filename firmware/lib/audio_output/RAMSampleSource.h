#ifndef __ram_sample_source_h__
#define __ram_sample_source_h__

#include "SampleSource.h"

class RAMSampleSource : public SampleSource
{
private:
    const int16_t *m_samples;
    int m_total_samples;
    int m_position;

public:
    RAMSampleSource();
    void setBuffer(const int16_t *samples, int total_samples);
    int getFrames(Frame_t *frames, int number_frames) override;
    bool available() override;
};

#endif
