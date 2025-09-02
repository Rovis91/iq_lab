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
        result = handler->convert_to_iq(&actual_request);
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

    // Recherche par extension
    const char *ext = strrchr(filename, '.');
    if (ext) {
        if (strcmp(ext, ".wav") == 0 || strcmp(ext, ".WAV") == 0) {
            return FORMAT_WAV;
        } else if (strcmp(ext, ".iq") == 0 || strcmp(ext, ".IQ") == 0) {
            // For .iq files, we'll need to examine content to determine s8 or s16
            return FORMAT_IQ_S16; // Default to s16, will be refined later
        } else if (strcmp(ext, ".sigmf") == 0 || strcmp(ext, ".SIGMF") == 0) {
            return FORMAT_SIGMF;
        }
    }

    // Search by content (header)
    FILE *file = fopen(filename, "rb");
    if (file) {
        uint8_t header[12];
        size_t bytes_read = fread(header, 1, sizeof(header), file);
        fclose(file);

        if (bytes_read >= 4) {
            if (memcmp(header, "RIFF", 4) == 0 || memcmp(header, "RF64", 4) == 0) {
                return FORMAT_WAV;
            }
        }
    }

    return FORMAT_AUTO; // Detection failed
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
