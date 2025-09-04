/*
 * IQ Lab - I/O IQ Unit Tests
 *
 * Tests for IQ data loading, saving, and format conversion
 * Covers all supported formats: s8, s16, WAV
 * Tests error conditions and edge cases
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../../src/iq_core/io_iq.h"

// Test counters
static int tests_run = 0;
static int tests_passed = 0;

// Test helper macros
#define TEST_START(name) \
    printf("Running: %s\n", name); \
    tests_run++;

#define TEST_PASS() \
    printf("  ✅ PASSED\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    printf("  ❌ FAILED: %s\n", msg);

#define TEST_END() \
    printf("  Tests run: %d, Passed: %d\n\n", tests_run, tests_passed);

// Test format detection
void test_format_detection() {
    TEST_START("Format Detection");

    // Test s8 detection
    iq_format_t fmt = iq_detect_format("test.s8");
    if (fmt == IQ_FORMAT_S8) {
        TEST_PASS();
    } else {
        TEST_FAIL("s8 format not detected correctly");
    }

    // Test s16 detection
    fmt = iq_detect_format("test.s16");
    if (fmt == IQ_FORMAT_S16) {
        TEST_PASS();
    } else {
        TEST_FAIL("s16 format not detected correctly");
    }

    // Test WAV detection
    fmt = iq_detect_format("test.wav");
    if (fmt == IQ_FORMAT_S16) {
        TEST_PASS();
    } else {
        TEST_FAIL("WAV format not detected correctly");
    }

    TEST_END();
}

// Test s8 to float conversion
void test_s8_to_float_conversion() {
    TEST_START("S8 to Float Conversion");

    // Test data: alternating positive/negative values
    // When cast to int8_t: {128,64,192,0} becomes {-128,64,-64,0}
    uint8_t raw_s8[] = {128, 64, 192, 0};  // -128, +64, -64, 0 in signed
    float output[4];

    bool success = iq_convert_to_float(raw_s8, sizeof(raw_s8), IQ_FORMAT_S8, output, 2);

    if (!success) {
        TEST_FAIL("Conversion failed");
        return;
    }

    // Check conversion results
    // raw_s8[0] = 128 (-128 in signed) -> -128/128 = -1.0
    // raw_s8[1] = 64 (+64 in signed) -> +64/128 = +0.5
    // raw_s8[2] = 192 (-64 in signed) -> -64/128 = -0.5
    // raw_s8[3] = 0 (0 in signed) -> 0/128 = 0.0

    int correct = 0;
    if (fabs(output[0] + 1.0) < 1e-6) correct++;      // I component
    if (fabs(output[1] - 0.5) < 1e-6) correct++;      // Q component
    if (fabs(output[2] + 0.5) < 1e-6) correct++;      // I component
    if (fabs(output[3] - 0.0) < 1e-6) correct++;      // Q component

    if (correct == 4) {
        TEST_PASS();
        printf("    ✅ S8 conversion: [128,64,192,0] -> [-1.0, 0.5, -0.5, 0.0]\n");
    } else {
        TEST_FAIL("S8 conversion values incorrect");
        printf("    Expected: [-1.0, 0.5, -0.5, 0.0]\n");
        printf("    Got: [%.3f, %.3f, %.3f, %.3f]\n", output[0], output[1], output[2], output[3]);
    }

    TEST_END();
}

// Test s16 to float conversion
void test_s16_to_float_conversion() {
    TEST_START("S16 to Float Conversion");

    // Create test data
    int16_t raw_s16[] = {0, 16384, -16384, -32768};  // 0, +0.5, -0.5, -1.0 in float
    float output[4];

    bool success = iq_convert_to_float((uint8_t*)raw_s16, sizeof(raw_s16), IQ_FORMAT_S16, output, 2);

    if (!success) {
        TEST_FAIL("Conversion failed");
        return;
    }

    int correct = 0;
    if (fabs(output[0] - 0.0) < 1e-5) correct++;        // I component
    if (fabs(output[1] - 0.5) < 1e-5) correct++;        // Q component
    if (fabs(output[2] + 0.5) < 1e-5) correct++;        // I component
    if (fabs(output[3] + 1.0) < 1e-5) correct++;        // Q component

    if (correct == 4) {
        TEST_PASS();
        printf("    ✅ S16 conversion: [0,16384,-16384,-32768] -> [0.0, 0.5, -0.5, -1.0]\n");
    } else {
        TEST_FAIL("S16 conversion values incorrect");
        printf("    Expected: [0.0, 0.5, -0.5, -1.0]\n");
        printf("    Got: [%.3f, %.3f, %.3f, %.3f]\n", output[0], output[1], output[2], output[3]);
    }

    TEST_END();
}

// Test buffer overflow protection
void test_buffer_overflow_protection() {
    TEST_START("Buffer Overflow Protection");

    // Test with too small output buffer
    uint8_t raw_data[] = {0, 64, 128, 192};  // 2 complex samples
    float small_output[2];  // Only space for 1 complex sample

    bool success = iq_convert_to_float(raw_data, sizeof(raw_data), IQ_FORMAT_S8, small_output, 1);

    if (!success) {
        TEST_PASS();
        printf("    ✅ Buffer overflow correctly prevented\n");
    } else {
        TEST_FAIL("Buffer overflow not prevented");
    }

    TEST_END();
}

// Test IQ reader functionality
void test_iq_reader() {
    TEST_START("IQ Reader Functionality");

    // Create a temporary test file
    const char* test_filename = "test_iq.tmp";
    FILE* test_file = fopen(test_filename, "wb");

    if (!test_file) {
        TEST_FAIL("Could not create test file");
        return;
    }

    // Write test data: 4 complex samples (I,Q,I,Q,I,Q,I,Q)
    int16_t test_data[] = {1000, 2000, -1000, -2000, 3000, 4000, -3000, -4000};
    fwrite(test_data, sizeof(int16_t), 8, test_file);
    fclose(test_file);

    // Test reader
    iq_reader_t reader;
    if (!iq_reader_init(&reader, test_filename, IQ_FORMAT_S16)) {
        TEST_FAIL("Reader initialization failed");
        remove(test_filename);
        return;
    }

    // Read samples
    float buffer[8];
    size_t samples_read = iq_read_samples(&reader, buffer, 4);

    if (samples_read != 4) {
        TEST_FAIL("Wrong number of samples read");
        iq_reader_close(&reader);
        remove(test_filename);
        return;
    }

    // Check conversion accuracy
    int correct = 0;
    float expected[] = {1000.0/32768.0, 2000.0/32768.0, -1000.0/32768.0, -2000.0/32768.0,
                       3000.0/32768.0, 4000.0/32768.0, -3000.0/32768.0, -4000.0/32768.0};

    for (int i = 0; i < 8; i++) {
        if (fabs(buffer[i] - expected[i]) < 1e-5) correct++;
    }

    if (correct == 8) {
        TEST_PASS();
        printf("    ✅ IQ reader correctly converted %zu samples\n", samples_read);
    } else {
        TEST_FAIL("IQ reader conversion incorrect");
    }

    iq_reader_close(&reader);
    remove(test_filename);

    TEST_END();
}

// Test error conditions
void test_error_conditions() {
    TEST_START("Error Conditions");

    int errors_handled = 0;

    // Test NULL filename
    iq_data_t data1 = {0};
    if (!iq_load_file(NULL, &data1)) {
        errors_handled++;
        printf("    ✅ NULL filename handled\n");
    }

    // Test NULL data pointer
    if (!iq_load_file("nonexistent.iq", NULL)) {
        errors_handled++;
        printf("    ✅ NULL data pointer handled\n");
    }

    // Test nonexistent file
    iq_data_t data2 = {0};
    if (!iq_load_file("nonexistent_file.iq", &data2)) {
        errors_handled++;
        printf("    ✅ Nonexistent file handled\n");
    }

    if (errors_handled == 3) {
        TEST_PASS();
    } else {
        TEST_FAIL("Some error conditions not handled");
        printf("    Handled: %d/3\n", errors_handled);
    }

    TEST_END();
}

// Test file size detection
void test_file_size_detection() {
    TEST_START("File Size Detection");

    // Create a test file with known size
    const char* test_file = "size_test.tmp";
    FILE* f = fopen(test_file, "wb");

    if (!f) {
        TEST_FAIL("Could not create test file");
        return;
    }

    // Write 100 bytes
    uint8_t data[100];
    memset(data, 0xAA, sizeof(data));
    fwrite(data, 1, sizeof(data), f);
    fclose(f);

    // Test size detection
    size_t detected_size = iq_get_file_size(test_file);

    if (detected_size == 100) {
        TEST_PASS();
        printf("    ✅ File size correctly detected: %zu bytes\n", detected_size);
    } else {
        TEST_FAIL("File size detection failed");
        printf("    Expected: 100, Got: %zu\n", detected_size);
    }

    remove(test_file);
    TEST_END();
}

// Main test runner
int main(void) {
    printf("========================================\n");
    printf("IQ Lab - I/O IQ Unit Tests\n");
    printf("========================================\n\n");

    test_format_detection();
    test_s8_to_float_conversion();
    test_s16_to_float_conversion();
    test_buffer_overflow_protection();
    test_iq_reader();
    test_error_conditions();
    test_file_size_detection();

    printf("========================================\n");
    printf("Test Summary: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return tests_passed == tests_run ? EXIT_SUCCESS : EXIT_FAILURE;
}
