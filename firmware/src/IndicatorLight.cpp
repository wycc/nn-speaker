#include <Arduino.h>
#include "IndicatorLight.h"
#include "driver/uart.h"
#include <stdio.h>

static int hexNibble(char c)
{
    if (c >= '0' && c <= '9')
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return 10 + (c - 'a');
    }
    if (c >= 'A' && c <= 'F')
    {
        return 10 + (c - 'A');
    }
    return -1;
}

static bool parseHexByte(char hi, char lo, uint8_t &out)
{
    int h = hexNibble(hi);
    int l = hexNibble(lo);
    if (h < 0 || l < 0)
    {
        return false;
    }
    out = static_cast<uint8_t>((h << 4) | l);
    return true;
}

static bool parseHexWord(char c0, char c1, char c2, char c3, uint16_t &out)
{
    int n0 = hexNibble(c0);
    int n1 = hexNibble(c1);
    int n2 = hexNibble(c2);
    int n3 = hexNibble(c3);
    if (n0 < 0 || n1 < 0 || n2 < 0 || n3 < 0)
    {
        return false;
    }
    out = static_cast<uint16_t>((n0 << 12) | (n1 << 8) | (n2 << 4) | n3);
    return true;
}

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
    m_red = 255;
    m_green = 0;
    m_blue = 0;
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
    m_red = red;
    m_green = green;
    m_blue = blue;

    // {81nnxdddd..}
    // 81: set LED register
    // nn: byte count (6 * 4 = 24 bytes = 0x18)
    // x : start LED index (0)
    // dddd..: 6 groups of RR GG BB PP
    char buf[96];
    int pos = snprintf(buf, sizeof(buf), "{81180");
    if (pos < 0 || pos >= static_cast<int>(sizeof(buf)))
    {
        return;
    }

    for (int i = 0; i < 6; ++i)
    {
        int wrote = snprintf(buf + pos, sizeof(buf) - pos, "%02X%02X%02X1F", red, green, blue);
        if (wrote < 0 || wrote >= static_cast<int>(sizeof(buf) - pos))
        {
            return;
        }
        pos += wrote;
    }

    if (pos < static_cast<int>(sizeof(buf)) - 1)
    {
        buf[pos++] = '}';
        buf[pos] = '\0';
    }
    else
    {
        return;
    }

    uart2_send(buf);
}

void IndicatorLight::getColor(uint8_t &red, uint8_t &green, uint8_t &blue)
{
    red = m_red;
    green = m_green;
    blue = m_blue;
}

bool IndicatorLight::getMicronStatus(uint8_t &statusFlags, uint16_t &batteryRaw)
{
    while (Serial2.available() > 0)
    {
        (void)Serial2.read();
    }

    uart2_send((char *)"{04}");

    const uint32_t timeoutMs = 800;
    uint32_t start = millis();
    bool inFrame = false;
    String frame;

    while ((millis() - start) < timeoutMs)
    {
        while (Serial2.available() > 0)
        {
            char ch = static_cast<char>(Serial2.read());
            if (ch == '{')
            {
                frame = "{";
                inFrame = true;
                continue;
            }

            if (!inFrame)
            {
                continue;
            }

            frame += ch;
            if (ch == '}')
            {
                // Ignore unrelated UART frames and keep waiting until timeout.
                if (!frame.startsWith("{44"))
                {
                    inFrame = false;
                    frame = "";
                    continue;
                }

                // frame without braces, e.g. 440003ffbbbb or 44mmmmffbbbbtttt
                String payload = frame.substring(1, frame.length() - 1);
                if (payload.length() < 12)
                {
                    inFrame = false;
                    frame = "";
                    continue;
                }

                if (!parseHexByte(payload[6], payload[7], statusFlags))
                {
                    inFrame = false;
                    frame = "";
                    continue;
                }

                if (!parseHexWord(payload[8], payload[9], payload[10], payload[11], batteryRaw))
                {
                    inFrame = false;
                    frame = "";
                    continue;
                }

                return true;
            }
        }
        vTaskDelay(2 / portTICK_PERIOD_MS);
    }

    return false;
}

IndicatorState IndicatorLight::getState()
{
    return m_state;
}
