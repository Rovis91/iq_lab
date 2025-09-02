#include "iq_converter.h"
#include "../../iq_core/io_iq.h"
#include "../utils/file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>

bool iq_can_handle(const char *filename) {
    if (!filename) return false;

    // Check by file extension
    const char *ext = strrchr(filename, '.');
    if (ext && (strcmp(ext, ".iq") == 0 || strcmp(ext, ".IQ") == 0)) {
        return true;
    }

    // For now, we consider that any files without specific extensions
    // that are not WAV files could be IQ files
    // More sophisticated analysis would be needed for proper detection

    return false;
}

converter_result_t iq_to_iq(const conversion_request_t *request) {
    converter_result_t result = {0};
    clock_t start_time = clock();

    if (request->options.verbose) {
        printf("IQ to IQ conversion: %s → %s\n",
               get_format_description(request->input_format),
               get_format_description(request->output_format));
        printf("Files: %s → %s\n", request->input_file, request->output_file);
    }

    // For now, we only handle simple copy (same format)
    // Resolution conversions (s8 ↔ s16) would need to be implemented later

    if (request->input_format == request->output_format) {
        // Simple file copy
        if (!file_copy(request->input_file, request->output_file)) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "Error during file copy");
            result.success = false;
            return result;
        }

        // Get copied file size
        size_t file_size = get_file_size(request->output_file);
        if (file_size == 0) {
            snprintf(result.error_message, sizeof(result.error_message),
                    "Error: copied file is empty");
            result.success = false;
            return result;
        }

        // Estimate number of samples
        size_t bytes_per_sample = (request->input_format == FORMAT_IQ_S8) ? 2 : 4;
        size_t num_samples = file_size / bytes_per_sample;

        result.success = true;
        result.samples_converted = num_samples;
        result.bytes_processed = file_size;

        if (request->options.verbose) {
            printf("✅ Copy successful!\n");
            printf("Size: %zu bytes\n", file_size);
            printf("Samples: %zu\n", num_samples);
        }

    } else {
        // Format conversion (s8 ↔ s16) - not implemented yet
        snprintf(result.error_message, sizeof(result.error_message),
                "Conversion between different IQ formats not implemented");
        result.success = false;
        return result;
    }

    // Calculate time
    clock_t end_time = clock();
    result.conversion_time_seconds = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    return result;
}

bool iq_extract_metadata(const char *filename, file_metadata_t *metadata) {
    if (!filename || !metadata) return false;

    // Detect IQ file format using the core function
    iq_format_t core_format = iq_detect_format(filename);
    file_format_t detected_format = (core_format == IQ_FORMAT_S8) ? FORMAT_IQ_S8 : FORMAT_IQ_S16;

    // Get file size
    size_t file_size = get_file_size(filename);
    if (file_size == 0) {
        return false;
    }

    // Calculate number of samples
    size_t bytes_per_sample = (detected_format == FORMAT_IQ_S8) ? 2 : 4;
    size_t num_samples = file_size / bytes_per_sample;

    // Fill metadata
    metadata->format = detected_format;
    metadata->sample_rate = 0;  // Unknown for raw IQ files
    metadata->num_samples = num_samples;
    metadata->data_size_bytes = file_size;

    // Description
    const char *format_name = (detected_format == FORMAT_IQ_S8) ? "8-bit" : "16-bit";
    snprintf(metadata->description, sizeof(metadata->description),
            "Native IQ file %s - %zu complex samples",
            format_name, num_samples);

    return true;
}

// Internal utility functions
static bool iq_converter_detect_format(const char *filename, file_format_t *format) {
    if (!filename || !format) return false;

    size_t file_size = get_file_size(filename);
    if (file_size == 0) {
        return false;
    }

    // Simple analysis based on file size
    // Real IQ files always have even size
    // and correspond to integer number of complex samples

    if (file_size % 4 == 0) {
        // Multiple of 4 bytes = probably s16 (2 bytes I + 2 bytes Q)
        *format = FORMAT_IQ_S16;
        return true;
    } else if (file_size % 2 == 0) {
        // Multiple of 2 bytes = could be s8 (1 byte I + 1 byte Q)
        *format = FORMAT_IQ_S8;
        return true;
    }

    // Format undetermined
    *format = FORMAT_AUTO;
    return false;
}

static size_t iq_estimate_file_size(size_t num_samples, file_format_t format) {
    size_t bytes_per_sample = (format == FORMAT_IQ_S8) ? 2 : 4;
    return num_samples * bytes_per_sample;
}
