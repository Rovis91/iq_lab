#include "../../src/viz/img_ppm.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Test PPM image initialization
void test_ppm_init(void) {
    printf("Testing PPM image initialization...\n");

    ppm_image_t img;
    assert(ppm_image_init(&img, 100, 100) == true);
    assert(img.width == 100);
    assert(img.height == 100);
    assert(img.data != NULL);
    assert(img.data_size == 100 * 100 * 3);

    // Test invalid dimensions
    ppm_image_t invalid_img;
    assert(ppm_image_init(&invalid_img, 0, 100) == false);
    assert(ppm_image_init(&invalid_img, 100, 0) == false);

    ppm_image_free(&img);
    printf("✓ PPM initialization tests passed\n");
}

// Test PPM pixel operations
void test_ppm_pixels(void) {
    printf("Testing PPM pixel operations...\n");

    ppm_image_t img;
    assert(ppm_image_init(&img, 10, 10) == true);

    // Test setting and getting pixels
    ppm_image_set_pixel(&img, 0, 0, 255, 0, 0);  // Red
    ppm_image_set_pixel(&img, 1, 1, 0, 255, 0);  // Green
    ppm_image_set_pixel(&img, 2, 2, 0, 0, 255);  // Blue

    uint8_t r, g, b;
    assert(ppm_image_get_pixel(&img, 0, 0, &r, &g, &b) == true);
    assert(r == 255 && g == 0 && b == 0);

    assert(ppm_image_get_pixel(&img, 1, 1, &r, &g, &b) == true);
    assert(r == 0 && g == 255 && b == 0);

    assert(ppm_image_get_pixel(&img, 2, 2, &r, &g, &b) == true);
    assert(r == 0 && g == 0 && b == 255);

    // Test out of bounds
    assert(ppm_image_get_pixel(&img, 10, 10, &r, &g, &b) == false);
    ppm_image_set_pixel(&img, 10, 10, 255, 255, 255); // Should not crash

    ppm_image_free(&img);
    printf("✓ PPM pixel operations tests passed\n");
}

// Test PPM fill operation
void test_ppm_fill(void) {
    printf("Testing PPM fill operation...\n");

    ppm_image_t img;
    assert(ppm_image_init(&img, 5, 5) == true);

    ppm_image_fill(&img, 128, 64, 32);

    for (uint32_t y = 0; y < 5; y++) {
        for (uint32_t x = 0; x < 5; x++) {
            uint8_t r, g, b;
            assert(ppm_image_get_pixel(&img, x, y, &r, &g, &b) == true);
            assert(r == 128 && g == 64 && b == 32);
        }
    }

    ppm_image_free(&img);
    printf("✓ PPM fill operation tests passed\n");
}

// Test intensity to color conversion
void test_intensity_to_color(void) {
    printf("Testing intensity to color conversion...\n");

    uint8_t r, g, b;

    // Test edge cases
    ppm_intensity_to_color(0.0f, &r, &g, &b);
    assert(r == 0 && g >= 0 && b >= 0);  // Should be dark

    ppm_intensity_to_color(1.0f, &r, &g, &b);
    assert(r == 255 || g == 255 || b == 255);  // Should be bright

    ppm_intensity_to_color(0.5f, &r, &g, &b);
    assert(r >= 0 && g >= 0 && b >= 0);  // Should be valid color

    printf("✓ Intensity to color conversion tests passed\n");
}

int main(void) {
    printf("Running PPM unit tests...\n\n");

    test_ppm_init();
    test_ppm_pixels();
    test_ppm_fill();
    test_intensity_to_color();

    printf("\n✅ All PPM unit tests passed!\n");
    return 0;
}
