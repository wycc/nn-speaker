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

public:
    IndicatorLight();
    void setState(IndicatorState state);
    void setColor(uint8_t red, uint8_t green, uint8_t blue);
    IndicatorState getState();
};

#endif
