/*
 *  SharedMemoryAllocator.h - Shared memory allocator.
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

#ifndef SHARED_MEMORY_ALLOCATOR_H
#define SHARED_MEMORY_ALLOCATOR_H

#include <stddef.h>
#include <stdint.h>

/* --- Common types (shared across all sketches) --- */

struct SharedMemoryArea {
  void*     data;        /* フェンス後の実データ部 */
  size_t    bytes;       /* 1サンプルあたりのサイズ（バイト数） */
  size_t    count;       /* サンプル数 */
  uint32_t* fence;       /* [内部用] フェンス付きメモリブロックの先頭 */
  size_t    total_bytes; /* [内部用] 確保メモリ全体（uint32_t単位にaligned）：payloadSize + 2（フェンス用） */
};

struct SharedLayoutArea {
  size_t size;
  size_t count;
};

class SharedMemoryAllocator {
public:
  bool alloc(const SharedLayoutArea *map, size_t areaCount, SharedMemoryArea *areas);
  bool free(SharedMemoryArea *areas, size_t areaCount);
  bool isAllocated(const SharedMemoryArea *areas, size_t areaCount) const;

private:
  static void resetArea(SharedMemoryArea *area);
  static void applyLayoutMap(SharedMemoryArea *areas, size_t areaCount, const SharedLayoutArea *map);
  static size_t calcTotalWords(size_t entrySizeBytes, size_t entryCount);
  static void initFenceWords(uint32_t *rawData, size_t totalBytes);
  static size_t roundUpAllocBytes(size_t bytes);
  void *allocateChunk(size_t requestBytes);

  uint8_t *blockBasePtr_ = NULL;
  size_t blockSizeBytes_ = 0;
  uint8_t *freeAreaPtr_ = NULL;
  size_t freeSizeBytes_ = 0;
  size_t lastLayoutAllocBytes_ = 0;
};

#endif
