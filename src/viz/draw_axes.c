#include "draw_axes.h"
#include <string.h>
#include <stdio.h>
#include <math.h>

// Font and drawing constants
#define FONT_WIDTH 5
#define FONT_HEIGHT 7
#define FONT_BITS_PER_CHARACTER 5
#define CHARACTER_SPACING 6
#define FONT_NUM_CHARACTERS 95
#define FONT_DATA_SIZE (FONT_NUM_CHARACTERS * FONT_HEIGHT)

// Simple 5x7 bitmap font data (ASCII 32-126)
// Each character is 5 bits wide, 7 bits tall
// Bit 0 = leftmost pixel, MSB = rightmost pixel
static const uint8_t font_5x7_data[95][7] = {
    // Space (32)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00},
    // ! (33)
    {0x04, 0x04, 0x04, 0x04, 0x00, 0x00, 0x04},
    // " (34)
    {0x0A, 0x0A, 0x0A, 0x00, 0x00, 0x00, 0x00},
    // # (35)
    {0x0A, 0x0A, 0x1F, 0x0A, 0x1F, 0x0A, 0x0A},
    // $ (36)
    {0x04, 0x0F, 0x14, 0x0E, 0x05, 0x1E, 0x04},
    // % (37)
    {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03},
    // & (38)
    {0x0C, 0x12, 0x14, 0x08, 0x15, 0x12, 0x0D},
    // ' (39)
    {0x0C, 0x04, 0x08, 0x00, 0x00, 0x00, 0x00},
    // ( (40)
    {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02},
    // ) (41)
    {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08},
    // * (42)
    {0x00, 0x0A, 0x04, 0x1F, 0x04, 0x0A, 0x00},
    // + (43)
    {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00},
    // , (44)
    {0x00, 0x00, 0x00, 0x0C, 0x04, 0x08, 0x00},
    // - (45)
    {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00},
    // . (46)
    {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C},
    // / (47)
    {0x01, 0x02, 0x04, 0x08, 0x10, 0x00, 0x00},
    // 0 (48)
    {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E},
    // 1 (49)
    {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E},
    // 2 (50)
    {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F},
    // 3 (51)
    {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E},
    // 4 (52)
    {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02},
    // 5 (53)
    {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E},
    // 6 (54)
    {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E},
    // 7 (55)
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08},
    // 8 (56)
    {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E},
    // 9 (57)
    {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C},
    // : (58)
    {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x0C, 0x00},
    // ; (59)
    {0x00, 0x0C, 0x0C, 0x00, 0x0C, 0x04, 0x08},
    // < (60)
    {0x02, 0x04, 0x08, 0x10, 0x08, 0x04, 0x02},
    // = (61)
    {0x00, 0x00, 0x1F, 0x00, 0x1F, 0x00, 0x00},
    // > (62)
    {0x08, 0x04, 0x02, 0x01, 0x02, 0x04, 0x08},
    // ? (63)
    {0x0E, 0x11, 0x01, 0x02, 0x00, 0x04, 0x04},
    // @ (64)
    {0x0E, 0x11, 0x17, 0x15, 0x17, 0x10, 0x0E},
    // A (65)
    {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    // B (66)
    {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E},
    // C (67)
    {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E},
    // D (68)
    {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C},
    // E (69)
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F},
    // F (70)
    {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10},
    // G (71)
    {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0E},
    // H (72)
    {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11},
    // I (73)
    {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E},
    // J (74)
    {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C},
    // K (75)
    {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11},
    // L (76)
    {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F},
    // M (77)
    {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11},
    // N (78)
    {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11},
    // O (79)
    {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    // P (80)
    {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10},
    // Q (81)
    {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D},
    // R (82)
    {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11},
    // S (83)
    {0x0F, 0x10, 0x10, 0x0E, 0x01, 0x01, 0x1E},
    // T (84)
    {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04},
    // U (85)
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E},
    // V (86)
    {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04},
    // W (87)
    {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11},
    // X (88)
    {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11},
    // Y (89)
    {0x11, 0x11, 0x11, 0x0A, 0x04, 0x04, 0x04},
    // Z (90)
    {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F}
};

// Font definition
const font_t font_5x7 = {
    .width = FONT_WIDTH,
    .height = FONT_HEIGHT,
    .data = &font_5x7_data[0][0]
};

// Basic drawing functions
void draw_horizontal_line(uint8_t *image_data, uint32_t width, uint32_t height,
                         uint32_t x1, uint32_t x2, uint32_t y,
                         uint8_t r, uint8_t g, uint8_t b) {
    if (!image_data || width == 0 || height == 0 || y >= height) return;
    if (x1 > x2) { uint32_t temp = x1; x1 = x2; x2 = temp; }
    if (x1 >= width) return;
    if (x2 >= width) x2 = width - 1;
    for (uint32_t x = x1; x <= x2; x++) {
        size_t pixel_idx = (y * width + x) * 3;
        image_data[pixel_idx + 0] = r;
        image_data[pixel_idx + 1] = g;
        image_data[pixel_idx + 2] = b;
    }
}

void draw_vertical_line(uint8_t *image_data, uint32_t width, uint32_t height,
                       uint32_t x, uint32_t y1, uint32_t y2,
                       uint8_t r, uint8_t g, uint8_t b) {
    if (!image_data || width == 0 || height == 0 || x >= width) return;
    if (y1 > y2) { uint32_t temp = y1; y1 = y2; y2 = temp; }
    if (y1 >= height) return;
    if (y2 >= height) y2 = height - 1;
    for (uint32_t y = y1; y <= y2; y++) {
        size_t pixel_idx = (y * width + x) * 3;
        image_data[pixel_idx + 0] = r;
        image_data[pixel_idx + 1] = g;
        image_data[pixel_idx + 2] = b;
    }
}

void draw_character(uint8_t *image_data, uint32_t width, uint32_t height,
                   uint32_t x, uint32_t y, char c, const font_t *font,
                   uint8_t r, uint8_t g, uint8_t b) {
    // Comprehensive validation to prevent buffer overflows
    if (!image_data || !font || !font->data || c < 32 || c > 126) return;
    if (font->width == 0 || font->height == 0) return;

    // Get character index in font data (0-94 for 95 characters)
    uint32_t char_index = c - 32;

    // Validate character index bounds
    if (char_index >= FONT_NUM_CHARACTERS) return;  // Validate character index

    // Draw each pixel of the character
    for (uint8_t char_y = 0; char_y < font->height; char_y++) {
        // Validate font data array bounds
        size_t font_data_index = char_index * font->height + char_y;
        if (font_data_index >= FONT_DATA_SIZE) continue;  // Safety check for font data

        for (uint8_t char_x = 0; char_x < font->width; char_x++) {
            uint32_t pixel_x = x + char_x;
            uint32_t pixel_y = y + char_y;

            // Validate pixel coordinates
            if (pixel_x >= width || pixel_y >= height) continue;

            // Extract bit from font data (5-bit wide characters)
            // Font data is organized as [character][row], each row is a byte
            uint8_t font_byte = font->data[font_data_index];

            // Bits are stored MSB first (FONT_BITS_PER_CHARACTER-1 is leftmost, bit 0 is rightmost)
            uint8_t bit_idx = (FONT_BITS_PER_CHARACTER - 1) - char_x;
            if (font_byte & (1 << bit_idx)) {
                // Final bounds check for image buffer
                size_t pixel_idx = (pixel_y * width + pixel_x) * 3;
                if (pixel_idx + 2 < (size_t)width * height * 3) {
                    image_data[pixel_idx + 0] = r;
                    image_data[pixel_idx + 1] = g;
                    image_data[pixel_idx + 2] = b;
                }
            }
        }
    }
}

void draw_text(uint8_t *image_data, uint32_t width, uint32_t height,
              uint32_t x, uint32_t y, const char *text, const font_t *font,
              uint8_t r, uint8_t g, uint8_t b) {
    // Comprehensive null and validity checks
    if (!image_data || !text || !font || width == 0 || height == 0) return;

    uint32_t current_x = x;
    for (size_t i = 0; text[i] != '\0'; i++) {
        draw_character(image_data, width, height, current_x, y, text[i], font, r, g, b);
        current_x += CHARACTER_SPACING;
    }
}

void draw_frequency_axis(uint8_t *image_data, uint32_t width, uint32_t height,
                        uint32_t axis_margin, double center_freq_hz,
                        uint32_t sample_rate_hz, uint32_t fft_size,
                        uint8_t r, uint8_t g, uint8_t b) {
    // Critical validation checks
    if (!image_data || axis_margin >= height || width == 0 || height == 0) return;
    if (fft_size == 0 || sample_rate_hz == 0) return;  // Prevent division by zero

    uint32_t axis_y = height - axis_margin;
    draw_horizontal_line(image_data, width, height, 0, width - 1, axis_y, r, g, b);

    // Draw tick marks and frequency labels
    uint32_t num_ticks = 10;
    // Calculate actual graph width (excluding left margin)
    uint32_t graph_width = width - axis_margin;
    if (graph_width == 0) return;  // Prevent division by zero

    // Use FFT size to calculate proper frequency resolution
    double freq_resolution = (double)sample_rate_hz / (double)fft_size;
    double freq_range = freq_resolution * graph_width;  // Total frequency span shown
    double freq_per_pixel = freq_range / graph_width;   // Hz per pixel

    for (uint32_t i = 0; i <= num_ticks; i++) {
        // Position ticks within the actual graph area
        uint32_t graph_x = (i * graph_width) / num_ticks;
        uint32_t x = axis_margin + graph_x;  // Offset by left margin

        // Draw tick mark
        draw_vertical_line(image_data, width, height, x, axis_y - 5, axis_y + 5, r, g, b);

        // Calculate frequency at this position (relative to graph area)
        double half_width = graph_width / 2.0;
        double freq_offset = (graph_x - half_width) * freq_per_pixel;
        double freq_hz = center_freq_hz + freq_offset;

        // Format frequency label - ensure proper units
        char label[16];
        if (freq_hz >= 1e9) {
            snprintf(label, sizeof(label), "%.1fGHz", freq_hz / 1e9);
        } else if (freq_hz >= 1e6) {
            snprintf(label, sizeof(label), "%.1fMHz", freq_hz / 1e6);
        } else if (freq_hz >= 1e3) {
            snprintf(label, sizeof(label), "%.1fkHz", freq_hz / 1e3);
        } else {
            snprintf(label, sizeof(label), "%.0fHz", freq_hz);
        }

        // Position label below the axis line
        int text_width = strlen(label) * 6; // Approximate width (5px char + 1px spacing)
        int text_x = x - text_width / 2;
        if (text_x < 0) text_x = 0;
        if (text_x + text_width > (int)width) text_x = width - text_width;

        draw_text(image_data, width, height, (uint32_t)text_x, axis_y + 8, label,
                 &font_5x7, r, g, b);
    }
}

void draw_time_axis(uint8_t *image_data, uint32_t width, uint32_t height,
                   uint32_t axis_margin, double total_duration_s,
                   uint32_t time_slices, uint8_t r, uint8_t g, uint8_t b) {
    (void)time_slices; // Not used in current implementation
    if (!image_data || axis_margin >= width) return;

    uint32_t axis_x = axis_margin - 1;
    // Draw axis line only over the graph area (not over axis margins)
    uint32_t graph_height = height - axis_margin;
    draw_vertical_line(image_data, width, height, axis_x, 0, graph_height - 1, r, g, b);

    // Draw tick marks and time labels over the graph area
    uint32_t num_ticks = 8;

    for (uint32_t i = 0; i <= num_ticks; i++) {
        // Position ticks within graph area: from y=0 (top) to y=graph_height-1 (bottom)
        uint32_t y = (i * graph_height) / num_ticks;

        // Draw tick mark
        draw_horizontal_line(image_data, width, height, axis_x - 5, axis_x + 5, y, r, g, b);

        // Calculate time at this position
        // In waterfall: y=0 (top) = newest data = total_duration
        // y=graph_height-1 (bottom) = oldest data = 0
        double time_s = total_duration_s * (num_ticks - i) / num_ticks;

        // Format time label
        char label[16];
        if (time_s >= 1.0) {
            snprintf(label, sizeof(label), "%.1fs", time_s);
        } else if (time_s >= 0.001) {
            snprintf(label, sizeof(label), "%.1fms", time_s * 1000.0);
        } else {
            snprintf(label, sizeof(label), "%.1fus", time_s * 1000000.0);
        }

        // Position label to the left of the axis line
        draw_text(image_data, width, height, 5, y - 3, label, &font_5x7, r, g, b);
    }
}

void draw_db_scale(uint8_t *image_data, uint32_t width, uint32_t height,
                  uint32_t axis_margin, double min_dbfs, double max_dbfs,
                  uint8_t r, uint8_t g, uint8_t b) {
    if (!image_data || axis_margin >= width) return;

    uint32_t axis_x = axis_margin - 1;
    // Draw axis line only over the graph area (not over axis margins)
    uint32_t graph_height = height - axis_margin;
    draw_vertical_line(image_data, width, height, axis_x, 0, graph_height - 1, r, g, b);

    // Draw tick marks and dB labels
    uint32_t num_ticks = 6;
    double db_range = max_dbfs - min_dbfs;

    for (uint32_t i = 0; i <= num_ticks; i++) {
        // Position ticks within graph area: from y=0 (top) to y=graph_height-1 (bottom)
        uint32_t y = (i * graph_height) / num_ticks;

        // Draw tick mark
        draw_horizontal_line(image_data, width, height, axis_x - 5, axis_x + 5, y, r, g, b);

        // Calculate dB value at this position
        // In spectrogram: y=0 (top) = max_dB, y=graph_height-1 (bottom) = min_dB
        double db_value = max_dbfs - (i * db_range) / num_ticks;

        // Format dB label
        char label[16];
        if (db_value >= 0.0) {
            snprintf(label, sizeof(label), "+%.0fdB", db_value);
        } else {
            snprintf(label, sizeof(label), "%.0fdB", db_value);
        }

        // Position label to the left of the axis line
        draw_text(image_data, width, height, 5, y - 3, label, &font_5x7, r, g, b);
    }
}

// Utility function to format frequency values with appropriate units
void format_frequency(char *buffer, size_t buffer_size, double frequency_hz) {
    if (!buffer || buffer_size == 0) return;

    if (frequency_hz >= 1e9) {
        snprintf(buffer, buffer_size, "%.1fG", frequency_hz / 1e9);
    } else if (frequency_hz >= 1e6) {
        snprintf(buffer, buffer_size, "%.1fM", frequency_hz / 1e6);
    } else if (frequency_hz >= 1e3) {
        snprintf(buffer, buffer_size, "%.1fk", frequency_hz / 1e3);
    } else {
        snprintf(buffer, buffer_size, "%.0f", frequency_hz);
    }
}

// Utility function to format time values with appropriate units
void format_time(char *buffer, size_t buffer_size, double time_seconds) {
    if (!buffer || buffer_size == 0) return;

    if (time_seconds >= 1.0) {
        snprintf(buffer, buffer_size, "%.1fs", time_seconds);
    } else if (time_seconds >= 0.001) {
        snprintf(buffer, buffer_size, "%.0fms", time_seconds * 1000.0);
    } else if (time_seconds >= 1e-6) {
        snprintf(buffer, buffer_size, "%.0fus", time_seconds * 1e6);
    } else {
        snprintf(buffer, buffer_size, "%.1fus", time_seconds * 1e6);
    }
}