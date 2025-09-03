#ifndef IQ_IMG_PNG_H
#define IQ_IMG_PNG_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// PNG image context
typedef struct {
    uint8_t *data;      // RGB data buffer (3 bytes per pixel)
    uint32_t width;     // Image width in pixels
    uint32_t height;    // Image height in pixels
    size_t data_size;   // Size of data buffer in bytes
} png_image_t;

/*
 * Initialize PNG image with given dimensions
 * Allocates memory for the RGB data buffer
 * Returns true on success, false on memory allocation failure
 */
bool png_image_init(png_image_t *img, uint32_t width, uint32_t height);

/*
 * Set pixel color at (x,y) coordinates
 * Coordinates are 0-based from top-left
 * r, g, b values should be 0-255
 */
void png_image_set_pixel(png_image_t *img, uint32_t x, uint32_t y,
                        uint8_t r, uint8_t g, uint8_t b);

/*
 * Get pixel color at (x,y) coordinates
 * Returns true if coordinates are valid, false otherwise
 * r, g, b pointers will be filled with pixel values
 */
bool png_image_get_pixel(const png_image_t *img, uint32_t x, uint32_t y,
                        uint8_t *r, uint8_t *g, uint8_t *b);

/*
 * Fill entire image with a solid color
 */
void png_image_fill(png_image_t *img, uint8_t r, uint8_t g, uint8_t b);

/*
 * Convert grayscale intensity (0.0-1.0) to RGB using HF radio color mapping
 * This provides better visibility for weak signals in radio frequency analysis
 */
void png_intensity_to_color(float intensity, uint8_t *r, uint8_t *g, uint8_t *b);

/*
 * Write PNG image to file
 * Returns true on success, false on failure
 */
bool png_image_write(const png_image_t *img, const char *filename);

/*
 * Free PNG image resources
 */
void png_image_free(png_image_t *img);

#endif // IQ_IMG_PNG_H
