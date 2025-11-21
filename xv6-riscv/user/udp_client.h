#ifndef UDP_CLIENT_H
#define UDP_CLIENT_H

#include "kernel/types.h"
#include <stdint.h>

// Base protocol functions
int llm_meta_request(uint8_t file_id, uint32_t *file_size, uint32_t *total_chunks, unsigned char *file_hash);
int llm_data_range_request(uint8_t file_id, uint32_t start_idx, uint16_t count);
int llm_retrans_request(uint8_t file_id, uint32_t *indices, uint16_t count);
char* llm_fetch_file(uint8_t file_id, int *size_out);

// LLM-specific wrapper functions
char* fetch_model_weights(int *size_out);
char* fetch_tokenizer(int *size_out);

// Protocol message types
#define MSG_META_REQ      0x01
#define MSG_META_RESP     0x02
#define MSG_DATA_RANGE_REQ 0x03
#define MSG_RETRANS_REQ   0x04
#define MSG_DATA_PACKET   0x05
#define MSG_ERROR         0x06

// Constants from the FTP specification
#define SERVER_PORT 9999
#define SERVER_IP ((10 << 24) | (0 << 16) | (2 << 8) | 2)  // 10.0.2.2
#define CHUNK_SIZE 512
#define MAX_RANGE 16
#define MAX_RETRANS 32
#define MAX_RECEIVE_SPINS 1000000
#define MAX_RETRY_ROUNDS 3

// File identifiers
#define FILE_WEIGHTS 0x01
#define FILE_TOKENIZER 0x02

#endif