/*
 *  InterCoreRingBuffer.h - Byte-oriented ring buffer for multi-core data sharing on Spresense
 *  Copyright 2026 Spresense User
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

#ifndef INTER_CORE_RING_BUFFER_H
#define INTER_CORE_RING_BUFFER_H

#include <stdint.h>
#include <string.h>

/**
 * @brief Type-safe ring buffer for inter-core communication
 * 
 * Stores entries of type T with separate read pointer (rp, operated by reader core)
 * and write pointer (wp, operated by writer core). Includes full flag to detect
 * write overruns.
 * 
 * Thread-safe through atomic operations. No spinlock needed as each core operates
 * its own pointer independently.
 * 
 * Usage:
 *   struct SensorData { float acc[3]; float gyro[3]; };
 *   InterCoreRingBuffer<SensorData, 128> ringBuf;  // 128 entries of SensorData
 * 
 * Writer (SubCore):
 *   SensorData data = {...};
 *   if (!ringBuf.write(data)) {
 *     // Buffer full - check full() for overflow detection
 *   }
 * 
 * Reader (MainCore):
 *   SensorData data;
 *   if (ringBuf.read(data)) {
 *     // Process data
 *   }
 * 
 * @tparam T Data structure type to store (any POD type)
 * @tparam NUM Maximum number of T entries the buffer can hold
 */
template<typename T, uint32_t NUM>
class InterCoreRingBuffer {
private:
  T buffer[NUM];
  volatile uint32_t rp;        // Read pointer (operated by reader core)
  volatile uint32_t wp;        // Write pointer (operated by writer core)
  volatile bool fullFlag;      // Full flag (set by writer when buffer is full)

public:
  /**
   * @brief Constructor - initializes buffer to empty state
   */
  InterCoreRingBuffer() 
    : rp(0), wp(0), fullFlag(false) {
    memset(buffer, 0, sizeof(buffer));
  }

  /**
   * @brief Write one entry of type T
   * 
   * Called by writer core (typically SubCore). Non-blocking.
   * Does not write if it would exceed the read pointer.
   * 
   * @param data Reference to data to write
   * @return true if successfully written, false if buffer full
   */
  bool write(const T& data) {
    uint32_t currentRp = __atomic_load_n(&rp, __ATOMIC_ACQUIRE);
    uint32_t currentWp = __atomic_load_n(&wp, __ATOMIC_ACQUIRE);

    // Calculate next write position
    uint32_t nextWp = (currentWp + 1) % NUM;

    // Check if buffer is full (next write would catch read pointer)
    if (nextWp == currentRp) {
      __atomic_store_n(&fullFlag, true, __ATOMIC_RELEASE);
      return false;  // Buffer full, cannot write
    }

    // Write data
    buffer[currentWp] = data;

    // Update write pointer atomically
    __atomic_store_n(&wp, nextWp, __ATOMIC_RELEASE);
    
    // Clear full flag on successful write
    __atomic_store_n(&fullFlag, false, __ATOMIC_RELEASE);

    return true;
  }

  /**
   * @brief Read one entry of type T
   * 
   * Called by reader core (typically MainCore). Non-blocking.
   * 
   * @param data Reference to where data will be stored
   * @return true if successfully read, false if buffer empty
   */
  bool read(T& data) {
    uint32_t currentRp = __atomic_load_n(&rp, __ATOMIC_ACQUIRE);
    uint32_t currentWp = __atomic_load_n(&wp, __ATOMIC_ACQUIRE);

    // Check if buffer is empty
    if (currentRp == currentWp) {
      return false;
    }

    // Read data
    data = buffer[currentRp];

    // Update read pointer atomically
    uint32_t nextRp = (currentRp + 1) % NUM;
    __atomic_store_n(&rp, nextRp, __ATOMIC_RELEASE);

    return true;
  }

  /**
   * @brief Get current read pointer value
   * @return Current rp index (0 to NUM-1)
   */
  uint32_t getRp() const {
    return __atomic_load_n(&rp, __ATOMIC_ACQUIRE);
  }

  /**
   * @brief Get current write pointer value
   * @return Current wp index (0 to NUM-1)
   */
  uint32_t getWp() const {
    return __atomic_load_n(&wp, __ATOMIC_ACQUIRE);
  }

  /**
   * @brief Check full flag (write overflow detection)
   * 
   * Indicates that a write operation was attempted when buffer was full.
   * Reader should check this to detect potential data loss.
   * 
   * @return true if a write was rejected due to buffer full, false otherwise
   */
  bool full() const {
    return __atomic_load_n(&fullFlag, __ATOMIC_ACQUIRE);
  }

  /**
   * @brief Check if buffer is empty
   * @return true if rp == wp, false otherwise
   */
  bool empty() const {
    return __atomic_load_n(&rp, __ATOMIC_ACQUIRE) == 
           __atomic_load_n(&wp, __ATOMIC_ACQUIRE);
  }

  /**
   * @brief Get current number of entries in buffer
   * @return Number of unread entries
   */
  uint32_t getCount() const {
    uint32_t currentRp = __atomic_load_n(&rp, __ATOMIC_ACQUIRE);
    uint32_t currentWp = __atomic_load_n(&wp, __ATOMIC_ACQUIRE);
    
    if (currentWp >= currentRp) {
      return currentWp - currentRp;
    } else {
      return NUM - currentRp + currentWp;
    }
  }

  /**
   * @brief Get total buffer capacity in entries
   * @return NUM constant
   */
  uint32_t getCapacity() const {
    return NUM;
  }

  /**
   * @brief Reset buffer to empty state
   * 
   * Clears all pointers and full flag. Should only be called during
   * initialization when no cores are accessing the buffer.
   */
  void reset() {
    __atomic_store_n(&rp, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&wp, 0u, __ATOMIC_RELEASE);
    __atomic_store_n(&fullFlag, false, __ATOMIC_RELEASE);
    memset(buffer, 0, sizeof(buffer));
  }
};


#endif // INTER_CORE_RING_BUFFER_H
