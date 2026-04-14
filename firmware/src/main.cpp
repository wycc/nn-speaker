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

// ── Part 1: Memory Status Function ───────────────────────────────────────────
// Prints current memory usage for all major capability types.
// Called before and after allocation to observe changes (Parts 3).
void printMemoryStatus(const char* label)
{
    Serial.printf("\n=== %s ===\n", label);

    // ── Default heap (Internal SRAM + PSRAM combined) ─────────────────────────
    // MALLOC_CAP_DEFAULT covers all memory the allocator can hand out.
    Serial.printf("Heap total        : %6u bytes\n", heap_caps_get_total_size(MALLOC_CAP_DEFAULT));
    Serial.printf("Heap free         : %6u bytes\n", heap_caps_get_free_size(MALLOC_CAP_DEFAULT));

    // ── Internal SRAM ─────────────────────────────────────────────────────────
    // MALLOC_CAP_INTERNAL = on-chip SRAM only; never includes PSRAM.
    // "largest block" reveals fragmentation: free may be large but usable
    // contiguous space can be much smaller.
    Serial.printf("Internal total    : %6u bytes\n", heap_caps_get_total_size(MALLOC_CAP_INTERNAL));
    Serial.printf("Internal free     : %6u bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
    Serial.printf("Internal largest  : %6u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));

    // ── 8-bit capable memory ──────────────────────────────────────────────────
    // MALLOC_CAP_8BIT = memory that supports byte-level access.
    // On ESP32: internal SRAM + PSRAM both qualify; IRAM does NOT.
    Serial.printf("8-bit total       : %6u bytes\n", heap_caps_get_total_size(MALLOC_CAP_8BIT));
    Serial.printf("8-bit free        : %6u bytes\n", heap_caps_get_free_size(MALLOC_CAP_8BIT));

    // ── PSRAM / SPIRAM ────────────────────────────────────────────────────────
    // MALLOC_CAP_SPIRAM = external SPI-connected RAM (PSRAM).
    // Same physical chip, two names: SPIRAM (capability flag) vs PSRAM (hardware term).
    Serial.printf("SPIRAM total      : %6u bytes\n", heap_caps_get_total_size(MALLOC_CAP_SPIRAM));
    Serial.printf("SPIRAM free       : %6u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
    Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
    Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
}
// ─────────────────────────────────────────────────────────────────────────────

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

void setup()
{
  Serial.begin(115200);
  delay(1000);
  Serial.println("Starting up");

#ifdef BOARD_HAS_PSRAM
  // Prefer external RAM for generic malloc to keep internal RAM for TLS handshake.
  heap_caps_malloc_extmem_enable(0);
#endif

  // start up wifi
  // launch WiFi
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PSWD);
  if (WiFi.waitForConnectResult() != WL_CONNECTED)
  {
    Serial.println("Connection Failed! Rebooting...");
    delay(5000);
    //ESP.restart();
  }
  // Original individual memory prints (kept for reference):
  // Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  // Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  // Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  // Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
  // Serial.printf("Internal heap free: %u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  // Serial.printf("Internal heap largest block: %u\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  // Serial.printf("Chip model: %s\n", ESP.getChipModel());

  // Part 1 + Part 3: print memory status BEFORE allocation
  printMemoryStatus("=== Before Allocation ===");

#ifdef HW2
  // ── Part 2: Allocate different memory types ───────────────────────────────
  // Each call uses heap_caps_malloc(size, capability) to request memory
  // from a specific region. NULL is returned if the region has no space.

  // 16 KB from internal SRAM (on-chip, fastest, limited ~200 KB free)
  uint8_t* internal_buf = (uint8_t*) heap_caps_malloc(16 * 1024, MALLOC_CAP_INTERNAL);

  // 8 KB DMA-capable memory (subset of internal SRAM; required by peripherals
  // like I2S, SPI that do direct memory access without CPU involvement)
  uint8_t* dma_buf = (uint8_t*) heap_caps_malloc(8 * 1024, MALLOC_CAP_DMA);

  // 64 KB from external PSRAM (slow SPI bus, but ~4 MB available)
  uint8_t* psram_buf = (uint8_t*) heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);

  // 100 KB from Internal SRAM — intentionally large to force the allocator
  // to cut into the largest contiguous block (110,580 B).
  // After this, Internal largest should drop from ~110 KB to ~10 KB,
  // demonstrating heap fragmentation.
  uint8_t* frag_buf = (uint8_t*) heap_caps_malloc(100 * 1024, MALLOC_CAP_INTERNAL);

  // Report whether each allocation succeeded
  Serial.println("\n--- Allocation Results ---");
  Serial.printf("internal_buf  ( 16 KB, MALLOC_CAP_INTERNAL) : %s\n", internal_buf ? "OK" : "FAILED");
  Serial.printf("dma_buf       (  8 KB, MALLOC_CAP_DMA)      : %s\n", dma_buf      ? "OK" : "FAILED");
  Serial.printf("psram_buf     ( 64 KB, MALLOC_CAP_SPIRAM)   : %s\n", psram_buf    ? "OK" : "FAILED");
  Serial.printf("frag_buf      (100 KB, MALLOC_CAP_INTERNAL) : %s\n", frag_buf     ? "OK" : "FAILED");

  // Part 3: print memory status AFTER allocation to observe the change
  // Key observation: Internal largest should drop significantly (fragmentation)
  printMemoryStatus("=== After Allocation ===");
#endif // HW2

  // startup SPIFFS for the wav files
  SPIFFS.begin();
  // make sure we don't get killed for our long running tasks
  esp_task_wdt_init(10, false);

  es8388_init();
  // start up the I2S input (from either an I2S microphone or Analogue microphone via the ADC)
#ifdef USE_I2S_MIC_INPUT
  // Direct i2s input from INMP441 or the SPH0645
  I2SSampler *i2s_sampler = new I2SMicSampler(i2s_codec_pins, false);
#else
  // Use the internal ADC
  I2SSampler *i2s_sampler = new ADCSampler(ADC_UNIT_1, ADC_MIC_CHANNEL);
#endif

  // start the i2s speaker output
  I2SOutput *i2s_output = new I2SOutput();
  i2s_output->start(I2S_NUM_0, i2s_codec_pins, i2sCodecConfig);
  Speaker *speaker = new Speaker(i2s_output);

  // indicator light to show when we are listening
  IndicatorLight *indicator_light = new IndicatorLight();

  // and the intent processor
  IntentProcessor *intent_processor = new IntentProcessor(speaker);
  /*
  intent_processor->addDevice("kitchen", GPIO_NUM_5);
  intent_processor->addDevice("bedroom", GPIO_NUM_21);
  intent_processor->addDevice("table", GPIO_NUM_23);
  */

  // create our application
  Application *application = new Application(i2s_sampler, intent_processor, speaker, indicator_light);

  // set up the i2s sample writer task
  TaskHandle_t applicationTaskHandle;
  xTaskCreate(applicationTask, "Application Task", 4096, application, 1, &applicationTaskHandle);

  // start sampling from i2s device - use I2S_NUM_0 as that's the one that supports the internal ADC
#ifdef USE_I2S_MIC_INPUT
  i2s_sampler->start(I2S_NUM_0, i2sCodecConfig, applicationTaskHandle);
#else
  i2s_sampler->start(I2S_NUM_0, adcI2SConfig, applicationTaskHandle);
#endif
}

void loop()
{
  vTaskDelay(1000);
}
