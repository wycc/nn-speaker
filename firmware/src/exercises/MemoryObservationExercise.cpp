#include "exercises/MemoryObservationExercise.h"

#include <esp_heap_caps.h>
#include <inttypes.h>
#include <string.h>

namespace
{
void printField(const char *label, size_t value)
{
    Serial.printf("%-28s %u bytes\r\n", label, static_cast<unsigned>(value));
}

void printDeltaField(const char *label, size_t before, size_t after)
{
    const long delta = static_cast<long>(after) - static_cast<long>(before);
    Serial.printf("%-28s before=%u after=%u delta=%ld\r\n",
                  label,
                  static_cast<unsigned>(before),
                  static_cast<unsigned>(after),
                  delta);
}
} // namespace

void MemoryObservationExercise::runOnce()
{
    Serial.println();
    Serial.println(F("=== Exercise 2: ESP32 Memory Observation ==="));

    const MemorySnapshot before = captureSnapshot();
    printSnapshot("Before Allocation", before);

    AllocationSet buffers = allocateBuffers();
    printAllocationResult(buffers);

    const MemorySnapshot after = captureSnapshot();
    printSnapshot("After Allocation", after);
    printDelta(before, after);

    releaseBuffers(buffers);
    Serial.println(F("=== Release Result ==="));
    Serial.println(F("All allocated buffers have been released."));
    Serial.println();

    Serial.println(F("=== Exercise 2 Complete ==="));
    Serial.println();
}

MemoryObservationExercise::MemorySnapshot MemoryObservationExercise::captureSnapshot() const
{
    MemorySnapshot snapshot;

    snapshot.heap_total = ESP.getHeapSize();
    snapshot.heap_free = ESP.getFreeHeap();
    snapshot.internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot.psram_free = hasPsram() ? ESP.getFreePsram() : 0;

    snapshot.cap_internal_free = heap_caps_get_free_size(MALLOC_CAP_INTERNAL);
    snapshot.cap_8bit_free = heap_caps_get_free_size(MALLOC_CAP_8BIT);
    snapshot.cap_dma_free = heap_caps_get_free_size(MALLOC_CAP_DMA);
    snapshot.cap_spiram_free = hasPsram() ? heap_caps_get_free_size(MALLOC_CAP_SPIRAM) : 0;

    snapshot.largest_internal_block = heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL);
    snapshot.largest_8bit_block = heap_caps_get_largest_free_block(MALLOC_CAP_8BIT);
    snapshot.largest_dma_block = heap_caps_get_largest_free_block(MALLOC_CAP_DMA);
    snapshot.largest_spiram_block = hasPsram() ? heap_caps_get_largest_free_block(MALLOC_CAP_SPIRAM) : 0;

    return snapshot;
}

void MemoryObservationExercise::printSnapshot(const char *title, const MemorySnapshot &snapshot) const
{
    Serial.printf("=== %s ===\r\n", title);
    printField("Heap total:", snapshot.heap_total);
    printField("Heap free:", snapshot.heap_free);
    printField("Internal free:", snapshot.internal_free);
    printField("PSRAM free:", snapshot.psram_free);
    printField("MALLOC_CAP_INTERNAL:", snapshot.cap_internal_free);
    printField("MALLOC_CAP_8BIT:", snapshot.cap_8bit_free);
    printField("MALLOC_CAP_DMA:", snapshot.cap_dma_free);
    printField("MALLOC_CAP_SPIRAM:", snapshot.cap_spiram_free);
    printField("Largest internal block:", snapshot.largest_internal_block);
    printField("Largest 8-bit block:", snapshot.largest_8bit_block);
    printField("Largest DMA block:", snapshot.largest_dma_block);
    printField("Largest SPIRAM block:", snapshot.largest_spiram_block);
    Serial.println();
}

void MemoryObservationExercise::printDelta(const MemorySnapshot &before, const MemorySnapshot &after) const
{
    Serial.println(F("=== Allocation Delta ==="));
    printDeltaField("Heap total:", before.heap_total, after.heap_total);
    printDeltaField("Heap free:", before.heap_free, after.heap_free);
    printDeltaField("Internal free:", before.internal_free, after.internal_free);
    printDeltaField("PSRAM free:", before.psram_free, after.psram_free);
    printDeltaField("MALLOC_CAP_INTERNAL:", before.cap_internal_free, after.cap_internal_free);
    printDeltaField("MALLOC_CAP_8BIT:", before.cap_8bit_free, after.cap_8bit_free);
    printDeltaField("MALLOC_CAP_DMA:", before.cap_dma_free, after.cap_dma_free);
    printDeltaField("MALLOC_CAP_SPIRAM:", before.cap_spiram_free, after.cap_spiram_free);
    printDeltaField("Largest internal block:", before.largest_internal_block, after.largest_internal_block);
    printDeltaField("Largest 8-bit block:", before.largest_8bit_block, after.largest_8bit_block);
    printDeltaField("Largest DMA block:", before.largest_dma_block, after.largest_dma_block);
    printDeltaField("Largest SPIRAM block:", before.largest_spiram_block, after.largest_spiram_block);
    Serial.println();
}

MemoryObservationExercise::AllocationSet MemoryObservationExercise::allocateBuffers() const
{
    AllocationSet buffers;

    buffers.internal_buf = static_cast<uint8_t *>(heap_caps_malloc(buffers.internal_size, MALLOC_CAP_INTERNAL));
    buffers.dma_buf = static_cast<uint8_t *>(heap_caps_malloc(buffers.dma_size, MALLOC_CAP_DMA));

    if (hasPsram())
    {
        buffers.psram_buf = static_cast<uint8_t *>(heap_caps_malloc(buffers.psram_size, MALLOC_CAP_SPIRAM));
    }

    if (buffers.internal_buf != nullptr)
    {
        memset(buffers.internal_buf, 0x11, buffers.internal_size);
    }

    if (buffers.dma_buf != nullptr)
    {
        memset(buffers.dma_buf, 0x22, buffers.dma_size);
    }

    if (buffers.psram_buf != nullptr)
    {
        memset(buffers.psram_buf, 0x33, buffers.psram_size);
    }

    return buffers;
}

void MemoryObservationExercise::printAllocationResult(const AllocationSet &buffers) const
{
    Serial.println(F("=== Allocation Result ==="));

    if (buffers.internal_buf != nullptr)
    {
        Serial.printf("Internal SRAM allocation success: %u bytes at 0x%08" PRIXPTR "\r\n",
                      static_cast<unsigned>(buffers.internal_size),
                      reinterpret_cast<uintptr_t>(buffers.internal_buf));
    }
    else
    {
        Serial.printf("Internal SRAM allocation failed: %u bytes\r\n",
                      static_cast<unsigned>(buffers.internal_size));
    }

    if (buffers.dma_buf != nullptr)
    {
        Serial.printf("DMA allocation success: %u bytes at 0x%08" PRIXPTR "\r\n",
                      static_cast<unsigned>(buffers.dma_size),
                      reinterpret_cast<uintptr_t>(buffers.dma_buf));
    }
    else
    {
        Serial.printf("DMA allocation failed: %u bytes\r\n",
                      static_cast<unsigned>(buffers.dma_size));
    }

    if (!hasPsram())
    {
        Serial.println(F("PSRAM allocation skipped: PSRAM not available on this target"));
    }
    else if (buffers.psram_buf != nullptr)
    {
        Serial.printf("PSRAM allocation success: %u bytes at 0x%08" PRIXPTR "\r\n",
                      static_cast<unsigned>(buffers.psram_size),
                      reinterpret_cast<uintptr_t>(buffers.psram_buf));
    }
    else
    {
        Serial.printf("PSRAM allocation failed: %u bytes\r\n",
                      static_cast<unsigned>(buffers.psram_size));
    }

    Serial.println();
}

void MemoryObservationExercise::releaseBuffers(AllocationSet &buffers) const
{
    if (buffers.internal_buf != nullptr)
    {
        heap_caps_free(buffers.internal_buf);
        buffers.internal_buf = nullptr;
    }

    if (buffers.dma_buf != nullptr)
    {
        heap_caps_free(buffers.dma_buf);
        buffers.dma_buf = nullptr;
    }

    if (buffers.psram_buf != nullptr)
    {
        heap_caps_free(buffers.psram_buf);
        buffers.psram_buf = nullptr;
    }
}

bool MemoryObservationExercise::hasPsram() const
{
    return psramFound() && ESP.getPsramSize() > 0;
}
