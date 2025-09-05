#include "ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// Clay renderer integration
#define CLAY_IMPLEMENTATION
#include "clay.h"
#include "clay_renderer_gdi.c"

// Global UI state
static IQ_UI_State g_ui_state = {0};
static IQ_UI_Config g_ui_config = {0};

// Color definitions
static const Clay_Color UI_COLOR_BACKGROUND = {30, 30, 30, 255};
static const Clay_Color UI_COLOR_SURFACE = {45, 45, 45, 255};
static const Clay_Color UI_COLOR_TEXT = {230, 230, 230, 255};
static const Clay_Color UI_COLOR_TEXT_SECONDARY = {180, 180, 180, 255};

// Font ID
#define FONT_ID_BODY 0

// Forward declarations for Clay renderer
Clay_Dimensions Clay_Win32_MeasureText(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData);
void Clay_Win32_Render(HWND hwnd, Clay_RenderCommandArray renderCommands, HFONT* fonts);

bool iq_ui_init(IQ_UI_Config* config) {
    if (!config) return false;

    memcpy(&g_ui_config, config, sizeof(IQ_UI_Config));

    // Initialize default values
    g_ui_state.fft_size = 4096;
    g_ui_state.hop_size = 1024;
    g_ui_state.avg_count = 20;

    // Create window
    g_ui_state.hwnd = iq_ui_create_window(config);
    if (!g_ui_state.hwnd) {
        return false;
    }

    // Initialize Clay
    if (!iq_ui_init_clay()) {
        return false;
    }

    // Allocate spectrum buffer
    g_ui_state.spectrum_size = g_ui_state.fft_size / 2;
    g_ui_state.spectrum_data = (float*)malloc(g_ui_state.spectrum_size * sizeof(float));
    if (!g_ui_state.spectrum_data) {
        return false;
    }

    g_ui_state.is_running = true;
    return true;
}

void iq_ui_shutdown(void) {
    g_ui_state.is_running = false;

    if (g_ui_state.spectrum_data) {
        free(g_ui_state.spectrum_data);
        g_ui_state.spectrum_data = NULL;
    }

    if (g_ui_state.clay_memory) {
        free(g_ui_state.clay_memory);
        g_ui_state.clay_memory = NULL;
    }

    if (g_ui_state.hwnd) {
        DestroyWindow(g_ui_state.hwnd);
        g_ui_state.hwnd = NULL;
    }
}

HWND iq_ui_create_window(IQ_UI_Config* config) {
    WNDCLASS wc = {0};
    wc.lpfnWndProc = iq_ui_wnd_proc;
    wc.hInstance = GetModuleHandle(NULL);
    wc.lpszClassName = "IQLabUI";
    wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
    wc.hCursor = LoadCursor(NULL, IDC_ARROW);

    if (!RegisterClass(&wc)) {
        return NULL;
    }

    RECT rc = {0, 0, config->window_width, config->window_height};
    AdjustWindowRect(&rc, WS_OVERLAPPEDWINDOW, FALSE);

    HWND hwnd = CreateWindow(
        "IQLabUI",
        config->window_title,
        WS_OVERLAPPEDWINDOW | WS_VISIBLE,
        CW_USEDEFAULT, CW_USEDEFAULT,
        rc.right - rc.left, rc.bottom - rc.top,
        NULL, NULL,
        GetModuleHandle(NULL),
        NULL
    );

    return hwnd;
}

LRESULT CALLBACK iq_ui_wnd_proc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
        case WM_DESTROY:
            PostQuitMessage(0);
            return 0;

        case WM_SIZE:
            if (g_ui_state.hwnd) {
                RECT rect;
                GetClientRect(hwnd, &rect);
                iq_ui_update_clay_dimensions(rect.right - rect.left, rect.bottom - rect.top);
            }
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_MOUSEMOVE:
        case WM_LBUTTONDOWN:
        case WM_LBUTTONUP:
        case WM_RBUTTONDOWN:
        case WM_RBUTTONUP:
            iq_ui_handle_mouse_event(msg, wParam, lParam);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_MOUSEWHEEL:
            iq_ui_handle_scroll_event(wParam);
            InvalidateRect(hwnd, NULL, FALSE);
            return 0;

        case WM_KEYDOWN:
            if (wParam == VK_ESCAPE) {
                DestroyWindow(hwnd);
                return 0;
            }
            if (wParam == VK_F12) {
                g_ui_config.debug_mode = !g_ui_config.debug_mode;
                Clay_SetDebugModeEnabled(g_ui_config.debug_mode);
                InvalidateRect(hwnd, NULL, FALSE);
                return 0;
            }
            break;

        case WM_PAINT:
        {
            Clay_RenderCommandArray render_commands = iq_ui_create_layout();
            Clay_Win32_Render(hwnd, render_commands, &g_ui_state.font);
            return 0;
        }
    }

    return DefWindowProc(hwnd, msg, wParam, lParam);
}

bool iq_ui_init_clay(void) {
    // Calculate required memory
    g_ui_state.clay_memory_size = Clay_MinMemorySize();
    printf("Clay memory size required: %llu bytes\n", (unsigned long long)g_ui_state.clay_memory_size);
    g_ui_state.clay_memory = malloc(g_ui_state.clay_memory_size);
    if (!g_ui_state.clay_memory) {
        printf("Failed to allocate Clay memory\n");
        return false;
    }

    Clay_Arena clay_memory = {
        .memory = g_ui_state.clay_memory,
        .capacity = g_ui_state.clay_memory_size
    };

    // Initialize Clay
    Clay_Initialize(clay_memory,
                   (Clay_Dimensions){g_ui_config.window_width, g_ui_config.window_height},
                   (Clay_ErrorHandler){ .errorHandlerFunction = iq_ui_handle_clay_errors, .userData = NULL });

    printf("Clay initialized successfully\n");

    // Set up font
    printf("Loading font from: %s\n", g_ui_config.font_path);
    g_ui_state.font = Clay_Win32_SimpleCreateFont(g_ui_config.font_path, "Arial", -g_ui_config.font_size, FW_NORMAL);
    printf("Font handle: %p\n", (void*)g_ui_state.font);

    // Set renderer flags like in the example
    Clay_Win32_SetRendererFlags(CLAYGDI_RF_ALPHABLEND | CLAYGDI_RF_SMOOTHCORNERS);

    Clay_SetMeasureTextFunction(Clay_Win32_MeasureText, &g_ui_state.font);
    printf("Clay setup complete\n");

    return true;
}

void iq_ui_update_clay_dimensions(int width, int height) {
    Clay_SetLayoutDimensions((Clay_Dimensions){(float)width, (float)height});
}

void iq_ui_handle_mouse_event(UINT msg, WPARAM wParam, LPARAM lParam) {
    int x = GET_X_LPARAM(lParam);
    int y = GET_Y_LPARAM(lParam);
    bool left_button_down = (msg == WM_LBUTTONDOWN) || (msg == WM_LBUTTONUP && (wParam & MK_LBUTTON));

    Clay_SetPointerState((Clay_Vector2){(float)x, (float)y}, left_button_down);
}

void iq_ui_handle_scroll_event(WPARAM wParam) {
    short delta = GET_WHEEL_DELTA_WPARAM(wParam);
    Clay_UpdateScrollContainers(true, (Clay_Vector2){0, (float)delta}, 0.016f); // ~60fps
}

// This function is no longer used since we handle WM_PAINT directly
void iq_ui_render_frame(void) {
    // No longer needed - WM_PAINT handles this directly
}

Clay_RenderCommandArray iq_ui_create_layout(void) {
    Clay_BeginLayout();

    // Main application layout
    CLAY({
        .id = CLAY_ID("Root"),
        .layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
            .layoutDirection = CLAY_TOP_TO_BOTTOM,
            .padding = CLAY_PADDING_ALL(16),
            .childGap = 16
        },
        .backgroundColor = UI_COLOR_BACKGROUND
    }) {
        // Header
        CLAY({
            .id = CLAY_ID("Header"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(60)},
                .padding = CLAY_PADDING_ALL(16),
                .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}
            },
            .backgroundColor = UI_COLOR_SURFACE,
            .cornerRadius = CLAY_CORNER_RADIUS(8)
        }) {
            CLAY_TEXT(CLAY_STRING("IQ Lab - Signal Analysis Tool"), CLAY_TEXT_CONFIG({
                .fontId = FONT_ID_BODY,
                .fontSize = 20,
                .textColor = UI_COLOR_TEXT
            }));
        }

        // Main content area
        CLAY({
            .id = CLAY_ID("ContentArea"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                .layoutDirection = CLAY_LEFT_TO_RIGHT,
                .childGap = 16
            }
        }) {
            // Left panel - Controls
            CLAY({
                .id = CLAY_ID("LeftPanel"),
                .layout = {
                    .sizing = {CLAY_SIZING_FIXED(320), CLAY_SIZING_GROW(0)},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 12
                }
            }) {
                // File status
                CLAY({
                    .id = CLAY_ID("FilePanel"),
                    .layout = {
                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(80)},
                        .padding = CLAY_PADDING_ALL(12),
                        .childGap = 8
                    },
                    .backgroundColor = UI_COLOR_SURFACE,
                    .cornerRadius = CLAY_CORNER_RADIUS(6)
                }) {
                    CLAY_TEXT(CLAY_STRING("File Status"), CLAY_TEXT_CONFIG({
                        .fontId = FONT_ID_BODY,
                        .fontSize = 14,
                        .textColor = UI_COLOR_TEXT
                    }));

                    CLAY({
                        .id = CLAY_ID("FileStatus"),
                        .layout = {
                            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(24)},
                            .padding = CLAY_PADDING_ALL(6),
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}
                        },
                        .backgroundColor = {100, 50, 50, 255},
                        .cornerRadius = CLAY_CORNER_RADIUS(4)
                    }) {
                        CLAY_TEXT(CLAY_STRING("No IQ file loaded"), CLAY_TEXT_CONFIG({
                            .fontId = FONT_ID_BODY,
                            .fontSize = 12,
                            .textColor = UI_COLOR_TEXT
                        }));
                    }
                }

                // Controls panel
                CLAY({
                    .id = CLAY_ID("ControlsPanel"),
                    .layout = {
                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                        .padding = CLAY_PADDING_ALL(12),
                        .childGap = 8
                    },
                    .backgroundColor = UI_COLOR_SURFACE,
                    .cornerRadius = CLAY_CORNER_RADIUS(6)
                }) {
                    CLAY_TEXT(CLAY_STRING("Processing Controls"), CLAY_TEXT_CONFIG({
                        .fontId = FONT_ID_BODY,
                        .fontSize = 14,
                        .textColor = UI_COLOR_TEXT
                    }));

                    // FFT Size control
                    CLAY({
                        .id = CLAY_ID("FFTControl"),
                        .layout = {
                            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32)},
                            .padding = CLAY_PADDING_ALL(6),
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}
                        }
                    }) {
                        CLAY_TEXT(CLAY_STRING("FFT Size: 4096"), CLAY_TEXT_CONFIG({
                            .fontId = FONT_ID_BODY,
                            .fontSize = 12,
                            .textColor = UI_COLOR_TEXT_SECONDARY
                        }));
                    }

                    // Hop size control
                    CLAY({
                        .id = CLAY_ID("HopControl"),
                        .layout = {
                            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32)},
                            .padding = CLAY_PADDING_ALL(6),
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}
                        }
                    }) {
                        CLAY_TEXT(CLAY_STRING("Hop Size: 1024"), CLAY_TEXT_CONFIG({
                            .fontId = FONT_ID_BODY,
                            .fontSize = 12,
                            .textColor = UI_COLOR_TEXT_SECONDARY
                        }));
                    }

                    // Averaging control
                    CLAY({
                        .id = CLAY_ID("AvgControl"),
                        .layout = {
                            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(32)},
                            .padding = CLAY_PADDING_ALL(6),
                            .childAlignment = {.y = CLAY_ALIGN_Y_CENTER}
                        }
                    }) {
                        CLAY_TEXT(CLAY_STRING("Averaging: 20"), CLAY_TEXT_CONFIG({
                            .fontId = FONT_ID_BODY,
                            .fontSize = 12,
                            .textColor = UI_COLOR_TEXT_SECONDARY
                        }));
                    }
                }
            }

            // Right panel - Visualization
            CLAY({
                .id = CLAY_ID("RightPanel"),
                .layout = {
                    .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                    .layoutDirection = CLAY_TOP_TO_BOTTOM,
                    .childGap = 12
                }
            }) {
                // Spectrum display area
                CLAY({
                    .id = CLAY_ID("SpectrumArea"),
                    .layout = {
                        .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                        .padding = CLAY_PADDING_ALL(12)
                    },
                    .backgroundColor = UI_COLOR_SURFACE,
                    .cornerRadius = CLAY_CORNER_RADIUS(6)
                }) {
                    CLAY_TEXT(CLAY_STRING("Spectrum Visualization"), CLAY_TEXT_CONFIG({
                        .fontId = FONT_ID_BODY,
                        .fontSize = 16,
                        .textColor = UI_COLOR_TEXT
                    }));

                    CLAY({
                        .id = CLAY_ID("SpectrumPlaceholder"),
                        .layout = {
                            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
                            .padding = CLAY_PADDING_ALL(20),
                            .childAlignment = {.x = CLAY_ALIGN_X_CENTER, .y = CLAY_ALIGN_Y_CENTER}
                        },
                        .backgroundColor = {50, 50, 50, 255},
                        .cornerRadius = CLAY_CORNER_RADIUS(4)
                    }) {
                        CLAY_TEXT(CLAY_STRING("Load an IQ file to view spectrum"), CLAY_TEXT_CONFIG({
                            .fontId = FONT_ID_BODY,
                            .fontSize = 14,
                            .textColor = UI_COLOR_TEXT_SECONDARY
                        }));
                    }
                }
            }
        }
    }

    return Clay_EndLayout();
}

void iq_ui_render_file_browser(void) {
    CLAY({
        .id = CLAY_ID("FileBrowser"),
        .layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(200)},
            .padding = CLAY_PADDING_ALL(10),
            .childGap = 5
        },
        .backgroundColor = UI_COLOR_SURFACE,
        .cornerRadius = CLAY_CORNER_RADIUS(5)
    }) {
        CLAY_TEXT(CLAY_STRING("File Browser"), CLAY_TEXT_CONFIG({
            .fontId = FONT_ID_BODY,
            .fontSize = 14,
            .textColor = UI_COLOR_TEXT
        }));

        // File status
        CLAY({
            .id = CLAY_ID("FileStatus"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(30)},
                .padding = CLAY_PADDING_ALL(5)
            },
            .backgroundColor = g_ui_state.has_file_loaded ? (Clay_Color){50, 100, 50, 255} : (Clay_Color){100, 50, 50, 255},
            .cornerRadius = CLAY_CORNER_RADIUS(3)
        }) {
            CLAY_TEXT(
                g_ui_state.has_file_loaded ?
                CLAY_STRING("File Loaded") :
                CLAY_STRING("No File Loaded"),
                CLAY_TEXT_CONFIG({
                    .fontId = FONT_ID_BODY,
                    .fontSize = 12,
                    .textColor = UI_COLOR_TEXT
                })
            );
        }
    }
}

void iq_ui_render_spectrum_view(void) {
    CLAY({
        .id = CLAY_ID("SpectrumView"),
        .layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
            .padding = CLAY_PADDING_ALL(10)
        }
    }) {
        CLAY_TEXT(
            g_ui_state.has_file_loaded ?
            CLAY_STRING("Spectrum View (Processing...)") :
            CLAY_STRING("Load an IQ file to view spectrum"),
            CLAY_TEXT_CONFIG({
                .fontId = FONT_ID_BODY,
                .fontSize = 16,
                .textColor = UI_COLOR_TEXT_SECONDARY
            })
        );
    }
}

void iq_ui_render_controls(void) {
    CLAY({
        .id = CLAY_ID("Controls"),
        .layout = {
            .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_GROW(0)},
            .padding = CLAY_PADDING_ALL(10),
            .childGap = 10
        },
        .backgroundColor = UI_COLOR_SURFACE,
        .cornerRadius = CLAY_CORNER_RADIUS(5)
    }) {
        CLAY_TEXT(CLAY_STRING("Controls"), CLAY_TEXT_CONFIG({
            .fontId = FONT_ID_BODY,
            .fontSize = 14,
            .textColor = UI_COLOR_TEXT
        }));

        // FFT Size control would go here
        CLAY({
            .id = CLAY_ID("FFTControl"),
            .layout = {
                .sizing = {CLAY_SIZING_GROW(0), CLAY_SIZING_FIXED(40)},
                .padding = CLAY_PADDING_ALL(5)
            }
        }) {
            CLAY_TEXT(CLAY_STRING("FFT Size: 4096"), CLAY_TEXT_CONFIG({
                .fontId = FONT_ID_BODY,
                .fontSize = 12,
                .textColor = UI_COLOR_TEXT_SECONDARY
            }));
        }
    }
}

void iq_ui_run_main_loop(void) {
    MSG msg;
    while (g_ui_state.is_running && GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
}

Clay_Dimensions iq_ui_measure_text(Clay_StringSlice text, Clay_TextElementConfig* config, void* userData) {
    (void)userData; // Suppress unused parameter warning
    // This is a simple implementation - in production, you'd use proper text measurement
    return (Clay_Dimensions){
        .width = (float)(text.length * config->fontSize * 0.6f),
        .height = (float)config->fontSize
    };
}

void iq_ui_handle_clay_errors(Clay_ErrorData errorData) {
    printf("Clay Error: %.*s\n", (int)errorData.errorText.length, errorData.errorText.chars);
}

// Stub implementations for now
bool iq_ui_load_iq_file(const char* filepath) {
    strncpy(g_ui_state.current_file_path, filepath, sizeof(g_ui_state.current_file_path) - 1);
    g_ui_state.has_file_loaded = true;
    g_ui_state.spectrum_dirty = true;
    return true;
}

void iq_ui_process_spectrum(void) {
    // TODO: Integrate with existing IQ processing
    if (g_ui_state.spectrum_data) {
        // Fill with dummy data for now
        for (size_t i = 0; i < g_ui_state.spectrum_size; i++) {
            g_ui_state.spectrum_data[i] = (float)rand() / RAND_MAX;
        }
        g_ui_state.spectrum_dirty = false;
    }
}
