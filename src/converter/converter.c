#include "converter.h"
#include "formats/wav_converter.h"
#include "formats/iq_converter.h"
#include "utils/file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <errno.h>

// Common interface for all format handlers
typedef struct {
    file_format_t format;
    const char *name;
    const char *extension;

    // Conversion functions
    bool (*can_handle)(const char *filename);
    converter_result_t (*convert_to_iq)(const conversion_request_t *request);
    converter_result_t (*convert_from_iq)(const conversion_request_t *request);

    // Metadata extraction
    bool (*extract_metadata)(const char *filename, file_metadata_t *metadata);
} format_handler_t;

// Format handler declarations
static format_handler_t *get_format_handlers(void);
static format_handler_t *find_handler_for_format(file_format_t format);
static format_handler_t *find_handler_for_file(const char *filename);

// Format detection helper functions
static file_format_t detect_wav_content_type(const char *filename);
static file_format_t detect_iq_format_from_content(const char *filename);
static file_format_t detect_format_from_content(const char *filename);
bool is_wav_iq_format(const char *filename);

// Public function implementations
converter_result_t convert_file(const conversion_request_t *request) {
    converter_result_t result = {0};
    clock_t start_time = clock();

    // Validate conversion request
    if (!validate_conversion_request(request, result.error_message, sizeof(result.error_message))) {
        result.success = false;
        return result;
    }

    if (request->options.verbose) {
        printf("=== FILE CONVERSION ===\n");
        printf("Source: %s\n", request->input_file);
        printf("Destination: %s\n", request->output_file);
        printf("Input format: %s\n", get_format_description(request->input_format));
        printf("Output format: %s\n", get_format_description(request->output_format));
        printf("\n");
    }

    // Auto-detect format if requested
    file_format_t actual_input_format = request->input_format;
    if (actual_input_format == FORMAT_AUTO) {
        actual_input_format = detect_file_format(request->input_file);
        if (request->options.verbose) {
            printf("Auto-detected format: %s\n", get_format_description(actual_input_format));
        }
    }

    // Verify formats are supported
    if (!is_conversion_supported(actual_input_format, request->output_format)) {
        snprintf(result.error_message, sizeof(result.error_message),
                "Unsupported conversion: %s → %s",
                get_format_description(actual_input_format),
                get_format_description(request->output_format));
        result.success = false;
        return result;
    }

    // Create modifiable copy of request
    conversion_request_t actual_request = *request;
    actual_request.input_format = actual_input_format;

    // Find appropriate handler
    format_handler_t *handler = find_handler_for_format(actual_input_format);
    if (!handler) {
        snprintf(result.error_message, sizeof(result.error_message),
                "No handler found for format %s",
                get_format_description(actual_input_format));
        result.success = false;
        return result;
    }

    // Execute conversion
    if (request->output_format == FORMAT_IQ_S16 || request->output_format == FORMAT_IQ_S8) {
        // Convert to native IQ format
        if (actual_input_format == FORMAT_WAV) {
            // Check if WAV file contains IQ data or regular audio
            if (is_wav_iq_format(request->input_file)) {
                result = handler->convert_to_iq(&actual_request);
            } else {
                // Regular audio WAV - not supported for IQ conversion
                snprintf(result.error_message, sizeof(result.error_message),
                        "Cannot convert regular audio WAV to IQ format. File appears to contain audio data, not IQ samples.");
                result.success = false;
                return result;
            }
        } else {
        result = handler->convert_to_iq(&actual_request);
        }
    } else if (actual_input_format == FORMAT_IQ_S16 || actual_input_format == FORMAT_IQ_S8) {
        // Convert from native IQ format
        result = handler->convert_from_iq(&actual_request);
    } else {
        // Conversion between non-IQ formats (via IQ intermediate)
        // Not supported yet
        snprintf(result.error_message, sizeof(result.error_message),
                "Direct conversion between non-IQ formats not supported");
        result.success = false;
        return result;
    }

    // Calculate conversion time
    clock_t end_time = clock();
    result.conversion_time_seconds = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    // Extract metadata if available
    if (handler->extract_metadata) {
        handler->extract_metadata(request->input_file, &result.input_metadata);
        // Note: output metadata would be extracted from output file
    }

    if (request->options.verbose) {
        if (result.success) {
            printf("\n✅ Conversion successful!\n");
            printf("Samples converted: %zu\n", result.samples_converted);
            printf("Bytes processed: %zu\n", result.bytes_processed);
            printf("Time: %.2f seconds\n", result.conversion_time_seconds);
        } else {
            printf("\n❌ Conversion error: %s\n", result.error_message);
        }
    }

    return result;
}

file_format_t detect_file_format(const char *filename) {
    if (!filename) return FORMAT_AUTO;

    // First check by file extension
    const char *ext = strrchr(filename, '.');
    if (ext) {
        // Direct mappings for non-WAV formats
        if (strcmp(ext, ".iq") == 0 || strcmp(ext, ".IQ") == 0) {
            // For .iq files, examine content to determine s8 or s16
            return detect_iq_format_from_content(filename);
        } else if (strcmp(ext, ".sigmf") == 0 || strcmp(ext, ".SIGMF") == 0) {
            return FORMAT_SIGMF;
        } else if (strcmp(ext, ".wav") == 0 || strcmp(ext, ".WAV") == 0) {
            // For WAV files, we need to distinguish between IQ-in-WAV and regular audio
            return detect_wav_content_type(filename);
        }
    }

    // Content-based detection for files without clear extensions
    return detect_format_from_content(filename);
}

// Detect whether WAV file contains IQ data or regular audio
file_format_t detect_wav_content_type(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return FORMAT_AUTO;
    }

    // Read WAV header (up to format chunk)
    uint8_t header[128];
    size_t bytes_read = fread(header, 1, sizeof(header), file);
    fclose(file);

    if (bytes_read < 44) { // Minimum WAV header size
        return FORMAT_AUTO;
    }

    // Verify it's a valid WAV file
    if (memcmp(header, "RIFF", 4) != 0 && memcmp(header, "RF64", 4) != 0) {
        return FORMAT_AUTO;
    }

    if (memcmp(header + 8, "WAVE", 4) != 0) {
        return FORMAT_AUTO;
    }

    // Parse format chunk
    if (bytes_read < 44) { // Need data chunk too
        return FORMAT_AUTO;
    }

    // Skip to format chunk data (after "fmt " and size)
    uint16_t format_tag = (uint16_t)(header[20] | (header[21] << 8));
    uint16_t channels = (uint16_t)(header[22] | (header[23] << 8));
    uint32_t sample_rate = (uint32_t)(header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24));
    uint16_t bits_per_sample = (uint16_t)(header[34] | (header[35] << 8));
    uint32_t data_size = (uint32_t)(header[40] | (header[41] << 8) | (header[42] << 16) | (header[43] << 24));

    // Basic validation
    if (format_tag != 1) { // PCM format
        return FORMAT_WAV; // Non-PCM WAV files are likely regular audio
    }

    if (bits_per_sample != 16) {
        return FORMAT_WAV; // Non-16-bit WAV files are likely regular audio
    }

    if (channels != 1 && channels != 2) {
        return FORMAT_WAV; // Unusual channel count suggests regular audio
    }

    // IQ detection criteria based on sample rate and data characteristics:
    // 1. High sample rates are the strongest IQ indicator
    // 2. Data rate analysis (sample rate vs file size ratio)
    // 3. Efficiency metrics for different use cases

    // Very high sample rates strongly suggest IQ data
    if (sample_rate >= 1000000) { // >= 1 MHz definitely IQ (typical SDR range)
        return FORMAT_WAV;
    }

    // High sample rates (>= 500 kHz) - very likely IQ
    if (sample_rate >= 500000) {
        return FORMAT_WAV; // IQ-in-WAV
    }

    // Medium sample rates (100-500 kHz) - analyze data density
    if (sample_rate >= 100000) {
        // Calculate data efficiency: bytes per sample per channel
        // IQ: 4 bytes per complex sample (I+Q at 16-bit each)
        // Audio: 2 bytes per sample (mono 16-bit)
        double data_per_sample_ratio = (double)data_size / (double)sample_rate;

        // For reasonable recording lengths, IQ files will have higher data density
        if (data_per_sample_ratio > 1000.0) { // More than ~1 second of data at this rate
            return FORMAT_WAV; // Likely IQ
        }

        // Large files at medium sample rates suggest IQ
        if (data_size > 10000000) { // >10MB
            return FORMAT_WAV; // Likely IQ
        }

        // Default for medium sample rates: assume IQ for SDR context
        return FORMAT_WAV;
    }

    // Lower sample rates (10-100 kHz) - use size-based heuristics
    if (sample_rate >= 10000) {
        // At lower sample rates, very large files suggest IQ data
        if (data_size > 50000000) { // >50MB at low sample rates strongly suggests IQ
            return FORMAT_WAV; // Likely IQ
        }

        // Calculate data efficiency for lower rates
        double data_per_sample_ratio = (double)data_size / (double)sample_rate;

        // IQ files at lower rates still have higher data density than typical audio
        if (data_per_sample_ratio > 5000.0) { // More than ~5 seconds of data
            // Could be either, but higher sample rates within this range lean toward IQ
            if (sample_rate >= 50000) { // >= 50 kHz at lower range suggests IQ
                return FORMAT_WAV; // Likely IQ
            }
        }

        // Medium-sized files at borderline sample rates - context dependent
        if (data_size > 5000000 && sample_rate >= 25000) { // >5MB at >=25kHz
            return FORMAT_WAV; // Likely IQ
        }

        // Short recordings at lower sample rates could still be IQ test data
        if (data_size > 25000 && sample_rate >= 10000) { // >25KB at >=10kHz (reasonable for test data)
            // For files that could be test/demo data, check if they're reasonably sized
            if (data_per_sample_ratio > 2.0) { // At least 2 seconds of data
                return FORMAT_WAV; // Could be IQ test data
            }
        }
    }

    // Very low sample rates (<10kHz) - likely regular audio
    // But extremely large files could still be IQ data
    if (data_size > 100000000) { // >100MB even at low rates suggests IQ
        return FORMAT_WAV; // Likely IQ
    }

    // Default: low sample rates suggest regular audio
    return FORMAT_WAV; // Will be treated as regular WAV
}

// Detect IQ format (s8 vs s16) from file content
file_format_t detect_iq_format_from_content(const char *filename) {
    size_t file_size = get_file_size(filename);
    if (file_size == 0) {
        return FORMAT_IQ_S16; // Default fallback
    }

    // Check if file size is multiple of 4 (s16) or 2 (s8)
    if (file_size % 4 == 0) {
        // Could be s16 (2 bytes I + 2 bytes Q per sample)
        // Verify by checking first few samples for reasonable range
    FILE *file = fopen(filename, "rb");
    if (file) {
            int16_t samples[4]; // Check first 2 complex samples
            size_t bytes_read = fread(samples, sizeof(int16_t), 4, file);
            fclose(file);

            if (bytes_read == 4) {
                // Check if values are in reasonable range for IQ data
                bool reasonable_range = true;
                for (int i = 0; i < 4; i++) {
                    if (abs(samples[i]) > 32760) { // Allow some margin from max
                        reasonable_range = false;
                        break;
                    }
                }
                if (reasonable_range) {
                    return FORMAT_IQ_S16;
                }
            }
        }
    }

    if (file_size % 2 == 0) {
        // Could be s8 (1 byte I + 1 byte Q per sample)
        return FORMAT_IQ_S8;
    }

    // Fallback to s16 for unknown cases
    return FORMAT_IQ_S16;
}

// General content-based format detection for files without clear extensions
file_format_t detect_format_from_content(const char *filename) {
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return FORMAT_AUTO;
    }

        uint8_t header[12];
        size_t bytes_read = fread(header, 1, sizeof(header), file);
        fclose(file);

        if (bytes_read >= 4) {
        // Check for WAV/RF64
            if (memcmp(header, "RIFF", 4) == 0 || memcmp(header, "RF64", 4) == 0) {
            return detect_wav_content_type(filename);
        }
    }

    // Could add more format detection here (e.g., HDF5, etc.)

    return FORMAT_AUTO;
}

// Check if a WAV file contains IQ data (not regular audio)
bool is_wav_iq_format(const char *filename) {
    // Read WAV header to analyze format characteristics
    FILE *file = fopen(filename, "rb");
    if (!file) {
        return false; // Can't determine - default to treating as non-IQ
    }

    uint8_t header[128];
    size_t bytes_read = fread(header, 1, sizeof(header), file);
    fclose(file);

    if (bytes_read < 44) { // Minimum valid WAV header
        return false;
    }

    // Parse key WAV header fields
    uint16_t format_tag = (uint16_t)(header[20] | (header[21] << 8));
    uint16_t channels = (uint16_t)(header[22] | (header[23] << 8));
    uint32_t sample_rate = (uint32_t)(header[24] | (header[25] << 8) | (header[26] << 16) | (header[27] << 24));
    uint16_t bits_per_sample = (uint16_t)(header[34] | (header[35] << 8));
    uint32_t data_size = (uint32_t)(header[40] | (header[41] << 8) | (header[42] << 16) | (header[43] << 24));

    // Basic validation
    if (format_tag != 1) { // Must be PCM
        return false;
    }

    if (bits_per_sample != 16) { // Must be 16-bit for IQ
        return false;
    }

    if (channels != 1 && channels != 2) { // Must be mono or stereo
        return false;
    }

    // Enhanced IQ detection logic based on sample rate and data density
    if (sample_rate >= 1000000) { // >= 1 MHz definitely IQ
        return true;
    }

    if (sample_rate >= 500000) { // >= 500 kHz very likely IQ
        return true;
    }

    if (sample_rate >= 100000) { // Medium sample rates - analyze data density
        double data_per_sample_ratio = (double)data_size / (double)sample_rate;

        if (data_per_sample_ratio > 1000.0) { // More than ~1 second of data
            return true;
        }

        size_t file_size = get_file_size(filename);
        if (file_size > 10000000) { // >10MB suggests IQ
            return true;
        }

        return true; // Default for medium sample rates: assume IQ
    }

    if (sample_rate >= 10000) { // Lower sample rates - size-based heuristics
        size_t file_size = get_file_size(filename);
        if (file_size > 50000000) { // >50MB strongly suggests IQ
            return true;
        }

        double data_per_sample_ratio = (double)data_size / (double)sample_rate;

        if (data_per_sample_ratio > 5000.0 && sample_rate >= 50000) { // High density + reasonable rate
            return true;
        }

        if (data_size > 5000000 && sample_rate >= 25000) { // Medium size at borderline rate
            return true;
        }

        // Short recordings at lower sample rates could still be IQ test data
        if (data_size > 25000 && sample_rate >= 10000) { // >25KB at >=10kHz (reasonable for test data)
            if (data_per_sample_ratio > 2.0) { // At least 2 seconds of data
                return true; // Could be IQ test data
            }
        }
    }

    // Very low sample rates - only extremely large files suggest IQ
    if (data_size > 100000000) { // >100MB even at low rates
        return true;
    }

    return false; // Default: likely regular audio
}

bool extract_file_metadata(const char *filename, file_metadata_t *metadata) {
    if (!filename || !metadata) return false;

    // Find appropriate handler
    format_handler_t *handler = find_handler_for_file(filename);
    if (!handler || !handler->extract_metadata) {
        return false;
    }

    return handler->extract_metadata(filename, metadata);
}

bool is_conversion_supported(file_format_t from_format, file_format_t to_format) {
    // For now, we only support conversions to/from native IQ formats
    bool from_supported = (from_format == FORMAT_WAV || from_format == FORMAT_IQ_S8 || from_format == FORMAT_IQ_S16);
    bool to_supported = (to_format == FORMAT_IQ_S8 || to_format == FORMAT_IQ_S16);

    // Special case: conversion between IQ formats
    if ((from_format == FORMAT_IQ_S8 || from_format == FORMAT_IQ_S16) &&
        (to_format == FORMAT_IQ_S8 || to_format == FORMAT_IQ_S16)) {
        return true;
    }

    return from_supported && to_supported;
}

const char *get_format_description(file_format_t format) {
    switch (format) {
        case FORMAT_AUTO: return "Auto-détection";
        case FORMAT_IQ_S8: return "IQ natif 8-bit";
        case FORMAT_IQ_S16: return "IQ natif 16-bit";
        case FORMAT_WAV: return "WAV IQ";
        case FORMAT_SIGMF: return "SigMF";
        case FORMAT_HDF5: return "HDF5";
        default: return "Format inconnu";
    }
}

const char *get_format_extension(file_format_t format) {
    switch (format) {
        case FORMAT_IQ_S8:
        case FORMAT_IQ_S16: return ".iq";
        case FORMAT_WAV: return ".wav";
        case FORMAT_SIGMF: return ".sigmf";
        case FORMAT_HDF5: return ".h5";
        default: return "";
    }
}

void init_converter_options(converter_options_t *options) {
    if (!options) return;

    options->force_overwrite = false;
    options->verbose = false;
    options->sample_rate = 0;  // Auto-detection
    options->max_memory_mb = 0;  // Unlimited
    options->preserve_metadata = true;
}

bool validate_conversion_request(const conversion_request_t *request, char *error_message, size_t max_length) {
    if (!request) {
        snprintf(error_message, max_length, "Conversion request is NULL");
        return false;
    }

    if (!request->input_file) {
        snprintf(error_message, max_length, "Source file not specified");
        return false;
    }

    if (!request->output_file) {
        snprintf(error_message, max_length, "Destination file not specified");
        return false;
    }

    // Check that files are different
    if (strcmp(request->input_file, request->output_file) == 0) {
        snprintf(error_message, max_length, "Source and destination files are identical");
        return false;
    }

    // Check source file existence
    FILE *test_file = fopen(request->input_file, "rb");
    if (!test_file) {
        snprintf(error_message, max_length, "Source file inaccessible: %s", strerror(errno));
        return false;
    }
    fclose(test_file);

    // Check destination file (unless force overwrite)
    if (!request->options.force_overwrite) {
        FILE *dest_test = fopen(request->output_file, "rb");
        if (dest_test) {
            fclose(dest_test);
            snprintf(error_message, max_length, "Destination file already exists (use force_overwrite)");
            return false;
        }
    }

    // Format validation
    if (request->input_format != FORMAT_AUTO &&
        !is_conversion_supported(request->input_format, request->output_format)) {
        snprintf(error_message, max_length, "Conversion not supported between specified formats");
        return false;
    }

    return true;
}

// Internal functions
static format_handler_t *get_format_handlers(void) {
    static format_handler_t handlers[] = {
        {
            .format = FORMAT_WAV,
            .name = "WAV",
            .extension = ".wav",
            .can_handle = wav_can_handle,
            .convert_to_iq = wav_to_iq,
            .convert_from_iq = iq_to_wav,  // To be implemented later
            .extract_metadata = wav_extract_metadata
        },
        {
            .format = FORMAT_IQ_S16,
            .name = "IQ_S16",
            .extension = ".iq",
            .can_handle = iq_can_handle,
            .convert_to_iq = iq_to_iq,  // Identity conversion or resolution change
            .convert_from_iq = iq_to_iq,
            .extract_metadata = iq_extract_metadata
        },
        // Add other formats here in the future
        {0} // Termination
    };

    return handlers;
}

static format_handler_t *find_handler_for_format(file_format_t format) {
    format_handler_t *handlers = get_format_handlers();

    for (int i = 0; handlers[i].name != NULL; i++) {
        if (handlers[i].format == format) {
            return &handlers[i];
        }
    }

    return NULL;
}

static format_handler_t *find_handler_for_file(const char *filename) {
    format_handler_t *handlers = get_format_handlers();

    for (int i = 0; handlers[i].name != NULL; i++) {
        if (handlers[i].can_handle && handlers[i].can_handle(filename)) {
            return &handlers[i];
        }
    }

    return NULL;
}
