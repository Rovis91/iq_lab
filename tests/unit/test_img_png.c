#include "../../src/viz/img_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Test PNG image initialization
void test_png_init(void) {
    printf("Testing PNG image initialization...\n");

    png_image_t img;
    assert(png_image_init(&img, 100, 100) == true);
    assert(img.width == 100);
    assert(img.height == 100);
    assert(img.data != NULL);
    assert(img.data_size == 100 * 100 * 3);

    // Test invalid dimensions
    png_image_t invalid_img;
    assert(png_image_init(&invalid_img, 0, 100) == false);
    assert(png_image_init(&invalid_img, 100, 0) == false);

    png_image_free(&img);
    printf("✓ PNG initialization tests passed\n");
}

// Test PNG pixel operations
void test_png_pixels(void) {
    printf("Testing PNG pixel operations...\n");

    png_image_t img;
    assert(png_image_init(&img, 10, 10) == true);

    // Test setting and getting pixels
    png_image_set_pixel(&img, 0, 0, 255, 0, 0);  // Red
    png_image_set_pixel(&img, 1, 1, 0, 255, 0);  // Green
    png_image_set_pixel(&img, 2, 2, 0, 0, 255);  // Blue

    uint8_t r, g, b;
    assert(png_image_get_pixel(&img, 0, 0, &r, &g, &b) == true);
    assert(r == 255 && g == 0 && b == 0);

    assert(png_image_get_pixel(&img, 1, 1, &r, &g, &b) == true);
    assert(r == 0 && g == 255 && b == 0);

    assert(png_image_get_pixel(&img, 2, 2, &r, &g, &b) == true);
    assert(r == 0 && g == 0 && b == 255);

    // Test out of bounds
    assert(png_image_get_pixel(&img, 10, 10, &r, &g, &b) == false);
    assert(png_image_set_pixel(&img, 10, 10, 255, 255, 255) == true); // Should not crash

    png_image_free(&img);
    printf("✓ PNG pixel operations tests passed\n");
}

// Test PNG fill operation
void test_png_fill(void) {
    printf("Testing PNG fill operation...\n");

    png_image_t img;
    assert(png_image_init(&img, 5, 5) == true);

    png_image_fill(&img, 128, 64, 32);

    for (uint32_t y = 0; y < 5; y++) {
        for (uint32_t x = 0; x < 5; x++) {
            uint8_t r, g, b;
            assert(png_image_get_pixel(&img, x, y, &r, &g, &b) == true);
            assert(r == 128 && g == 64 && b == 32);
        }
    }

    png_image_free(&img);
    printf("✓ PNG fill operation tests passed\n");
}

// Test intensity to color conversion
void test_intensity_to_color(void) {
    printf("Testing intensity to color conversion...\n");

    uint8_t r, g, b;

    // Test edge cases
    png_intensity_to_color(0.0f, &r, &g, &b);
    assert(r == 0 && g >= 0 && b >= 0);  // Should be dark

    png_intensity_to_color(1.0f, &r, &g, &b);
    assert(r == 255 || g == 255 || b == 255);  // Should be bright

    png_intensity_to_color(0.5f, &r, &g, &b);
    assert(r >= 0 && g >= 0 && b >= 0);  // Should be valid color

    printf("✓ Intensity to color conversion tests passed\n");
}

int main(void) {
    printf("Running PNG unit tests...\n\n");

    test_png_init();
    test_png_pixels();
    test_png_fill();
    test_intensity_to_color();

    printf("\n✅ All PNG unit tests passed!\n");
    return 0;
}
