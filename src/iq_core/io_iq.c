#include "io_iq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>

/*
 * IQ data is stored as interleaved I/Q samples:
 * [I0, Q0, I1, Q1, I2, Q2, ...]
 *
 * Raw formats:
 * - s8: 8-bit signed integers (-128 to +127)
 * - s16: 16-bit signed integers (-32768 to +32767)
 *
 * We convert to float [-1,1] for DSP operations
 */

// Internal buffer size for streaming (4MB worth of complex samples)
#define IQ_BUFFER_SIZE (1024 * 1024)  // 1M complex samples = 4MB for s16

bool iq_reader_init(iq_reader_t *reader, const char *filename, iq_format_t format) {
    if (!reader || !filename) {
        return false;
    }

    // Open file in binary mode for raw IQ data
    reader->file = fopen(filename, "rb");
    if (!reader->file) {
        fprintf(stderr, "Error opening IQ file '%s': %s\n", filename, strerror(errno));
        return false;
    }

    reader->format = format;
    reader->buffer_size = IQ_BUFFER_SIZE;
    reader->bytes_read = 0;
    reader->eof = false;

    return true;
}

size_t iq_read_samples(iq_reader_t *reader, float *buffer, size_t max_samples) {
    if (!reader || !reader->file || !buffer || reader->eof) {
        return 0;
    }

    // Calculate bytes per complex sample (2 values: I and Q)
    size_t bytes_per_sample = (reader->format == IQ_FORMAT_S8) ? 2 : 4;
    size_t max_bytes = max_samples * bytes_per_sample;

    // Allocate temporary buffer for raw bytes
    uint8_t *raw_buffer = (uint8_t *)malloc(max_bytes);
    if (!raw_buffer) {
        fprintf(stderr, "Memory allocation failed for raw IQ buffer\n");
        return 0;
    }

    // Read raw bytes from file
    size_t bytes_to_read = max_bytes;
    size_t bytes_read = fread(raw_buffer, 1, bytes_to_read, reader->file);

    if (bytes_read == 0) {
        if (feof(reader->file)) {
            reader->eof = true;
        }
        free(raw_buffer);
        return 0;
    }

    // Convert bytes to float samples
    size_t samples_read = bytes_read / bytes_per_sample;
    bool conversion_ok = iq_convert_to_float(raw_buffer, bytes_read,
                                           reader->format, buffer, samples_read);

    free(raw_buffer);

    if (!conversion_ok) {
        fprintf(stderr, "IQ conversion failed\n");
        return 0;
    }

    reader->bytes_read += bytes_read;
    return samples_read;
}

void iq_reader_close(iq_reader_t *reader) {
    if (reader && reader->file) {
        fclose(reader->file);
        reader->file = NULL;
        reader->eof = true;
    }
}

bool iq_load_file(const char *filename, iq_data_t *iq_data) {
    if (!filename || !iq_data) {
        return false;
    }

    // Get file size
    size_t file_size = iq_get_file_size(filename);
    if (file_size == 0) {
        return false;
    }

    // Auto-detect format
    iq_format_t format = iq_detect_format(filename);
    size_t bytes_per_sample = (format == IQ_FORMAT_S8) ? 2 : 4;
    size_t num_samples = file_size / bytes_per_sample;

    if (num_samples == 0) {
        fprintf(stderr, "File too small to contain IQ data\n");
        return false;
    }

    // Allocate memory for float data (2 floats per complex sample)
    iq_data->data = (float *)malloc(num_samples * 2 * sizeof(float));
    if (!iq_data->data) {
        fprintf(stderr, "Memory allocation failed for IQ data\n");
        return false;
    }

    // Read entire file into memory
    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error opening file '%s': %s\n", filename, strerror(errno));
        free(iq_data->data);
        iq_data->data = NULL;
        return false;
    }

    // Allocate temporary buffer for raw data
    uint8_t *raw_data = (uint8_t *)malloc(file_size);
    if (!raw_data) {
        fprintf(stderr, "Memory allocation failed for raw data buffer\n");
        fclose(file);
        free(iq_data->data);
        iq_data->data = NULL;
        return false;
    }

    size_t bytes_read = fread(raw_data, 1, file_size, file);
    fclose(file);

    if (bytes_read != file_size) {
        fprintf(stderr, "Incomplete file read: expected %zu bytes, got %zu\n",
                file_size, bytes_read);
        free(raw_data);
        free(iq_data->data);
        iq_data->data = NULL;
        return false;
    }

    // Convert to float
    bool conversion_ok = iq_convert_to_float(raw_data, file_size, format,
                                           iq_data->data, num_samples);

    free(raw_data);

    if (!conversion_ok) {
        free(iq_data->data);
        iq_data->data = NULL;
        return false;
    }

    // Fill metadata
    iq_data->num_samples = num_samples;
    iq_data->format = format;
    iq_data->sample_rate = 0;  // Will be set from SigMF metadata

    return true;
}

void iq_free(iq_data_t *iq_data) {
    if (iq_data && iq_data->data) {
        free(iq_data->data);
        iq_data->data = NULL;
        iq_data->num_samples = 0;
    }
}

bool iq_convert_to_float(const uint8_t *raw_data, size_t num_bytes,
                        iq_format_t format, float *output, size_t max_samples) {

    if (!raw_data || !output) {
        return false;
    }

    size_t bytes_per_sample = (format == IQ_FORMAT_S8) ? 2 : 4;
    size_t expected_samples = num_bytes / bytes_per_sample;

    if (expected_samples > max_samples) {
        fprintf(stderr, "Buffer too small: need %zu samples, have %zu\n",
                expected_samples, max_samples);
        return false;
    }

    if (format == IQ_FORMAT_S8) {
        // Convert s8 to float [-1,1]
        const int8_t *s8_data = (const int8_t *)raw_data;

        for (size_t i = 0; i < expected_samples; i++) {
            // I component
            output[i * 2] = (float)s8_data[i * 2] / 128.0f;
            // Q component
            output[i * 2 + 1] = (float)s8_data[i * 2 + 1] / 128.0f;
        }
    } else if (format == IQ_FORMAT_S16) {
        // Convert s16 to float [-1,1]
        const int16_t *s16_data = (const int16_t *)raw_data;

        for (size_t i = 0; i < expected_samples; i++) {
            // I component
            output[i * 2] = (float)s16_data[i * 2] / 32768.0f;
            // Q component
            output[i * 2 + 1] = (float)s16_data[i * 2 + 1] / 32768.0f;
        }
    } else {
        fprintf(stderr, "Unsupported IQ format\n");
        return false;
    }

    return true;
}

size_t iq_get_file_size(const char *filename) {
    if (!filename) {
        return 0;
    }

    FILE *file = fopen(filename, "rb");
    if (!file) {
        return 0;
    }

    // Seek to end to get file size
    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    long size = ftell(file);
    fclose(file);

    return (size < 0) ? 0 : (size_t)size;
}

iq_format_t iq_detect_format(const char *filename) {
    // For now, assume s16 if file extension suggests it
    // In a real implementation, we'd examine the first few samples
    // to determine the range and detect the format automatically

    if (strstr(filename, ".s16") || strstr(filename, ".iq16")) {
        return IQ_FORMAT_S16;
    }

    // Default to s8 for most SDR applications
    return IQ_FORMAT_S8;
}
