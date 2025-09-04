#ifndef IQ_DRAW_AXES_H
#define IQ_DRAW_AXES_H

#include <stdint.h>
#include <stdbool.h>

// Font definition for simple text rendering
typedef struct {
    uint8_t width;      // Character width in pixels
    uint8_t height;     // Character height in pixels
    const uint8_t *data; // Font bitmap data
} font_t;

// Simple 5x7 bitmap font for basic text rendering
extern const font_t font_5x7;

/*
 * Draw a horizontal line on the image
 */
void draw_horizontal_line(uint8_t *image_data, uint32_t width, uint32_t height,
                         uint32_t x1, uint32_t x2, uint32_t y,
                         uint8_t r, uint8_t g, uint8_t b);

/*
 * Draw a vertical line on the image
 */
void draw_vertical_line(uint8_t *image_data, uint32_t width, uint32_t height,
                       uint32_t x, uint32_t y1, uint32_t y2,
                       uint8_t r, uint8_t g, uint8_t b);

/*
 * Draw a single character at specified position
 * Uses the provided font to render text
 */
void draw_character(uint8_t *image_data, uint32_t width, uint32_t height,
                   uint32_t x, uint32_t y, char c, const font_t *font,
                   uint8_t r, uint8_t g, uint8_t b);

/*
 * Draw a text string starting at specified position
 * Characters are drawn from left to right
 */
void draw_text(uint8_t *image_data, uint32_t width, uint32_t height,
              uint32_t x, uint32_t y, const char *text, const font_t *font,
              uint8_t r, uint8_t g, uint8_t b);

/*
 * Draw frequency axis labels on spectrogram
 * Shows frequency values at regular intervals
 */
void draw_frequency_axis(uint8_t *image_data, uint32_t width, uint32_t height,
                        uint32_t axis_margin, double center_freq_hz,
                        uint32_t sample_rate_hz, uint32_t fft_size,
                        uint8_t r, uint8_t g, uint8_t b);

/*
 * Draw time axis labels on waterfall plot
 * Shows time values at regular intervals
 */
void draw_time_axis(uint8_t *image_data, uint32_t width, uint32_t height,
                   uint32_t axis_margin, double total_duration_s,
                   uint32_t time_slices, uint8_t r, uint8_t g, uint8_t b);

/*
 * Draw dB scale labels on the right side of spectrogram
 * Shows power levels in dBFS
 */
void draw_db_scale(uint8_t *image_data, uint32_t width, uint32_t height,
                  uint32_t axis_margin, double min_dbfs, double max_dbfs,
                  uint8_t r, uint8_t g, uint8_t b);

// Formatting utility functions for frequency and time values
void format_frequency(char *buffer, size_t buffer_size, double frequency_hz);
void format_time(char *buffer, size_t buffer_size, double time_seconds);

#endif // IQ_DRAW_AXES_H
