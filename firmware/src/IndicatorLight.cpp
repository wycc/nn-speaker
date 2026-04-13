#include <Arduino.h>
#include "IndicatorLight.h"
#include "driver/uart.h"
#include <stdio.h>

void uart2_send(char * buf)
{
  while (*buf) {
    Serial2.write(*buf);
    vTaskDelay(2);
    buf++;
  }
}

// This task does all the heavy lifting for our application
void indicatorLedTask(void *param)
{
    IndicatorLight *indicator_light = static_cast<IndicatorLight *>(param);
    const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
    while (true)
    {
        // wait for someone to trigger us
        uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
        if (ulNotificationValue > 0)
        {
            switch (indicator_light->getState())
            {
            case OFF:
            {
                ledcWrite(0, 0);
                uart2_send((char *)"{8701fe}"); // off
                Serial.println("LED set to OFF.");
                break;
            }
            case ON:
            {
                ledcWrite(0, 255);
                uart2_send((char *)"{8701ff}"); // on
                Serial.println("LED set to ON.");
                break;
            }
            case PULSING:
            {
                // do a nice pulsing effect
                float angle = 0;
                while (indicator_light->getState() == PULSING)
                {
                    //ledcWrite(0, 255 * (0.5 * cos(angle) + 0.5));
                    int brightness = static_cast<int>(0x1f * (0.5 * cos(angle) + 0.5));
                    vTaskDelay(50 / portTICK_PERIOD_MS);
                    angle += 0.02 * M_PI;
                    char buf[10];
                    sprintf(buf, "{8701%02x}", brightness); 
                    uart2_send(buf);
                }
            }
            }
        }
    }
}

IndicatorLight::IndicatorLight()
{
    Serial2.begin(115200, SERIAL_8N1, 15, 19);
    uart2_send((char *)"{8701ff}"); // on
    vTaskDelay(100);
    uart2_send((char *)"{8701fe}"); // off

    // use the build in LED as an indicator - we'll set it up as a pwm output so we can make it glow nicely
    ledcSetup(0, 10000, 8);
    ledcAttachPin(2, 0);
    ledcWrite(0, 0);
    // start off with the light off
    m_state = OFF;
    // set up the task for controlling the light
    xTaskCreate(indicatorLedTask, "Indicator LED Task", 4096, this, 1, &m_taskHandle);
}

void IndicatorLight::setState(IndicatorState state)
{
    m_state = state;
    xTaskNotify(m_taskHandle, 1, eSetBits);
}

void IndicatorLight::setColor(uint8_t red, uint8_t green, uint8_t blue)
{
    // {81nnxdddd..}
    // 81: set LED register
    // nn: byte count (04 for one LED register: R,G,B,Power)
    // x : start LED index (0)
    // dddd..: data bytes = RR GG BB PP
    char buf[32];
    sprintf(buf, "{81040%02X%02X%02X1F}", red, green, blue);
    uart2_send(buf);
}

IndicatorState IndicatorLight::getState()
{
    return m_state;
}
