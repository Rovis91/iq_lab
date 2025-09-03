#include "io_iq.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>

// WAV file header structure
typedef struct {
    char riff[4];           // "RIFF"
    uint32_t file_size;     // File size - 8
    char wave[4];           // "WAVE"
    char fmt[4];            // "fmt "
    uint32_t fmt_size;      // Format chunk size
    uint16_t format;        // Audio format (1 = PCM)
    uint16_t channels;      // Number of channels
    uint32_t sample_rate;   // Sample rate
    uint32_t byte_rate;     // Byte rate
    uint16_t block_align;   // Block align
    uint16_t bits_per_sample; // Bits per sample
    char data[4];           // "data"
    uint32_t data_size;     // Data size
} wav_header_t;

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

// Forward declarations
bool iq_load_wav_file(const char *filename, iq_data_t *iq_data);

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

    // Check if it's a WAV file first
    if (strstr(filename, ".wav") || strstr(filename, ".WAV")) {
        return iq_load_wav_file(filename, iq_data);
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

// Load WAV file and convert to IQ format
bool iq_load_wav_file(const char *filename, iq_data_t *iq_data) {
    if (!filename || !iq_data) {
        return false;
    }

    FILE *file = fopen(filename, "rb");
    if (!file) {
        fprintf(stderr, "Error opening WAV file '%s': %s\n", filename, strerror(errno));
        return false;
    }

    // Read WAV header
    wav_header_t header;
    if (fread(&header, sizeof(header), 1, file) != 1) {
        fprintf(stderr, "Error reading WAV header from '%s'\n", filename);
        fclose(file);
        return false;
    }

    // Validate WAV header
    if (memcmp(header.riff, "RIFF", 4) != 0 ||
        memcmp(header.wave, "WAVE", 4) != 0 ||
        memcmp(header.fmt, "fmt ", 4) != 0 ||
        memcmp(header.data, "data", 4) != 0) {
        fprintf(stderr, "Invalid WAV header in '%s'\n", filename);
        fclose(file);
        return false;
    }

    if (header.format != 1) {
        fprintf(stderr, "Unsupported WAV format: %u (only PCM supported)\n", header.format);
        fclose(file);
        return false;
    }

    if (header.bits_per_sample != 16) {
        fprintf(stderr, "Unsupported bit depth: %u (only 16-bit supported)\n", header.bits_per_sample);
        fclose(file);
        return false;
    }

    if (header.channels > 2) {
        fprintf(stderr, "Unsupported channel count: %u (mono/stereo only)\n", header.channels);
        fclose(file);
        return false;
    }

    printf("WAV: %d channels, %d Hz, %d bits, %d samples\n",
           header.channels, header.sample_rate, header.bits_per_sample,
           header.data_size / (header.channels * header.bits_per_sample / 8));

    // Calculate number of samples
    size_t bytes_per_sample = header.channels * header.bits_per_sample / 8;
    size_t num_samples = header.data_size / bytes_per_sample;

    if (num_samples == 0) {
        fprintf(stderr, "WAV file contains no audio data\n");
        fclose(file);
        return false;
    }

    // Allocate memory for IQ data (always 2 floats per complex sample)
    iq_data->data = (float *)malloc(num_samples * 2 * sizeof(float));
    if (!iq_data->data) {
        fprintf(stderr, "Memory allocation failed for WAV IQ data\n");
        fclose(file);
        return false;
    }

    // Allocate buffer for WAV samples
    size_t buffer_size = num_samples * header.channels;
    int16_t *wav_buffer = (int16_t *)malloc(buffer_size * sizeof(int16_t));
    if (!wav_buffer) {
        fprintf(stderr, "Memory allocation failed for WAV buffer\n");
        free(iq_data->data);
        iq_data->data = NULL;
        fclose(file);
        return false;
    }

    // Read WAV data
    size_t samples_read = fread(wav_buffer, sizeof(int16_t), buffer_size, file);
    fclose(file);

    if (samples_read != buffer_size) {
        fprintf(stderr, "Incomplete WAV data read: expected %zu samples, got %zu\n",
                buffer_size, samples_read);
        free(wav_buffer);
        free(iq_data->data);
        iq_data->data = NULL;
        return false;
    }

    // Convert WAV to IQ format
    for (size_t i = 0; i < num_samples; i++) {
        if (header.channels == 1) {
            // Mono: I = audio sample, Q = 0
            iq_data->data[i * 2] = (float)wav_buffer[i] / 32768.0f;     // I component
            iq_data->data[i * 2 + 1] = 0.0f;                           // Q component
        } else {
            // Stereo: left = I, right = Q
            iq_data->data[i * 2] = (float)wav_buffer[i * 2] / 32768.0f;       // I component
            iq_data->data[i * 2 + 1] = (float)wav_buffer[i * 2 + 1] / 32768.0f; // Q component
        }
    }

    free(wav_buffer);

    // Fill metadata
    iq_data->num_samples = num_samples;
    iq_data->format = IQ_FORMAT_S16; // WAV was 16-bit
    iq_data->sample_rate = header.sample_rate;

    printf("✅ Converted WAV to IQ: %zu complex samples\n", num_samples);
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
    if (!filename) {
        return IQ_FORMAT_S16; // Safe default
    }

    // First check file extension hints
    if (strstr(filename, ".wav") || strstr(filename, ".WAV")) {
        return IQ_FORMAT_S16; // WAV files are typically 16-bit
    }

    if (strstr(filename, ".s16") || strstr(filename, ".iq16")) {
        return IQ_FORMAT_S16;
    }

    if (strstr(filename, ".s8") || strstr(filename, ".iq8")) {
        return IQ_FORMAT_S8;
    }

    // For files without explicit format hints, analyze content
    size_t file_size = iq_get_file_size(filename);
    if (file_size == 0) {
        return IQ_FORMAT_S16; // Safe default
    }

    // Analyze file structure based on size
    if (file_size % 4 == 0) {
        // File size is multiple of 4 bytes - likely s16 format
        // Verify by examining first few samples
        FILE *file = fopen(filename, "rb");
        if (file) {
            int16_t samples[8]; // Check first 4 complex samples (8 values)
            size_t bytes_read = fread(samples, sizeof(int16_t), 8, file);
            fclose(file);

            if (bytes_read >= 8) {
                // Check if values are in reasonable range for IQ data
                bool reasonable_range = true;
                int max_abs_value = 0;

                for (int i = 0; i < 8; i++) {
                    int abs_val = abs(samples[i]);
                    if (abs_val > max_abs_value) max_abs_value = abs_val;

                    // Check for saturation or unusual values
                    if (abs_val > 32760) { // Close to max 16-bit value
                        reasonable_range = false;
                        break;
                    }
                }

                // If max value is reasonable for IQ data, confirm s16
                if (reasonable_range && max_abs_value > 1000) { // Some minimum signal level
                    return IQ_FORMAT_S16;
                }
            }
        }
    }

    if (file_size % 2 == 0) {
        // File size is multiple of 2 bytes - could be s8 format
        // Verify by examining first few samples
        FILE *file = fopen(filename, "rb");
        if (file) {
            int8_t samples[8]; // Check first 4 complex samples (8 values)
            size_t bytes_read = fread(samples, sizeof(int8_t), 8, file);
            fclose(file);

            if (bytes_read >= 8) {
                // Check if values are in reasonable range for IQ data
                bool reasonable_range = true;
                int max_abs_value = 0;

                for (int i = 0; i < 8; i++) {
                    int abs_val = abs(samples[i]);
                    if (abs_val > max_abs_value) max_abs_value = abs_val;

                    // Check for saturation or unusual values
                    if (abs_val > 126) { // Close to max 8-bit value
                        reasonable_range = false;
                        break;
                    }
                }

                // If max value is reasonable for IQ data, confirm s8
                if (reasonable_range && max_abs_value > 10) { // Some minimum signal level
                    return IQ_FORMAT_S8;
                }
            }
        }
    }

    // Fallback: analyze statistical properties of the data
    FILE *file = fopen(filename, "rb");
    if (file) {
        // Read first 1024 samples (or whatever fits)
        const size_t max_samples = 1024;
        int16_t buffer[max_samples];
        size_t bytes_read = fread(buffer, 1, sizeof(buffer), file);
        fclose(file);

        if (bytes_read >= 16) { // At least a few samples
            size_t num_samples = bytes_read / 2; // Assume 16-bit for analysis
            double mean = 0.0;
            double variance = 0.0;

            // Calculate mean
            for (size_t i = 0; i < num_samples; i++) {
                mean += (double)buffer[i];
            }
            mean /= (double)num_samples;

            // Calculate variance
            for (size_t i = 0; i < num_samples; i++) {
                double diff = (double)buffer[i] - mean;
                variance += diff * diff;
            }
            variance /= (double)num_samples;

            // IQ data typically has:
            // - Near-zero mean (DC-balanced)
            // - High variance (signal content)
            // - Symmetric distribution around zero

            double std_dev = sqrt(variance);
            double mean_ratio = fabs(mean) / std_dev;

            // If mean is much smaller than standard deviation, likely IQ data
            if (mean_ratio < 0.1) { // Mean < 10% of standard deviation
                // Determine bit depth based on range
                int max_abs = 0;
                for (size_t i = 0; i < num_samples; i++) {
                    int abs_val = abs(buffer[i]);
                    if (abs_val > max_abs) max_abs = abs_val;
                }

                // If values use most of 16-bit range, likely s16
                if (max_abs > 10000) { // > 30% of 16-bit range
                    return IQ_FORMAT_S16;
                } else {
                    return IQ_FORMAT_S8;
                }
            }
        }
    }

    // Final fallback
    return IQ_FORMAT_S16;
}
