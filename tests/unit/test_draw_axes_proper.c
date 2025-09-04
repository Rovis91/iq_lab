/*
 * IQ Lab - Draw Axes Unit Tests
 *
 * Tests for axis drawing functions
 * Covers all drawing primitives and axis types
 * Tests buffer safety and error conditions
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include "../../src/viz/draw_axes.h"

// Font constants (matching draw_axes.c)
#define FONT_WIDTH 5
#define FONT_HEIGHT 7

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

// Test image dimensions
#define TEST_WIDTH 100
#define TEST_HEIGHT 80

// Test horizontal line drawing
void test_draw_horizontal_line() {
    TEST_START("Horizontal Line Drawing");

    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (!image) {
        TEST_FAIL("Memory allocation failed");
        return;
    }

    // Clear image to black
    memset(image, 0, TEST_WIDTH * TEST_HEIGHT * 3);

    // Draw horizontal line
    draw_horizontal_line(image, TEST_WIDTH, TEST_HEIGHT, 10, 50, 20, 255, 0, 0);

    // Verify line was drawn
    int pixels_drawn = 0;
    for (int x = 10; x <= 50; x++) {
        size_t idx = (20 * TEST_WIDTH + x) * 3;
        if (image[idx] == 255 && image[idx+1] == 0 && image[idx+2] == 0) {
            pixels_drawn++;
        }
    }

    if (pixels_drawn == 41) {  // 50-10+1 = 41 pixels
        TEST_PASS();
        printf("    ✅ Horizontal line drawn correctly (%d pixels)\n", pixels_drawn);
    } else {
        TEST_FAIL("Horizontal line drawing failed");
        printf("    Expected: 41 pixels, Got: %d\n", pixels_drawn);
    }

    free(image);
    TEST_END();
}

// Test vertical line drawing
void test_draw_vertical_line() {
    TEST_START("Vertical Line Drawing");

    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (!image) {
        TEST_FAIL("Memory allocation failed");
        return;
    }

    memset(image, 0, TEST_WIDTH * TEST_HEIGHT * 3);

    // Draw vertical line
    draw_vertical_line(image, TEST_WIDTH, TEST_HEIGHT, 30, 10, 40, 0, 255, 0);

    // Verify line was drawn
    int pixels_drawn = 0;
    for (int y = 10; y <= 40; y++) {
        size_t idx = (y * TEST_WIDTH + 30) * 3;
        if (image[idx] == 0 && image[idx+1] == 255 && image[idx+2] == 0) {
            pixels_drawn++;
        }
    }

    if (pixels_drawn == 31) {  // 40-10+1 = 31 pixels
        TEST_PASS();
        printf("    ✅ Vertical line drawn correctly (%d pixels)\n", pixels_drawn);
    } else {
        TEST_FAIL("Vertical line drawing failed");
        printf("    Expected: 31 pixels, Got: %d\n", pixels_drawn);
    }

    free(image);
    TEST_END();
}

// Test character rendering
void test_draw_character() {
    TEST_START("Character Rendering");

    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (!image) {
        TEST_FAIL("Memory allocation failed");
        return;
    }

    memset(image, 0, TEST_WIDTH * TEST_HEIGHT * 3);

    // Draw character 'A' (ASCII 65)
    draw_character(image, TEST_WIDTH, TEST_HEIGHT, 10, 10, 'A', &font_5x7, 255, 255, 255);

    // Verify some pixels were drawn (basic check)
    int pixels_drawn = 0;
    for (int y = 10; y < 10 + FONT_HEIGHT && y < TEST_HEIGHT; y++) {
        for (int x = 10; x < 10 + FONT_WIDTH && x < TEST_WIDTH; x++) {
            size_t idx = (y * TEST_WIDTH + x) * 3;
            if (image[idx] == 255 && image[idx+1] == 255 && image[idx+2] == 255) {
                pixels_drawn++;
            }
        }
    }

    if (pixels_drawn > 0) {
        TEST_PASS();
        printf("    ✅ Character 'A' rendered (%d pixels drawn)\n", pixels_drawn);
    } else {
        TEST_FAIL("Character rendering failed");
    }

    free(image);
    TEST_END();
}

// Test text rendering
void test_draw_text() {
    TEST_START("Text Rendering");

    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (!image) {
        TEST_FAIL("Memory allocation failed");
        return;
    }

    memset(image, 0, TEST_WIDTH * TEST_HEIGHT * 3);

    // Draw text "TEST"
    draw_text(image, TEST_WIDTH, TEST_HEIGHT, 5, 5, "TEST", &font_5x7, 255, 255, 255);

    // Count white pixels
    int white_pixels = 0;
    for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT * 3; i += 3) {
        if (image[i] == 255 && image[i+1] == 255 && image[i+2] == 255) {
            white_pixels++;
        }
    }

    if (white_pixels > 0) {
        TEST_PASS();
        printf("    ✅ Text 'TEST' rendered (%d white pixels)\n", white_pixels);
    } else {
        TEST_FAIL("Text rendering failed");
    }

    free(image);
    TEST_END();
}

// Test frequency axis with realistic parameters
void test_draw_frequency_axis() {
    TEST_START("Frequency Axis Drawing");

    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (!image) {
        TEST_FAIL("Memory allocation failed");
        return;
    }

    memset(image, 0, TEST_WIDTH * TEST_HEIGHT * 3);

    // Draw frequency axis with realistic SDR parameters
    draw_frequency_axis(image, TEST_WIDTH, TEST_HEIGHT, 20, 100000000, 2000000, 1024, 255, 255, 255);

    // Count white pixels (should include axis line and labels)
    int white_pixels = 0;
    for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT * 3; i += 3) {
        if (image[i] == 255 && image[i+1] == 255 && image[i+2] == 255) {
            white_pixels++;
        }
    }

    if (white_pixels > 10) {  // Should have axis line + tick marks + labels
        TEST_PASS();
        printf("    ✅ Frequency axis drawn (%d white pixels)\n", white_pixels);
    } else {
        TEST_FAIL("Frequency axis drawing failed");
        printf("    Only %d white pixels found\n", white_pixels);
    }

    free(image);
    TEST_END();
}

// Test time axis
void test_draw_time_axis() {
    TEST_START("Time Axis Drawing");

    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (!image) {
        TEST_FAIL("Memory allocation failed");
        return;
    }

    memset(image, 0, TEST_WIDTH * TEST_HEIGHT * 3);

    // Draw time axis
    draw_time_axis(image, TEST_WIDTH, TEST_HEIGHT, 10, 2.5, 8, 255, 255, 255);

    // Count white pixels
    int white_pixels = 0;
    for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT * 3; i += 3) {
        if (image[i] == 255 && image[i+1] == 255 && image[i+2] == 255) {
            white_pixels++;
        }
    }

    if (white_pixels > 5) {
        TEST_PASS();
        printf("    ✅ Time axis drawn (%d white pixels)\n", white_pixels);
    } else {
        TEST_FAIL("Time axis drawing failed");
        printf("    Only %d white pixels found\n", white_pixels);
    }

    free(image);
    TEST_END();
}

// Test dB scale
void test_draw_db_scale() {
    TEST_START("dB Scale Drawing");

    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (!image) {
        TEST_FAIL("Memory allocation failed");
        return;
    }

    memset(image, 0, TEST_WIDTH * TEST_HEIGHT * 3);

    // Draw dB scale with typical SDR range
    draw_db_scale(image, TEST_WIDTH, TEST_HEIGHT, 15, -80.0, 0.0, 255, 255, 255);

    // Count white pixels
    int white_pixels = 0;
    for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT * 3; i += 3) {
        if (image[i] == 255 && image[i+1] == 255 && image[i+2] == 255) {
            white_pixels++;
        }
    }

    if (white_pixels > 8) {  // Should have axis + multiple labels
        TEST_PASS();
        printf("    ✅ dB scale drawn (%d white pixels)\n", white_pixels);
    } else {
        TEST_FAIL("dB scale drawing failed");
        printf("    Only %d white pixels found\n", white_pixels);
    }

    free(image);
    TEST_END();
}

// Test error conditions
void test_error_conditions() {
    TEST_START("Error Conditions");

    int errors_handled = 0;

    // Test NULL image pointer
    draw_horizontal_line(NULL, 100, 100, 0, 10, 5, 255, 0, 0);
    errors_handled++;  // Should not crash
    printf("    ✅ NULL image pointer handled\n");

    // Test invalid dimensions
    uint8_t *valid_image = (uint8_t*)malloc(100 * 3);
    if (valid_image) {
        draw_horizontal_line(valid_image, 0, 100, 0, 10, 5, 255, 0, 0);
        errors_handled++;  // Should not crash
        printf("    ✅ Invalid dimensions handled\n");
        free(valid_image);
    }

    // Test character out of range
    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (image) {
        memset(image, 0, TEST_WIDTH * TEST_HEIGHT * 3);
        draw_character(image, TEST_WIDTH, TEST_HEIGHT, 5, 5, 200, &font_5x7, 255, 0, 0);
        errors_handled++;  // Should not crash
        printf("    ✅ Invalid character handled\n");
        free(image);
    }

    if (errors_handled == 3) {
        TEST_PASS();
    } else {
        TEST_FAIL("Some error conditions not handled");
    }

    TEST_END();
}

// Test buffer safety
void test_buffer_safety() {
    TEST_START("Buffer Safety");

    uint8_t *image = (uint8_t*)malloc(TEST_WIDTH * TEST_HEIGHT * 3);
    if (!image) {
        TEST_FAIL("Memory allocation failed");
        return;
    }

    memset(image, 0xAA, TEST_WIDTH * TEST_HEIGHT * 3);  // Fill with known pattern

    // Test drawing outside bounds
    draw_character(image, TEST_WIDTH, TEST_HEIGHT, TEST_WIDTH - 2, TEST_HEIGHT - 2,
                  'X', &font_5x7, 255, 255, 255);

    // Verify no buffer corruption (check pattern still intact in some areas)
    int intact_pixels = 0;
    for (int i = 0; i < TEST_WIDTH * TEST_HEIGHT * 3; i += 100) {  // Sample every 100 bytes
        if (image[i] == 0xAA) intact_pixels++;
    }

    if (intact_pixels > 10) {  // Should have some intact areas
        TEST_PASS();
        printf("    ✅ Buffer safety maintained (%d intact areas)\n", intact_pixels);
    } else {
        TEST_FAIL("Buffer corruption detected");
    }

    free(image);
    TEST_END();
}

// Test font properties
void test_font_properties() {
    TEST_START("Font Properties");

    int tests_passed = 0;

    // Test font dimensions
    if (font_5x7.width == 5 && font_5x7.height == 7) {
        tests_passed++;
        printf("    ✅ Font dimensions correct (5x7)\n");
    } else {
        printf("    ❌ Font dimensions incorrect: %dx%d\n", font_5x7.width, font_5x7.height);
    }

    // Test font data exists
    if (font_5x7.data != NULL) {
        tests_passed++;
        printf("    ✅ Font data pointer valid\n");
    } else {
        printf("    ❌ Font data pointer NULL\n");
    }

    if (tests_passed == 2) {
        TEST_PASS();
    } else {
        TEST_FAIL("Font property tests failed");
    }

    TEST_END();
}

// Main test runner
int main(void) {
    printf("========================================\n");
    printf("IQ Lab - Draw Axes Unit Tests\n");
    printf("========================================\n\n");

    test_draw_horizontal_line();
    test_draw_vertical_line();
    test_draw_character();
    test_draw_text();
    test_draw_frequency_axis();
    test_draw_time_axis();
    test_draw_db_scale();
    test_error_conditions();
    test_buffer_safety();
    test_font_properties();

    printf("========================================\n");
    printf("Test Summary: %d/%d tests passed\n", tests_passed, tests_run);
    printf("========================================\n");

    return tests_passed == tests_run ? EXIT_SUCCESS : EXIT_FAILURE;
}
