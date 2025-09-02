#ifndef IQ_IQ_CONVERTER_H
#define IQ_IQ_CONVERTER_H

#include "../converter.h"

/*
 * Functions for conversion between native IQ formats
 * (s8 ↔ s16, resolution changes, etc.)
 */

// Check if file can be handled by this converter
bool iq_can_handle(const char *filename);

// Convert between IQ formats (s8 ↔ s16)
converter_result_t iq_to_iq(const conversion_request_t *request);

// Extract metadata from native IQ file
bool iq_extract_metadata(const char *filename, file_metadata_t *metadata);

// Internal utility functions (defined in iq_converter.c)

#endif // IQ_IQ_CONVERTER_H
