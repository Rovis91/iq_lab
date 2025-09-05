#include <stdio.h>
#include <stdlib.h>
#include <windows.h>

#include "../src/ui/ui.h"

int main(int argc, char* argv[]) {
    (void)argc; (void)argv; // Suppress unused parameter warnings
    printf("IQ Lab UI - Starting...\n");

    // Initialize UI configuration
    IQ_UI_Config config = {
        .window_width = 1200,
        .window_height = 800,
        .window_title = "IQ Lab - Signal Analysis Tool",
        .font_path = "C:\\Windows\\Fonts\\arial.ttf", // Use system Arial font
        .font_size = 12,
        .debug_mode = false
    };

    printf("UI Config: %dx%d, font: %s, size: %d\n", config.window_width, config.window_height, config.font_path, config.font_size);

    // Initialize UI
    if (!iq_ui_init(&config)) {
        fprintf(stderr, "Failed to initialize UI\n");
        return EXIT_FAILURE;
    }

    printf("UI initialized successfully!\n");
    printf("Controls:\n");
    printf("  ESC - Exit\n");
    printf("  F12 - Toggle debug mode\n");
    printf("  Mouse - Interact with UI\n");
    printf("  Scroll wheel - Scroll\n");

    // Run main loop
    iq_ui_run_main_loop();

    // Cleanup
    iq_ui_shutdown();

    printf("IQ Lab UI - Exiting...\n");
    return EXIT_SUCCESS;
}
