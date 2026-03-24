#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include "I2SMicSampler.h"
#include "ADCSampler.h"
#include "I2SOutput.h"
#include "config.h"
#include "Application.h"
#include "SPIFFS.h"
#include "IntentProcessor.h"
#include "Speaker.h"
#include "IndicatorLight.h"
#include "AudioKitHAL.h"

// i2s config for using the internal ADC
i2s_config_t adcI2SConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_ADC_BUILT_IN),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_ONLY_LEFT,
    .communication_format = I2S_COMM_FORMAT_STAND_MSB,
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// i2s config for reading from both channels of I2S
i2s_config_t i2sMemsConfigBothChannels = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_32BIT,
    .channel_format = I2S_MIC_CHANNEL,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// i2s config for codec
i2s_config_t i2sCodecConfig = {
    .mode = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX | I2S_MODE_TX),
    .sample_rate = 16000,
    .bits_per_sample = I2S_BITS_PER_SAMPLE_16BIT,
    .channel_format = I2S_CHANNEL_FMT_RIGHT_LEFT,
    .communication_format = i2s_comm_format_t(I2S_COMM_FORMAT_STAND_I2S),
    .intr_alloc_flags = ESP_INTR_FLAG_LEVEL1,
    .dma_buf_count = 4,
    .dma_buf_len = 64,
    .use_apll = false,
    .tx_desc_auto_clear = false,
    .fixed_mclk = 0};

// i2s codec pins
i2s_pin_config_t i2s_codec_pins = {
    .bck_io_num = 5,
    .ws_io_num = 25,
    .data_out_num = 26,
    .data_in_num = 35};

// i2s microphone pins
i2s_pin_config_t i2s_mic_pins = {
    .bck_io_num = I2S_MIC_SERIAL_CLOCK,
    .ws_io_num = I2S_MIC_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_PIN_NO_CHANGE,
    .data_in_num = I2S_MIC_SERIAL_DATA};

// i2s speaker pins
i2s_pin_config_t i2s_speaker_pins = {
    .bck_io_num = I2S_SPEAKER_SERIAL_CLOCK,
    .ws_io_num = I2S_SPEAKER_LEFT_RIGHT_CLOCK,
    .data_out_num = I2S_SPEAKER_SERIAL_DATA,
    .data_in_num = I2S_PIN_NO_CHANGE};

void es8388_init(void)
{
    audiokit::AudioKit kit;
    auto cfg = kit.defaultConfig(audiokit::KitInputOutput);
    cfg.sample_rate = AUDIO_HAL_16K_SAMPLES;
    cfg.i2s_active = false;
    Serial.println("set AudioKit");
    kit.begin(cfg);
    kit.setVolume(100);
}

// This task does all the heavy lifting for our application
void applicationTask(void *param)
{
  Application *application = static_cast<Application *>(param);

  const TickType_t xMaxBlockTime = pdMS_TO_TICKS(100);
  while (true)
  {
    // wait for some audio samples to arrive
    uint32_t ulNotificationValue = ulTaskNotifyTake(pdTRUE, xMaxBlockTime);
    if (ulNotificationValue > 0)
    {
      application->run();
    }
  }
}

namespace
{
uint8_t *g_internalBuf = nullptr;
uint8_t *g_dmaBuf = nullptr;
uint8_t *g_psramBuf = nullptr;
unsigned long g_lastMemoryExerciseMs = 0;
constexpr unsigned long kMemoryExerciseIntervalMs = 5000;
}

void printCapabilityStatus(const char *label, uint32_t capability)
{
  Serial.printf("%s free: %u bytes\n", label, heap_caps_get_free_size(capability));
  Serial.printf("%s largest block: %u bytes\n", label, heap_caps_get_largest_free_block(capability));
}

void printMemoryStatus(const char *phase)
{
  Serial.printf("\n=== %s ===\n", phase);
  Serial.printf("Heap total: %u bytes\n", ESP.getHeapSize());
  Serial.printf("Heap free: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Internal free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("PSRAM free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  printCapabilityStatus("MALLOC_CAP_INTERNAL", MALLOC_CAP_INTERNAL);
  printCapabilityStatus("MALLOC_CAP_8BIT", MALLOC_CAP_8BIT);
  printCapabilityStatus("MALLOC_CAP_DMA", MALLOC_CAP_DMA);
  printCapabilityStatus("MALLOC_CAP_SPIRAM", MALLOC_CAP_SPIRAM);
}

void releaseMemoryBuffers()
{
  if (g_internalBuf != nullptr)
  {
    heap_caps_free(g_internalBuf);
    g_internalBuf = nullptr;
  }

  if (g_dmaBuf != nullptr)
  {
    heap_caps_free(g_dmaBuf);
    g_dmaBuf = nullptr;
  }

  if (g_psramBuf != nullptr)
  {
    heap_caps_free(g_psramBuf);
    g_psramBuf = nullptr;
  }
}

void runMemoryExercise()
{
  releaseMemoryBuffers();
  printMemoryStatus("Before Allocation");

  Serial.println(">>> Performing heap_caps_malloc...");
  g_internalBuf = static_cast<uint8_t *>(heap_caps_malloc(16 * 1024, MALLOC_CAP_INTERNAL));
  g_dmaBuf = static_cast<uint8_t *>(heap_caps_malloc(8 * 1024, MALLOC_CAP_DMA));
  g_psramBuf = static_cast<uint8_t *>(heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM));

  Serial.printf("Internal SRAM allocation (16 KB): %s\n", g_internalBuf ? "success" : "failed");
  Serial.printf("DMA memory allocation (8 KB): %s\n", g_dmaBuf ? "success" : "failed");
  Serial.printf("PSRAM allocation (64 KB): %s\n", g_psramBuf ? "success" : "failed");
  printMemoryStatus("After Allocation");
}

void setup()
{
  Serial.begin(115200);
  unsigned long serialWaitStart = millis();
  while (!Serial && millis() - serialWaitStart < 5000)
  {
    delay(10);
  }
  delay(1000);
  Serial.println();
  Serial.println("Starting Exercise 2 memory monitor");

#ifdef BOARD_HAS_PSRAM
  // Prefer external RAM for generic malloc to keep internal RAM for TLS handshake.
  heap_caps_malloc_extmem_enable(0);
#endif

  runMemoryExercise();
  g_lastMemoryExerciseMs = millis();
  Serial.println("Exercise mode active: only memory information will be printed.");
}

void loop()
{
  if (millis() - g_lastMemoryExerciseMs >= kMemoryExerciseIntervalMs)
  {
    Serial.println("\nRepeating memory exercise for easier Serial Monitor capture...");
    runMemoryExercise();
    g_lastMemoryExerciseMs = millis();
  }

  vTaskDelay(1000);
}
