#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <cstring>
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

struct MemorySnapshot
{
  uint32_t heapTotal;
  uint32_t heapFree;
  uint32_t internalFree;
  uint32_t psramFree;

  uint32_t capInternalFree;
  uint32_t cap8BitFree;
  uint32_t capDmaFree;
  uint32_t capSpiramFree;

  uint32_t largestInternal;
  uint32_t largest8Bit;
  uint32_t largestDma;
  uint32_t largestSpiram;
};

struct MemoryBuffers
{
  uint8_t *internalBuf;
  uint8_t *dmaBuf;
  uint8_t *psramBuf;
};

static MemoryBuffers g_memBuf{nullptr, nullptr, nullptr};

static MemorySnapshot captureMemorySnapshot()
{
  MemorySnapshot s{};

  s.heapTotal = ESP.getHeapSize();
  s.heapFree = ESP.getFreeHeap();
  s.internalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  s.psramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  s.capInternalFree = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
  s.cap8BitFree = heap_caps_get_free_size(MALLOC_CAP_8BIT);
  s.capDmaFree = heap_caps_get_free_size(MALLOC_CAP_DMA);
  s.capSpiramFree = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);

  s.largestInternal = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
  s.largest8Bit = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
  s.largestDma = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
  s.largestSpiram = heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM);

  return s;
}

static void printMemoryStatus(const char *title, const MemorySnapshot &s)
{
  Serial.printf("=== %s ===\n", title);
  Serial.printf("Heap total size       : %u\n", s.heapTotal);
  Serial.printf("Heap free size        : %u\n", s.heapFree);
  Serial.printf("Internal free (SRAM)  : %u\n", s.internalFree);
  Serial.printf("PSRAM free            : %u\n", s.psramFree);

  Serial.println("-- Free by capability --");
  Serial.printf("MALLOC_CAP_INTERNAL   : %u\n", s.capInternalFree);
  Serial.printf("MALLOC_CAP_8BIT       : %u\n", s.cap8BitFree);
  Serial.printf("MALLOC_CAP_DMA        : %u\n", s.capDmaFree);
  Serial.printf("MALLOC_CAP_SPIRAM     : %u\n", s.capSpiramFree);

  Serial.println("-- Largest free block --");
  Serial.printf("INTERNAL largest      : %u\n", s.largestInternal);
  Serial.printf("8BIT largest          : %u\n", s.largest8Bit);
  Serial.printf("DMA largest           : %u\n", s.largestDma);
  Serial.printf("SPIRAM largest        : %u\n", s.largestSpiram);
  Serial.println();
}

static void printMemoryDelta(const MemorySnapshot &before, const MemorySnapshot &after)
{
  auto d = [](uint32_t b, uint32_t a) -> int32_t {
    return static_cast<int32_t>(a) - static_cast<int32_t>(b);
  };

  Serial.println("=== Delta (After - Before) ===");
  Serial.printf("Heap free delta       : %ld\n", static_cast<long>(d(before.heapFree, after.heapFree)));
  Serial.printf("Internal free delta   : %ld\n", static_cast<long>(d(before.internalFree, after.internalFree)));
  Serial.printf("PSRAM free delta      : %ld\n", static_cast<long>(d(before.psramFree, after.psramFree)));
  Serial.printf("CAP_INTERNAL delta    : %ld\n", static_cast<long>(d(before.capInternalFree, after.capInternalFree)));
  Serial.printf("CAP_8BIT delta        : %ld\n", static_cast<long>(d(before.cap8BitFree, after.cap8BitFree)));
  Serial.printf("CAP_DMA delta         : %ld\n", static_cast<long>(d(before.capDmaFree, after.capDmaFree)));
  Serial.printf("CAP_SPIRAM delta      : %ld\n", static_cast<long>(d(before.capSpiramFree, after.capSpiramFree)));
  Serial.println();
}

static void allocateExerciseBuffers()
{
  g_memBuf.internalBuf = static_cast<uint8_t *>(heap_caps_malloc(16 * 1024, MALLOC_CAP_INTERNAL | MALLOC_CAP_8BIT));
  g_memBuf.dmaBuf = static_cast<uint8_t *>(heap_caps_malloc(8 * 1024, MALLOC_CAP_DMA | MALLOC_CAP_8BIT));

  if (ESP.getPsramSize() > 0)
  {
    g_memBuf.psramBuf = static_cast<uint8_t *>(heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  }

  Serial.println("Allocation result:");
  Serial.printf("internal_buf: %s\n", g_memBuf.internalBuf ? "OK" : "FAIL");
  Serial.printf("dma_buf     : %s\n", g_memBuf.dmaBuf ? "OK" : "FAIL");
  Serial.printf("psram_buf   : %s\n", g_memBuf.psramBuf ? "OK" : "FAIL");

  if (g_memBuf.internalBuf)
  {
    memset(g_memBuf.internalBuf, 0x11, 16 * 1024);
  }
  if (g_memBuf.dmaBuf)
  {
    memset(g_memBuf.dmaBuf, 0x22, 8 * 1024);
  }
  if (g_memBuf.psramBuf)
  {
    memset(g_memBuf.psramBuf, 0x33, 64 * 1024);
  }
}

static void freeExerciseBuffers()
{
  if (g_memBuf.internalBuf)
  {
    heap_caps_free(g_memBuf.internalBuf);
    g_memBuf.internalBuf = nullptr;
  }
  if (g_memBuf.dmaBuf)
  {
    heap_caps_free(g_memBuf.dmaBuf);
    g_memBuf.dmaBuf = nullptr;
  }
  if (g_memBuf.psramBuf)
  {
    heap_caps_free(g_memBuf.psramBuf);
    g_memBuf.psramBuf = nullptr;
  }
}

static void runExercise2MemoryDemo()
{
  MemorySnapshot before = captureMemorySnapshot();
  printMemoryStatus("Before Allocation", before);

  allocateExerciseBuffers();

  MemorySnapshot after = captureMemorySnapshot();
  printMemoryStatus("After Allocation", after);
  printMemoryDelta(before, after);

  freeExerciseBuffers();
}

static void printMemoryInfo()
{
  MemorySnapshot current = captureMemorySnapshot();
  printMemoryStatus("Current Snapshot", current);
}

static void handleSerialCommand()
{
  static String commandBuffer;

  while (Serial.available() > 0)
  {
    char c = static_cast<char>(Serial.read());
    if (c == '\r')
    {
      continue;
    }

    if (c == '\n')
    {
      commandBuffer.trim();
      commandBuffer.toLowerCase();

      if (commandBuffer == "mem" || commandBuffer == "memory")
      {
        printMemoryInfo();
      }

      commandBuffer = "";
      continue;
    }

    commandBuffer += c;
  }
}

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

enum class LabQueueEventType : uint8_t
{
  PlayOK = 1,
};

struct LabQueueEvent
{
  LabQueueEventType type;
};

static constexpr uint32_t LAB_QUEUE_LENGTH = 8;
static constexpr uint32_t LAB_PRODUCER_PERIOD_MS = 2000;
static QueueHandle_t s_labQueue = nullptr;

void labProducerTask(void *param)
{
  (void)param;
  const TickType_t producerPeriodTicks = pdMS_TO_TICKS(LAB_PRODUCER_PERIOD_MS);

  while (true)
  {
    LabQueueEvent event = {LabQueueEventType::PlayOK};
    BaseType_t status = xQueueSend(s_labQueue, &event, pdMS_TO_TICKS(100));
    if (status == pdPASS)
    {
      Serial.println("[LAB][Producer] event sent: PlayOK");
    }
    else
    {
      Serial.println("[LAB][Producer] failed to send event");
    }

    vTaskDelay(producerPeriodTicks);
  }
}

void labConsumerTask(void *param)
{
  Speaker *speaker = static_cast<Speaker *>(param);
  LabQueueEvent event;

  while (true)
  {
    if (xQueueReceive(s_labQueue, &event, portMAX_DELAY) == pdPASS)
    {
      Serial.println("[LAB][Consumer] event received");
      if (event.type == LabQueueEventType::PlayOK)
      {
        Serial.println("[LAB][Consumer] start playing ok.wav");
        speaker->playOK();
      }
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

  // Exercise 2: capture before/after/delta snapshots during startup.
  runExercise2MemoryDemo();

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
  Serial.printf("Total heap: %d\n", ESP.getHeapSize());
  Serial.printf("Free heap: %d\n", ESP.getFreeHeap());
  Serial.printf("Total PSRAM: %d\n", ESP.getPsramSize());
  Serial.printf("Free PSRAM: %d\n", ESP.getFreePsram());
  Serial.printf("Internal heap free: %u\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("Internal heap largest block: %u\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  Serial.printf("Chip model: %s\n", ESP.getChipModel());

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

  // Lab queue + tasks (producer/consumer)
  s_labQueue = xQueueCreate(LAB_QUEUE_LENGTH, sizeof(LabQueueEvent));
  if (s_labQueue == nullptr)
  {
    Serial.println("[LAB] Failed to create queue");
  }
  else
  {
    xTaskCreate(labProducerTask, "Lab Producer Task", 2048, nullptr, 1, nullptr);
    xTaskCreate(labConsumerTask, "Lab Consumer Task", 2048, speaker, 1, nullptr);
    Serial.println("[LAB] Producer/Consumer tasks started");
  }

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
  handleSerialCommand();
  vTaskDelay(pdMS_TO_TICKS(50));
}
