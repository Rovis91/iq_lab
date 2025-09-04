/*
 * IQ Lab - WAV Writer Module
 *
 * Purpose: Professional WAV file writer with RIFF header support
 * Author: IQ Lab Development Team
 * Date: 2025
 *
 * This module provides comprehensive WAV file writing capabilities for
 * audio demodulation outputs. It supports 16-bit PCM format with mono
 * and stereo configurations, automatically handling RIFF header generation
 * and finalization.
 *
 * Key Features:
 * - RIFF/WAVE header generation with proper chunk structure
 * - 16-bit PCM audio format (standard for professional audio)
 * - Mono and stereo channel support
 * - Automatic header size calculations and updates
 * - Streaming write capability with header finalization
 * - Error handling and validation
 *
 * Usage:
 *   wave_writer_t writer;
 *   wave_writer_init(&writer, "output.wav", 44100, 2);
 *   wave_writer_write_samples(&writer, samples, num_samples);
 *   wave_writer_close(&writer);
 *
 * Technical Details:
 * - RIFF Header: 44-byte standard structure
 * - Format Chunk: PCM format specification
 * - Data Chunk: Audio sample storage
 * - Endianness: Little-endian (Windows WAV standard)
 * - Sample Format: 16-bit signed integer (-32768 to +32767)
 *
 * Performance:
 * - O(1) per sample write operation
 * - Memory efficient (no sample buffering)
 * - Real-time capable for streaming applications
 *
 * Standards Compliance:
 * - Microsoft RIFF/WAVE specification
 * - 16-bit PCM audio format
 * - Compatible with all major audio applications
 *
 * Dependencies: stdlib.h, string.h, assert.h
 * Thread Safety: Not thread-safe (single writer instance per file)
 * Error Handling: Comprehensive validation with graceful failure
 */

#include "wave.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

// Initialize WAV writer and write headers
bool wave_writer_init(wave_writer_t *writer, const char *filename,
                     uint32_t sample_rate, uint16_t channels) {
    if (!writer || !filename || sample_rate == 0 ||
        (channels != 1 && channels != 2)) {
        return false;
    }

    // Open file for writing
    writer->file = fopen(filename, "wb");
    if (!writer->file) {
        return false;
    }

    // Initialize writer context
    writer->sample_rate = sample_rate;
    writer->channels = channels;
    writer->total_samples = 0;
    writer->data_size = 0;

    // Write RIFF header (will be updated at close)
    wave_riff_header_t riff_header;
    memcpy(riff_header.riff_id, "RIFF", 4);
    riff_header.file_size = 0; // Placeholder, updated at close
    memcpy(riff_header.wave_id, "WAVE", 4);

    if (fwrite(&riff_header, sizeof(riff_header), 1, writer->file) != 1) {
        fclose(writer->file);
        return false;
    }

    // Write format chunk
    wave_format_chunk_t fmt_chunk;
    memcpy(fmt_chunk.chunk_id, "fmt ", 4);
    fmt_chunk.chunk_size = 16; // PCM format
    fmt_chunk.format_tag = 1;  // PCM
    fmt_chunk.channels = channels;
    fmt_chunk.sample_rate = sample_rate;
    fmt_chunk.bits_per_sample = 16;
    fmt_chunk.block_align = channels * fmt_chunk.bits_per_sample / 8;
    fmt_chunk.byte_rate = sample_rate * fmt_chunk.block_align;

    if (fwrite(&fmt_chunk, sizeof(fmt_chunk), 1, writer->file) != 1) {
        fclose(writer->file);
        return false;
    }

    // Write data chunk header (will be updated at close)
    wave_data_chunk_t data_chunk;
    memcpy(data_chunk.chunk_id, "data", 4);
    data_chunk.chunk_size = 0; // Placeholder, updated at close

    if (fwrite(&data_chunk, sizeof(data_chunk), 1, writer->file) != 1) {
        fclose(writer->file);
        return false;
    }

    return true;
}

// Write audio samples
bool wave_writer_write_samples(wave_writer_t *writer,
                              const int16_t *samples, uint32_t num_samples) {
    if (!writer || !writer->file || !samples || num_samples == 0) {
        return false;
    }

    // Write samples (num_samples is per channel, so total bytes = num_samples * channels * 2)
    uint32_t bytes_to_write = num_samples * writer->channels * sizeof(int16_t);
    if (fwrite(samples, 1, bytes_to_write, writer->file) != bytes_to_write) {
        return false;
    }

    writer->total_samples += num_samples;
    writer->data_size += bytes_to_write;

    return true;
}

// Close WAV writer and finalize headers
bool wave_writer_close(wave_writer_t *writer) {
    if (!writer || !writer->file) {
        return false;
    }

    // Get current file position
    long current_pos = ftell(writer->file);
    if (current_pos < 0) {
        fclose(writer->file);
        return false;
    }

    // Update data chunk size
    uint32_t data_chunk_size = writer->data_size;
    fseek(writer->file, 36, SEEK_SET); // Position of data chunk size
    if (fwrite(&data_chunk_size, sizeof(uint32_t), 1, writer->file) != 1) {
        fclose(writer->file);
        return false;
    }

    // Update RIFF file size
    uint32_t file_size = 36 + writer->data_size; // RIFF header + fmt chunk + data header + data
    fseek(writer->file, 4, SEEK_SET); // Position of file size in RIFF header
    if (fwrite(&file_size, sizeof(uint32_t), 1, writer->file) != 1) {
        fclose(writer->file);
        return false;
    }

    // Close file
    if (fclose(writer->file) != 0) {
        return false;
    }

    writer->file = NULL;
    return true;
}

// Utility functions
uint32_t wave_calculate_data_size(uint32_t num_samples, uint16_t channels) {
    return num_samples * channels * sizeof(int16_t);
}

uint32_t wave_calculate_file_size(uint32_t data_size) {
    return 44 + data_size; // 44 bytes for headers + data
}
