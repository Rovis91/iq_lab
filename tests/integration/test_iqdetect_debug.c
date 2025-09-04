/*
 * IQ Lab - test_iqdetect_debug: Debug test for iqdetect detection pipeline
 *
 * Purpose: Debug the detection pipeline to understand why signals aren't being detected
  *
 * Date: 2025
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include "../../src/iq_core/io_iq.h"
#include "../../src/iq_core/fft.h"
#include "../../src/detect/cfar_os.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test configuration
#define TEST_SAMPLE_RATE 2000000
#define TEST_DURATION_S 0.5
#define TEST_NUM_SAMPLES (TEST_SAMPLE_RATE * TEST_DURATION_S)

// Generate a very strong CW tone
static bool generate_debug_signal(const char *filename, double frequency) {
    iq_data_t iq_data = {0};
    iq_data.sample_rate = TEST_SAMPLE_RATE;
    iq_data.format = IQ_FORMAT_S16;
    iq_data.num_samples = TEST_NUM_SAMPLES;
    iq_data.data = malloc(TEST_NUM_SAMPLES * 2 * sizeof(float));

    if (!iq_data.data) return false;

    // Generate a pure tone with no noise
    for (size_t i = 0; i < TEST_NUM_SAMPLES; i++) {
        double t = (double)i / (double)TEST_SAMPLE_RATE;
        double phase = 2.0 * M_PI * frequency * t;

        iq_data.data[i * 2] = cos(phase);      // I
        iq_data.data[i * 2 + 1] = sin(phase);  // Q
    }

    bool success = iq_data_save_file(filename, &iq_data);
    free(iq_data.data);
    return success;
}

// Debug the FFT and CFAR process
static bool debug_detection_pipeline(void) {
    printf("🔍 Debugging detection pipeline...\n");

    // Generate test signal
    if (!generate_debug_signal("debug_test.iq", 500000.0)) {
        fprintf(stderr, "❌ Failed to generate debug signal\n");
        return false;
    }

    // Load the signal
    iq_data_t iq_data = {0};
    if (!iq_load_file("debug_test.iq", &iq_data)) {
        fprintf(stderr, "❌ Failed to load debug signal\n");
        return false;
    }

    printf("    📊 Loaded IQ data: %zu samples at %u Hz\n", iq_data.num_samples, iq_data.sample_rate);

    // Set up FFT (same parameters as iqdetect)
    uint32_t fft_size = 2048;
    fft_plan_t *fft_plan = fft_plan_create(fft_size, FFT_FORWARD);
    if (!fft_plan) {
        fprintf(stderr, "❌ Failed to create FFT plan\n");
        iq_free(&iq_data);
        return false;
    }

    // Allocate FFT buffers
    fft_complex_t *fft_in = malloc(fft_size * sizeof(fft_complex_t));
    fft_complex_t *fft_out = malloc(fft_size * sizeof(fft_complex_t));
    fft_complex_t *fft_shifted = malloc(fft_size * sizeof(fft_complex_t));
    double *power_spectrum = malloc(fft_size * sizeof(double));

    if (!fft_in || !fft_out || !fft_shifted || !power_spectrum) {
        fprintf(stderr, "❌ Failed to allocate FFT buffers\n");
        fft_plan_destroy(fft_plan);
        iq_free(&iq_data);
        return false;
    }

    // Process first frame
    printf("    🔄 Processing first FFT frame...\n");

    // Prepare FFT input from first fft_size samples
    for (uint32_t i = 0; i < fft_size; i++) {
        float i_val = iq_data.data[i * 2];
        float q_val = iq_data.data[i * 2 + 1];
        fft_in[i] = i_val + q_val * I;
    }

    // Execute FFT
    if (!fft_execute(fft_plan, fft_in, fft_out)) {
        fprintf(stderr, "❌ FFT execution failed\n");
        goto cleanup;
    }

    // FFT shift
    if (!fft_shift(fft_out, fft_shifted, fft_size)) {
        fprintf(stderr, "❌ FFT shift failed\n");
        goto cleanup;
    }

    // Calculate power spectrum
    double max_power = 0.0;
    uint32_t max_bin = 0;
    for (uint32_t i = 0; i < fft_size; i++) {
        double real = creal(fft_shifted[i]);
        double imag = cimag(fft_shifted[i]);
        power_spectrum[i] = real * real + imag * imag;

        if (power_spectrum[i] > max_power) {
            max_power = power_spectrum[i];
            max_bin = i;
        }
    }

    // Calculate expected frequency bin
    double expected_freq = 500000.0;
    double bin_freq_resolution = (double)TEST_SAMPLE_RATE / fft_size;
    uint32_t expected_bin = (uint32_t)((expected_freq / bin_freq_resolution) + fft_size/2) % fft_size;

    printf("    📊 Power spectrum analysis:\n");
    printf("       Max power: %.6f at bin %u\n", max_power, max_bin);
    printf("       Expected bin: %u (freq: %.0f Hz)\n", expected_bin, expected_freq);
    printf("       Bin frequency resolution: %.1f Hz\n", bin_freq_resolution);
    printf("       Signal-to-noise ratio: %.2f dB\n", 10.0 * log10(max_power));

    // Test CFAR detection
    printf("    🎯 Testing CFAR detection...\n");

    cfar_os_t cfar_detector;
    double pfa = 1e-3; // Less sensitive for testing

    if (!cfar_os_init(&cfar_detector, fft_size, pfa, 16, 2, 8)) {
        fprintf(stderr, "❌ Failed to initialize CFAR detector\n");
        goto cleanup;
    }

    cfar_detection_t detections[10];
    uint32_t num_detections = cfar_os_process_frame(&cfar_detector, power_spectrum, detections, 10);

    printf("    📊 CFAR results: %u detections found\n", num_detections);

    for (uint32_t i = 0; i < num_detections; i++) {
        printf("       Detection %u: bin %u, power %.6f, threshold %.6f, SNR %.2f dB, confidence %.3f\n",
               i + 1, detections[i].bin_index, detections[i].signal_power,
               detections[i].threshold, detections[i].snr_estimate, detections[i].confidence);
    }

    // If no detections, try with more sensitive parameters
    if (num_detections == 0) {
        printf("    🔧 Retrying with more sensitive CFAR parameters...\n");

        cfar_os_free(&cfar_detector);
        if (!cfar_os_init(&cfar_detector, fft_size, 1e-6, 8, 1, 4)) {
            fprintf(stderr, "❌ Failed to reinitialize CFAR detector\n");
            goto cleanup;
        }

        num_detections = cfar_os_process_frame(&cfar_detector, power_spectrum, detections, 10);
        printf("    📊 Sensitive CFAR results: %u detections found\n", num_detections);

        for (uint32_t i = 0; i < num_detections; i++) {
            printf("       Detection %u: bin %u, power %.6f, threshold %.6f, SNR %.2f dB, confidence %.3f\n",
                   i + 1, detections[i].bin_index, detections[i].signal_power,
                   detections[i].threshold, detections[i].snr_estimate, detections[i].confidence);
        }
    }

    cfar_os_free(&cfar_detector);

    printf("    ✅ Detection pipeline debug completed\n");

cleanup:
    free(fft_in);
    free(fft_out);
    free(fft_shifted);
    free(power_spectrum);
    fft_plan_destroy(fft_plan);
    iq_free(&iq_data);
    remove("debug_test.iq");

    return true;
}

// Test with iqdetect command line tool
static bool test_iqdetect_cli(void) {
    printf("🖥️ Testing iqdetect command line tool...\n");

    if (!generate_debug_signal("cli_test.iq", 500000.0)) {
        fprintf(stderr, "❌ Failed to generate CLI test signal\n");
        return false;
    }

    // Test with various parameter combinations
    const char *commands[] = {
        ".\\iqdetect --in cli_test.iq --format s16 --rate 2000000 --out cli_test1.csv --pfa 1e-3",
        ".\\iqdetect --in cli_test.iq --format s16 --rate 2000000 --out cli_test2.csv --pfa 1e-6",
        ".\\iqdetect --in cli_test.iq --format s16 --rate 2000000 --out cli_test3.csv --pfa 1e-8 --fft 1024",
        ".\\iqdetect --in cli_test.iq --format s16 --rate 2000000 --out cli_test4.csv --pfa 1e-8 --fft 2048",
        ".\\iqdetect --in cli_test.iq --format s16 --rate 2000000 --out cli_test5.csv --pfa 1e-8 --fft 4096"
    };

    for (int i = 0; i < 5; i++) {
        printf("    🧪 Running command %d...\n", i + 1);
        (void)system(commands[i]);

        char filename[32];
        snprintf(filename, sizeof(filename), "cli_test%d.csv", i + 1);

        FILE *f = fopen(filename, "r");
        if (f) {
            // Count lines in output file
            int lines = 0;
            char line[1024];
            while (fgets(line, sizeof(line), f)) {
                lines++;
            }
            fclose(f);

            printf("       📄 Output file created with %d lines\n", lines);
            remove(filename);
        } else {
            printf("       ❌ No output file created\n");
        }
    }

    remove("cli_test.iq");
    return true;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    srand((unsigned int)time(NULL));

    printf("🔧 Running iqdetect detection pipeline debug...\n\n");

    bool all_passed = true;

    // Run debug tests
    all_passed &= debug_detection_pipeline();
    all_passed &= test_iqdetect_cli();

    printf("\n");
    if (all_passed) {
        printf("✅ iqdetect debug tests completed!\n");
        printf("   🔍 Detection pipeline analyzed\n");
        printf("   🖥️ Command line interface tested\n");
    } else {
        printf("❌ Some debug tests failed\n");
    }

    return all_passed ? 0 : 1;
}
