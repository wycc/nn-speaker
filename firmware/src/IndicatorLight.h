#ifndef _indicator_light_h_
#define _indicator_light_h_

#include <stdint.h>

enum IndicatorState
{
    OFF,
    ON,
    PULSING
};

class IndicatorLight
{
private:
    IndicatorState m_state;
    TaskHandle_t m_taskHandle;
    uint8_t m_red;
    uint8_t m_green;
    uint8_t m_blue;

public:
    IndicatorLight();
    void setState(IndicatorState state);
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    IndicatorState getState();
    void getColor(uint8_t &red, uint8_t &green, uint8_t &blue);
    bool getMicronStatus(uint8_t &statusFlags, uint16_t &batteryRaw);
};

#endif
