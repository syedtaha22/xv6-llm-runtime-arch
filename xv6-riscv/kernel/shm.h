/**
 * @file shm.h
 * @brief Shared memory subsystem header for xv6 with persistence and lazy deletion.
 *
 * @author Syed Taha
 * @date 27th November 2025
 *
 * @details
 * This header defines the shared memory API for xv6, providing POSIX-compliant
 * shared memory operations with persistence across processes and lazy deletion
 * semantics. The implementation supports large memory segments suitable for
 * LLM weight storage and includes proper synchronization for concurrent access.
 *
 * Key features:
 * - Persistent shared memory segments that survive process termination
 * - Lazy deletion with IPC_RMID for safe cleanup when last process detaches
 * - Large buffer support (up to ~70MB with 4KB pages)
 * - Thread-safe operations with spinlock protection
 * - Memory-mapped access with configurable permissions
 */

#ifndef _SHM_H_
#define _SHM_H_

#include "types.h"

// Shared memory constants
/** @brief Maximum number of shared memory segments in the system. */
#define NSHM 16
/** @brief Maximum length of shared memory segment names. */
#define SHM_NAME_LEN 32
/** @brief Maximum pages per shared memory segment (~70MB with 4KB pages). */
#define MAX_PAGES_PER_SEG 17825 // Enough ~70MB shared resources with 4KB pages
/** @brief Flag to create persistent shared memory segments. */
#define SHM_PERSIST 0x01
/** @brief Flag for read-only shared memory access. */
#define SHM_RDONLY 0x01
/** @brief Flag for read-write shared memory access. */
#define SHM_RDWR 0x02

// IPC flags
/** @brief Flag to create shared memory segment if it doesn't exist. */
#define IPC_CREAT 0x1000
/** @brief Flag to fail if shared memory segment already exists. */
#define IPC_EXCL 0x2000
/** @brief Command to mark shared memory segment for removal. */
#define IPC_RMID 0  // Remove shared memory segment

/**
 * @brief Shared memory segment structure.
 *
 * @details
 * Represents a single shared memory segment in the system. Contains metadata
 * for identification, physical page mappings, reference counting, and
 * synchronization primitives.
 */
struct shm_segment {
  int id;  /**< Shared memory ID */
  char name[SHM_NAME_LEN];  /**< Name of the shared memory segment */
  uint64 phys_pages[MAX_PAGES_PER_SEG];  /**< Physical addresses of allocated pages */
  uint64 size;  /**< Total size of the segment in bytes */
  uint64 npages;  /**< Number of pages allocated */
  int refcount;  /**< Number of processes currently attached */
  int persistent;  /**< Whether segment persists after creator exits */
  struct spinlock lock;  /**< Lock for thread-safe access to segment */
};

/**
 * @brief Global shared memory table structure.
 *
 * @details
 * Contains the global table of all shared memory segments and provides
 * synchronization for table-wide operations.
 */
struct shm_table {
  struct spinlock lock;  /**< Lock for thread-safe access to table */
  struct shm_segment segs[NSHM];  /**< Array of shared memory segments */
};

// Global shared memory table
extern struct shm_table shm_table;

#endif // _SHM_H_