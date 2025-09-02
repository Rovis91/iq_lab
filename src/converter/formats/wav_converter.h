#ifndef IQ_WAV_CONVERTER_H
#define IQ_WAV_CONVERTER_H

#include "../converter.h"

/*
 * Functions for WAV IQ file conversion
 */

// Check if file can be handled by this converter
bool wav_can_handle(const char *filename);

// Convert WAV file to native IQ format
converter_result_t wav_to_iq(const conversion_request_t *request);

// Convert native IQ file to WAV format (future)
converter_result_t iq_to_wav(const conversion_request_t *request);

// Extract metadata from WAV file
bool wav_extract_metadata(const char *filename, file_metadata_t *metadata);

#endif // IQ_WAV_CONVERTER_H
