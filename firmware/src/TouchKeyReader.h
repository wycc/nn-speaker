#ifndef _touch_key_reader_h_
#define _touch_key_reader_h_

#include <Arduino.h>
#include <driver/i2c.h>

class Speaker;

class TouchKeyReader
{
public:
    TouchKeyReader(Speaker *speaker,
                   i2c_port_t i2cPort,
                   gpio_num_t sdaPin,
                   gpio_num_t sclPin,
                   uint8_t deviceAddr = 0x50);

    bool begin();

private:
    static void taskEntry(void *param);
    void taskLoop();

    bool initI2C();
    esp_err_t readState(uint16_t &state);
    static void filterNoise(uint8_t *data);
    static const char *channelName(int channel);

    Speaker *m_speaker;
    i2c_port_t m_i2cPort;
    gpio_num_t m_sdaPin;
    gpio_num_t m_sclPin;
    uint8_t m_deviceAddr;
};

#endif

