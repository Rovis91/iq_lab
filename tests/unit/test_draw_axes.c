#include "../../src/viz/draw_axes.h"
#include "../../src/viz/img_png.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

// Test horizontal line drawing
void test_draw_horizontal_line(void) {
    printf("Testing horizontal line drawing...\n");

    png_image_t img;
    assert(png_image_init(&img, 10, 10) == true);
    png_image_fill(&img, 0, 0, 0); // Black background

    // Draw a horizontal red line
    draw_horizontal_line(img.data, 10, 10, 2, 7, 5, 255, 0, 0);

    // Check that pixels on the line are red
    for (uint32_t x = 2; x <= 7; x++) {
        uint8_t r, g, b;
        assert(png_image_get_pixel(&img, x, 5, &r, &g, &b) == true);
        assert(r == 255 && g == 0 && b == 0);
    }

    // Check that pixels outside the line are still black
    uint8_t r, g, b;
    assert(png_image_get_pixel(&img, 1, 5, &r, &g, &b) == true);
    assert(r == 0 && g == 0 && b == 0);
    assert(png_image_get_pixel(&img, 8, 5, &r, &g, &b) == true);
    assert(r == 0 && g == 0 && b == 0);

    png_image_free(&img);
    printf("✓ Horizontal line drawing tests passed\n");
}

// Test vertical line drawing
void test_draw_vertical_line(void) {
    printf("Testing vertical line drawing...\n");

    png_image_t img;
    assert(png_image_init(&img, 10, 10) == true);
    png_image_fill(&img, 0, 0, 0); // Black background

    // Draw a vertical green line
    draw_vertical_line(img.data, 10, 10, 5, 2, 7, 0, 255, 0);

    // Check that pixels on the line are green
    for (uint32_t y = 2; y <= 7; y++) {
        uint8_t r, g, b;
        assert(png_image_get_pixel(&img, 5, y, &r, &g, &b) == true);
        assert(r == 0 && g == 255 && b == 0);
    }

    // Check that pixels outside the line are still black
    uint8_t r, g, b;
    assert(png_image_get_pixel(&img, 5, 1, &r, &g, &b) == true);
    assert(r == 0 && g == 0 && b == 0);
    assert(png_image_get_pixel(&img, 5, 8, &r, &g, &b) == true);
    assert(r == 0 && g == 0 && b == 0);

    png_image_free(&img);
    printf("✓ Vertical line drawing tests passed\n");
}

// Test character drawing
void test_draw_character(void) {
    printf("Testing character drawing...\n");

    png_image_t img;
    assert(png_image_init(&img, 20, 20) == true);
    png_image_fill(&img, 0, 0, 0); // Black background

    // Draw character 'A' in white
    draw_character(img.data, 20, 20, 5, 5, 'A', &font_5x7, 255, 255, 255);

    // Check that some pixels in the character area are white
    uint8_t r, g, b;
    bool found_white_pixel = false;
    for (uint32_t y = 5; y < 5 + 7 && !found_white_pixel; y++) {
        for (uint32_t x = 5; x < 5 + 5; x++) {
            if (png_image_get_pixel(&img, x, y, &r, &g, &b)) {
                if (r == 255 && g == 255 && b == 255) {
                    found_white_pixel = true;
                    break;
                }
            }
        }
    }
    assert(found_white_pixel == true);

    png_image_free(&img);
    printf("✓ Character drawing tests passed\n");
}

// Test text drawing
void test_draw_text(void) {
    printf("Testing text drawing...\n");

    png_image_t img;
    assert(png_image_init(&img, 50, 20) == true);
    png_image_fill(&img, 0, 0, 0); // Black background

    // Draw text "HI" in blue
    draw_text(img.data, 50, 20, 5, 5, "HI", &font_5x7, 0, 0, 255);

    // Check that some pixels in the text area are blue
    uint8_t r, g, b;
    bool found_blue_pixel = false;
    for (uint32_t y = 5; y < 5 + 7 && !found_blue_pixel; y++) {
        for (uint32_t x = 5; x < 5 + 12; x++) { // "HI" takes about 12 pixels width
            if (png_image_get_pixel(&img, x, y, &r, &g, &b)) {
                if (r == 0 && g == 0 && b == 255) {
                    found_blue_pixel = true;
                    break;
                }
            }
        }
    }
    assert(found_blue_pixel == true);

    png_image_free(&img);
    printf("✓ Text drawing tests passed\n");
}

// Test frequency formatting
void test_format_frequency(void) {
    printf("Testing frequency formatting...\n");

    char buffer[32];

    format_frequency(buffer, sizeof(buffer), 1000.0);
    assert(strcmp(buffer, "1.000k") == 0);

    format_frequency(buffer, sizeof(buffer), 1500000.0);
    assert(strcmp(buffer, "1.500M") == 0);

    format_frequency(buffer, sizeof(buffer), 1000000000.0);
    assert(strncmp(buffer, "1.000G", 6) == 0);

    format_frequency(buffer, sizeof(buffer), 500.0);
    assert(strncmp(buffer, "500", 3) == 0);

    printf("✓ Frequency formatting tests passed\n");
}

// Test time formatting
void test_format_time(void) {
    printf("Testing time formatting...\n");

    char buffer[32];

    format_time(buffer, sizeof(buffer), 1.5);
    assert(strncmp(buffer, "1.5s", 4) == 0);

    format_time(buffer, sizeof(buffer), 0.05);
    assert(strncmp(buffer, "50ms", 4) == 0);

    format_time(buffer, sizeof(buffer), 0.00005);
    assert(strncmp(buffer, "50us", 4) == 0);

    printf("✓ Time formatting tests passed\n");
}

int main(void) {
    printf("Running draw_axes unit tests...\n\n");

    test_draw_horizontal_line();
    test_draw_vertical_line();
    test_draw_character();
    test_draw_text();
    test_format_frequency();
    test_format_time();

    printf("\n✅ All draw_axes unit tests passed!\n");
    return 0;
}
