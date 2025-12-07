#ifndef TADMOR_UTIL_H
#define TADMOR_UTIL_H

#include <stdint.h>
#include "str_util.h"

// Opcodes pour les requêtes
#define OP_LIST            0x4c53  // 'LS'
#define OP_CREATE          0x4352  // 'CR'
#define OP_COMBINE         0x4342  // 'CB'
#define OP_REMOVE          0x524d  // 'RM'
#define OP_TIMES_EXITCODES 0x5458  // 'TX'
#define OP_STDOUT          0x534f  // 'SO'
#define OP_STDERR          0x5345  // 'SE'
#define OP_TERMINATE       0x544d  // 'TM'

// Types de réponse
#define ANS_OK    0x4f4b  // 'OK'
#define ANS_ERROR 0x4552  // 'ER'

// Codes d'erreur
#define ERR_NOT_FOUND 0x4e46  // 'NF'
#define ERR_NOT_RUN   0x4e52  // 'NR'

// Type de combinaison
#define COMBINE_SEQUENTIAL 0x5351  // 'SQ'


/**
 * Write data to a pipe atomically in chunks
 * @param fd pipe file descriptor
 * @param data data to write
 * @param len length of data
 * @return 0 on success, -1 on error
 */
int write_atomic_chunks(int fd, char *data, size_t len);

/**
 * Write a uint16 in big-endian to a string_t
 * @param msg destination string_t
 * @param val value to write
 * @return 0 on success, -1 on error
 */
int write_uint16(string_t *msg, uint16_t val);

/**
 * Write a uint32 in big-endian to a string_t
 * @param msg destination string_t
 * @param val value to write
 * @return 0 on success, -1 on error
 */
int write_uint32(string_t *msg, uint32_t val);

/**
 * Write a uint64 in big-endian to a string_t
 * @param msg destination string_t
 * @param val value to write
 * @return 0 on success, -1 on error
 */
int write_uint64(string_t *msg, uint64_t val);

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
int write_timing(string_t *msg, const char *minutes, const char *hours, 
                 const char *days, int no_timing);

/**
 * Write <arguments> structure to a string_t according to the protocol
 * Format: ARGC <uint32>, then for each argument: LENGTH <uint32>, DATA <bytes>
 * @param msg destination string_t
 * @param argc number of arguments
 * @param argv array of arguments
 * @return 0 on success, -1 on error
 */
int write_arguments(string_t *msg, int argc, char **argv);

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
int read_uint16(int fd, uint16_t *val);

/**
 * Read a uint32 in big-endian from a file descriptor
 * @param fd file descriptor
 * @param val pointer to store the read value
 * @return 0 on success, -1 on error
 */
int read_uint32(int fd, uint32_t *val);

/**
 * Read a uint64 in big-endian from a file descriptor
 * @param fd file descriptor
 * @param val pointer to store the read value
 * @return 0 on success, -1 on error
 */
int read_uint64(int fd, uint64_t *val);

#endif // TADMOR_UTIL_H