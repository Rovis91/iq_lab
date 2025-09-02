#include "wav_converter.h"
#include "../utils/file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <stdint.h>
#include <stdbool.h>

// WAV file format structures and constants
#define WAV_FORMAT_PCM 1
#define RIFF_CHUNK_SIZE 12
#define RF64_DATA_SIZE_OFFSET 16

// Structure for WAV/RF64 header
typedef struct {
    char riff_id[4];       // "RIFF" or "RF64"
    uint32_t file_size;    // File size (or 0xFFFFFFFF for RF64)
    char wave_id[4];       // "WAVE"

    // Format chunk
    char fmt_id[4];        // "fmt "
    uint32_t fmt_size;     // Format chunk size
    uint16_t format;       // Audio format (1=PCM)
    uint16_t channels;     // Number of channels
    uint32_t sample_rate;  // Sample rate
    uint32_t byte_rate;    // Bytes per second
    uint16_t block_align;  // Bytes per sample * channels
    uint16_t bits_per_sample; // Bits per sample

    // Data chunk
    char data_id[4];       // "data" or "ds64"
    uint64_t data_size;    // Data size (64-bit for RF64)
} wav_header_t;

// Structure for reading WAV IQ files
typedef struct {
    FILE *file;
    wav_header_t header;
    size_t data_offset;    // Offset of data in file
    size_t bytes_read;
    bool is_rf64;          // True if RF64 format
} wav_reader_t;

// Utility functions for little-endian integers
static uint32_t read_u32_le(const uint8_t *data) {
    return (uint32_t)data[0] |
           ((uint32_t)data[1] << 8) |
           ((uint32_t)data[2] << 16) |
           ((uint32_t)data[3] << 24);
}

static uint16_t read_u16_le(const uint8_t *data) {
    return (uint16_t)data[0] | ((uint16_t)data[1] << 8);
}

static uint64_t read_u64_le(const uint8_t *data) {
    return (uint64_t)data[0] |
           ((uint64_t)data[1] << 8) |
           ((uint64_t)data[2] << 16) |
           ((uint64_t)data[3] << 24) |
           ((uint64_t)data[4] << 32) |
           ((uint64_t)data[5] << 40) |
           ((uint64_t)data[6] << 48) |
           ((uint64_t)data[7] << 56);
}

// Utility function to write little-endian integers
static void write_u32_le(uint8_t *data, uint32_t value) {
    data[0] = (uint8_t)(value & 0xFF);
    data[1] = (uint8_t)((value >> 8) & 0xFF);
    data[2] = (uint8_t)((value >> 16) & 0xFF);
    data[3] = (uint8_t)((value >> 24) & 0xFF);
}

// WAV reader functions (moved from io_wav.c)
bool wav_reader_open(wav_reader_t *reader, const char *filename) {
    if (!reader || !filename) {
        return false;
    }

    // Open file
    reader->file = fopen(filename, "rb");
    if (!reader->file) {
        fprintf(stderr, "Error opening WAV file '%s': %s\n", filename, strerror(errno));
        return false;
    }

    // Read header
    uint8_t header_data[128];
    size_t bytes_read = fread(header_data, 1, sizeof(header_data), reader->file);

    if (bytes_read < 12) {
        fprintf(stderr, "File too small to be a valid WAV\n");
        fclose(reader->file);
        return false;
    }

    // Detect format (RIFF or RF64)
    if (memcmp(header_data, "RIFF", 4) == 0) {
        reader->is_rf64 = false;
        reader->header.file_size = read_u32_le(header_data + 4);
    } else if (memcmp(header_data, "RF64", 4) == 0) {
        reader->is_rf64 = true;
        reader->header.file_size = 0xFFFFFFFF; // Special RF64 value
    } else {
        fprintf(stderr, "Unrecognized format - expected RIFF or RF64\n");
        fclose(reader->file);
        return false;
    }

    // Verify it's a WAVE file
    if (memcmp(header_data + 8, "WAVE", 4) != 0) {
        fprintf(stderr, "Non-WAVE format detected\n");
        fclose(reader->file);
        return false;
    }

    // Read format chunk
    size_t offset = RIFF_CHUNK_SIZE;

    while (offset < bytes_read - 8) {
        const char *chunk_id = (const char *)header_data + offset;
        uint32_t chunk_size = read_u32_le(header_data + offset + 4);

        if (memcmp(chunk_id, "fmt ", 4) == 0) {
            // Format chunk found
            memcpy(reader->header.fmt_id, chunk_id, 4);
            reader->header.fmt_size = chunk_size;

            if (chunk_size >= 16) {
                reader->header.format = read_u16_le(header_data + offset + 8);
                reader->header.channels = read_u16_le(header_data + offset + 10);
                reader->header.sample_rate = read_u32_le(header_data + offset + 12);
                reader->header.byte_rate = read_u32_le(header_data + offset + 16);
                reader->header.block_align = read_u16_le(header_data + offset + 20);
                reader->header.bits_per_sample = read_u16_le(header_data + offset + 22);
            }
            offset += 8 + chunk_size;
        } else if (memcmp(chunk_id, "data", 4) == 0) {
            // Data chunk found (standard format)
            memcpy(reader->header.data_id, chunk_id, 4);
            reader->header.data_size = chunk_size;
            reader->data_offset = offset + 8;
            break;
        } else if (memcmp(chunk_id, "ds64", 4) == 0 && reader->is_rf64) {
            // ds64 chunk for RF64 (64-bit size)
            if (chunk_size >= 24) {
                reader->header.data_size = read_u64_le(header_data + offset + 8);
                memcpy(reader->header.data_id, "ds64", 4);
            }
            offset += 8 + chunk_size;
        } else {
            // Unknown chunk, skip
            offset += 8 + chunk_size;
        }
    }

    // For RF64, look for data chunk after ds64
    if (reader->is_rf64 && memcmp(reader->header.data_id, "ds64", 4) == 0) {
        // Read more data to find the data chunk
        while (offset < bytes_read - 8) {
            const char *chunk_id = (const char *)header_data + offset;
            uint32_t chunk_size = read_u32_le(header_data + offset + 4);

            if (memcmp(chunk_id, "data", 4) == 0) {
                reader->header.data_size = chunk_size; // May be 0xFFFFFFFF
                reader->data_offset = offset + 8;
                break;
            }
            offset += 8 + chunk_size;
        }
    }

    // Final checks
    if (reader->header.format != WAV_FORMAT_PCM) {
        fprintf(stderr, "Unsupported audio format: %d (expected PCM)\n", reader->header.format);
        fclose(reader->file);
        return false;
    }

    if (reader->header.bits_per_sample != 16) {
        fprintf(stderr, "Unsupported resolution: %d bits (expected 16-bit)\n", reader->header.bits_per_sample);
        fclose(reader->file);
        return false;
    }

    reader->bytes_read = 0;
    return true;
}

void wav_reader_close(wav_reader_t *reader) {
    if (reader && reader->file) {
        fclose(reader->file);
        reader->file = NULL;
        memset(reader, 0, sizeof(wav_reader_t));
    }
}

size_t wav_read_samples(wav_reader_t *reader, float *buffer, size_t max_samples) {
    if (!reader || !reader->file || !buffer) {
        return 0;
    }

    // Calculate number of bytes to read
    size_t bytes_per_sample = (reader->header.bits_per_sample / 8) * reader->header.channels;
    size_t max_bytes = max_samples * bytes_per_sample;

    // Allocate temporary buffer
    uint8_t *raw_buffer = (uint8_t *)malloc(max_bytes);
    if (!raw_buffer) {
        fprintf(stderr, "Memory allocation failed for WAV reading\n");
        return 0;
    }

    // Seek to correct offset if necessary
    if (reader->bytes_read == 0 && reader->data_offset > 0) {
        if (fseek(reader->file, (long)reader->data_offset, SEEK_SET) != 0) {
            free(raw_buffer);
            return 0;
        }
    }

    // Read data
    size_t bytes_to_read = max_bytes;
    size_t bytes_read = fread(raw_buffer, 1, bytes_to_read, reader->file);

    if (bytes_read == 0) {
        free(raw_buffer);
        return 0;
    }

    // Convert to float according to channel format
    size_t samples_read = bytes_read / bytes_per_sample;

    if (reader->header.channels == 2) {
        // Stereo format: left channel = I, right channel = Q
        const int16_t *samples = (const int16_t *)raw_buffer;

        for (size_t i = 0; i < samples_read; i++) {
            buffer[i * 2] = (float)samples[i * 2] / 32768.0f;     // I (left channel)
            buffer[i * 2 + 1] = (float)samples[i * 2 + 1] / 32768.0f; // Q (right channel)
        }
    } else if (reader->header.channels == 1) {
        // Mono format: treat as interleaved IQ data
        const int16_t *samples = (const int16_t *)raw_buffer;

        for (size_t i = 0; i < samples_read; i += 2) {
            buffer[i] = (float)samples[i] / 32768.0f;         // I
            buffer[i + 1] = (float)samples[i + 1] / 32768.0f; // Q
        }
        samples_read /= 2; // Each I/Q pair counts as one complex sample
    } else {
        fprintf(stderr, "Unsupported number of channels: %d\n", reader->header.channels);
        free(raw_buffer);
        return 0;
    }

    free(raw_buffer);
    reader->bytes_read += bytes_read;

    return samples_read;
}

// Additional utility functions from io_wav.c
bool wav_analyze_header(const char *filename, wav_header_t *header) {
    wav_reader_t reader = {0};

    if (!wav_reader_open(&reader, filename)) {
        return false;
    }

    memcpy(header, &reader.header, sizeof(wav_header_t));
    wav_reader_close(&reader);

    return true;
}

bool wav_is_iq_format(const wav_header_t *header) {
    // Criteria for detecting WAV IQ file:
    // 1. PCM format
    // 2. 16 bits per sample (sufficient quality for IQ)
    // 3. 1 or 2 channels (mono = interleaved IQ, stereo = separate I/Q)
    // 4. Realistic sample rate for SDR (typically > 1 MHz for wide bands)

    if (header->format != WAV_FORMAT_PCM) {
        return false;
    }

    if (header->bits_per_sample != 16) {
        return false;
    }

    if (header->channels != 1 && header->channels != 2) {
        return false;
    }

    // Typical sample rates for SDR
    // Very low frequencies suggest regular audio
    if (header->sample_rate < 100000) {  // Less than 100 kHz
        return false;
    }

    return true;
}

bool wav_can_handle(const char *filename) {
    if (!filename) return false;

    // Check by file extension
    const char *ext = strrchr(filename, '.');
    if (ext && (strcmp(ext, ".wav") == 0 || strcmp(ext, ".WAV") == 0)) {
        return true;
    }

    // Check by content (RIFF/RF64 header)
    FILE *file = fopen(filename, "rb");
    if (file) {
        char header[4];
        size_t bytes_read = fread(header, 1, 4, file);
        fclose(file);

        if (bytes_read == 4) {
            if (memcmp(header, "RIFF", 4) == 0 || memcmp(header, "RF64", 4) == 0) {
                return true;
            }
        }
    }

    return false;
}

converter_result_t wav_to_iq(const conversion_request_t *request) {
    converter_result_t result = {0};
    clock_t start_time = clock();

    if (request->options.verbose) {
        printf("WAV to IQ conversion: %s → %s\n", request->input_file, request->output_file);
    }

    // Open WAV file
    wav_reader_t reader = {0};
    if (!wav_reader_open(&reader, request->input_file)) {
        snprintf(result.error_message, sizeof(result.error_message),
                "Cannot open WAV file: %s", request->input_file);
        result.success = false;
        return result;
    }

    // Validate WAV format
    if (!wav_validate_header(&reader.header, result.error_message, sizeof(result.error_message))) {
        wav_reader_close(&reader);
        result.success = false;
        return result;
    }

    if (request->options.verbose) {
        printf("Detected WAV format: %s\n", reader.is_rf64 ? "RF64" : "RIFF");
        printf("Sample rate: %u Hz\n", reader.header.sample_rate);
        printf("Resolution: %d bits\n", reader.header.bits_per_sample);
        printf("Channels: %d\n", reader.header.channels);
        printf("Data size: %llu bytes\n", reader.header.data_size);
    }

    // Create output IQ file
    FILE *iq_file = fopen(request->output_file, "wb");
    if (!iq_file) {
        snprintf(result.error_message, sizeof(result.error_message),
                "Cannot create IQ file: %s (%s)",
                request->output_file, strerror(errno));
        wav_reader_close(&reader);
        result.success = false;
        return result;
    }

    // Configure conversion
    const size_t BUFFER_SIZE = 65536;  // 64 KB of WAV data at a time
    float *float_buffer = (float *)malloc(BUFFER_SIZE * 2 * sizeof(float));
    int16_t *int16_buffer = (int16_t *)malloc(BUFFER_SIZE * 2 * sizeof(int16_t));

    if (!float_buffer || !int16_buffer) {
        snprintf(result.error_message, sizeof(result.error_message),
                "Memory allocation failed");
        free(float_buffer);
        free(int16_buffer);
        fclose(iq_file);
        wav_reader_close(&reader);
        result.success = false;
        return result;
    }

    // Block conversion
    size_t total_samples = 0;
    size_t total_bytes = 0;
    bool conversion_error = false;

    if (request->options.verbose) {
        printf("\nStarting conversion...\n");
        printf("Progress: 0%%");
        fflush(stdout);
    }

    while (!conversion_error) {
        // Read a block of WAV data
        size_t samples_read = wav_read_samples(&reader, float_buffer, BUFFER_SIZE);

        if (samples_read == 0) {
            // End of file
            break;
        }

        // Convert float to int16 for IQ format
        for (size_t i = 0; i < samples_read * 2; i++) {
            // Clip to avoid overflow
            float sample = float_buffer[i];
            if (sample > 1.0f) sample = 1.0f;
            if (sample < -1.0f) sample = -1.0f;

            int16_buffer[i] = (int16_t)(sample * 32767.0f);
        }

        // Write to IQ file
        size_t bytes_to_write = samples_read * 4;  // 4 bytes per complex sample (I+Q)
        size_t bytes_written = fwrite(int16_buffer, 1, bytes_to_write, iq_file);

        if (bytes_written != bytes_to_write) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "Error writing to IQ file");
            conversion_error = true;
            break;
        }

        total_samples += samples_read;
        total_bytes += bytes_written;

        // Display progress
        if (request->options.verbose && total_samples % 10000 == 0) {
            double progress = (double)reader.bytes_read / (double)reader.header.data_size * 100.0;
            printf("\rProgress: %.1f%%", progress);
            fflush(stdout);
        }
    }

    if (request->options.verbose) {
        printf("\rProgress: 100.0%% - Complete!\n");
    }

    // Cleanup
    free(float_buffer);
    free(int16_buffer);
    fclose(iq_file);
    wav_reader_close(&reader);

    // Final result
    if (!conversion_error) {
        result.success = true;
        result.samples_converted = total_samples;
        result.bytes_processed = total_bytes;

        // Input metadata
        result.input_metadata.format = FORMAT_WAV;
        result.input_metadata.sample_rate = reader.header.sample_rate;
        result.input_metadata.num_samples = total_samples;
        result.input_metadata.data_size_bytes = reader.header.data_size;

        // Output metadata
        result.output_metadata.format = (request->output_format == FORMAT_IQ_S8) ? FORMAT_IQ_S8 : FORMAT_IQ_S16;
        result.output_metadata.sample_rate = reader.header.sample_rate;
        result.output_metadata.num_samples = total_samples;
        result.output_metadata.data_size_bytes = total_bytes;

        if (request->options.verbose) {
            printf("\n✅ WAV to IQ conversion successful!\n");
            printf("Samples converted: %zu\n", total_samples);
            printf("Bytes written: %zu\n", total_bytes);
        }
    } else {
        result.success = false;
        // Error message has already been set
    }

    // Calculate time
    clock_t end_time = clock();
    result.conversion_time_seconds = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    return result;
}

converter_result_t iq_to_wav(const conversion_request_t *request) {
    converter_result_t result = {0};

    // Not implemented yet
    snprintf(result.error_message, sizeof(result.error_message),
            "IQ to WAV conversion not implemented yet");
    result.success = false;

    return result;
}

bool wav_extract_metadata(const char *filename, file_metadata_t *metadata) {
    if (!filename || !metadata) return false;

    wav_header_t header = {0};
    if (!wav_analyze_header(filename, &header)) {
        return false;
    }

    // Fill metadata
    metadata->format = FORMAT_WAV;
    metadata->sample_rate = header.sample_rate;
    metadata->data_size_bytes = header.data_size;

    // Estimate number of samples
    if (header.bits_per_sample > 0 && header.channels > 0) {
        metadata->num_samples = header.data_size / (header.channels * header.bits_per_sample / 8);
    } else {
        metadata->num_samples = 0;
    }

    // Description
    snprintf(metadata->description, sizeof(metadata->description),
            "Fichier WAV IQ - %u Hz, %d bits, %d canaux",
            header.sample_rate, header.bits_per_sample, header.channels);

    return true;
}

// Internal utility functions
static bool wav_validate_header(const wav_header_t *header, char *error_message, size_t max_length) {
    // Basic checks
    if (header->format != 1) {
        // We still accept non-PCM formats because some IQ files have corrupted headers
        if (error_message) {
            snprintf(error_message, max_length,
                    "Unusual audio format: %d (normally 1 for PCM)", header->format);
        }
        // Continue anyway
    }

    if (header->bits_per_sample != 16) {
        if (error_message) {
            snprintf(error_message, max_length,
                    "Unsupported resolution: %d bits (16 bits expected)", header->bits_per_sample);
        }
        return false;
    }

    if (header->channels < 1 || header->channels > 2) {
        if (error_message) {
            snprintf(error_message, max_length,
                    "Nombre de canaux invalide: %d (1 ou 2 attendus)", header->channels);
        }
        return false;
    }

    if (header->sample_rate == 0) {
        if (error_message) {
            snprintf(error_message, max_length, "Fréquence d'échantillonnage invalide");
        }
        return false;
    }

    return true;
}

static size_t wav_estimate_samples(const wav_header_t *header) {
    if (header->bits_per_sample == 0 || header->channels == 0) {
        return 0;
    }

    return header->data_size / (header->channels * header->bits_per_sample / 8);
}

static double wav_calculate_duration(const wav_header_t *header) {
    if (header->sample_rate == 0) {
        return 0.0;
    }

    size_t samples = wav_estimate_samples(header);
    return (double)samples / (double)header->sample_rate;
}
