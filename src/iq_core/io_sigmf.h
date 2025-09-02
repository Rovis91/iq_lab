#ifndef IQ_IO_SIGMF_H
#define IQ_IO_SIGMF_H

#include <stdbool.h>
#include <stdint.h>
#include <time.h>

/*
 * Minimal SigMF v1.2.0 implementation for IQ Lab
 * Focus on core fields needed for basic RF analysis
 */

// SigMF data types
typedef enum {
    SIGMF_DATATYPE_CI8,    // Complex int8 (s8 I/Q)
    SIGMF_DATATYPE_CI16,   // Complex int16 (s16 I/Q)
    SIGMF_DATATYPE_CI32,   // Complex int32
    SIGMF_DATATYPE_CF32,   // Complex float32
    SIGMF_DATATYPE_CF64    // Complex float64
} sigmf_datatype_t;

// SigMF global metadata
typedef struct {
    char version[16];           // "1.2.0"
    char datatype[8];           // "ci8", "ci16", etc.
    uint64_t sample_rate;       // Sample rate in Hz
    uint64_t frequency;         // Center frequency in Hz
    char description[256];      // Capture description
    char author[128];           // Author name
    char datetime[32];          // ISO 8601 datetime
    char license[128];          // License info
    char hw_info[256];          // Hardware information
} sigmf_global_t;

// SigMF capture metadata (per capture segment)
typedef struct {
    uint64_t sample_start;      // Starting sample index
    uint64_t frequency;         // Frequency for this capture
    char datetime[32];          // Capture datetime
    double duration_s;          // Duration in seconds
} sigmf_capture_t;

// SigMF annotation metadata
typedef struct {
    uint64_t sample_start;      // Start sample
    uint64_t sample_count;      // Number of samples
    uint64_t freq_lower_edge;   // Lower frequency bound
    uint64_t freq_upper_edge;   // Upper frequency bound
    char label[128];            // Annotation label
    char description[256];      // Description
} sigmf_annotation_t;

// Complete SigMF metadata structure
typedef struct {
    sigmf_global_t global;
    sigmf_capture_t *captures;     // Array of captures
    size_t num_captures;
    sigmf_annotation_t *annotations; // Array of annotations
    size_t num_annotations;
} sigmf_metadata_t;

/*
 * Function declarations
 */

// Initialize SigMF metadata structure
void sigmf_init_metadata(sigmf_metadata_t *metadata);

// Free SigMF metadata structure
void sigmf_free_metadata(sigmf_metadata_t *metadata);

// Read SigMF metadata from .sigmf-meta file
bool sigmf_read_metadata(const char *filename, sigmf_metadata_t *metadata);

// Write SigMF metadata to .sigmf-meta file
bool sigmf_write_metadata(const char *filename, const sigmf_metadata_t *metadata);

// Create SigMF metadata from basic parameters
bool sigmf_create_basic_metadata(sigmf_metadata_t *metadata,
                                const char *datatype,
                                uint64_t sample_rate,
                                uint64_t frequency,
                                const char *description,
                                const char *author);

// Add capture segment to metadata
bool sigmf_add_capture(sigmf_metadata_t *metadata,
                      uint64_t sample_start,
                      uint64_t frequency,
                      const char *datetime);

// Add annotation to metadata
bool sigmf_add_annotation(sigmf_metadata_t *metadata,
                         uint64_t sample_start,
                         uint64_t sample_count,
                         uint64_t freq_lower,
                         uint64_t freq_upper,
                         const char *label);

// Get SigMF metadata filename from IQ filename
// e.g., "capture.iq" -> "capture.sigmf-meta"
void sigmf_get_meta_filename(const char *iq_filename, char *meta_filename, size_t max_len);

// Check if SigMF metadata file exists
bool sigmf_meta_file_exists(const char *iq_filename);

// Convert datatype string to enum
sigmf_datatype_t sigmf_parse_datatype(const char *datatype_str);

// Convert datatype enum to string
const char *sigmf_datatype_to_string(sigmf_datatype_t datatype);

// Get current datetime in ISO 8601 format
void sigmf_get_current_datetime(char *datetime_str, size_t max_len);

#endif // IQ_IO_SIGMF_H
