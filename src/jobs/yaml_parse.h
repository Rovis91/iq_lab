/*
 * IQ Lab - yaml_parse.h: Minimal YAML Parser for Pipeline Configuration
 *
 * Purpose: Parse YAML pipeline configurations without external dependencies
 * Supports the specific YAML structure needed for IQ Lab batch processing.
 *
  *
 * Date: 2025
 *
 * Supported YAML Structure:
 *   inputs:
 *     - file: data/capture.iq
 *       meta: data/capture.sigmf-meta
 *   pipeline:
 *     - tool_name: { param1: value1, param2: value2 }
 *   report:
 *     consolidate: { events: true, spectra: true }
 */

#ifndef YAML_PARSE_H
#define YAML_PARSE_H

#include <stdint.h>
#include <stdbool.h>

// Forward declarations
typedef struct yaml_parser_t yaml_parser_t;
typedef struct yaml_document_t yaml_document_t;
typedef struct yaml_node_t yaml_node_t;
typedef struct yaml_input_t yaml_input_t;
typedef struct yaml_pipeline_step_t yaml_pipeline_step_t;
typedef struct yaml_report_config_t yaml_report_config_t;

/**
 * @brief YAML Input Configuration
 */
struct yaml_input_t {
    char file[256];         // Input file path
    char meta[256];         // Metadata file path (optional)
    char format[16];        // Data format (optional)
    uint64_t sample_rate;   // Sample rate (optional)
    double frequency;       // Center frequency (optional)
};

/**
 * @brief YAML Pipeline Step Configuration
 */
struct yaml_pipeline_step_t {
    char tool_name[64];     // Tool name (iqls, iqdetect, etc.)
    char output_dir[256];   // Output directory for this step

    // Tool-specific parameters (key-value pairs)
    struct {
        char key[64];
        char value[256];
    } params[32];           // Up to 32 parameters per step
    uint32_t param_count;
};

/**
 * @brief YAML Report Configuration
 */
struct yaml_report_config_t {
    bool consolidate_events;
    bool consolidate_spectra;
    bool consolidate_audio;
    char output_dir[256];
};

/**
 * @brief YAML Document Structure
 */
struct yaml_document_t {
    // Inputs section
    yaml_input_t inputs[16];        // Up to 16 input files
    uint32_t input_count;

    // Pipeline section
    yaml_pipeline_step_t pipeline[32];  // Up to 32 pipeline steps
    uint32_t pipeline_count;

    // Report section
    yaml_report_config_t report;
};

/**
 * @brief YAML Parser Context
 */
struct yaml_parser_t {
    const char *input_text;     // Input YAML text
    size_t input_length;        // Length of input text
    size_t position;            // Current parsing position
    uint32_t line_number;       // Current line number for error reporting
    uint32_t indent_level;      // Current indentation level
    char error_message[256];    // Error message buffer
};

/**
 * @brief Initialize YAML parser
 *
 * @param parser Pointer to parser structure
 * @param yaml_text YAML text to parse
 * @param text_length Length of YAML text
 * @return true on success, false on error
 */
bool yaml_parser_init(yaml_parser_t *parser, const char *yaml_text, size_t text_length);

/**
 * @brief Parse YAML document
 *
 * @param parser Pointer to initialized parser
 * @param document Pointer to document structure to fill
 * @return true on success, false on error
 */
bool yaml_parse_document(yaml_parser_t *parser, yaml_document_t *document);

/**
 * @brief Get parser error message
 *
 * @param parser Pointer to parser
 * @return Error message string
 */
const char *yaml_parser_get_error(const yaml_parser_t *parser);

/**
 * @brief Load YAML from file
 *
 * @param filename Path to YAML file
 * @param document Pointer to document structure to fill
 * @return true on success, false on error
 */
bool yaml_load_file(const char *filename, yaml_document_t *document);

/**
 * @brief Validate YAML document structure
 *
 * @param document Pointer to document to validate
 * @return true if valid, false if invalid
 */
bool yaml_validate_document(const yaml_document_t *document);

/**
 * @brief Free YAML document resources
 *
 * @param document Pointer to document to free
 */
void yaml_free_document(yaml_document_t *document);

/**
 * @brief Get string representation of YAML document
 *
 * @param document Pointer to document
 * @param buffer Output buffer
 * @param buffer_size Size of output buffer
 * @return true on success, false on error
 */
bool yaml_document_to_string(const yaml_document_t *document,
                           char *buffer, uint32_t buffer_size);

/**
 * @brief Helper function to get pipeline step parameter value
 *
 * @param step Pointer to pipeline step
 * @param key Parameter key to find
 * @return Parameter value string, or NULL if not found
 */
const char *yaml_get_step_param(const yaml_pipeline_step_t *step, const char *key);

/**
 * @brief Helper function to set pipeline step parameter
 *
 * @param step Pointer to pipeline step
 * @param key Parameter key
 * @param value Parameter value
 * @return true on success, false on error (too many parameters)
 */
bool yaml_set_step_param(yaml_pipeline_step_t *step, const char *key, const char *value);

#endif /* YAML_PARSE_H */
