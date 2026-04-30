/*
 *  SharedMemoryAllocator.cpp - Shared memory allocator.
 *  Author Interested-In-Spresense
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#include <Arduino.h>
#include <MP.h>

#include "SharedMemoryAllocator.h"

#ifndef SUBCORE

/* Debug print control: set to 1 to enable allocator debug output, 0 to disable */
#ifndef DEBUG_ALLOCATOR
#define DEBUG_ALLOCATOR 0
#endif

#if DEBUG_ALLOCATOR
#define DEBUG_PRINT(x)   Serial.print(x)
#define DEBUG_PRINTLN(x) Serial.println(x)
#else
#define DEBUG_PRINT(x)
#define DEBUG_PRINTLN(x)
#endif

const uint32_t SHARED_MEMORY_FENCE_PATTERN = 0xDEADBEEF;
const size_t SHARED_MEMORY_FENCE_SIZE = 2;  /* Head and tail fence words */
const size_t SHARED_MEMORY_ALLOC_GRANULARITY_BYTES = 128 * 1024;

void SharedMemoryAllocator::resetArea(SharedMemoryArea *area)
{
  area->data = NULL;
  area->bytes = 0;
  area->count = 0;
  area->fence = NULL;
  area->total_bytes = 0;
}

void SharedMemoryAllocator::applyLayoutMap(SharedMemoryArea *areas, size_t areaCount, const SharedLayoutArea *map)
{
  for (size_t index = 0; index < areaCount; ++index) {
    areas[index].bytes = map[index].size;
    areas[index].count = map[index].count;
  }
}

void SharedMemoryAllocator::initFenceWords(uint32_t *rawData, size_t totalBytes)
{
  rawData[0] = SHARED_MEMORY_FENCE_PATTERN;
  rawData[totalBytes - 1] = SHARED_MEMORY_FENCE_PATTERN;
}

size_t SharedMemoryAllocator::calcTotalWords(size_t entrySizeBytes,
                                             size_t entryCount)
{
  const size_t dataBytes = entrySizeBytes * entryCount;
  const size_t payloadSize = (dataBytes + sizeof(uint32_t) - 1) / sizeof(uint32_t);
  return payloadSize + SHARED_MEMORY_FENCE_SIZE;
}

size_t SharedMemoryAllocator::roundUpAllocBytes(size_t bytes)
{
  if (bytes == 0) {
    return 0;
  }

  return ((bytes + SHARED_MEMORY_ALLOC_GRANULARITY_BYTES - 1) /
          SHARED_MEMORY_ALLOC_GRANULARITY_BYTES) * SHARED_MEMORY_ALLOC_GRANULARITY_BYTES;
}

void *SharedMemoryAllocator::allocateChunk(size_t requestBytes)
{
  if (requestBytes == 0) {
    return NULL;
  }

  if (freeSizeBytes_ <= requestBytes) {
    const size_t allocBytes = roundUpAllocBytes(requestBytes);

    DEBUG_PRINT("[Alloc] free bytes before refill=");
    DEBUG_PRINTLN((unsigned long)freeSizeBytes_);
    DEBUG_PRINT("[Alloc] requesting new shared block bytes=");
    DEBUG_PRINT((unsigned long)requestBytes);
    DEBUG_PRINT(" rounded=");
    DEBUG_PRINTLN((unsigned long)allocBytes);

    void *sharedMemPtr = MP.AllocSharedMemory(requestBytes);
    if (!sharedMemPtr) {
      DEBUG_PRINTLN("[Alloc] MP.AllocSharedMemory failed (pool)");
      return NULL;
    }

    DEBUG_PRINT("[Alloc] block raw=0x");
    DEBUG_PRINTLN((uintptr_t)sharedMemPtr, HEX);

    uintptr_t alignAddr = reinterpret_cast<uintptr_t>(sharedMemPtr);
    if ((alignAddr % sizeof(uint32_t)) != 0) {
      DEBUG_PRINT("[Alloc] WARNING: unaligned pointer 0x");
      DEBUG_PRINTLN(alignAddr, HEX);
    }

    blockBasePtr_ = static_cast<uint8_t *>(sharedMemPtr);
    blockSizeBytes_ = allocBytes;
    freeAreaPtr_ = blockBasePtr_;
    freeSizeBytes_ = allocBytes;
    lastLayoutAllocBytes_ = 0;
  }

  void *allocated = freeAreaPtr_;

  DEBUG_PRINT("[Alloc] using free area raw=0x");
  DEBUG_PRINT((uintptr_t)allocated, HEX);
  DEBUG_PRINT(" free bytes before consume=");
  DEBUG_PRINTLN((unsigned long)freeSizeBytes_);

  freeAreaPtr_ += requestBytes;
  freeSizeBytes_ -= requestBytes;

  DEBUG_PRINT("[Alloc] free bytes after consume=");
  DEBUG_PRINTLN((unsigned long)freeSizeBytes_);

  return allocated;
}

bool SharedMemoryAllocator::alloc(const SharedLayoutArea *map, size_t areaCount, SharedMemoryArea *areas)
{
  for (size_t index = 0; index < areaCount; ++index) {
    resetArea(&areas[index]);
  }

  applyLayoutMap(areas, areaCount, map);

  size_t poolWords = 0;
  for (size_t index = 0; index < areaCount; ++index) {
    const size_t dataBytes = areas[index].bytes * areas[index].count;
    areas[index].total_bytes = calcTotalWords(areas[index].bytes, areas[index].count);
    poolWords += areas[index].total_bytes;

    DEBUG_PRINT("[Alloc] area_index=");
    DEBUG_PRINTLN((unsigned long)index);
    DEBUG_PRINT("[Alloc] request bytes=");
    DEBUG_PRINT((unsigned long)areas[index].bytes);
    DEBUG_PRINT(" count=");
    DEBUG_PRINT((unsigned long)areas[index].count);
    DEBUG_PRINT(" dataBytes=");
    DEBUG_PRINT((unsigned long)dataBytes);
    DEBUG_PRINT(" totalWords=");
    DEBUG_PRINT((unsigned long)areas[index].total_bytes);
    DEBUG_PRINT(" totalBytes=");
    DEBUG_PRINTLN((unsigned long)(sizeof(uint32_t) * areas[index].total_bytes));
  }

  DEBUG_PRINT("[Alloc] pool totalWords=");
  DEBUG_PRINT((unsigned long)poolWords);
  DEBUG_PRINT(" totalBytes=");
  DEBUG_PRINTLN((unsigned long)(sizeof(uint32_t) * poolWords));

  const size_t requestBytes = sizeof(uint32_t) * poolWords;
  uint32_t *poolRaw = static_cast<uint32_t *>(allocateChunk(requestBytes));
  if (!poolRaw) {
    for (size_t index = 0; index < areaCount; ++index) {
      resetArea(&areas[index]);
    }
    return false;
  }

  lastLayoutAllocBytes_ = requestBytes;

  size_t cumulativeBytes = 0;
  size_t offsetWords = 0;
  for (size_t index = 0; index < areaCount; ++index) {
    areas[index].fence = poolRaw + offsetWords;
    initFenceWords(areas[index].fence, areas[index].total_bytes);
    areas[index].data = reinterpret_cast<void *>(areas[index].fence + 1);

    size_t areaBytes = sizeof(uint32_t) * areas[index].total_bytes;
    cumulativeBytes += areaBytes;
    DEBUG_PRINT("[Alloc] success area_index=");
    DEBUG_PRINT((unsigned long)index);
    DEBUG_PRINT(" offsetWords=");
    DEBUG_PRINT((unsigned long)offsetWords);
    DEBUG_PRINT(" bytes=");
    DEBUG_PRINT((unsigned long)areaBytes);
    DEBUG_PRINT(" cumulative=");
    DEBUG_PRINT((unsigned long)cumulativeBytes);
    DEBUG_PRINT(" raw=0x");
    DEBUG_PRINT((uintptr_t)areas[index].fence, HEX);
    DEBUG_PRINT(" data=0x");
    DEBUG_PRINTLN((uintptr_t)areas[index].data, HEX);
    memset(areas[index].data, 0,
           areas[index].bytes * areas[index].count);

    offsetWords += areas[index].total_bytes;
  }

  DEBUG_PRINT("[Alloc] total allocated: ");
  DEBUG_PRINT((unsigned long)cumulativeBytes);
  DEBUG_PRINTLN(" bytes");

  if (!isAllocated(areas, areaCount)) {
    DEBUG_PRINTLN("[Alloc] isAllocated check failed after allocation");
    free(areas, areaCount);
    return false;
  }

  return true;
}

bool SharedMemoryAllocator::free(SharedMemoryArea *areas, size_t areaCount)
{
  bool fenceOk = true;

  for (size_t index = 0; index < areaCount; ++index) {
    if (areas[index].fence) {
      fenceOk = fenceOk && areas[index].total_bytes >= 2 &&
                areas[index].fence[0] == SHARED_MEMORY_FENCE_PATTERN &&
                areas[index].fence[areas[index].total_bytes - 1] == SHARED_MEMORY_FENCE_PATTERN;
    }
  }

  if (!fenceOk) {
    DEBUG_PRINTLN("[Main] shared memory fence broken before free");
  }

  for (size_t index = 0; index < areaCount; ++index) {
    resetArea(&areas[index]);
  }

  if (lastLayoutAllocBytes_ > 0) {
    if (freeAreaPtr_ && freeAreaPtr_ >= blockBasePtr_ + lastLayoutAllocBytes_) {
      freeAreaPtr_ -= lastLayoutAllocBytes_;
      freeSizeBytes_ += lastLayoutAllocBytes_;
    }
    lastLayoutAllocBytes_ = 0;
  }

  if (freeSizeBytes_ >= SHARED_MEMORY_ALLOC_GRANULARITY_BYTES) {
    if (blockBasePtr_) {
      DEBUG_PRINT("[Alloc] releasing shared block raw=0x");
      DEBUG_PRINT((uintptr_t)blockBasePtr_, HEX);
      DEBUG_PRINT(" bytes=");
      DEBUG_PRINTLN((unsigned long)blockSizeBytes_);
      MP.FreeSharedMemory(blockBasePtr_);
      blockBasePtr_ = NULL;
      blockSizeBytes_ = 0;
      freeAreaPtr_ = NULL;
      freeSizeBytes_ = 0;
    }
  }

  DEBUG_PRINT("[Alloc] retained free area ptr=0x");
  DEBUG_PRINT((uintptr_t)freeAreaPtr_, HEX);
  DEBUG_PRINT(" bytes=");
  DEBUG_PRINTLN((unsigned long)freeSizeBytes_);

  return fenceOk;
}

bool SharedMemoryAllocator::isAllocated(const SharedMemoryArea *areas, size_t areaCount) const
{
  if (areaCount == 0) {
    return false;
  }

  for (size_t index = 0; index < areaCount; ++index) {
    if (!areas[index].data) {
      return false;
    }
  }
  return true;
}

#endif
