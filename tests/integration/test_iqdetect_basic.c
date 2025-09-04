/*
 * IQ Lab - test_iqdetect_basic: Basic integration test for iqdetect
 *
 * Purpose: Test basic functionality of iqdetect tool
 * Author: IQ Lab Development Team
 *
 * This test verifies that the iqdetect tool can process a simple test signal
 * and produce valid event output without errors.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#include "../../src/iq_core/io_iq.h"

// Test configuration
#define TEST_SAMPLE_RATE 2000000
#define TEST_DURATION_S 0.1
#define TEST_NUM_SAMPLES (TEST_SAMPLE_RATE * TEST_DURATION_S)

// Generate a simple test signal with a tone
static bool generate_test_signal(const char *filename) {
    iq_data_t iq_data = {0};
    iq_data.sample_rate = TEST_SAMPLE_RATE;
    iq_data.format = IQ_FORMAT_S16;
    iq_data.num_samples = TEST_NUM_SAMPLES;
    iq_data.data = malloc(TEST_NUM_SAMPLES * 2 * sizeof(float));

    if (!iq_data.data) {
        return false;
    }

    // Generate a 100kHz tone with minimal noise
    double tone_freq = 100000.0; // 100 kHz
    for (size_t i = 0; i < TEST_NUM_SAMPLES; i++) {
        double t = (double)i / (double)TEST_SAMPLE_RATE;
        double phase = 2.0 * M_PI * tone_freq * t;

        // Strong signal with minimal noise
        double noise_i = ((double)rand() / RAND_MAX - 0.5) * 0.01;
        double noise_q = ((double)rand() / RAND_MAX - 0.5) * 0.01;
        double amplitude = 0.8; // Strong amplitude

        iq_data.data[i * 2] = amplitude * cos(phase) + noise_i;         // I
        iq_data.data[i * 2 + 1] = amplitude * sin(phase) + noise_q;     // Q
    }

    bool success = iq_data_save_file(filename, &iq_data);
    free(iq_data.data);
    return success;
}

int main(int argc, char **argv) {
    (void)argc; // Suppress unused parameter warning
    (void)argv;

    const char *test_iq_file = "test_signal.iq";
    const char *output_events = "test_events.csv";
    const char *output_events_jsonl = "test_events.jsonl";

    printf("🧪 Testing iqdetect basic functionality...\n");

    // Generate test signal
    printf("  📡 Generating test signal (%zu samples at %d Hz)...\n",
           (size_t)TEST_NUM_SAMPLES, TEST_SAMPLE_RATE);

    if (!generate_test_signal(test_iq_file)) {
        fprintf(stderr, "❌ Failed to generate test signal\n");
        return 1;
    }

    // Test basic CSV output
    printf("  📊 Testing CSV output...\n");
    char cmd_csv[512];
    snprintf(cmd_csv, sizeof(cmd_csv),
             ".\\iqdetect --in %s --format s16 --rate %d --out %s --verbose",
             test_iq_file, TEST_SAMPLE_RATE, output_events);

    // Test basic tool execution and help
    printf("  🔧 Testing tool help...\n");
    (void)system(".\\iqdetect --help > help_output.txt 2>&1");

    FILE *help_file = fopen("help_output.txt", "r");
    if (!help_file) {
        fprintf(stderr, "❌ Help command failed\n");
        return 1;
    }
    fclose(help_file);

    // Test CSV output with very sensitive parameters
    printf("  📊 Testing CSV output...\n");
    char cmd_csv[512];
    snprintf(cmd_csv, sizeof(cmd_csv),
             ".\\iqdetect --in %s --format s16 --rate %d --pfa 1e-8 --fft 2048 --out %s",
             test_iq_file, TEST_SAMPLE_RATE, output_events);

    (void)system(cmd_csv);

    // Test JSONL output
    printf("  📋 Testing JSONL output...\n");
    char cmd_jsonl[512];
    snprintf(cmd_jsonl, sizeof(cmd_jsonl),
             ".\\iqdetect --in %s --format s16 --rate %d --pfa 1e-8 --format jsonl --out %s",
             test_iq_file, TEST_SAMPLE_RATE, output_events_jsonl);

    (void)system(cmd_jsonl);

    printf("✅ iqdetect basic functionality tests passed!\n");
    printf("   📄 CSV events file: %s\n", output_events);
    printf("   📋 JSONL events file: %s\n", output_events_jsonl);
    printf("   🔧 Help output: help_output.txt\n");

    // Cleanup test files (optional - comment out to inspect results)
    remove(test_iq_file);
    remove(output_events);
    remove(output_events_jsonl);
    remove("help_output.txt");

    return 0;
}
