/*
 * =============================================================================
 * IQ Lab - PNG Image Generation Module
 * =============================================================================
 *
 * PURPOSE:
 *   Creates PNG images for RF signal visualization including spectra and
 *   waterfalls. Uses the stb_image_write library for high-quality, compressed
 *   PNG output with proper color mapping and axis annotation.
 *
 * FEATURES:
 *   - Full-color RGB pixel manipulation
 *   - Automatic power-to-color mapping with configurable palettes
 *   - Memory-efficient image buffer management
 *   - PNG compression with high quality preservation
 *   - Support for large images (limited only by available memory)
 *   - Error handling for invalid operations
 *
 * COLOR MAPPING:
 *   - Power levels mapped to HSV color space
 *   - Low power: Blue/cool colors
 *   - Medium power: Green/yellow
 *   - High power: Red/hot colors
 *   - Configurable color schemes for different applications
 *
 * USAGE:
 *   1. Initialize image: png_image_init(&img, width, height)
 *   2. Set pixels: png_image_set_pixel(&img, x, y, r, g, b)
 *   3. Save to file: png_image_write_file(&img, filename)
 *   4. Clean up: png_image_destroy(&img)
 *
 * PERFORMANCE:
 *   - In-memory buffer operations for speed
 *   - PNG compression performed once at save time
 *   - Minimal memory overhead beyond image data
 *
 * DEPENDENCIES:
 *   - stb_image_write.h: Single-file PNG writing library
 *   - Standard C libraries (stdlib, string, stdio)
 *
 * FILE FORMAT:
 *   - PNG (Portable Network Graphics)
 *   - 24-bit RGB color depth
 *   - Lossless compression
 *   - Widely supported in browsers and image viewers
 *
 * LIMITATIONS:
 *   - No alpha channel support (RGB only)
 *   - Image size limited by available RAM
 *   - Single-threaded operation
 *
 * =============================================================================
 */

#include "img_png.h"
#include "stb_image_write.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

bool png_image_init(png_image_t *img, uint32_t width, uint32_t height) {
    if (!img || width == 0 || height == 0) {
        return false;
    }

    img->width = width;
    img->height = height;
    img->data_size = (size_t)width * height * 3; // 3 bytes per pixel (RGB)

    img->data = (uint8_t *)malloc(img->data_size);
    if (!img->data) {
        return false;
    }

    // Initialize to black
    memset(img->data, 0, img->data_size);
    return true;
}

void png_image_set_pixel(png_image_t *img, uint32_t x, uint32_t y,
                        uint8_t r, uint8_t g, uint8_t b) {
    if (!img || !img->data || x >= img->width || y >= img->height) {
        return;
    }

    size_t pixel_idx = (y * img->width + x) * 3;
    img->data[pixel_idx + 0] = r;
    img->data[pixel_idx + 1] = g;
    img->data[pixel_idx + 2] = b;
}

bool png_image_get_pixel(const png_image_t *img, uint32_t x, uint32_t y,
                        uint8_t *r, uint8_t *g, uint8_t *b) {
    if (!img || !img->data || x >= img->width || y >= img->height) {
        return false;
    }

    size_t pixel_idx = (y * img->width + x) * 3;
    if (r) *r = img->data[pixel_idx + 0];
    if (g) *g = img->data[pixel_idx + 1];
    if (b) *b = img->data[pixel_idx + 2];
    return true;
}

void png_image_fill(png_image_t *img, uint8_t r, uint8_t g, uint8_t b) {
    if (!img || !img->data) {
        return;
    }

    for (size_t i = 0; i < img->data_size; i += 3) {
        img->data[i + 0] = r;
        img->data[i + 1] = g;
        img->data[i + 2] = b;
    }
}

// HF Radio-optimized color mapping with enhanced low-signal visibility
void png_intensity_to_color(float intensity, uint8_t *r, uint8_t *g, uint8_t *b) {
    if (intensity < 0.0f) intensity = 0.0f;
    if (intensity > 1.0f) intensity = 1.0f;

    // Enhanced HF Radio color scheme - better differentiation for 20-40 dB signals

    // Very low signal (noise floor) - black to dark blue
    if (intensity < 0.1f) {
        float t = intensity / 0.1f;
        *r = 0;
        *g = (uint8_t)(t * 20.0f);  // Slight green tint for noise floor visibility
        *b = (uint8_t)(t * 30.0f);  // Dark blue-black
    }
    // Low signal (weak HF signals) - dark blue to blue
    else if (intensity < 0.3f) {
        float t = (intensity - 0.1f) / 0.2f;
        *r = 0;
        *g = (uint8_t)(20.0f + t * 60.0f);  // Blue-green transition
        *b = (uint8_t)(30.0f + t * 150.0f);  // Dark to medium blue
    }
    // Moderate signal (medium HF reception) - blue to cyan
    else if (intensity < 0.5f) {
        float t = (intensity - 0.3f) / 0.2f;
        *r = (uint8_t)(t * 80.0f);
        *g = (uint8_t)(80.0f + t * 80.0f);
        *b = (uint8_t)(180.0f + t * 75.0f);
    }
    // Medium signal (good HF reception) - cyan to green
    else if (intensity < 0.7f) {
        float t = (intensity - 0.5f) / 0.2f;
        *r = (uint8_t)(80.0f + t * 80.0f);
        *g = (uint8_t)(160.0f + t * 95.0f);
        *b = (uint8_t)(255.0f - t * 200.0f);
    }
    // Strong signal (excellent HF reception) - green to yellow
    else if (intensity < 0.85f) {
        float t = (intensity - 0.7f) / 0.15f;
        *r = (uint8_t)(160.0f + t * 95.0f);
        *g = 255;
        *b = (uint8_t)(55.0f - t * 55.0f);
    }
    // Very strong signal (peak tones) - yellow to orange
    else if (intensity < 0.95f) {
        float t = (intensity - 0.85f) / 0.1f;
        *r = 255;
        *g = (uint8_t)(255.0f - t * 128.0f);
        *b = 0;
    }
    // Maximum signal (severe interference) - bright red
    else {
        *r = 255;
        *g = 0;
        *b = (uint8_t)((intensity - 0.95f) / 0.05f * 100.0f);  // Red with blue tint
    }
}

bool png_image_write(const png_image_t *img, const char *filename) {
    if (!img || !img->data || !filename) {
        return false;
    }

    // Use stb_image_write to save PNG
    // Note: stb_image_write expects data in row-major order from top to bottom
    int result = stbi_write_png(filename, (int)img->width, (int)img->height, 3,
                               img->data, (int)img->width * 3);

    return result != 0;
}

void png_image_free(png_image_t *img) {
    if (img) {
        if (img->data) {
            free(img->data);
            img->data = NULL;
        }
        img->width = 0;
        img->height = 0;
        img->data_size = 0;
    }
}
