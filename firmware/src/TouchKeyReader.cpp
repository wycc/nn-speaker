#include "TouchKeyReader.h"

#include "Speaker.h"

#include <esp_log.h>

namespace
{
    constexpr const char *TAG = "TouchKeyReader";
    constexpr uint8_t BS8112_STATUS_REG = 0x08;
    constexpr int TOUCH_FIRST_CHANNEL = 2;
    constexpr int TOUCH_LAST_CHANNEL = 9;
    constexpr int POLL_INTERVAL_MS = 10;
    constexpr int LONG_PRESS_VALID_MS = 150;
    constexpr uint8_t NOISE_FILTER_MASK = 0x80;
    constexpr int I2C_TIMEOUT_MS = 20;
}

TouchKeyReader::TouchKeyReader(Speaker *speaker,
                               i2c_port_t i2cPort,
                               gpio_num_t sdaPin,
                               gpio_num_t sclPin,
                               uint8_t deviceAddr)
    : m_speaker(speaker),
      m_i2cPort(i2cPort),
      m_sdaPin(sdaPin),
      m_sclPin(sclPin),
      m_deviceAddr(deviceAddr)
{
}

bool TouchKeyReader::begin()
{
    if (!initI2C())
    {
        ESP_LOGE(TAG, "I2C init failed");
        return false;
    }

    BaseType_t ok = xTaskCreate(taskEntry,
                                "TouchKeyTask",
                                4096,
                                this,
                                5,
                                nullptr);
    if (ok != pdPASS)
    {
        ESP_LOGE(TAG, "Create touch task failed");
        return false;
    }

    ESP_LOGI(TAG, "Touch key task started");
    return true;
}

void TouchKeyReader::taskEntry(void *param)
{
    auto *self = static_cast<TouchKeyReader *>(param);
    self->taskLoop();
}

void TouchKeyReader::taskLoop()
{
    uint16_t prevState = 0;
    TickType_t pressTick[10] = {0};
    bool keyValid[10] = {false};
    const TickType_t validTicks = pdMS_TO_TICKS(LONG_PRESS_VALID_MS);

    for (;;)
    {
        uint16_t state = 0;
        if (readState(state) != ESP_OK)
        {
            vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
            continue;
        }

        TickType_t now = xTaskGetTickCount();

        if (state != prevState)
        {
            uint16_t changed = state ^ prevState;
            for (int ch = TOUCH_FIRST_CHANNEL; ch <= TOUCH_LAST_CHANNEL; ++ch)
            {
                uint16_t mask = static_cast<uint16_t>(1U << ch);
                if ((changed & mask) == 0)
                {
                    continue;
                }

                bool pressed = (state & mask) != 0;
                if (pressed)
                {
                    pressTick[ch] = now;
                    keyValid[ch] = false;
                }
                else
                {
                    uint32_t holdMs = static_cast<uint32_t>((now - pressTick[ch]) * portTICK_PERIOD_MS);
                    if (keyValid[ch])
                    {
                        ESP_LOGI(TAG, "[RELEASE] %s hold=%lums", channelName(ch), static_cast<unsigned long>(holdMs));
                    }
                    else
                    {
                        ESP_LOGD(TAG, "[DISCARD] %s short=%lums", channelName(ch), static_cast<unsigned long>(holdMs));
                    }
                    keyValid[ch] = false;
                }
            }
        }

        for (int ch = TOUCH_FIRST_CHANNEL; ch <= TOUCH_LAST_CHANNEL; ++ch)
        {
            uint16_t mask = static_cast<uint16_t>(1U << ch);
            if ((state & mask) == 0 || keyValid[ch])
            {
                continue;
            }

            if ((now - pressTick[ch]) >= validTicks)
            {
                keyValid[ch] = true;
                ESP_LOGI(TAG, "[PRESS] %s", channelName(ch));
                Serial.printf("Channel %d (%s) pressed\n", ch, channelName(ch));

                // 依參考程式映射：bit5 (T5) 對應 PLAY
                if (ch == 5 && m_speaker != nullptr)
                {
                    ESP_LOGI(TAG, "PLAY pressed -> play OK sound");
                    m_speaker->playOK();
                }
            }
        }

        prevState = state;
        vTaskDelay(pdMS_TO_TICKS(POLL_INTERVAL_MS));
    }
}

bool TouchKeyReader::initI2C()
{
    // 可能已被其他模組初始化；先刪除可避免 install error。
    // 若尚未安裝，回傳錯誤可忽略。
    i2c_driver_delete(m_i2cPort);

    i2c_config_t conf = {};
    conf.mode = I2C_MODE_MASTER;
    conf.sda_io_num = m_sdaPin;
    conf.scl_io_num = m_sclPin;
    conf.sda_pullup_en = GPIO_PULLUP_ENABLE;
    conf.scl_pullup_en = GPIO_PULLUP_ENABLE;
    conf.master.clk_speed = 400000;

    esp_err_t ret = i2c_param_config(m_i2cPort, &conf);
    if (ret != ESP_OK)
    {
        ESP_LOGE(TAG, "i2c_param_config failed: %d", ret);
        return false;
    }

    ret = i2c_driver_install(m_i2cPort, conf.mode, 0, 0, 0);
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "i2c_driver_install failed once: %d, retrying", ret);
        i2c_driver_delete(m_i2cPort);
        ret = i2c_driver_install(m_i2cPort, conf.mode, 0, 0, 0);
        if (ret != ESP_OK)
        {
            ESP_LOGE(TAG, "i2c_driver_install failed again: %d", ret);
            return false;
        }
    }

    return true;
}

esp_err_t TouchKeyReader::readState(uint16_t &state)
{
    uint8_t reg = BS8112_STATUS_REG;
    uint8_t data[2] = {0, 0};
    esp_err_t ret = i2c_master_write_read_device(
        m_i2cPort,
        m_deviceAddr,
        &reg,
        1,
        data,
        sizeof(data),
        pdMS_TO_TICKS(I2C_TIMEOUT_MS));
    if (ret != ESP_OK)
    {
        ESP_LOGW(TAG, "I2C read failed: %d", ret);
        return ret;
    }

    filterNoise(data);
    state = static_cast<uint16_t>(data[0] | (data[1] << 8));
    return ESP_OK;
}

void TouchKeyReader::filterNoise(uint8_t *data)
{
    if (data[1] & NOISE_FILTER_MASK)
    {
        data[1] = 0;
    }
}

const char *TouchKeyReader::channelName(int channel)
{
    switch (channel)
    {
    case 2:
        return "LIT- (T2)";
    case 3:
        return "LIT+ (T3)";
    case 4:
        return "PREV (T4)";
    case 5:
        return "PLAY (T5)";
    case 6:
        return "NEXT (T6)";
    case 7:
        return "VOL- (T7)";
    case 8:
        return "VOL+ (T8)";
    case 9:
        return "FAVO (T9)";
    default:
        return "UNKNOWN";
    }
}
