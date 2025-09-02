#include <stdio.h>
#include <stdlib.h>
#include <getopt.h>
#include <string.h>
#include "../src/converter/converter.h"

#define PROGRAM_NAME "IQ Lab File Converter"
#define PROGRAM_VERSION "1.0.0"

// Help function
static void print_help(const char *program_name) {
    printf("%s v%s - Universal IQ File Converter\n", PROGRAM_NAME, PROGRAM_VERSION);
    printf("Usage: %s [OPTIONS] <input_file> <output_file>\n", program_name);
    printf("\nOPTIONS:\n");
    printf("  -f, --from <format>    Source file format (auto, iq_s8, iq_s16, wav)\n");
    printf("  -t, --to <format>      Destination file format (iq_s8, iq_s16)\n");
    printf("  -r, --rate <Hz>        Force sample rate\n");
    printf("  --force                Overwrite destination file if it exists\n");
    printf("  -v, --verbose          Verbose mode\n");
    printf("  -h, --help             Show this help\n");
    printf("\nEXAMPLES:\n");
    printf("  # Automatic conversion (format detection)\n");
    printf("  %s capture.wav capture.iq\n", program_name);
    printf("\n  # Explicit WAV to 16-bit IQ conversion\n");
    printf("  %s -f wav -t iq_s16 capture.wav capture.iq\n", program_name);
    printf("\n  # Conversion with forced sample rate\n");
    printf("  %s -r 2000000 mystery_file.dat output.iq\n", program_name);
    printf("\nSUPPORTED FORMATS:\n");
    printf("  auto     - Automatic format detection\n");
    printf("  iq_s8    - Native IQ 8-bit\n");
    printf("  iq_s16   - Native IQ 16-bit\n");
    printf("  wav      - WAV IQ (RIFF/RF64)\n");
    printf("\nNOTES:\n");
    printf("  - Destination formats must be native IQ formats\n");
    printf("  - Auto-detection works for standard file extensions\n");
    printf("  - Large files are processed in blocks to save RAM\n");
}

static file_format_t parse_format(const char *format_str) {
    if (!format_str) return FORMAT_AUTO;

    if (strcmp(format_str, "auto") == 0) return FORMAT_AUTO;
    if (strcmp(format_str, "iq_s8") == 0) return FORMAT_IQ_S8;
    if (strcmp(format_str, "iq_s16") == 0) return FORMAT_IQ_S16;
    if (strcmp(format_str, "wav") == 0) return FORMAT_WAV;
    if (strcmp(format_str, "sigmf") == 0) return FORMAT_SIGMF;
    if (strcmp(format_str, "hdf5") == 0) return FORMAT_HDF5;

    return FORMAT_AUTO; // Unknown format = auto-detection
}

int main(int argc, char *argv[]) {
    // Default options
    conversion_request_t request;
    memset(&request, 0, sizeof(request));
    init_converter_options(&request.options);

    // Parse command line arguments
    static struct option long_options[] = {
        {"from", required_argument, 0, 'f'},
        {"to", required_argument, 0, 't'},
        {"rate", required_argument, 0, 'r'},
        {"force", no_argument, 0, 'F'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    int option_index = 0;

    while ((opt = getopt_long(argc, argv, "f:t:r:Fvh", long_options, &option_index)) != -1) {
        switch (opt) {
            case 'f':
                request.input_format = parse_format(optarg);
                break;
            case 't':
                request.output_format = parse_format(optarg);
                break;
            case 'r':
                request.options.sample_rate = (uint32_t)atoi(optarg);
                break;
            case 'F':
                request.options.force_overwrite = true;
                break;
            case 'v':
                request.options.verbose = true;
                break;
            case 'h':
            default:
                print_help(argv[0]);
                return EXIT_SUCCESS;
        }
    }

    // Check positional arguments
    if (optind + 2 > argc) {
        fprintf(stderr, "Error: source and destination files required\n");
        print_help(argv[0]);
        return EXIT_FAILURE;
    }

    request.input_file = argv[optind];
    request.output_file = argv[optind + 1];

    // Default configuration if not specified
    if (request.input_format == FORMAT_AUTO) {
        request.input_format = FORMAT_AUTO;
    }

    if (request.output_format == FORMAT_AUTO) {
        // Default to 16-bit IQ conversion
        request.output_format = FORMAT_IQ_S16;
    }

    // Validation
    char error_message[512];
    if (!validate_conversion_request(&request, error_message, sizeof(error_message))) {
        fprintf(stderr, "Validation error: %s\n", error_message);
        return EXIT_FAILURE;
    }

    // Display header
    if (request.options.verbose) {
        printf("=== %s ===\n", PROGRAM_NAME);
        printf("Conversion: %s → %s\n",
               get_format_description(request.input_format),
               get_format_description(request.output_format));
        printf("Files: %s → %s\n", request.input_file, request.output_file);
        if (request.options.sample_rate > 0) {
            printf("Forced sample rate: %u Hz\n", request.options.sample_rate);
        }
        printf("\n");
    }

    // Execute conversion
    converter_result_t result = convert_file(&request);

    // Display result
    if (result.success) {
        printf("✅ Conversion successful!\n");
        printf("Samples converted: %zu\n", result.samples_converted);
        printf("Bytes processed: %zu\n", result.bytes_processed);
        printf("Time: %.2f seconds\n", result.conversion_time_seconds);

        if (result.samples_converted > 0 && result.conversion_time_seconds > 0) {
            double throughput = (double)result.bytes_processed / result.conversion_time_seconds / 1024 / 1024;
            printf("Throughput: %.1f MB/s\n", throughput);
        }
    } else {
        fprintf(stderr, "❌ Conversion error: %s\n", result.error_message);
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}
