#include <Arduino.h>
#include "esp_heap_caps.h"

// 指標先宣告成全域，方便配置後還能存取
uint8_t* internal_buf = nullptr;
uint8_t* dma_buf = nullptr;
uint8_t* psram_buf = nullptr;

// 印出目前記憶體狀態
void printMemoryStatus(const char* title) {
  Serial.println();
  Serial.println("========================================");
  Serial.println(title);
  Serial.println("========================================");

  // 一般 Heap 資訊
  Serial.printf("Heap total size            : %u bytes\n", ESP.getHeapSize());
  Serial.printf("Heap free size             : %u bytes\n", ESP.getFreeHeap());
  Serial.printf("Heap min free size         : %u bytes\n", ESP.getMinFreeHeap());
  Serial.printf("Heap max alloc size        : %u bytes\n", ESP.getMaxAllocHeap());

  Serial.println();

  // 各種 capability 的剩餘空間
  Serial.printf("Free MALLOC_CAP_INTERNAL   : %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
  Serial.printf("Free MALLOC_CAP_8BIT       : %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_8BIT));
  Serial.printf("Free MALLOC_CAP_DMA        : %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_DMA));
  Serial.printf("Free MALLOC_CAP_SPIRAM     : %u bytes\n",
                heap_caps_get_free_size(MALLOC_CAP_SPIRAM));

  Serial.println();

  // 最大可分配連續區塊，拿來觀察 fragmentation
  Serial.printf("Largest block INTERNAL     : %u bytes\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL));
  Serial.printf("Largest block 8BIT         : %u bytes\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_8BIT));
  Serial.printf("Largest block DMA          : %u bytes\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_DMA));
  Serial.printf("Largest block SPIRAM       : %u bytes\n",
                heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM));

  Serial.println("========================================");
}

// 配置不同類型記憶體
void allocateMemory() {
  Serial.println();
  Serial.println("Allocating memory...");

  // Internal SRAM
  internal_buf = (uint8_t*)heap_caps_malloc(16 * 1024, MALLOC_CAP_INTERNAL);

  // DMA memory
  dma_buf = (uint8_t*)heap_caps_malloc(8 * 1024, MALLOC_CAP_DMA);

  // PSRAM
  psram_buf = (uint8_t*)heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);

  if (internal_buf != nullptr) {
    Serial.println("[OK] Internal SRAM allocated: 16 KB");
    memset(internal_buf, 0x11, 16 * 1024);
  } else {
    Serial.println("[FAIL] Internal SRAM allocation failed");
  }

  if (dma_buf != nullptr) {
    Serial.println("[OK] DMA memory allocated: 8 KB");
    memset(dma_buf, 0x22, 8 * 1024);
  } else {
    Serial.println("[FAIL] DMA memory allocation failed");
  }

  if (psram_buf != nullptr) {
    Serial.println("[OK] PSRAM allocated: 64 KB");
    memset(psram_buf, 0x33, 64 * 1024);
  } else {
    Serial.println("[FAIL] PSRAM allocation failed");
    Serial.println("Note: Your board may not support PSRAM, or PSRAM is not enabled.");
  }
}

// 可選：釋放記憶體，再觀察一次
void freeMemory() {
  Serial.println();
  Serial.println("Freeing memory...");

  if (internal_buf != nullptr) {
    free(internal_buf);
    internal_buf = nullptr;
    Serial.println("[OK] Freed internal SRAM");
  }

  if (dma_buf != nullptr) {
    free(dma_buf);
    dma_buf = nullptr;
    Serial.println("[OK] Freed DMA memory");
  }

  if (psram_buf != nullptr) {
    free(psram_buf);
    psram_buf = nullptr;
    Serial.println("[OK] Freed PSRAM");
  }
}

void setup() {
  Serial.begin(115200);
  delay(2000);

  Serial.println("ESP32 Exercise 2 - Memory Observation and Allocation");

  // Part 1：配置前
  printMemoryStatus("=== Before Allocation ===");

  // Part 2：配置不同記憶體
  allocateMemory();

  // Part 3：配置後再觀察
  printMemoryStatus("=== After Allocation ===");

  // 這段不是作業硬性要求，但可以加分
  freeMemory();
  printMemoryStatus("=== After Free ===");
}

void loop() {
  // 不需要重複執行
}