#include "../src/viz/img_png.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Testing basic PNG image generation...\n");

    // Create a simple test image
    png_image_t img;
    if (!png_image_init(&img, 256, 128)) {
        printf("Failed to initialize image\n");
        return 1;
    }

    // Fill with a gradient pattern
    for (uint32_t y = 0; y < 128; y++) {
        for (uint32_t x = 0; x < 256; x++) {
            uint8_t r = (uint8_t)(x * 255 / 256);
            uint8_t g = (uint8_t)(y * 255 / 128);
            uint8_t b = (uint8_t)((x + y) * 255 / (256 + 128));
            png_image_set_pixel(&img, x, y, r, g, b);
        }
    }

    // Save the image
    if (png_image_write(&img, "test_gradient.png")) {
        printf("✅ Successfully created test_gradient.png\n");
    } else {
        printf("❌ Failed to save image\n");
        png_image_free(&img);
        return 1;
    }

    png_image_free(&img);
    printf("✅ Basic PNG test completed successfully!\n");
    return 0;
}
