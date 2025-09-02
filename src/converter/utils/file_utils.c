#include "file_utils.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <time.h>

#ifdef _WIN32
#include <direct.h>
#define mkdir(path, mode) _mkdir(path)
#endif

bool file_copy(const char *source_path, const char *dest_path) {
    if (!source_path || !dest_path) {
        return false;
    }

    FILE *source_file = fopen(source_path, "rb");
    if (!source_file) {
        return false;
    }

    FILE *dest_file = fopen(dest_path, "wb");
    if (!dest_file) {
        fclose(source_file);
        return false;
    }

    // Copy in 64 KB blocks
    const size_t BUFFER_SIZE = 65536;
    uint8_t *buffer = (uint8_t *)malloc(BUFFER_SIZE);

    if (!buffer) {
        fclose(source_file);
        fclose(dest_file);
        return false;
    }

    bool success = true;
    size_t bytes_read;

    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, source_file)) > 0) {
        size_t bytes_written = fwrite(buffer, 1, bytes_read, dest_file);
        if (bytes_written != bytes_read) {
            success = false;
            break;
        }
    }

    free(buffer);
    fclose(source_file);
    fclose(dest_file);

    return success;
}

size_t get_file_size(const char *filepath) {
    if (!filepath) {
        return 0;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return 0;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return 0;
    }

    long size = ftell(file);
    fclose(file);

    return (size < 0) ? 0 : (size_t)size;
}

bool file_exists(const char *filepath) {
    if (!filepath) {
        return false;
    }

    FILE *file = fopen(filepath, "rb");
    if (file) {
        fclose(file);
        return true;
    }

    return false;
}

bool create_parent_directories(const char *filepath) {
    if (!filepath) {
        return false;
    }

    // Copie du chemin pour le modifier
    char *path_copy = strdup(filepath);
    if (!path_copy) {
        return false;
    }

    // Find the last directory separator
    char *last_sep = strrchr(path_copy, '/');
    if (!last_sep) {
        last_sep = strrchr(path_copy, '\\');
    }

    if (last_sep) {
        *last_sep = '\0';  // Cut path at parent directory level

        // Create directory (works recursively on some platforms)
        if (mkdir(path_copy, S_IRWXU) != 0 && errno != EEXIST) {
            free(path_copy);
            return false;
        }
    }

    free(path_copy);
    return true;
}

bool generate_temp_filename(char *buffer, size_t buffer_size, const char *prefix) {
    if (!buffer || buffer_size == 0) {
        return false;
    }

    // Use current time to generate unique name
    time_t now = time(NULL);
    struct tm *tm_info = localtime(&now);

    if (!tm_info) {
        return false;
    }

    // Format: prefix_YYYYMMDD_HHMMSS
    const char *actual_prefix = prefix ? prefix : "temp";
    int written = snprintf(buffer, buffer_size, "%s_%04d%02d%02d_%02d%02d%02d",
                          actual_prefix,
                          tm_info->tm_year + 1900,
                          tm_info->tm_mon + 1,
                          tm_info->tm_mday,
                          tm_info->tm_hour,
                          tm_info->tm_min,
                          tm_info->tm_sec);

    return (written > 0 && (size_t)written < buffer_size);
}

bool safe_remove_file(const char *filepath) {
    if (!filepath) {
        return false;
    }

    // Check that file exists before deleting
    if (!file_exists(filepath)) {
        return true;  // Consider successful if file doesn't exist
    }

    return (remove(filepath) == 0);
}

uint32_t calculate_file_checksum(const char *filepath) {
    if (!filepath) {
        return 0;
    }

    FILE *file = fopen(filepath, "rb");
    if (!file) {
        return 0;
    }

    uint32_t checksum = 0;
    const size_t BUFFER_SIZE = 4096;
    uint8_t buffer[BUFFER_SIZE];

    size_t bytes_read;
    while ((bytes_read = fread(buffer, 1, BUFFER_SIZE, file)) > 0) {
        // Simple checksum calculation
        for (size_t i = 0; i < bytes_read; i++) {
            checksum = (checksum << 5) + checksum + buffer[i];  // hash simple
        }
    }

    fclose(file);
    return checksum;
}
