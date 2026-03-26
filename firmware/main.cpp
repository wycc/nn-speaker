#include <stdio.h>
#include <stdint.h>
#include "esp_heap_caps.h"

typedef struct {
    size_t heap_total_8bit;
    size_t heap_free_8bit;
    size_t internal_free;
    size_t dma_free;
    size_t psram_free;
    size_t largest_8bit_block;
} MemorySnapshot;

MemorySnapshot getMemorySnapshot() {
    MemorySnapshot snap;
    snap.heap_total_8bit    = heap_caps_get_total_size(MALLOC_CAP_8BIT);
    snap.heap_free_8bit     = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    snap.internal_free      = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snap.dma_free           = heap_caps_get_free_size(MALLOC_CAP_DMA);
    snap.psram_free         = heap_caps_get_free_size(MALLOC_CAP_SPIRAM);
    snap.largest_8bit_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    return snap;
}

void printMemoryStatus(const char* stage, const MemorySnapshot& snap) {
    printf("\n=== %s ===\n", stage);
    printf("Heap total (8-bit):     %u bytes\n", (unsigned int)snap.heap_total_8bit);
    printf("Heap free  (8-bit):     %u bytes\n", (unsigned int)snap.heap_free_8bit);
    printf("Largest 8-bit block:    %u bytes\n", (unsigned int)snap.largest_8bit_block);

    printf("Internal free:          %u bytes\n", (unsigned int)snap.internal_free);
    printf("DMA free:               %u bytes\n", (unsigned int)snap.dma_free);
    printf("PSRAM free:             %u bytes\n", (unsigned int)snap.psram_free);
}

void printMemoryDifference(const MemorySnapshot& before, const MemorySnapshot& after) {
    printf("\n=== Difference (After - Before) ===\n");
    printf("Heap free  (8-bit):     %d bytes\n", (int)(after.heap_free_8bit - before.heap_free_8bit));
    printf("Largest 8-bit block:    %d bytes\n", (int)(after.largest_8bit_block - before.largest_8bit_block));

    printf("Internal free:          %d bytes\n", (int)(after.internal_free - before.internal_free));
    printf("DMA free:               %d bytes\n", (int)(after.dma_free - before.dma_free));
    printf("PSRAM free:             %d bytes\n", (int)(after.psram_free - before.psram_free));
}

extern "C" void app_main() {
    // Step 1: Observe memory before allocation
    MemorySnapshot before = getMemorySnapshot();
    printMemoryStatus("Before Allocation", before);

    // Step 2: Allocate different types of memory
    uint8_t* internal_buf = (uint8_t*)heap_caps_malloc(16 * 1024, MALLOC_CAP_INTERNAL);
    uint8_t* dma_buf      = (uint8_t*)heap_caps_malloc(8 * 1024, MALLOC_CAP_DMA);
    uint8_t* psram_buf    = (uint8_t*)heap_caps_malloc(64 * 1024, MALLOC_CAP_SPIRAM);

    printf("\n=== Allocation Result ===\n");
    printf("Internal SRAM allocation: %s\n", internal_buf ? "SUCCESS" : "FAILED");
    printf("DMA memory allocation:    %s\n", dma_buf ? "SUCCESS" : "FAILED");
    printf("PSRAM allocation:         %s\n", psram_buf ? "SUCCESS" : "FAILED");

    // Step 3: Observe memory after allocation
    MemorySnapshot after = getMemorySnapshot();
    printMemoryStatus("After Allocation", after);

    // Step 4: Compare difference
    printMemoryDifference(before, after);

    // Optional: free memory after observation
    if (internal_buf) {
        heap_caps_free(internal_buf);
        internal_buf = NULL;
    }
    if (dma_buf) {
        heap_caps_free(dma_buf);
        dma_buf = NULL;
    }
    if (psram_buf) {
        heap_caps_free(psram_buf);
        psram_buf = NULL;
    }

    printf("\n=== Program Finished ===\n");
}