#include "../src/viz/img_ppm.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    printf("Testing PPM image generation...\n");

    // Create a test PPM image
    ppm_image_t img;
    if (!ppm_image_init(&img, 256, 128)) {
        printf("Failed to initialize PPM image\n");
        return 1;
    }

    // Create a color pattern
    for (uint32_t y = 0; y < 128; y++) {
        for (uint32_t x = 0; x < 256; x++) {
            uint8_t r = (uint8_t)(x * 255 / 256);
            uint8_t g = (uint8_t)(y * 255 / 128);
            uint8_t b = 128; // Constant blue channel
            ppm_image_set_pixel(&img, x, y, r, g, b);
        }
    }

    // Save as PPM
    if (ppm_image_write(&img, "test_output.ppm")) {
        printf("✅ Successfully created test_output.ppm\n");
    } else {
        printf("❌ Failed to save PPM image\n");
        ppm_image_free(&img);
        return 1;
    }

    ppm_image_free(&img);
    printf("✅ PPM test completed successfully!\n");
    return 0;
}
