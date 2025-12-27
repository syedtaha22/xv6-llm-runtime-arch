/**
 * @file shm.c
 * @brief Shared memory subsystem implementation for xv6 with persistence and lazy deletion.
 *
 * @author Syed Taha
 * @date 27th November 2025
 *
 * @details
 * This file implements the core shared memory functionality for xv6, providing
 * POSIX-compliant shared memory operations with persistence across processes and
 * lazy deletion semantics. The implementation supports large memory segments
 * suitable for LLM weight storage and includes proper synchronization for
 * concurrent access.
 *
 * Key components:
 * - shm_init(): Initializes the shared memory table and segments
 * - shm_get(): Creates or retrieves shared memory segments
 * - shm_attach(): Maps shared memory into process address space
 * - shm_detach(): Unmaps shared memory from process address space
 * - shmctl(): Controls shared memory segments (removal, etc.)
 * - shm_cleanup_proc(): Cleans up shared memory on process exit
 *
 * The implementation uses physical page allocation (kalloc/kfree) and virtual
 * memory mapping (mappages/uvmunmap) to provide efficient shared memory access.
 * Persistence is achieved by maintaining segments beyond creator lifetime, and
 * lazy deletion ensures safe cleanup when the last process detaches.
 */

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "shm.h"
#include "defs.h"

// Global shared memory table
struct shm_table shm_table;

/**
 * @brief Find a free virtual address region for shared memory mapping.
 *
 * @param p Process structure for address space checking.
 * @param size Size of the memory region needed.
 * @return uint64 Free virtual address on success, 0 on failure.
 *
 * @details
 * Searches the process's virtual address space starting from 0x60000000 (1.5GB)
 * for a contiguous free region large enough to hold the requested size. The
 * search checks for conflicts with existing page mappings by walking the page
 * table for each potential address.
 */
uint64 find_free_shm_addr(struct proc *p, uint64 size) {
  uint64 start_addr = 0x60000000;  // Start searching from 1.5GB
  uint64 addr = start_addr;

  while (addr < MAXVA - size) {
    int conflict = 0;

    // Check if this address range conflicts with existing mappings
    for (uint64 va = addr; va < addr + size; va += PGSIZE) {
      if (walkaddr(p->pagetable, va) != 0) {
        conflict = 1;
        break;
      }
    }

    if (!conflict) return addr;  // Found a free region

    addr += PGSIZE;  // Try next page-aligned address
  }

  return 0;  // No free region found
}

/**
 * @brief Initialize the shared memory system.
 *
 * @details
 * Sets up the global shared memory table with proper locking and initializes
 * all shared memory segments to their default (unused) state. This function
 * should be called during system initialization.
 */
void shm_init(void) {
  initlock(&shm_table.lock, "shm_table");

  for (int i = 0; i < NSHM; i++) {
    shm_table.segs[i].id = -1;
    shm_table.segs[i].name[0] = '\0';
    for (int j = 0; j < MAX_PAGES_PER_SEG; j++) {
      shm_table.segs[i].phys_pages[j] = 0;
    }
    shm_table.segs[i].size = 0;
    shm_table.segs[i].npages = 0;
    shm_table.segs[i].refcount = 0;
    shm_table.segs[i].persistent = 0;
    initlock(&shm_table.segs[i].lock, "shm_segment");
  }
}

/**
 * @brief Get or create a shared memory segment.
 *
 * @param name Name of the shared memory segment.
 * @param size Size of the segment in bytes.
 * @param flags Creation flags (IPC_CREAT, IPC_EXCL, SHM_PERSIST).
 * @return int Shared memory ID on success, -1 on failure.
 *
 * @details
 * Implements the shmget system call. If IPC_CREAT is specified and the segment
 * doesn't exist, creates a new persistent segment. If the segment exists and
 * IPC_EXCL is not set, returns the existing segment ID. Handles size validation,
 * physical page allocation, and proper initialization.
 */
int shm_get(char *name, uint64 size, int flags) {
  struct shm_segment *seg;
  int create = flags & IPC_CREAT;
  int excl = flags & IPC_EXCL;

  if (name == 0 || name[0] == '\0' || size == 0)
    return -1;

  uint64 npages = (size + PGSIZE - 1) / PGSIZE;
  size = npages * PGSIZE;

  acquire(&shm_table.lock);

  // Search for existing segment
  for (int i = 0; i < NSHM; i++) {
    seg = &shm_table.segs[i];
    if (seg->id != -1 && strncmp(seg->name, name, SHM_NAME_LEN) == 0 && seg->persistent) {
      if (seg->size != size) {
        release(&shm_table.lock);
        return -1;
      }
      if (excl) {
        release(&shm_table.lock);
        return -1;  // Already exists
      }
      acquire(&seg->lock);
      seg->refcount++;
      release(&seg->lock);
      int id = seg->id;
      release(&shm_table.lock);
      return id;
    }
  }

  // Segment not found
  if (!create) {
    release(&shm_table.lock);
    return -1;  // Don't create
  }

  // Create new segment - find first free slot
  int free_idx = -1;
  for (int i = 0; i < NSHM; i++) {
    if (shm_table.segs[i].id == -1) {
      free_idx = i;
      break;
    }
  }

  if (free_idx == -1) {
    release(&shm_table.lock);
    return -1;  // No free slots
  }

  seg = &shm_table.segs[free_idx];

  // Allocate physical pages - this can fail if system runs out of memory
  for (uint64 i = 0; i < npages; i++) {
    seg->phys_pages[i] = (uint64)kalloc();
    if (seg->phys_pages[i] == 0) {
      // Free already allocated pages on failure
      for (uint64 j = 0; j < i; j++) {
        kfree((void*)seg->phys_pages[j]);
        seg->phys_pages[j] = 0;
      }
      release(&shm_table.lock);
      return -1;
    }
  }

  seg->id = free_idx;
  strncpy(seg->name, name, SHM_NAME_LEN);
  seg->name[SHM_NAME_LEN - 1] = '\0';
  seg->size = size;
  seg->npages = npages;
  seg->refcount = 1;
  seg->persistent = (flags & SHM_PERSIST) ? 1 : 0;

  // Initialize memory to zero for clean state
  for (uint64 i = 0; i < npages; i++) {
    memset((void*)seg->phys_pages[i], 0, PGSIZE);
  }

  int id = seg->id;
  release(&shm_table.lock);
  return id;
}

/**
 * @brief Attach shared memory segment to current process.
 *
 * @param shmid Shared memory segment ID.
 * @param uaddr Desired virtual address (0 for auto-allocation).
 * @param flags Access flags (SHM_RDONLY, SHM_RDWR).
 * @return void* Mapped virtual address on success, (void*)-1 on failure.
 *
 * @details
 * Implements the shmat system call. Maps the shared memory segment into the
 * current process's address space. If uaddr is 0, automatically finds a free
 * virtual address region. Validates permissions and ensures the address range
 * is available before mapping each physical page.
 */
void* shm_attach(int shmid, void *uaddr, int flags) {
  struct shm_segment *seg;
  struct proc *p = myproc();
  uint64 va;
  int perm;

  if (shmid < 0)
    return (void*)-1;

  acquire(&shm_table.lock);

  seg = 0;
  for (int i = 0; i < NSHM; i++) {
    if (shm_table.segs[i].id == shmid && shm_table.segs[i].id != -1) {
      seg = &shm_table.segs[i];
      break;
    }
  }

  if (seg == 0) {
    release(&shm_table.lock);
    return (void*)-1;
  }

  // Don't allow attaching to segments marked for deletion
  if (!seg->persistent) {
    release(&shm_table.lock);
    return (void*)-1;
  }

  if (uaddr == 0) {
    // Find a free virtual address region
    va = find_free_shm_addr(p, seg->size);
    if (va == 0) {
      release(&shm_table.lock);
      return (void*)-1;  // No free address space
    }
  } else {
    va = (uint64)uaddr;
  }

  if (va % PGSIZE != 0 || va >= MAXVA || va + seg->size >= MAXVA) {
    release(&shm_table.lock);
    return (void*)-1;
  }

  // Check if the address range is already mapped - prevents overwriting existing mappings
  for (uint64 check_va = va; check_va < va + seg->size; check_va += PGSIZE) {
    if (walkaddr(p->pagetable, check_va) != 0) {
      release(&shm_table.lock);
      return (void*)-1;  // Address already in use
    }
  }

  perm = PTE_U;
  if (flags & SHM_RDWR) {
    perm |= PTE_R | PTE_W;
  } else {
    perm |= PTE_R;
  }

  // Map each page separately - allows for non-contiguous physical memory
  for (uint64 i = 0; i < seg->npages; i++) {
    uint64 page_va = va + i * PGSIZE;
    uint64 page_pa = seg->phys_pages[i];
    if (mappages(p->pagetable, page_va, PGSIZE, page_pa, perm) < 0) {
      // Unmap already mapped pages on failure to maintain consistency
      for (uint64 j = 0; j < i; j++) {
        uint64 unmap_va = va + j * PGSIZE;
        uvmunmap(p->pagetable, unmap_va, 1, 0);
      }
      release(&shm_table.lock);
      return (void*)-1;
    }
  }

  // record the attachment in the process local table
  acquire(&p->lock);
  int slot = -1;
  for (int i = 0; i < NSHM; i++) {
    if (p->shm_attached[i].shmid == -1) {
      slot = i;
      break;
    }
  }
  if (slot == -1) {
    // no room to track this attachment; undo mappings and fail
    for (uint64 j = 0; j < seg->npages; j++) {
      uvmunmap(p->pagetable, va + j * PGSIZE, 1, 0);
    }
    release(&p->lock);
    release(&shm_table.lock);
    return (void*)-1;
  }

  p->shm_attached[slot].shmid = seg->id;
  p->shm_attached[slot].va = va;
  release(&p->lock);

  release(&shm_table.lock);
  return (void*)va;  // Return the mapped virtual address
}

/**
 * @brief Detach shared memory segment from current process.
 *
 * @param shmaddr Virtual address where the segment is mapped.
 * @return int 0 on success, -1 on failure.
 *
 * @details
 * Implements the shmdt system call. Unmaps the shared memory segment from the
 * current process's address space. Identifies the segment by matching the
 * virtual address to physical addresses in the segment's page array. Decrements
 * reference count and frees resources if this was the last attachment and the
 * segment is marked for deletion.
 */
int shm_detach(void *shmaddr) {
  struct shm_segment *seg;
  struct proc *p = myproc();
  uint64 va = (uint64)shmaddr;

  if (va % PGSIZE != 0 || va >= MAXVA)
    return -1;

  // Find the attachment slot in the process-local table
  acquire(&p->lock);
  int slot = -1;
  for (int i = 0; i < NSHM; i++) {
    if (p->shm_attached[i].shmid != -1 && p->shm_attached[i].va == va) {
      slot = i;
      break;
    }
  }
  if (slot == -1) {
    release(&p->lock);
    return -1; // not found
  }

  int shmid = p->shm_attached[slot].shmid;
  // mark slot freed
  p->shm_attached[slot].shmid = -1;
  p->shm_attached[slot].va = 0;
  release(&p->lock);

  acquire(&shm_table.lock);
  seg = 0;
  if (shmid >= 0 && shmid < NSHM) {
    seg = &shm_table.segs[shmid];
  }
  if (seg == 0 || seg->id == -1) {
    release(&shm_table.lock);
    return -1;
  }

  // Unmap the entire segment from this process's address space
  uvmunmap(p->pagetable, va, seg->npages, 0);

  acquire(&seg->lock);
  seg->refcount--;

  if (seg->refcount == 0 && !seg->persistent) {
    // Last process detached and segment marked for deletion - free resources
    for (uint64 i = 0; i < seg->npages; i++) {
      kfree((void*)seg->phys_pages[i]);
      seg->phys_pages[i] = 0;
    }
    seg->name[0] = '\0';
    seg->size = 0;
    seg->npages = 0;
    seg->id = -1;
  }

  release(&seg->lock);
  release(&shm_table.lock);

  return 0;
}

/**
 * @brief Control shared memory segment.
 *
 * @param shmid Shared memory segment ID.
 * @param cmd Control command (IPC_RMID for removal).
 * @param buf Unused buffer parameter (for future extensions).
 * @return int 0 on success, -1 on failure.
 *
 * @details
 * Implements the shmctl system call. Currently supports IPC_RMID command to
 * mark a shared memory segment for deletion. If no processes are attached,
 * removes the segment immediately. Otherwise, marks it as non-persistent so
 * it gets cleaned up when the last process detaches (lazy deletion).
 */
int shmctl(int shmid, int cmd, void *buf) {
  struct shm_segment *seg;

  if (shmid < 0)
    return -1;

  acquire(&shm_table.lock);

  seg = 0;
  for (int i = 0; i < NSHM; i++) {
    if (shm_table.segs[i].id == shmid && shm_table.segs[i].id != -1) {
      seg = &shm_table.segs[i];
      break;
    }
  }

  if (seg == 0) {
    release(&shm_table.lock);
    return -1;
  }

  switch (cmd) {
    case IPC_RMID:
      // Mark segment for removal - implements lazy deletion semantics
      acquire(&seg->lock);
      if (seg->refcount == 0) {
        // No processes attached, remove immediately
        for (uint64 i = 0; i < seg->npages; i++) {
          kfree((void*)seg->phys_pages[i]);
          seg->phys_pages[i] = 0;
        }
        seg->name[0] = '\0';
        seg->size = 0;
        seg->npages = 0;
        seg->id = -1;
      } else {
        // Processes still attached, mark as non-persistent so it gets cleaned up
        // when last process detaches
        seg->persistent = 0;
      }
      release(&seg->lock);
      break;

    default:
      release(&shm_table.lock);
      return -1;
  }

  release(&shm_table.lock);
  return 0;
}

/**
 * @brief Clean up shared memory mappings for exiting process.
 *
 * @param p Process structure being cleaned up.
 *
 * @details
 * Called during process exit to detach from all shared memory segments.
 * Iterates through all segments and unmaps them from the process's address
 * space, decrementing reference counts. Segments marked for deletion get
 * freed when their reference count reaches zero. This ensures proper cleanup
 * even when processes exit abnormally.
 */
void shm_cleanup_proc(struct proc *p) {
  // Iterate over the per-process attachment list and detach each mapped segment
  for (int i = 0; i < NSHM; i++) {
    int shmid = p->shm_attached[i].shmid;
    uint64 va = p->shm_attached[i].va;
    if (shmid == -1) continue;

    // Clear the per-process slot first
    p->shm_attached[i].shmid = -1;
    p->shm_attached[i].va = 0;

    // Unmap and update global segment refcount
    acquire(&shm_table.lock);
    if (shmid < 0 || shmid >= NSHM || shm_table.segs[shmid].id == -1) {
      release(&shm_table.lock);
      continue;
    }
    struct shm_segment *seg = &shm_table.segs[shmid];

    // Unmap the pages from this process
    uvmunmap(p->pagetable, va, seg->npages, 0);

    acquire(&seg->lock);
    seg->refcount--;
    if (seg->refcount == 0 && !seg->persistent) {
      for (uint64 k = 0; k < seg->npages; k++) {
        kfree((void*)seg->phys_pages[k]);
        seg->phys_pages[k] = 0;
      }
      seg->name[0] = '\0';
      seg->size = 0;
      seg->npages = 0;
      seg->id = -1;
    }
    release(&seg->lock);
    release(&shm_table.lock);
  }
}