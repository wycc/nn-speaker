#ifndef MEMORY_OBSERVATION_EXERCISE_H
#define MEMORY_OBSERVATION_EXERCISE_H

#include <Arduino.h>
#include <stddef.h>
#include <stdint.h>

class MemoryObservationExercise
{
public:
    struct MemorySnapshot
    {
        size_t heap_total = 0;
        size_t heap_free = 0;
        size_t internal_free = 0;
        size_t psram_free = 0;
        size_t cap_internal_free = 0;
        size_t cap_8bit_free = 0;
        size_t cap_dma_free = 0;
        size_t cap_spiram_free = 0;
        size_t largest_internal_block = 0;
        size_t largest_8bit_block = 0;
        size_t largest_dma_block = 0;
        size_t largest_spiram_block = 0;
    };

    struct AllocationSet
    {
        uint8_t *internal_buf = nullptr;
        uint8_t *dma_buf = nullptr;
        uint8_t *psram_buf = nullptr;
        size_t internal_size = 16 * 1024;
        size_t dma_size = 8 * 1024;
        size_t psram_size = 64 * 1024;
    };

    void runOnce();

private:
    MemorySnapshot captureSnapshot() const;
    void printSnapshot(const char *title, const MemorySnapshot &snapshot) const;
    void printDelta(const MemorySnapshot &before, const MemorySnapshot &after) const;
    AllocationSet allocateBuffers() const;
    void printAllocationResult(const AllocationSet &buffers) const;
    void releaseBuffers(AllocationSet &buffers) const;
    bool hasPsram() const;
};

#endif
