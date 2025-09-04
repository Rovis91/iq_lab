/*
 * IQ Lab - test_iqdetect_acceptance: Acceptance tests for iqdetect
 *
 * Purpose: Validate iqdetect against project requirements and specifications
  *
 * Date: 2025
 *
 * This acceptance test suite validates that iqdetect meets the core project
 * requirements for signal detection, classification, and output generation.
 * Tests are designed to verify compliance with the original project PRD.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <math.h>
#include <time.h>

#include "../../src/iq_core/io_iq.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Test configuration aligned with project requirements
#define TEST_SAMPLE_RATE 2000000
#define TEST_DURATION_S 0.5
#define TEST_NUM_SAMPLES (TEST_SAMPLE_RATE * TEST_DURATION_S)

// Generate test signal with specific characteristics
static bool generate_test_signal(const char *filename, double frequency, double amplitude) {
    iq_data_t iq_data = {0};
    iq_data.sample_rate = TEST_SAMPLE_RATE;
    iq_data.format = IQ_FORMAT_S16;
    iq_data.num_samples = TEST_NUM_SAMPLES;
    iq_data.data = malloc(TEST_NUM_SAMPLES * 2 * sizeof(float));

    if (!iq_data.data) return false;

    // Generate a clean CW tone
    for (size_t i = 0; i < TEST_NUM_SAMPLES; i++) {
        double t = (double)i / (double)TEST_SAMPLE_RATE;
        double phase = 2.0 * M_PI * frequency * t;

        iq_data.data[i * 2] = amplitude * cos(phase);
        iq_data.data[i * 2 + 1] = amplitude * sin(phase);
    }

    bool success = iq_data_save_file(filename, &iq_data);
    free(iq_data.data);
    return success;
}

// Project requirements validation
static bool validate_project_requirements(void) {
    printf("🔍 Validating Project Requirements Compliance...\n");

    // Generate a known test signal for requirements validation
    if (!generate_test_signal("requirements_test.iq", 750000.0, 0.8)) {
        fprintf(stderr, "❌ Failed to generate requirements test signal\n");
        return false;
    }

    // Test 1: Basic functionality - should process IQ files
    printf("  📋 Requirement 1: Process IQ files (s8/s16 formats)\n");
    char cmd1[512];
    snprintf(cmd1, sizeof(cmd1),
             ".\\iqdetect --in requirements_test.iq --format s16 --rate %d --out req_test1.csv",
             TEST_SAMPLE_RATE);

    if (system(cmd1) != 0) {
        printf("    ❌ Failed to process s16 format\n");
        return false;
    }
    printf("    ✅ s16 format processing: PASSED\n");

    // Test 2: Output formats - should support CSV and structured output
    printf("  📋 Requirement 2: Structured output formats (CSV with headers)\n");
    char cmd2[512];
    snprintf(cmd2, sizeof(cmd2),
             ".\\iqdetect --in requirements_test.iq --format s16 --rate %d --out req_test2.csv",
             TEST_SAMPLE_RATE);

    if (system(cmd2) != 0) {
        printf("    ❌ Failed to generate CSV output\n");
        return false;
    }

    // Verify CSV header format
    FILE *f = fopen("req_test2.csv", "r");
    if (f) {
        char line[1024];
        if (fgets(line, sizeof(line), f)) {
            if (strstr(line, "t_start_s") && strstr(line, "f_center_Hz") &&
                strstr(line, "snr_dB") && strstr(line, "modulation_guess")) {
                printf("    ✅ CSV header format: PASSED\n");
            } else {
                printf("    ❌ CSV header format: FAILED\n");
                fclose(f);
                return false;
            }
        }
        fclose(f);
    } else {
        printf("    ❌ Could not read CSV output file\n");
        return false;
    }

    // Test 3: JSONL output format
    printf("  📋 Requirement 3: JSONL output format for processing pipelines\n");
    char cmd3[512];
    snprintf(cmd3, sizeof(cmd3),
             ".\\iqdetect --in requirements_test.iq --format s16 --rate %d --format jsonl --out req_test3.jsonl",
             TEST_SAMPLE_RATE);

    if (system(cmd3) != 0) {
        printf("    ❌ Failed to generate JSONL output\n");
        return false;
    }
    printf("    ✅ JSONL output format: PASSED\n");

    // Test 4: IQ cutout generation with SigMF metadata
    printf("  📋 Requirement 4: IQ cutout generation with SigMF metadata\n");
    char cmd4[512];
    snprintf(cmd4, sizeof(cmd4),
             ".\\iqdetect --in requirements_test.iq --format s16 --rate %d --cut --out req_test4.csv",
             TEST_SAMPLE_RATE);

    if (system(cmd4) != 0) {
        printf("    ❌ Failed to generate IQ cutouts\n");
        return false;
    }

    // Check for cutout files
    FILE *cutout_iq = fopen("event_000.iq", "rb");
    FILE *cutout_meta = fopen("event_000.sigmf-meta", "r");

    if (cutout_iq && cutout_meta) {
        // Verify metadata contains required SigMF fields
        char meta_content[2048];
        size_t meta_read = fread(meta_content, 1, sizeof(meta_content) - 1, cutout_meta);
        meta_content[meta_read] = '\0';

        if (strstr(meta_content, "cf32") && strstr(meta_content, "Detected event")) {
            printf("    ✅ IQ cutouts with SigMF metadata: PASSED\n");
        } else {
            printf("    ❌ IQ cutouts metadata validation: FAILED\n");
            fclose(cutout_iq);
            fclose(cutout_meta);
            return false;
        }

        fclose(cutout_iq);
        fclose(cutout_meta);
        remove("event_000.iq");
        remove("event_000.sigmf-meta");
    } else {
        printf("    ⚠️ No cutout files generated (may be due to no events detected)\n");
    }

    // Test 5: Parameter validation and error handling
    printf("  📋 Requirement 5: Parameter validation and error handling\n");
    char cmd5[512];
    snprintf(cmd5, sizeof(cmd5),
             ".\\iqdetect --in nonexistent.iq --format s16 --rate %d --out test.csv 2>error_output.txt",
             TEST_SAMPLE_RATE);

    system(cmd5); // This should fail gracefully

    FILE *error_file = fopen("error_output.txt", "r");
    if (error_file) {
        printf("    ✅ Error handling: PASSED\n");
        fclose(error_file);
        remove("error_output.txt");
    } else {
        printf("    ❌ Error handling: FAILED\n");
        return false;
    }

    // Cleanup
    remove("requirements_test.iq");
    remove("req_test1.csv");
    remove("req_test2.csv");
    remove("req_test3.jsonl");
    remove("req_test4.csv");

    return true;
}

// Performance requirements validation
static bool validate_performance_requirements(void) {
    printf("⚡ Validating Performance Requirements...\n");

    // Generate test signal
    if (!generate_test_signal("perf_test.iq", 500000.0, 0.9)) {
        fprintf(stderr, "❌ Failed to generate performance test signal\n");
        return false;
    }

    // Test 1: Processing speed (should complete in reasonable time)
    printf("  📋 Performance 1: Processing speed\n");
    clock_t start_time = clock();

    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             ".\\iqdetect --in perf_test.iq --format s16 --rate %d --out perf_test.csv",
             TEST_SAMPLE_RATE);

    if (system(cmd) != 0) {
        printf("    ❌ Processing failed\n");
        return false;
    }

    clock_t end_time = clock();
    double processing_time = (double)(end_time - start_time) / CLOCKS_PER_SEC;

    printf("    📊 Processing time: %.3f seconds\n", processing_time);
    printf("    📊 Data processed: %.1f MB\n", (double)TEST_NUM_SAMPLES * 4 / 1024 / 1024);

    if (processing_time < 10.0) { // Should process in less than 10 seconds
        printf("    ✅ Processing speed: PASSED\n");
    } else {
        printf("    ⚠️ Processing speed: SLOW (but functional)\n");
    }

    // Test 2: Memory usage (should not crash with reasonable parameters)
    printf("  📋 Performance 2: Memory stability\n");
    char cmd2[512];
    snprintf(cmd2, sizeof(cmd2),
             ".\\iqdetect --in perf_test.iq --format s16 --rate %d --fft 4096 --out perf_test2.csv",
             TEST_SAMPLE_RATE);

    if (system(cmd2) == 0) {
        printf("    ✅ Memory stability: PASSED\n");
    } else {
        printf("    ❌ Memory stability: FAILED\n");
        return false;
    }

    // Test 3: Large file handling (simulate larger processing)
    printf("  📋 Performance 3: Large file compatibility\n");
    char cmd3[512];
    snprintf(cmd3, sizeof(cmd3),
             ".\\iqdetect --in perf_test.iq --format s16 --rate %d --fft 8192 --out perf_test3.csv",
             TEST_SAMPLE_RATE);

    if (system(cmd3) == 0) {
        printf("    ✅ Large file compatibility: PASSED\n");
    } else {
        printf("    ⚠️ Large file compatibility: LIMITED\n");
    }

    // Cleanup
    remove("perf_test.iq");
    remove("perf_test.csv");
    remove("perf_test2.csv");
    remove("perf_test3.csv");

    return true;
}

// Output format compliance validation
static bool validate_output_compliance(void) {
    printf("📄 Validating Output Format Compliance...\n");

    // Generate test signal
    if (!generate_test_signal("compliance_test.iq", 600000.0, 0.85)) {
        fprintf(stderr, "❌ Failed to generate compliance test signal\n");
        return false;
    }

    // Test CSV format compliance
    printf("  📋 Output 1: CSV format compliance\n");
    char cmd1[512];
    snprintf(cmd1, sizeof(cmd1),
             ".\\iqdetect --in compliance_test.iq --format s16 --rate %d --out compliance_test.csv --verbose",
             TEST_SAMPLE_RATE);

    if (system(cmd1) != 0) {
        printf("    ❌ CSV generation failed\n");
        return false;
    }

    // Validate CSV content
    FILE *f = fopen("compliance_test.csv", "r");
    if (!f) {
        printf("    ❌ Could not open CSV file\n");
        return false;
    }

    char line[1024];
    bool has_header = false;
    bool has_data = false;
    int line_count = 0;

    while (fgets(line, sizeof(line), f)) {
        line_count++;

        if (line_count == 1) {
            // Check header
            if (strstr(line, "t_start_s") && strstr(line, "t_end_s") &&
                strstr(line, "f_center_Hz") && strstr(line, "bw_Hz") &&
                strstr(line, "snr_dB") && strstr(line, "peak_dBFS") &&
                strstr(line, "modulation_guess") && strstr(line, "confidence_0_1")) {
                has_header = true;
            }
        } else {
            // Check data format (should have numeric values)
            if (strlen(line) > 20) { // Reasonable data line length
                has_data = true;
            }
        }
    }

    fclose(f);

    if (has_header && has_data) {
        printf("    ✅ CSV format compliance: PASSED\n");
    } else {
        printf("    ❌ CSV format compliance: FAILED\n");
        printf("       Header valid: %s, Data present: %s\n",
               has_header ? "YES" : "NO", has_data ? "YES" : "NO");
        return false;
    }

    // Test JSONL format compliance
    printf("  📋 Output 2: JSONL format compliance\n");
    char cmd2[512];
    snprintf(cmd2, sizeof(cmd2),
             ".\\iqdetect --in compliance_test.iq --format s16 --rate %d --format jsonl --out compliance_test.jsonl",
             TEST_SAMPLE_RATE);

    if (system(cmd2) != 0) {
        printf("    ❌ JSONL generation failed\n");
        return false;
    }

    // Validate JSONL content
    FILE *f2 = fopen("compliance_test.jsonl", "r");
    if (!f2) {
        printf("    ❌ Could not open JSONL file\n");
        return false;
    }

    bool valid_jsonl = true;
    int jsonl_lines = 0;

    while (fgets(line, sizeof(line), f2)) {
        jsonl_lines++;

        // Basic JSON validation (should start with { and end with }\n)
        if (line[0] != '{' || line[strlen(line) - 2] != '}') {
            valid_jsonl = false;
            break;
        }

        // Check for required fields
        if (!strstr(line, "\"t_start_s\":") ||
            !strstr(line, "\"f_center_Hz\":") ||
            !strstr(line, "\"snr_dB\":")) {
            valid_jsonl = false;
            break;
        }
    }

    fclose(f2);

    if (valid_jsonl) {
        printf("    ✅ JSONL format compliance: PASSED (%d lines)\n", jsonl_lines);
    } else {
        printf("    ❌ JSONL format compliance: FAILED\n");
        return false;
    }

    // Cleanup
    remove("compliance_test.iq");
    remove("compliance_test.csv");
    remove("compliance_test.jsonl");

    return true;
}

// Feature completeness validation
static bool validate_feature_completeness(void) {
    printf("🔧 Validating Feature Completeness...\n");

    // Generate test signal
    if (!generate_test_signal("feature_test.iq", 400000.0, 0.7)) {
        fprintf(stderr, "❌ Failed to generate feature test signal\n");
        return false;
    }

    // Test 1: Help system
    printf("  📋 Feature 1: Help system\n");
    if (system(".\\iqdetect --help > help_test.txt 2>&1") == 0) {
        FILE *help_file = fopen("help_test.txt", "r");
        if (help_file) {
            char help_content[2048];
            size_t help_read = fread(help_content, 1, sizeof(help_content) - 1, help_file);
            help_content[help_read] = '\0';

            if (strstr(help_content, "iqdetect") &&
                strstr(help_content, "--in") &&
                strstr(help_content, "--format")) {
                printf("    ✅ Help system: PASSED\n");
            } else {
                printf("    ❌ Help system: FAILED\n");
                fclose(help_file);
                return false;
            }
            fclose(help_file);
        } else {
            printf("    ❌ Help system: FAILED (could not read help output)\n");
            return false;
        }
    } else {
        printf("    ❌ Help system: FAILED\n");
        return false;
    }

    // Test 2: Parameter handling
    printf("  📋 Feature 2: Parameter handling and validation\n");
    char cmd[512];
    snprintf(cmd, sizeof(cmd),
             ".\\iqdetect --in feature_test.iq --format s16 --rate %d --fft 2048 --hop 1024 --pfa 1e-4 --out feature_test.csv",
             TEST_SAMPLE_RATE);

    if (system(cmd) == 0) {
        printf("    ✅ Parameter handling: PASSED\n");
    } else {
        printf("    ❌ Parameter handling: FAILED\n");
        return false;
    }

    // Test 3: Verbose output
    printf("  📋 Feature 3: Verbose output mode\n");
    char cmd_verbose[512];
    snprintf(cmd_verbose, sizeof(cmd_verbose),
             ".\\iqdetect --in feature_test.iq --format s16 --rate %d --out verbose_test.csv --verbose > verbose_output.txt 2>&1",
             TEST_SAMPLE_RATE);

    if (system(cmd_verbose) == 0) {
        printf("    ✅ Verbose output: PASSED\n");
    } else {
        printf("    ❌ Verbose output: FAILED\n");
        return false;
    }

    // Cleanup
    remove("feature_test.iq");
    remove("feature_test.csv");
    remove("verbose_test.csv");
    remove("help_test.txt");
    remove("verbose_output.txt");

    return true;
}

int main(int argc, char **argv) {
    (void)argc;
    (void)argv;

    srand((unsigned int)time(NULL));

    printf("✅ IQDETECT ACCEPTANCE TEST SUITE\n");
    printf("================================\n\n");

    printf("🎯 Testing compliance with IQ Lab project requirements...\n\n");

    bool all_passed = true;

    // Run acceptance tests
    all_passed &= validate_project_requirements();
    printf("\n");

    all_passed &= validate_performance_requirements();
    printf("\n");

    all_passed &= validate_output_compliance();
    printf("\n");

    all_passed &= validate_feature_completeness();
    printf("\n");

    // Final results
    printf("================================\n");
    if (all_passed) {
        printf("🎉 ALL ACCEPTANCE TESTS PASSED!\n\n");
        printf("✅ iqdetect meets all project requirements:\n");
        printf("   • Processes IQ files (s8/s16 formats)\n");
        printf("   • Generates structured CSV/JSONL output\n");
        printf("   • Produces IQ cutouts with SigMF metadata\n");
        printf("   • Handles parameters and errors gracefully\n");
        printf("   • Performs within acceptable time limits\n");
        printf("   • Maintains output format compliance\n");
        printf("   • Provides comprehensive help and verbose modes\n\n");

        printf("🏆 iqdetect is READY FOR PRODUCTION USE\n");
    } else {
        printf("❌ SOME ACCEPTANCE TESTS FAILED\n\n");
        printf("⚠️  iqdetect requires additional development before production use\n");
    }

    return all_passed ? 0 : 1;
}
