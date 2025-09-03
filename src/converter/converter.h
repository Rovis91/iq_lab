#ifndef IQ_CONVERTER_H
#define IQ_CONVERTER_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// Supported file formats
typedef enum {
    FORMAT_AUTO,      // Automatic detection
    FORMAT_IQ_S8,     // Native IQ 8-bit
    FORMAT_IQ_S16,    // Native IQ 16-bit
    FORMAT_WAV,       // WAV IQ
    FORMAT_SIGMF,     // SigMF (future)
    FORMAT_HDF5       // HDF5 (future)
} file_format_t;

// Conversion options
typedef struct {
    bool force_overwrite;      // Overwrite destination file
    bool verbose;             // Verbose mode
    uint32_t sample_rate;     // Sample rate (0 = auto-detection)
    size_t max_memory_mb;     // Memory limit in MB (0 = unlimited)
    bool preserve_metadata;   // Preserve metadata if possible
} converter_options_t;

// File metadata
typedef struct {
    file_format_t format;
    uint32_t sample_rate;
    size_t num_samples;
    size_t data_size_bytes;
    char description[256];
    // Format-specific metadata can be added here
} file_metadata_t;

// Conversion request
typedef struct {
    const char *input_file;
    const char *output_file;
    file_format_t input_format;
    file_format_t output_format;
    converter_options_t options;
} conversion_request_t;

// Conversion result
typedef struct {
    bool success;
    size_t samples_converted;
    size_t bytes_processed;
    double conversion_time_seconds;
    file_metadata_t input_metadata;
    file_metadata_t output_metadata;
    char error_message[512];
} converter_result_t;

/*
 * Fonction principale de conversion de fichiers
 * Convertit n'importe quel format supporté vers n'importe quel autre format supporté
 */
converter_result_t convert_file(const conversion_request_t *request);

/*
 * Détecte automatiquement le format d'un fichier
 * Retourne FORMAT_AUTO si la détection échoue
 */
file_format_t detect_file_format(const char *filename);

/*
 * Extrait les métadonnées d'un fichier sans le convertir
 */
bool extract_file_metadata(const char *filename, file_metadata_t *metadata);

/*
 * Vérifie si une conversion est possible entre deux formats
 */
bool is_conversion_supported(file_format_t from_format, file_format_t to_format);

/*
 * Retourne une description lisible d'un format
 */
const char *get_format_description(file_format_t format);

/*
 * Retourne l'extension de fichier recommandée pour un format
 */
const char *get_format_extension(file_format_t format);

/*
 * Initialise les options de conversion avec des valeurs par défaut
 */
void init_converter_options(converter_options_t *options);

/*
 * Valide une requête de conversion
 * Retourne true si la requête est valide, false sinon
 * Remplit error_message en cas d'erreur
 */
bool validate_conversion_request(const conversion_request_t *request, char *error_message, size_t max_length);

/*
 * Check if a WAV file contains IQ data (public function for analysis)
 * Returns true if IQ data is detected, false for regular audio
 */
bool is_wav_iq_format(const char *filename);

#endif // IQ_CONVERTER_H
