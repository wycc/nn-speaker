#include <Arduino.h>
#include <WiFi.h>
#include <driver/i2s.h>
#include <esp_task_wdt.h>
#include <esp_heap_caps.h>
#include <cstdlib>
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
struct MemoryBlock
{
  const char *name;
  uint32_t capability;
  void *ptr;
  size_t size;
};

MemoryBlock g_blocks[] = {
    {"INTERNAL", MALLOC_CAP_INTERNAL, nullptr, 0},
    {"DMA", MALLOC_CAP_DMA, nullptr, 0},
    {"SPIRAM", MALLOC_CAP_SPIRAM, nullptr, 0},
};

constexpr size_t kCommandBufferSize = 96;
char g_commandBuffer[kCommandBufferSize];
size_t g_commandLength = 0;
}

MemoryBlock *findBlock(const char *name)
{
  for (MemoryBlock &block : g_blocks)
  {
    if (strcasecmp(block.name, name) == 0)
    {
      return &block;
    }
  }
  return nullptr;
}

bool getCapabilityInfo(const char *name, uint32_t &capability)
{
  if (strcasecmp(name, "DEFAULT") == 0)
  {
    capability = MALLOC_CAP_DEFAULT;
    return true;
  }
  if (strcasecmp(name, "INTERNAL") == 0)
  {
    capability = MALLOC_CAP_INTERNAL;
    return true;
  }
  if (strcasecmp(name, "8BIT") == 0)
  {
    capability = MALLOC_CAP_8BIT;
    return true;
  }
  if (strcasecmp(name, "DMA") == 0)
  {
    capability = MALLOC_CAP_DMA;
    return true;
  }
  if (strcasecmp(name, "SPIRAM") == 0 || strcasecmp(name, "PSRAM") == 0)
  {
    capability = MALLOC_CAP_SPIRAM;
    return true;
  }
  return false;
}

void printRegionSummary(const char *label, uint32_t capability)
{
  Serial.printf("MEM %s => total=%u, free=%u, largest=%u\n",
                label,
                heap_caps_get_total_size(capability),
                heap_caps_get_free_size(capability),
                heap_caps_get_largest_free_block(capability));
}

void printAllMemoryStatus()
{
  Serial.println("MEM SHOW ALL");
  Serial.printf("Heap total size: %u bytes\n", ESP.getHeapSize());
  Serial.printf("Heap free size: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Internal memory free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("PSRAM free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  printRegionSummary("DEFAULT", MALLOC_CAP_DEFAULT);
  printRegionSummary("INTERNAL", MALLOC_CAP_INTERNAL);
  printRegionSummary("8BIT", MALLOC_CAP_8BIT);
  printRegionSummary("DMA", MALLOC_CAP_DMA);
  printRegionSummary("SPIRAM", MALLOC_CAP_SPIRAM);
}

void printAssignmentMemoryStatus(const char *title)
{
  Serial.printf("=== %s ===\n", title);
  Serial.printf("Heap total size: %u bytes\n", ESP.getHeapSize());
  Serial.printf("Heap free size: %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Internal free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("PSRAM free: %u bytes\n", heap_caps_get_free_size(MALLOC_CAP_SPIRAM));
  Serial.printf("Largest INTERNAL block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  Serial.printf("Largest 8BIT block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.printf("Largest DMA block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  Serial.printf("Largest SPIRAM block: %u bytes\n", heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));
  printRegionSummary("INTERNAL", MALLOC_CAP_INTERNAL);
  printRegionSummary("8BIT", MALLOC_CAP_8BIT);
  printRegionSummary("DMA", MALLOC_CAP_DMA);
  printRegionSummary("SPIRAM", MALLOC_CAP_SPIRAM);
  Serial.println();
}

void releaseBlock(MemoryBlock &block)
{
  if (block.ptr != nullptr)
  {
    heap_caps_free(block.ptr);
    block.ptr = nullptr;
    block.size = 0;
  }
}

void printHelp()
{
  Serial.println("Memory command help:");
  Serial.println("  MEM SHOW");
  Serial.println("  MEM SHOW <DEFAULT|INTERNAL|8BIT|DMA|SPIRAM>");
  Serial.println("  MEM FREE");
  Serial.println("  MEM ALLOC <INTERNAL|DMA|SPIRAM> <bytes>");
  Serial.println("  MEM RELEASE <INTERNAL|DMA|SPIRAM|ALL>");
  Serial.println("  MEM BLOCKS");
  Serial.println("  MEM DEMO");
  Serial.println("  HELP");
}

void printAllocatedBlocks()
{
  Serial.println("Allocated blocks:");
  for (const MemoryBlock &block : g_blocks)
  {
    Serial.printf("  %s => ptr=%p, bytes=%u\n", block.name, block.ptr, static_cast<unsigned>(block.size));
  }
}

void handleMemShow(const char *region)
{
  if (region == nullptr)
  {
    printAllMemoryStatus();
    return;
  }

  uint32_t capability = 0;
  if (!getCapabilityInfo(region, capability))
  {
    Serial.println("Unknown region. Use DEFAULT, INTERNAL, 8BIT, DMA, or SPIRAM.");
    return;
  }

  printRegionSummary(region, capability);
}

void handleMemAlloc(const char *region, const char *bytesText)
{
  if (region == nullptr || bytesText == nullptr)
  {
    Serial.println("Invalid format. Use: MEM ALLOC <REGION> <bytes>");
    return;
  }

  MemoryBlock *block = findBlock(region);
  if (block == nullptr)
  {
    Serial.println("Alloc region must be INTERNAL, DMA, or SPIRAM.");
    return;
  }

  char *endPtr = nullptr;
  unsigned long bytes = strtoul(bytesText, &endPtr, 10);
  if (bytes == 0 || (endPtr != nullptr && *endPtr != '\0'))
  {
    Serial.println("Allocation size must be a positive integer.");
    return;
  }

  printRegionSummary(block->name, block->capability);
  releaseBlock(*block);
  block->ptr = heap_caps_malloc(bytes, block->capability);
  block->size = block->ptr != nullptr ? bytes : 0;

  Serial.printf("Configured memory block: region=%s, bytes=%lu, ptr=%p\n",
                block->name,
                bytes,
                block->ptr);
  printRegionSummary(block->name, block->capability);
}

void handleMemRelease(const char *region)
{
  if (region == nullptr || strcasecmp(region, "ALL") == 0)
  {
    for (MemoryBlock &block : g_blocks)
    {
      releaseBlock(block);
    }
    Serial.println("Released all configured memory blocks.");
    printAllocatedBlocks();
    return;
  }

  MemoryBlock *block = findBlock(region);
  if (block == nullptr)
  {
    Serial.println("Release region must be INTERNAL, DMA, SPIRAM, or ALL.");
    return;
  }

  releaseBlock(*block);
  Serial.printf("Released region %s.\n", block->name);
  printAllocatedBlocks();
}

void runAssignmentAllocationDemo()
{
  constexpr size_t kInternalDemoBytes = 16 * 1024;
  constexpr size_t kDmaDemoBytes = 8 * 1024;
  constexpr size_t kSpiramDemoBytes = 64 * 1024;

  Serial.println("Running assignment memory allocation demo...");
  printAssignmentMemoryStatus("Before Allocation");

  releaseBlock(g_blocks[0]);
  g_blocks[0].ptr = heap_caps_malloc(kInternalDemoBytes, g_blocks[0].capability);
  g_blocks[0].size = g_blocks[0].ptr != nullptr ? kInternalDemoBytes : 0;
  Serial.printf("Allocated INTERNAL: requested=%u, ptr=%p\n",
                static_cast<unsigned>(kInternalDemoBytes),
                g_blocks[0].ptr);

  releaseBlock(g_blocks[1]);
  g_blocks[1].ptr = heap_caps_malloc(kDmaDemoBytes, g_blocks[1].capability);
  g_blocks[1].size = g_blocks[1].ptr != nullptr ? kDmaDemoBytes : 0;
  Serial.printf("Allocated DMA: requested=%u, ptr=%p\n",
                static_cast<unsigned>(kDmaDemoBytes),
                g_blocks[1].ptr);

  releaseBlock(g_blocks[2]);
  g_blocks[2].ptr = heap_caps_malloc(kSpiramDemoBytes, g_blocks[2].capability);
  g_blocks[2].size = g_blocks[2].ptr != nullptr ? kSpiramDemoBytes : 0;
  Serial.printf("Allocated SPIRAM: requested=%u, ptr=%p\n",
                static_cast<unsigned>(kSpiramDemoBytes),
                g_blocks[2].ptr);

  printAssignmentMemoryStatus("After Allocation");
  printAllocatedBlocks();
}

void handleMemoryCommand(char *line)
{
  char *command = strtok(line, " ");
  if (command == nullptr)
  {
    return;
  }

  if (strcasecmp(command, "HELP") == 0)
  {
    printHelp();
    return;
  }

  if (strcasecmp(command, "MEM") != 0)
  {
    Serial.println("Unknown command. Type HELP.");
    return;
  }

  char *action = strtok(nullptr, " ");
  if (action == nullptr)
  {
    printHelp();
    return;
  }

  if (strcasecmp(action, "SHOW") == 0)
  {
    handleMemShow(strtok(nullptr, " "));
    return;
  }

  if (strcasecmp(action, "FREE") == 0)
  {
    printAllMemoryStatus();
    return;
  }

  if (strcasecmp(action, "ALLOC") == 0)
  {
    handleMemAlloc(strtok(nullptr, " "), strtok(nullptr, " "));
    return;
  }

  if (strcasecmp(action, "RELEASE") == 0)
  {
    handleMemRelease(strtok(nullptr, " "));
    return;
  }

  if (strcasecmp(action, "BLOCKS") == 0)
  {
    printAllocatedBlocks();
    return;
  }

  if (strcasecmp(action, "DEMO") == 0)
  {
    runAssignmentAllocationDemo();
    return;
  }

  Serial.println("Unknown MEM action. Type HELP.");
}

void processSerialCommands()
{
  while (Serial.available() > 0)
  {
    const char ch = static_cast<char>(Serial.read());
    if (ch == '\r')
    {
      continue;
    }

    if (ch == '\n')
    {
      Serial.println();
      g_commandBuffer[g_commandLength] = '\0';
      if (g_commandLength > 0)
      {
        handleMemoryCommand(g_commandBuffer);
      }
      g_commandLength = 0;
      continue;
    }

    if (g_commandLength < kCommandBufferSize - 1)
    {
      g_commandBuffer[g_commandLength++] = ch;
      Serial.print(ch);
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

  Serial.println("Exercise 2 interactive memory monitor ready.");
  printHelp();
  printAllMemoryStatus();

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
  processSerialCommands();
  vTaskDelay(pdMS_TO_TICKS(50));
}
