/*
 * IQ Lab - SigMF Unit Tests
 *
 * Tests for SigMF metadata reading/writing functionality
 * Covers edge cases, error handling, and data validation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/iq_core/io_sigmf.h"

// Test counters
static int tests_run = 0;
static int tests_passed = 0;
static int tests_failed = 0;

// Test helper macros
#define TEST_START(name) \
    printf("Running: %s\n", name); \
    tests_run++;

#define TEST_PASS() \
    printf("  ✅ PASSED\n"); \
    tests_passed++;

#define TEST_FAIL(msg) \
    do { \
        printf("  ❌ FAILED: %s\n", msg); \
        tests_failed++; \
    } while (0)

#define TEST_END() \
    printf("\n");

// Test metadata initialization
void test_metadata_initialization() {
    TEST_START("Metadata Initialization");

    sigmf_metadata_t metadata = {0};
    sigmf_init_metadata(&metadata);

    // Check default values
    if (strcmp(metadata.global.version, "1.2.0") == 0 &&
        strcmp(metadata.global.datatype, "ci16") == 0 &&
        metadata.global.sample_rate == 1000000 &&
        metadata.global.frequency == 100000000) {
        TEST_PASS();
    } else {
        TEST_FAIL("Default values incorrect");
    }

    sigmf_free_metadata(&metadata);
    TEST_END();
}

// Test metadata creation with parameters
void test_metadata_creation() {
    TEST_START("Metadata Creation with Parameters");

    sigmf_metadata_t metadata = {0};

    bool success = sigmf_create_basic_metadata(&metadata,
                                             "ci16",
                                             2000000,        // 2 MHz
                                             145000000,      // 145 MHz
                                             "Test signal",
                                             "iq_lab_test");

    if (success &&
        strcmp(metadata.global.datatype, "ci16") == 0 &&
        metadata.global.sample_rate == 2000000 &&
        metadata.global.frequency == 145000000 &&
        strcmp(metadata.global.description, "Test signal") == 0 &&
        strcmp(metadata.global.author, "iq_lab_test") == 0) {
        TEST_PASS();
    } else {
        TEST_FAIL("Metadata creation failed or values incorrect");
    }

    sigmf_free_metadata(&metadata);
    TEST_END();
}

// Test filename generation
void test_filename_generation() {
    TEST_START("SigMF Filename Generation");

    char meta_filename[512];

    // Test normal case
    sigmf_get_meta_filename("capture.iq", meta_filename, sizeof(meta_filename));
    if (strcmp(meta_filename, "capture.sigmf-meta") == 0) {
        TEST_PASS();
    } else {
        TEST_FAIL("Normal filename generation failed");
    }

    // Test file without extension
    sigmf_get_meta_filename("capture", meta_filename, sizeof(meta_filename));
    if (strcmp(meta_filename, "capture.sigmf-meta") == 0) {
        TEST_PASS();
    } else {
        TEST_FAIL("No extension filename generation failed");
    }

    // Test file with multiple dots
    sigmf_get_meta_filename("capture.test.iq", meta_filename, sizeof(meta_filename));
    if (strcmp(meta_filename, "capture.test.sigmf-meta") == 0) {
        TEST_PASS();
    } else {
        TEST_FAIL("Multiple dots filename generation failed");
    }

    TEST_END();
}


// Test datatype parsing
void test_datatype_parsing() {
    TEST_START("Datatype Parsing");

    if (sigmf_parse_datatype("ci16") == SIGMF_DATATYPE_CI16 &&
        sigmf_parse_datatype("ci8") == SIGMF_DATATYPE_CI8 &&
        sigmf_parse_datatype("cf32") == SIGMF_DATATYPE_CF32 &&
        sigmf_parse_datatype("invalid") == SIGMF_DATATYPE_CI16) {  // Default fallback
        TEST_PASS();
    } else {
        TEST_FAIL("Datatype parsing failed");
    }

    TEST_END();
}

// Test datatype string conversion
void test_datatype_to_string() {
    TEST_START("Datatype to String Conversion");

    if (strcmp(sigmf_datatype_to_string(SIGMF_DATATYPE_CI16), "ci16") == 0 &&
        strcmp(sigmf_datatype_to_string(SIGMF_DATATYPE_CI8), "ci8") == 0 &&
        strcmp(sigmf_datatype_to_string(SIGMF_DATATYPE_CF32), "cf32") == 0) {
        TEST_PASS();
    } else {
        TEST_FAIL("Datatype to string conversion failed");
    }

    TEST_END();
}

// Test capture management
void test_capture_management() {
    TEST_START("Capture Segment Management");

    sigmf_metadata_t metadata = {0};
    sigmf_init_metadata(&metadata);

    // Add captures
    bool success1 = sigmf_add_capture(&metadata, 0, 100000000, "2024-01-01T00:00:00Z");
    bool success2 = sigmf_add_capture(&metadata, 1000000, 100050000, "2024-01-01T00:00:01Z");

    if (success1 && success2 && metadata.num_captures == 2) {
        // Check capture data
        if (metadata.captures[0].sample_start == 0 &&
            metadata.captures[0].frequency == 100000000 &&
            metadata.captures[1].sample_start == 1000000 &&
            metadata.captures[1].frequency == 100050000) {
            TEST_PASS();
        } else {
            TEST_FAIL("Capture data incorrect");
        }
    } else {
        TEST_FAIL("Capture addition failed");
    }

    sigmf_free_metadata(&metadata);
    TEST_END();
}

// Test annotation management
void test_annotation_management() {
    TEST_START("Annotation Management");

    sigmf_metadata_t metadata = {0};
    sigmf_init_metadata(&metadata);

    // Add annotation
    bool success = sigmf_add_annotation(&metadata,
                                      1000,    // sample_start
                                      5000,    // sample_count
                                      99000000, // freq_lower
                                      101000000, // freq_upper
                                      "Test signal");

    if (success && metadata.num_annotations == 1) {
        // Check annotation data
        if (metadata.annotations[0].sample_start == 1000 &&
            metadata.annotations[0].sample_count == 5000 &&
            metadata.annotations[0].freq_lower_edge == 99000000 &&
            strcmp(metadata.annotations[0].label, "Test signal") == 0) {
            TEST_PASS();
        } else {
            TEST_FAIL("Annotation data incorrect");
        }
    } else {
        TEST_FAIL("Annotation addition failed");
    }

    sigmf_free_metadata(&metadata);
    TEST_END();
}

// Test error handling
void test_error_handling() {
    TEST_START("Error Handling");

    sigmf_metadata_t metadata = {0};

    // Test NULL pointers
    bool result1 = sigmf_create_basic_metadata(NULL, "ci16", 1000000, 100000000, "test", "test");
    bool result2 = sigmf_read_metadata(NULL, &metadata);
    bool result3 = sigmf_write_metadata(NULL, &metadata);

    if (!result1 && !result2 && !result3) {
        TEST_PASS();
    } else {
        TEST_FAIL("NULL pointer handling failed");
    }

    sigmf_free_metadata(&metadata);
    TEST_END();
}

// Main test runner
int main() {
    printf("=====================================\n");
    printf("IQ Lab - SigMF Unit Tests\n");
    printf("=====================================\n\n");

    // Run all tests
    test_metadata_initialization();
    test_metadata_creation();
    test_filename_generation();
    test_datatype_parsing();
    test_datatype_to_string();
    test_capture_management();
    test_annotation_management();
    test_error_handling();

    // Summary
    printf("=====================================\n");
    printf("Test Results: %d/%d passed\n", tests_passed, tests_run);
    printf("=====================================\n");

    if (tests_failed == 0) {
        printf("🎉 All SigMF tests PASSED!\n");
        return EXIT_SUCCESS;
    } else {
        printf("❌ Some SigMF tests FAILED!\n");
        return EXIT_FAILURE;
    }
}
