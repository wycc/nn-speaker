#ifndef _indicator_light_h_
#define _indicator_light_h_

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
    IndicatorState getState();
    // for hw3 modified: blocking blink — turns LED on/off N times
    void blink(int times);
    // for hw3 modified: blocking blink in blue color via RGB LED (UART2)
    // format: {81041RRGGBBFF} — blue = {810410000ffff}, off = {81041000000ff}
    void blinkBlue(int times);
};

#endif