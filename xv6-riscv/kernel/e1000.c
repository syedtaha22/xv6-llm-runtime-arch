#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"
#include "e1000_dev.h"

#define TX_RING_SIZE 16
static struct tx_desc tx_ring[TX_RING_SIZE] __attribute__((aligned(16)));

#define RX_RING_SIZE 16
static struct rx_desc rx_ring[RX_RING_SIZE] __attribute__((aligned(16)));

// remember where the e1000's registers live.
static volatile uint32 *regs;

struct spinlock e1000_lock;

// called by pci_init().
// xregs is the memory address at which the
// e1000's registers are mapped.
// this code loosely follows the initialization directions
// in Chapter 14 of Intel's Software Developer's Manual.
void
e1000_init(uint32 *xregs)
{
  int i;

  initlock(&e1000_lock, "e1000");

  regs = xregs;

  // Reset the device
  regs[E1000_IMS] = 0; // disable interrupts
  regs[E1000_CTL] |= E1000_CTL_RST;
  regs[E1000_IMS] = 0; // redisable interrupts
  __sync_synchronize();

  // [E1000 14.5] Transmit initialization
  memset(tx_ring, 0, sizeof(tx_ring));
  for (i = 0; i < TX_RING_SIZE; i++) {
    tx_ring[i].status = E1000_TXD_STAT_DD;
    tx_ring[i].addr = 0;
  }
  regs[E1000_TDBAL] = (uint64) tx_ring;
  if(sizeof(tx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_TDLEN] = sizeof(tx_ring);
  regs[E1000_TDH] = regs[E1000_TDT] = 0;
  
  // [E1000 14.4] Receive initialization
  memset(rx_ring, 0, sizeof(rx_ring));
  for (i = 0; i < RX_RING_SIZE; i++) {
    rx_ring[i].addr = (uint64) kalloc();
    if (!rx_ring[i].addr)
      panic("e1000");
  }
  regs[E1000_RDBAL] = (uint64) rx_ring;
  if(sizeof(rx_ring) % 128 != 0)
    panic("e1000");
  regs[E1000_RDH] = 0;
  regs[E1000_RDT] = RX_RING_SIZE - 1;
  regs[E1000_RDLEN] = sizeof(rx_ring);

  // filter by qemu's MAC address, 52:54:00:12:34:56
  regs[E1000_RA] = 0x12005452;
  regs[E1000_RA+1] = 0x5634 | (1<<31);
  // multicast table
  for (int i = 0; i < 4096/32; i++)
    regs[E1000_MTA + i] = 0;

  // transmitter control bits.
  regs[E1000_TCTL] = E1000_TCTL_EN |  // enable
    E1000_TCTL_PSP |                  // pad short packets
    (0x10 << E1000_TCTL_CT_SHIFT) |   // collision stuff
    (0x40 << E1000_TCTL_COLD_SHIFT);
  regs[E1000_TIPG] = 10 | (8<<10) | (6<<20); // inter-pkt gap

  // receiver control bits.
  regs[E1000_RCTL] = E1000_RCTL_EN | // enable receiver
    E1000_RCTL_BAM |                 // enable broadcast
    E1000_RCTL_SZ_2048 |             // 2048-byte rx buffers
    E1000_RCTL_SECRC;                // strip CRC
  
  // ask e1000 for receive interrupts.
  regs[E1000_RDTR] = 0; // interrupt after every received packet (no timer)
  regs[E1000_RADV] = 0; // interrupt after every packet (no timer)
  regs[E1000_IMS] = (1 << 7); // RXDW -- Receiver Descriptor Write Back
}

/**
 * @brief Transmit an Ethernet frame using the E1000 network interface controller
 * @author Syed Taha
 * @date 2024
 * 
 * @details
 * This function programs a complete Ethernet frame into the transmit descriptor ring
 * for transmission by the E1000 hardware. It implements the software side of the
 * transmit path, managing descriptor allocation, buffer ownership, and hardware
 * synchronization.
 * 
 * The function operates on a producer-consumer model:
 * - Software (producer): enqueues packets for transmission in descriptor ring
 * - Hardware (consumer): transmits packets and signals completion via status bits
 * 
 * Key operations performed:
 * - Acquires transmit lock for exclusive access to descriptor ring
 * - Checks descriptor availability using DD status bit
 * - Manages buffer lifecycle (frees previous, assigns new)
 * - Configures descriptor with proper command flags and metadata
 * - Updates hardware registers to initiate transmission
 * 
 * @param buf Pointer to the Ethernet frame buffer containing the complete packet
 * @param len Length of the Ethernet frame in bytes
 * 
 * @return int 0 on successful transmission queuing, -1 on descriptor unavailability
 * 
 * @note The caller retains ownership of the buffer until transmission completes.
 * The RS (Report Status) flag ensures the hardware sets DD bit upon completion.
 * @note Buffer freeing occurs when the descriptor is reused, not immediately after transmission.
 * 
 * @warning This function must be called with a valid Ethernet frame in buf.
 * @warning The function returns -1 when transmit ring is full; caller should retry later.
 */
int
e1000_transmit(char *buf, int len)
{
  // buf contains an ethernet frame; program it into
  // the TX descriptor ring so that the e1000 sends it. Stash
  // a pointer so that it can be freed after send completes.
  //
  // return 0 on success.
  // return -1 on failure (e.g., there is no descriptor available)
  // so that the caller knows to free buf.
  //

  // PHASE 1: LOCK ACQUISITION
  // Acquire the transmit lock to ensure exclusive access to the descriptor ring.
  // This prevents race conditions when multiple threads attempt to transmit concurrently.
  // The lock protects the entire critical section from ring inspection to tail update.
  acquire(&e1000_lock);

  // PHASE 2: DESCRIPTOR INDEX CALCULATION
  // Read the Transmit Descriptor Tail (TDT) register to locate the next available descriptor.
  // The TDT register points to the next descriptor that software can use for transmission.
  // Hardware owns descriptors from TDH (Head) to TDT-1, software owns from TDT onward.
  uint32 tail = regs[E1000_TDT];

  // PHASE 3: DESCRIPTOR AVAILABILITY CHECK
  // Check if the current descriptor is available by examining the DD (Descriptor Done) status bit.
  // The DD bit is set by hardware when it has finished processing the descriptor.
  // If DD is not set, the descriptor is still in use by hardware - transmission would fail.
  if (!(tx_ring[tail].status & E1000_TXD_STAT_DD))
  {
    // Descriptor is not available - hardware hasn't finished with previous transmission.
    // Release the lock and return failure so caller can retry or handle appropriately.
    // Caller should free the buffer since we didn't take ownership for transmission.
    release(&e1000_lock);
    return -1;
  }

  // PHASE 4: PREVIOUS BUFFER CLEANUP
  // If the descriptor still holds a buffer address from previous transmission, free it.
  // This handles the buffer that was transmitted in the previous use of this descriptor.
  // The buffer was allocated by a previous call to this function and transmission is complete.
  if (tx_ring[tail].addr != 0)
  {
    kfree((char *)tx_ring[tail].addr);
  }

  // PHASE 5: DESCRIPTOR CONFIGURATION
  // Program the descriptor with the new transmission parameters:
  // - Buffer address: Physical address of the Ethernet frame to transmit
  // - Length: Size of the Ethernet frame in bytes
  // - Command flags: Control how the hardware processes the descriptor
  // - Status: Clear to indicate descriptor is now in use by software
  tx_ring[tail].addr = (uint64)buf;  // Transfer buffer ownership to hardware
  tx_ring[tail].length = len;        // Set exact frame length for transmission
  tx_ring[tail].cmd = E1000_TXD_CMD_EOP | E1000_TXD_CMD_RS;  // Key flags:
      // EOP (End of Packet): This descriptor contains the complete packet
      // RS (Report Status): Request hardware to set DD bit when transmission completes
  tx_ring[tail].status = 0; // Clear all status bits, marking descriptor as in-use

  // PHASE 6: HARDWARE NOTIFICATION
  // Update the Transmit Descriptor Tail (TDT) register to inform hardware of new packets.
  // Advancing TDT makes the newly programmed descriptor available to hardware for transmission.
  // The modulo operation ensures circular ring behavior - wrap around at ring boundary.
  regs[E1000_TDT] = (tail + 1) % TX_RING_SIZE;

  // PHASE 7: LOCK RELEASE
  // Release the transmit lock, allowing other threads to access the transmit ring.
  // The critical section is complete - descriptor is programmed and hardware notified.
  release(&e1000_lock);
  
  // Return success: packet has been queued for transmission.
  // Hardware will asynchronously transmit the packet and set DD bit upon completion.
  // The buffer will be freed when this descriptor slot is reused in the future.
  return 0;
}

/**
 * @brief Process received packets from the E1000 network interface controller
 * @author Syed Taha
 * @date 2024
 * 
 * @details
 * This interrupt-driven function handles incoming network packets by processing the receive
 * descriptor ring. It implements the software side of the hardware-software communication
 * protocol for packet reception. The function is typically called when the E1000 generates
 * an interrupt indicating that one or more packets have been received and are ready for
 * processing.
 * 
 * The function operates on a producer-consumer model:
 * - Hardware (producer): writes packets to receive buffers and sets descriptor status bits
 * - Software (consumer): processes packets and recycles buffers for future use
 * 
 * Key operations performed:
 * - Scans the receive descriptor ring for newly arrived packets
 * - Delivers complete Ethernet frames to the network stack for protocol processing
 * - Maintains buffer inventory by allocating replacement buffers
 * - Updates hardware registers to synchronize ring state
 * 
 * @note This function must never return with buffers unreplenished, as buffer exhaustion
 * would permanently disable packet reception. The function panics on allocation failure.
 * 
 * @invariant The RDT register always points to the last descriptor processed by software
 * @invariant All descriptors between (RDT + 1) and head are owned by hardware
 * @invariant All processed descriptors are recycled with valid empty buffers
 */
static void
e1000_recv(void)
{
  //
  // Check for packets that have arrived from the e1000
  // Create and deliver a buf for each packet (using net_rx()).
  //
 
  // Calculate the next descriptor index to examine.
  // The E1000_RDT register points to the last descriptor processed by software.
  // We add 1 (with ring wrap-around) to get the next expected packet location.
  // Hardware owns descriptors from (RDT + 1) to (head - 1) in the ring.
  uint32 head = (regs[E1000_RDT] + 1) % RX_RING_SIZE;

  // Process all available packets in the receive ring.
  // The E1000 can deliver multiple packets per interrupt to amortize overhead.
  // We continue processing as long as descriptors have the DD (Descriptor Done) bit set,
  // indicating the hardware has filled the buffer with a new packet.
  while (rx_ring[head].status & E1000_RXD_STAT_DD)
  {
    // Get a direct reference to the current receive descriptor for cleaner code
    struct rx_desc *desc = &rx_ring[head];

    // PHASE 1: PACKET DELIVERY TO NETWORK STACK
    // The hardware has filled the buffer at desc->addr with a complete Ethernet frame.
    // We pass ownership of this buffer to the network stack for protocol processing.
    // net_rx() will handle Ethernet, IP, and UDP header parsing and eventually
    // deliver the payload to the appropriate application queue.
    // Note: net_rx() is responsible for freeing the buffer after processing.
    net_rx((char*)desc->addr, desc->length);

    // PHASE 2: BUFFER REPLENISHMENT FOR SUSTAINABLE RECEPTION
    // We must immediately replace the consumed buffer to prevent reception halting.
    // Each descriptor must always point to a valid, empty buffer for the hardware.
    // Failure to allocate here is fatal - the RX ring would lose a buffer slot permanently.
    char *new_buf = kalloc();
    if (!new_buf) {
      // Critical system failure: cannot continue without losing network functionality
      // In a production system, we might try to recover, but for xv6 we panic
      panic("e1000_recv: kalloc failed - RX ring buffer exhaustion");
    }

    // PHASE 3: DESCRIPTOR RECONFIGURATION
    // Update the descriptor to point to the new empty buffer and reset its status.
    // The hardware will clear the DD bit when it writes a new packet to this buffer.
    // We set status to 0 to clear all status bits, putting the descriptor in "ready" state.
    desc->addr = (uint64)new_buf;
    desc->status = 0; // Clear DD bit and all other status flags

    // PHASE 4: HARDWARE SYNCHRONIZATION
    // Update the Receive Descriptor Tail (RDT) register to inform the hardware
    // that we have processed up to this descriptor. The hardware is now free to
    // use descriptors from (RDT + 1) onward. This update makes the recycled
    // descriptor available for hardware use again.
    // Important: RDT always points to the last descriptor we processed, not the next one.
    regs[E1000_RDT] = head;

    // PHASE 5: RING ADVANCEMENT
    // Move to the next descriptor in the ring for the next iteration.
    // The modulo operation ensures we wrap around at the end of the ring buffer.
    // This maintains the circular queue invariant of the descriptor ring.
    head = (head + 1) % RX_RING_SIZE;
  }

  // Exit point: No more packets with DD bit set in the receive ring.
  // The hardware may deliver more packets and trigger another interrupt later.
  // All processed descriptors have been recycled with fresh buffers.
  // The RDT register accurately reflects our processing progress.
}

void
e1000_intr(void)
{
  // tell the e1000 we've seen this interrupt;
  // without this the e1000 won't raise any
  // further interrupts.
  regs[E1000_ICR] = 0xffffffff;

  e1000_recv();
}
