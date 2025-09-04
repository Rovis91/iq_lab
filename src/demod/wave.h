#ifndef IQ_LAB_WAVE_H
#define IQ_LAB_WAVE_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// WAV file format structures (RIFF)
typedef struct {
    char riff_id[4];       // "RIFF"
    uint32_t file_size;    // File size - 8
    char wave_id[4];       // "WAVE"
} wave_riff_header_t;

typedef struct {
    char chunk_id[4];      // "fmt "
    uint32_t chunk_size;   // 16 for PCM
    uint16_t format_tag;   // 1 = PCM
    uint16_t channels;     // 1 = mono, 2 = stereo
    uint32_t sample_rate;  // Samples per second
    uint32_t byte_rate;    // sample_rate * channels * bits_per_sample / 8
    uint16_t block_align;  // channels * bits_per_sample / 8
    uint16_t bits_per_sample; // 16 for our use case
} wave_format_chunk_t;

typedef struct {
    char chunk_id[4];      // "data"
    uint32_t chunk_size;   // Number of bytes in data
    // Audio data follows...
} wave_data_chunk_t;

// WAV writer context
typedef struct {
    FILE *file;
    uint32_t sample_rate;
    uint16_t channels;     // 1 = mono, 2 = stereo
    uint32_t total_samples; // Total samples written
    uint32_t data_size;    // Size of data chunk in bytes
} wave_writer_t;

// Initialize WAV writer
bool wave_writer_init(wave_writer_t *writer, const char *filename,
                     uint32_t sample_rate, uint16_t channels);

// Write audio samples (16-bit PCM, interleaved if stereo)
bool wave_writer_write_samples(wave_writer_t *writer,
                              const int16_t *samples, uint32_t num_samples);

// Close WAV writer and finalize headers
bool wave_writer_close(wave_writer_t *writer);

// Utility functions
uint32_t wave_calculate_data_size(uint32_t num_samples, uint16_t channels);
uint32_t wave_calculate_file_size(uint32_t data_size);

#endif // IQ_LAB_WAVE_H
