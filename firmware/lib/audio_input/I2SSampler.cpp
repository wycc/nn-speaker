#include <Arduino.h>
#include "I2SSampler.h"
#include <driver/i2s.h>
#include <algorithm>
#include "RingBuffer.h"

void I2SSampler::addSample(int16_t sample)
{
    // store the sample in the circular ring buffer (wake-word pipeline)
    m_write_ring_buffer_accessor->setCurrentSample(sample);
    if (m_write_ring_buffer_accessor->moveToNextSample())
    {
        // trigger the processor task as we've filled a buffer
        xTaskNotify(m_processor_task_handle, 1, eSetBits);
    }

    // for hw3 modified: if recording hook is armed, also copy into flat buffer
    if (m_record_active && m_record_count < m_record_max)
    {
        m_record_buf[m_record_count++] = sample;
        // auto-disarm when buffer is full
        if (m_record_count >= m_record_max)
        {
            m_record_active = false;
        }
    }
}

void i2sReaderTask(void *param)
{
    I2SSampler *sampler = (I2SSampler *)param;
    while (true)
    {
        // wait for some data to arrive on the queue
        i2s_event_t evt;
        if (1)//(xQueueReceive(sampler->m_i2s_queue, &evt, portMAX_DELAY) == pdPASS)
        {
            if (1)//(evt.type == I2S_EVENT_RX_DONE)
            {
                Serial.println("RX start");
                size_t bytesRead = 0;
                do
                {
                    // read data from the I2S peripheral
                    uint8_t i2sData[1024];
                    // read from i2s
                    i2s_read(sampler->getI2SPort(), i2sData, 1024, &bytesRead, 10);
                    // process the raw data
                    sampler->processI2SData(i2sData, bytesRead);
                } while (bytesRead > 0);
            }
        }
    }
}

I2SSampler::I2SSampler()
{
    // allocate the audio buffers
    for (int i = 0; i < AUDIO_BUFFER_COUNT; i++)
    {
        m_audio_buffers[i] = new AudioBuffer();
    }
    m_write_ring_buffer_accessor = new RingBufferAccessor(m_audio_buffers, AUDIO_BUFFER_COUNT);

    // for hw3 modified: recording hook is idle until startRecording() is called
    m_record_buf    = nullptr;
    m_record_max    = 0;
    m_record_count  = 0;
    m_record_active = false;
}

// for hw3 modified: arm the recording hook ───────────────────────────────────
void I2SSampler::startRecording(int16_t *buf, int max_samples)
{
    m_record_buf    = buf;
    m_record_max    = max_samples;
    m_record_count  = 0;       // reset so previous data is overwritten
    m_record_active = true;    // arm — addSample() starts copying from here
}

// for hw3 modified: disarm the hook and return number of samples captured ────
int I2SSampler::stopRecording()
{
    m_record_active = false;
    return m_record_count;
}
// ─────────────────────────────────────────────────────────────────────────────

void I2SSampler::start(i2s_port_t i2s_port, i2s_config_t &i2s_config, TaskHandle_t processor_task_handle)
{
    Serial.println("Starting i2s");
    m_i2s_port = i2s_port;
    m_processor_task_handle = processor_task_handle;
    //install and start i2s driver
//    i2s_driver_install(m_i2s_port, &i2s_config, 4, &m_i2s_queue);
    // set up the I2S configuration from the subclass
//    configureI2S();
    // start a task to read samples
    xTaskCreate(i2sReaderTask, "i2s Reader Task", 4096, this, 1, &m_reader_task_handle);
}

RingBufferAccessor *I2SSampler::getRingBufferReader()
{
    RingBufferAccessor *reader = new RingBufferAccessor(m_audio_buffers, AUDIO_BUFFER_COUNT);
    // place the reaader at the same position as the writer - clients can move it around as required
    reader->setIndex(m_write_ring_buffer_accessor->getIndex());
    return reader;
}
