#ifndef IQ_IO_IQ_H
#define IQ_IO_IQ_H

#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// IQ data format enumeration
typedef enum {
    IQ_FORMAT_S8,   // 8-bit signed integers
    IQ_FORMAT_S16   // 16-bit signed integers
} iq_format_t;

// IQ data structure
typedef struct {
    float *data;        // Interleaved I/Q samples as float [-1,1]
    size_t num_samples; // Number of complex samples (not bytes)
    uint32_t sample_rate; // Samples per second
    iq_format_t format; // Original data format
} iq_data_t;

// File reading context for streaming large files
typedef struct {
    FILE *file;         // File handle
    iq_format_t format; // Data format
    size_t buffer_size; // Size of internal buffer in samples
    size_t bytes_read;  // Total bytes read so far
    bool eof;          // End of file reached
} iq_reader_t;

/*
 * Initialize IQ reader for streaming file reading
 * This prevents loading entire large IQ files into memory at once
 */
bool iq_reader_init(iq_reader_t *reader, const char *filename, iq_format_t format);

/*
 * Read next block of IQ samples into provided buffer
 * Returns number of complex samples read (not bytes)
 * Buffer should be pre-allocated with at least 'max_samples' capacity
 */
size_t iq_read_samples(iq_reader_t *reader, float *buffer, size_t max_samples);

/*
 * Close reader and free resources
 */
void iq_reader_close(iq_reader_t *reader);

/*
 * Load entire IQ file into memory (for small files only)
 * Automatically detects format from file extension or content
 */
bool iq_load_file(const char *filename, iq_data_t *iq_data);

/*
 * Free IQ data structure and its internal buffer
 */
void iq_free(iq_data_t *iq_data);

/*
 * Convert raw IQ bytes to normalized float array [-1,1]
 * Handles both s8 and s16 formats with proper scaling
 */
bool iq_convert_to_float(const uint8_t *raw_data, size_t num_bytes,
                        iq_format_t format, float *output, size_t max_samples);

/*
 * Get file size in bytes
 */
size_t iq_get_file_size(const char *filename);

/*
 * Auto-detect IQ format from file content
 * Examines first few bytes to determine s8 vs s16
 */
iq_format_t iq_detect_format(const char *filename);

#endif // IQ_IO_IQ_H
