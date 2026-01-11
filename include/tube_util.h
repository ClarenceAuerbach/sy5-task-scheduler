#ifndef TUBE_UTIL_H
#define TUBE_UTIL_H

#include <stdint.h>
#include "str_util.h"
#include "task.h"

#define U16(a,b) ((uint16_t)((a) << 8 | (b))) // Little endian

// Opcodes pour les requêtes
#define OP_LIST            U16('L','S')
#define OP_CREATE          U16('C','R')
#define OP_COMBINE         U16('C','B')
#define OP_TIMES_EXITCODES U16('T','X')
#define OP_REMOVE          U16('R','M')
#define OP_STDOUT          U16('S','O')
#define OP_STDERR          U16('S','E')
#define OP_TERMINATE       U16('T','M')

// Codes de réponses
#define ANS_OK             U16('O','K')
#define ANS_ERROR          U16('E','R')

// Codes d'erreur
#define ERR_NOT_FOUND      U16('N','F')
#define ERR_NOT_RUN        U16('N','R')
#define ERR_CANNOT_CREATE  U16('N','C')

// Type de combinaison
#define COMBINE_SEQUENTIAL U16('S','Q')

/**
 * Write data to a pipe atomically in chunks
 * @param fd pipe file descriptor
 * @param data data to write
 * @param len length of data
 * @return 0 on success, -1 on error
 */
int write_atomic_chunks(int fd, uint8_t *data, size_t len);

/**
 * Write a uint16 in big-endian to a string_t
 * @param msg destination string_t
 * @param val value to write
 * @return 0 on success, -1 on error
 */
int write16(buffer_t *msg, uint16_t val);

/**
 * Write a uint32 in big-endian to a string_t
 * @param msg destination string_t
 * @param val value to write
 * @return 0 on success, -1 on error
 */
int write32(buffer_t *msg, uint32_t val);

/**
 * Write a uint64 in big-endian to a string_t
 * @param msg destination string_t
 * @param val value to write
 * @return 0 on success, -1 on error
 */
int write64(buffer_t *msg, uint64_t val);

/**
 * Parse a comma-separated string of numbers and build a bitmap
 * Example: "0,3,6,9" -> bitmap with bits 0,3,6,9 set to 1
 * Example: "*" -> all bits set to 1
 * @param str string to parse
 * @param max_value maximum allowed value
 * @return resulting bitmap
 */
uint64_t parse_timing_field(const char *str, int max_value);

/**
 * Write a <timing> structure to a string_t according to the protocol
 * Format: MINUTES <uint64>, HOURS <uint32>, DAYSOFWEEK <uint8>
 * @param msg destination string_t
 * @param minutes string representing minutes (e.g. "0,30" or "*")
 * @param hours string representing hours (e.g. "9,14" or "*")
 * @param days string representing days (e.g. "1" for Monday, "*" for all)
 * @param no_timing if 1, all fields are set to 0 (task without timing)
 * @return 0 on success, -1 on error
 */
int write_timing(buffer_t *msg, const char *minutes, const char *hours, 
                 const char *days, int no_timing);

/**
 * Write <arguments> structure to a string_t according to the protocol
 * Format: ARGC <uint32>, then for each argument: LENGTH <uint32>, DATA <bytes>
 * @param msg destination string_t
 * @param argc number of arguments
 * @param argv array of arguments
 * @return 0 on success, -1 on error
 */
int write_arguments(buffer_t *msg, int argc, char **argv);

/**
 * Open communication pipes with the erraid daemon
 * @param pipes_dir directory containing the pipes (NULL for default)
 * @param req_fd pointer to store the request pipe fd
 * @param rep_fd pointer to store the reply pipe fd
 * @return 0 on success, -1 on error
 */
int open_pipes(const char *pipes_dir, int *req_fd, int *rep_fd);

/**
 * Read a uint16 in big-endian from a file descriptor
 * @param fd file descriptor
 * @param val pointer to store the read value
 * @return 0 on success, -1 on error
 */
int read16(int fd, uint16_t *val);

/**
 * Read a uint32 in big-endian from a file descriptor
 * @param fd file descriptor
 * @param val pointer to store the read value
 * @return 0 on success, -1 on error
 */
int read32(int fd, uint32_t *val);

/**
 * Read a uint64 in big-endian from a file descriptor
 * @param fd file descriptor
 * @param val pointer to store the read value
 * @return 0 on success, -1 on error
 */
int read64(int fd, uint64_t *val);

/**
 * Read a command from a file descriptor into a string_t
 * @param fd file descriptor
 * @param result destination string_t
 * @return 0 on success, -1 on error
 */
int read_command(int fd, string_t *result);

/**
 * Write a command structure to a buffer_t according to the protocol
 * @param msg destination buffer_t
 * @param cmd command structure to write
 * @return 0 on success, -1 on error
 */
int write_command(buffer_t *msg, command_t *cmd) ;

/**
 * Converts timing or bitmap to a readable string
 * @param bitmap th ebitmap in uint64_t
 * @param buf destination buffer for the string
 * @return 0 on success
 */
void bitmap_to_string(uint64_t bitmap, int max_val, char *buf, size_t bufsize);

uint64_t str_min_to_bitmap(const char *s);

uint32_t str_hours_to_bitmap(const char *s) ;

uint8_t str_days_to_bitmap(const char *s);

#endif
