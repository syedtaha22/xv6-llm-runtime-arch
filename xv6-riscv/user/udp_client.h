/**
 * @file udp_client.h
 * @author Hamna Sajid
 * @date 22nd November 2025
 * 
 * @brief Public API and protocol constants for the user-space LLM UDP client.
 *
 * @details
 * This header exposes the simple application-level protocol used by the UDP
 * client (udp_client.c). It provides:
 *  - high-level functions to fetch files from the server (llm_fetch_file and wrappers)
 *  - low-level protocol operations (META/DATA/RETRANS requests)
 *  - protocol message type and size constants
 *  - file identifier constants used by the server
 *
 * All multi-byte fields in wire messages are encoded in big-endian (network) order.
 */

#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "kernel/types.h"
#include <stdint.h>

/**
 * @brief Send META_REQ and parse META_RESP from server.
 * @param file_id File identifier to query.
 * @param file_size Out parameter for total file size in bytes.
 * @param total_chunks Out parameter for number of chunks.
 * @param file_hash Out buffer (32 bytes) to receive expected SHA-256 hash.
 * @return 0 on success, -1 on failure.
 *
 * @protocol
 * Sends a 4-byte META_REQ and expects a 48-byte META_RESP. Fields are parsed
 * as big-endian per-wire format.
 *
 * @details
 * Returns -1 if send/recv fail, response length is unexpected, or response type
 * doesn't match MSG_META_RESP.
 */
int llm_meta_request(uint8_t file_id, uint32_t *file_size, uint32_t *total_chunks, unsigned char *file_hash);

/**
 * @brief Request a contiguous range of chunks from the server.
 * @param file_id File identifier.
 * @param start_idx Starting chunk index (0-based).
 * @param count Number of chunks requested (16-bit).
 * @return 0 on success, -1 on failure.
 *
 * @details Sends a DATA_RANGE_REQ. All multi-byte fields are encoded in
 * big-endian (network) order.
 */
int llm_data_range_request(uint8_t file_id, uint32_t start_idx, uint16_t count);

/**
 * @brief Request retransmission for specific missing chunk indices.
 * @param file_id File identifier.
 * @param indices Array of chunk indices to retransmit (host order).
 * @param count Number of indices in the array (<= MAX_RETRANS).
 * @return 0 on success, -1 on failure.
 *
 * @details Constructs a RETRANS_REQ containing count 32-bit big-endian indices.
 */
int llm_retrans_request(uint8_t file_id, uint32_t *indices, uint16_t count);

/**
 * @brief Fetch a complete file from server using the LLM UDP protocol.
 * @param file_id ID of the file to fetch (FILE_WEIGHTS, FILE_TOKENIZER, etc).
 * @param size_out Out parameter receiving the total file size in bytes on success.
 * @return Pointer to malloc'd buffer containing file contents on success (caller must free),
 *         NULL (0) on failure.
 *
 * @details
 * Implements the full file transfer workflow:
 * 1. META_REQ to obtain file_size, total_chunks and expected SHA-256
 * 2. Iteratively request ranges with DATA_RANGE_REQ and process incoming DATA_PACKETs
 * 3. Use RETRANS_REQ to request specific missing chunks
 * 4. On completion, verify SHA-256 and return assembled buffer
 *
 * @note
 * Uses bounded retries (MAX_RETRY_ROUNDS) and per-range/backoff receive spins
 * to handle packet loss. Performs input validation and memory checks.
 */
char* llm_fetch_file(uint8_t file_id, int *size_out);

/* ----------------------------------------------------------------------------
 * Convenience wrappers
 */

/**
 * fetch_model_weights
 * @brief Convenience wrapper to download the model weights file.
 * @param size_out Out parameter for returned file size.
 * @return malloc'd buffer with weights on success, NULL on failure.
 */
char* fetch_model_weights(int *size_out);

/**
 * fetch_tokenizer
 * @brief Convenience wrapper to download the tokenizer file.
 * @param size_out Out parameter for returned file size.
 * @return malloc'd buffer with tokenizer on success, NULL on failure.
 */
char* fetch_tokenizer(int *size_out);

/* ----------------------------------------------------------------------------
 * Protocol message types (wire values)
 *
 * These values appear as the first byte of protocol messages exchanged with
 * the LLM UDP server.
 */
#define MSG_META_REQ      0x01
#define MSG_META_RESP     0x02
#define MSG_DATA_RANGE_REQ 0x03
#define MSG_RETRANS_REQ   0x04
#define MSG_DATA_PACKET   0x05
#define MSG_ERROR         0x06

/* ----------------------------------------------------------------------------
 * Protocol constants
 *
 * - SERVER_PORT / SERVER_IP: server endpoint
 * - CHUNK_SIZE: size of each data chunk on the wire
 * - MAX_RANGE: maximum number of chunks requested per DATA_RANGE_REQ
 * - MAX_RETRANS: maximum indices in a RETRANS_REQ
 * - MAX_RECEIVE_SPINS / MAX_RETRY_ROUNDS: client-side retry/backoff parameters
 */
#define SERVER_PORT 9999
#define SERVER_IP ((10 << 24) | (0 << 16) | (2 << 8) | 2)  // 10.0.2.2
#define CHUNK_SIZE 512
#define MAX_RANGE 16
#define MAX_RETRANS 32
#define MAX_RECEIVE_SPINS 1000000
#define MAX_RETRY_ROUNDS 3

/* ----------------------------------------------------------------------------
 * File identifiers used with protocol message header
 */
#define FILE_WEIGHTS 0x01
#define FILE_TOKENIZER 0x02

#endif