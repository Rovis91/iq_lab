#ifndef IQ_FILE_UTILS_H
#define IQ_FILE_UTILS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/*
 * File manipulation utilities
 * Common functions used by converters
 */

// Copy source file to destination
bool file_copy(const char *source_path, const char *dest_path);

// Get file size in bytes
size_t get_file_size(const char *filepath);

// Check if file exists
bool file_exists(const char *filepath);

// Create necessary directories for a file path
bool create_parent_directories(const char *filepath);

// Generate unique temporary filename
bool generate_temp_filename(char *buffer, size_t buffer_size, const char *prefix);

// Safely remove a file
bool safe_remove_file(const char *filepath);

// Calculate simple file checksum
uint32_t calculate_file_checksum(const char *filepath);

#endif // IQ_FILE_UTILS_H
