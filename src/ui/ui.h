#ifndef IQ_LAB_UI_H
#define IQ_LAB_UI_H

#include <stdbool.h>
#include <stdint.h>
#include <windows.h>
#include <windowsx.h>

// Include Clay header for type definitions
#include "clay.h"

// Forward declarations
typedef struct IQ_UI_State IQ_UI_State;
typedef struct IQ_UI_Config IQ_UI_Config;

// UI Configuration
struct IQ_UI_Config {
    int window_width;
    int window_height;
    const char* window_title;
    const char* font_path;
    int font_size;
    bool debug_mode;
};

// UI State
struct IQ_UI_State {
    // Window and rendering
    HWND hwnd;
    HDC hdc;
    HFONT font;

    // Clay state
    void* clay_memory;
    uint64_t clay_memory_size;

    // UI state
    bool is_running;
    bool show_file_browser;
    bool show_spectrum_view;

    // File state
    char current_file_path[1024];
    bool has_file_loaded;

    // Spectrum state
    float* spectrum_data;
    size_t spectrum_size;
    bool spectrum_dirty;

    // Control values
    int fft_size;
    int hop_size;
    int avg_count;
};

// Core UI functions
bool iq_ui_init(IQ_UI_Config* config);
void iq_ui_shutdown(void);
void iq_ui_run_main_loop(void);

// Window management
HWND iq_ui_create_window(IQ_UI_Config* config);
LRESULT CALLBACK iq_ui_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

// Clay integration
bool iq_ui_init_clay(void);
void iq_ui_update_clay_dimensions(int width, int height);
void iq_ui_handle_mouse_event(UINT msg, WPARAM wParam, LPARAM lParam);
void iq_ui_handle_scroll_event(WPARAM wParam);
void iq_ui_render_frame(void);

// UI components
Clay_RenderCommandArray iq_ui_create_layout(void);
void iq_ui_render_file_browser(void);
void iq_ui_render_spectrum_view(void);
void iq_ui_render_controls(void);

// File operations
bool iq_ui_load_iq_file(const char* filepath);
void iq_ui_process_spectrum(void);

// Utility functions
Clay_Dimensions iq_ui_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);
void iq_ui_handle_clay_errors(Clay_ErrorData errorData);

#endif // IQ_LAB_UI_H
