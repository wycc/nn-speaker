#ifndef _speaker_h_
#define _speaker_h_

#include <stdint.h>

class I2SOutput;
class WAVFileReader;
class RAMSampleSource;

class Speaker
{
private:
    WAVFileReader *m_ok;
    WAVFileReader *m_cantdo;
    WAVFileReader *m_ready_ping;
    WAVFileReader *m_light_on;
    WAVFileReader *m_light_off;
    WAVFileReader *m_life;
    WAVFileReader *m_jokes[5];
    RAMSampleSource *m_ram_source;

    I2SOutput *m_i2s_output;

public:
    Speaker(I2SOutput *i2s_output);
    ~Speaker();
    void playOK();
    void playReady();
    void playCantDo();
    void playLightOn();
    void playLightOff();
    void playRandomJoke();
    void playLife();
    void playRecordedAudio(const int16_t *samples, int sample_count);
    bool isPlaying() const;
};

#endif
