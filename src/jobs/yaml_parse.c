/*
 * IQ Lab - yaml_parse.c: Minimal YAML Parser Implementation
 *
 * Purpose: Parse YAML pipeline configurations without external dependencies.
 * Implements a recursive descent parser for the specific YAML structure used
 * in IQ Lab batch processing pipelines.
 *
 * Supported Features:
 * - Key-value pairs
 * - Nested mappings
 * - Arrays/lists
 * - Basic data types (strings, numbers, booleans)
 * - Comments (ignored)
 *
 * Limitations:
 * - No complex YAML features (anchors, aliases, etc.)
 * - Indentation-based structure only
 * - Single document per file
 *
 *
 * Date: 2025
 */

#include "yaml_parse.h"
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdio.h>

/**
 * @brief Skip whitespace characters
 */
static void skip_whitespace(const char *text, size_t *position, size_t length) {
    while (*position < length && isspace(text[*position])) {
        (*position)++;
    }
}

/**
 * @brief Skip comment lines
 */
static void skip_comments(const char *text, size_t *position, size_t length) {
    if (*position < length && text[*position] == '#') {
        while (*position < length && text[*position] != '\n') {
            (*position)++;
        }
        if (*position < length) (*position)++; // Skip newline
    }
}

/**
 * @brief Parse a quoted string value
 */
static bool parse_quoted_string(const char *text, size_t *position, size_t length,
                               char *buffer, size_t buffer_size) {
    if (*position >= length || text[*position] != '"') {
        return false;
    }

    (*position)++; // Skip opening quote
    size_t start = *position;

    while (*position < length && text[*position] != '"') {
        if (text[*position] == '\\') {
            (*position)++; // Skip escape character
        }
        (*position)++;
    }

    if (*position >= length) {
        return false; // Unterminated string
    }

    size_t string_length = *position - start;
    if (string_length >= buffer_size) {
        return false; // String too long
    }

    memcpy(buffer, &text[start], string_length);
    buffer[string_length] = '\0';

    (*position)++; // Skip closing quote
    return true;
}

/**
 * @brief Parse an unquoted string value
 */
static bool parse_unquoted_string(const char *text, size_t *position, size_t length,
                                 char *buffer, size_t buffer_size) {
    size_t i = 0;

    while (*position < length && !isspace(text[*position]) &&
           text[*position] != '\n' && text[*position] != '\r' &&
           text[*position] != ',' && text[*position] != '}' &&
           text[*position] != ']' && text[*position] != '#') {
        if (i >= buffer_size - 1) {
            return false; // String too long
        }
        buffer[i++] = text[*position];
        (*position)++;
    }

    buffer[i] = '\0';
    return i > 0;
}

/**
 * @brief Parse a key-value pair
 */
static bool parse_key_value(const char *text, size_t *position, size_t length,
                           char *key, size_t key_size, char *value, size_t value_size) {
    skip_whitespace(text, position, length);
    skip_comments(text, position, length);
    skip_whitespace(text, position, length);

    // Parse key
    if (!parse_unquoted_string(text, position, length, key, key_size)) {
        return false;
    }

    skip_whitespace(text, position, length);

    // Expect colon
    if (*position >= length || text[*position] != ':') {
        return false;
    }
    (*position)++;

    skip_whitespace(text, position, length);

    // Parse value
    if (*position < length && text[*position] == '"') {
        // Quoted string
        if (!parse_quoted_string(text, position, length, value, value_size)) {
            return false;
        }
    } else {
        // Unquoted value
        if (!parse_unquoted_string(text, position, length, value, value_size)) {
            return false;
        }
    }

    return true;
}

/**
 * @brief Parse inputs section
 */
static bool parse_inputs_section(yaml_parser_t *parser, yaml_document_t *document) {
    skip_whitespace(parser->input_text, &parser->position, parser->input_length);

    while (parser->position < parser->input_length) {
        skip_comments(parser->input_text, &parser->position, parser->input_length);
        skip_whitespace(parser->input_text, &parser->position, parser->input_length);

        // Check for end of section (next top-level key or end)
        if (parser->position < parser->input_length) {
            const char *line_start = &parser->input_text[parser->position];
            if (line_start[0] != ' ' && line_start[0] != '\t' &&
                line_start[0] != '\n' && line_start[0] != '\r') {
                // Check if this is a top-level key
                if (strstr(line_start, "pipeline:") == line_start ||
                    strstr(line_start, "report:") == line_start) {
                    break;
                }
            }
        }

        if (parser->position >= parser->input_length ||
            parser->input_text[parser->position] != '-') {
            break;
        }

        parser->position++; // Skip '-'
        skip_whitespace(parser->input_text, &parser->position, parser->input_length);

        if (document->input_count >= 16) {
            snprintf(parser->error_message, sizeof(parser->error_message),
                    "Too many inputs (maximum 16)");
            return false;
        }

        yaml_input_t *input = &document->inputs[document->input_count++];

        // Parse input mapping
        if (parser->position < parser->input_length &&
            parser->input_text[parser->position] == '{') {
            parser->position++; // Skip '{'

            while (parser->position < parser->input_length &&
                   parser->input_text[parser->position] != '}') {
                char key[64], value[256];

                if (!parse_key_value(parser->input_text, &parser->position,
                                   parser->input_length, key, sizeof(key),
                                   value, sizeof(value))) {
                    break;
                }

                if (strcmp(key, "file") == 0) {
                    strcpy(input->file, value);
                } else if (strcmp(key, "meta") == 0) {
                    strcpy(input->meta, value);
                } else if (strcmp(key, "format") == 0) {
                    strcpy(input->format, value);
                } else if (strcmp(key, "sample_rate") == 0) {
                    input->sample_rate = (uint64_t)atoll(value);
                } else if (strcmp(key, "frequency") == 0) {
                    input->frequency = atof(value);
                }

                skip_whitespace(parser->input_text, &parser->position, parser->input_length);
                if (parser->position < parser->input_length &&
                    parser->input_text[parser->position] == ',') {
                    parser->position++; // Skip comma
                }
            }

            if (parser->position < parser->input_length) {
                parser->position++; // Skip '}'
            }
        }
    }

    return true;
}

/**
 * @brief Parse pipeline section
 */
static bool parse_pipeline_section(yaml_parser_t *parser, yaml_document_t *document) {
    skip_whitespace(parser->input_text, &parser->position, parser->input_length);

    while (parser->position < parser->input_length) {
        skip_comments(parser->input_text, &parser->position, parser->input_length);
        skip_whitespace(parser->input_text, &parser->position, parser->input_length);

        // Check for end of section
        if (parser->position < parser->input_length) {
            const char *line_start = &parser->input_text[parser->position];
            if (line_start[0] != ' ' && line_start[0] != '\t' &&
                strstr(line_start, "report:") == line_start) {
                break;
            }
        }

        if (parser->position >= parser->input_length ||
            parser->input_text[parser->position] != '-') {
            break;
        }

        parser->position++; // Skip '-'
        skip_whitespace(parser->input_text, &parser->position, parser->input_length);

        if (document->pipeline_count >= 32) {
            snprintf(parser->error_message, sizeof(parser->error_message),
                    "Too many pipeline steps (maximum 32)");
            return false;
        }

        yaml_pipeline_step_t *step = &document->pipeline[document->pipeline_count++];

        // Parse tool name
        if (!parse_unquoted_string(parser->input_text, &parser->position,
                                 parser->input_length, step->tool_name,
                                 sizeof(step->tool_name))) {
            return false;
        }

        skip_whitespace(parser->input_text, &parser->position, parser->input_length);

        // Parse parameters
        if (parser->position < parser->input_length &&
            parser->input_text[parser->position] == ':') {
            parser->position++; // Skip ':'
            skip_whitespace(parser->input_text, &parser->position, parser->input_length);

            if (parser->position < parser->input_length &&
                parser->input_text[parser->position] == '{') {
                parser->position++; // Skip '{'

                while (parser->position < parser->input_length &&
                       parser->input_text[parser->position] != '}') {
                    char key[64], value[256];

                    if (!parse_key_value(parser->input_text, &parser->position,
                                       parser->input_length, key, sizeof(key),
                                       value, sizeof(value))) {
                        break;
                    }

                    if (step->param_count < 32) {
                        strcpy(step->params[step->param_count].key, key);
                        strcpy(step->params[step->param_count].value, value);
                        step->param_count++;
                    }

                    skip_whitespace(parser->input_text, &parser->position, parser->input_length);
                    if (parser->position < parser->input_length &&
                        parser->input_text[parser->position] == ',') {
                        parser->position++; // Skip comma
                    }
                }

                if (parser->position < parser->input_length) {
                    parser->position++; // Skip '}'
                }
            }
        }
    }

    return true;
}

/**
 * @brief Parse report section
 */
static bool parse_report_section(yaml_parser_t *parser, yaml_document_t *document) {
    skip_whitespace(parser->input_text, &parser->position, parser->input_length);

    if (parser->position < parser->input_length &&
        parser->input_text[parser->position] == '{') {
        parser->position++; // Skip '{'

        while (parser->position < parser->input_length &&
               parser->input_text[parser->position] != '}') {
            char key[64], value[256];

            if (!parse_key_value(parser->input_text, &parser->position,
                               parser->input_length, key, sizeof(key),
                               value, sizeof(value))) {
                break;
            }

            if (strcmp(key, "consolidate") == 0) {
                // Parse nested consolidate object
                if (parser->position < parser->input_length &&
                    parser->input_text[parser->position] == '{') {
                    parser->position++; // Skip '{'

                    while (parser->position < parser->input_length &&
                           parser->input_text[parser->position] != '}') {
                        char sub_key[64], sub_value[256];

                        if (!parse_key_value(parser->input_text, &parser->position,
                                           parser->input_length, sub_key, sizeof(sub_key),
                                           sub_value, sizeof(sub_value))) {
                            break;
                        }

                        if (strcmp(sub_key, "events") == 0) {
                            document->report.consolidate_events = (strcmp(sub_value, "true") == 0);
                        } else if (strcmp(sub_key, "spectra") == 0) {
                            document->report.consolidate_spectra = (strcmp(sub_value, "true") == 0);
                        } else if (strcmp(sub_key, "audio") == 0) {
                            document->report.consolidate_audio = (strcmp(sub_value, "true") == 0);
                        }

                        skip_whitespace(parser->input_text, &parser->position, parser->input_length);
                        if (parser->position < parser->input_length &&
                            parser->input_text[parser->position] == ',') {
                            parser->position++; // Skip comma
                        }
                    }

                    if (parser->position < parser->input_length) {
                        parser->position++; // Skip '}'
                    }
                }
            } else if (strcmp(key, "output_dir") == 0) {
                strcpy(document->report.output_dir, value);
            }

            skip_whitespace(parser->input_text, &parser->position, parser->input_length);
            if (parser->position < parser->input_length &&
                parser->input_text[parser->position] == ',') {
                parser->position++; // Skip comma
            }
        }

        if (parser->position < parser->input_length) {
            parser->position++; // Skip '}'
        }
    }

    return true;
}

/**
 * @brief Initialize YAML parser
 */
bool yaml_parser_init(yaml_parser_t *parser, const char *yaml_text, size_t text_length) {
    if (!parser || !yaml_text) {
        return false;
    }

    parser->input_text = yaml_text;
    parser->input_length = text_length;
    parser->position = 0;
    parser->line_number = 1;
    parser->indent_level = 0;
    parser->error_message[0] = '\0';

    return true;
}

/**
 * @brief Parse YAML document
 */
bool yaml_parse_document(yaml_parser_t *parser, yaml_document_t *document) {
    if (!parser || !document) {
        return false;
    }

    // Initialize document
    memset(document, 0, sizeof(yaml_document_t));

    // Parse sections
    while (parser->position < parser->input_length) {
        skip_whitespace(parser->input_text, &parser->position, parser->input_length);
        skip_comments(parser->input_text, &parser->position, parser->input_length);

        if (parser->position >= parser->input_length) {
            break;
        }

        // Check for section headers
        if (strstr(&parser->input_text[parser->position], "inputs:") ==
            &parser->input_text[parser->position]) {
            parser->position += 7; // Skip "inputs:"
            if (!parse_inputs_section(parser, document)) {
                return false;
            }
        } else if (strstr(&parser->input_text[parser->position], "pipeline:") ==
                   &parser->input_text[parser->position]) {
            parser->position += 9; // Skip "pipeline:"
            if (!parse_pipeline_section(parser, document)) {
                return false;
            }
        } else if (strstr(&parser->input_text[parser->position], "report:") ==
                   &parser->input_text[parser->position]) {
            parser->position += 7; // Skip "report:"
            if (!parse_report_section(parser, document)) {
                return false;
            }
        } else {
            // Skip unknown lines
            while (parser->position < parser->input_length &&
                   parser->input_text[parser->position] != '\n') {
                parser->position++;
            }
            if (parser->position < parser->input_length) {
                parser->position++;
            }
        }
    }

    return true;
}

/**
 * @brief Get parser error message
 */
const char *yaml_parser_get_error(const yaml_parser_t *parser) {
    return parser ? parser->error_message : "";
}

/**
 * @brief Load YAML from file
 */
bool yaml_load_file(const char *filename, yaml_document_t *document) {
    if (!filename || !document) {
        return false;
    }

    FILE *file = fopen(filename, "r");
    if (!file) {
        return false;
    }

    // Get file size
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    fseek(file, 0, SEEK_SET);

    if (file_size <= 0) {
        fclose(file);
        return false;
    }

    // Read file content
    char *buffer = (char *)malloc(file_size + 1);
    if (!buffer) {
        fclose(file);
        return false;
    }

    size_t bytes_read = fread(buffer, 1, file_size, file);
    buffer[bytes_read] = '\0';
    fclose(file);

    // Parse YAML
    yaml_parser_t parser;
    if (!yaml_parser_init(&parser, buffer, bytes_read)) {
        free(buffer);
        return false;
    }

    bool success = yaml_parse_document(&parser, document);
    free(buffer);

    return success;
}

/**
 * @brief Validate YAML document structure
 */
bool yaml_validate_document(const yaml_document_t *document) {
    if (!document) {
        return false;
    }

    // Must have at least one input
    if (document->input_count == 0) {
        return false;
    }

    // Must have at least one pipeline step
    if (document->pipeline_count == 0) {
        return false;
    }

    // Validate inputs
    for (uint32_t i = 0; i < document->input_count; i++) {
        const yaml_input_t *input = &document->inputs[i];
        if (strlen(input->file) == 0) {
            return false; // Must have file
        }
    }

    // Validate pipeline steps
    for (uint32_t i = 0; i < document->pipeline_count; i++) {
        const yaml_pipeline_step_t *step = &document->pipeline[i];
        if (strlen(step->tool_name) == 0) {
            return false; // Must have tool name
        }
    }

    return true;
}

/**
 * @brief Free YAML document resources
 */
void yaml_free_document(yaml_document_t *document) {
    if (!document) {
        return;
    }

    // Nothing to free currently - all data is stored inline
    memset(document, 0, sizeof(yaml_document_t));
}

/**
 * @brief Get string representation of YAML document
 */
bool yaml_document_to_string(const yaml_document_t *document,
                           char *buffer, uint32_t buffer_size) {
    if (!document || !buffer || buffer_size == 0) {
        return false;
    }

    int written = 0;
    int remaining = buffer_size;

    // Write inputs section
    written += snprintf(buffer + written, remaining, "inputs:\n");
    remaining -= written;

    for (uint32_t i = 0; i < document->input_count && remaining > 0; i++) {
        const yaml_input_t *input = &document->inputs[i];
        written += snprintf(buffer + written, remaining,
                          "  - file: %s\n", input->file);
        remaining -= written;

        if (strlen(input->meta) > 0 && remaining > 0) {
            written += snprintf(buffer + written, remaining,
                              "    meta: %s\n", input->meta);
            remaining -= written;
        }
    }

    // Write pipeline section
    if (remaining > 0) {
        written += snprintf(buffer + written, remaining, "\npipeline:\n");
        remaining -= written;
    }

    for (uint32_t i = 0; i < document->pipeline_count && remaining > 0; i++) {
        const yaml_pipeline_step_t *step = &document->pipeline[i];
        written += snprintf(buffer + written, remaining,
                          "  - %s:", step->tool_name);
        remaining -= written;

        if (step->param_count > 0 && remaining > 0) {
            written += snprintf(buffer + written, remaining, " {");
            remaining -= written;

            for (uint32_t j = 0; j < step->param_count && remaining > 0; j++) {
                written += snprintf(buffer + written, remaining,
                                  "%s: %s", step->params[j].key,
                                  step->params[j].value);
                remaining -= written;

                if (j < step->param_count - 1 && remaining > 0) {
                    written += snprintf(buffer + written, remaining, ", ");
                    remaining -= written;
                }
            }

            if (remaining > 0) {
                written += snprintf(buffer + written, remaining, "}\n");
                remaining -= written;
            }
        } else if (remaining > 0) {
            written += snprintf(buffer + written, remaining, " {}\n");
            remaining -= written;
        }
    }

    return written > 0 && remaining > 0;
}

/**
 * @brief Get pipeline step parameter value
 */
const char *yaml_get_step_param(const yaml_pipeline_step_t *step, const char *key) {
    if (!step || !key) {
        return NULL;
    }

    for (uint32_t i = 0; i < step->param_count; i++) {
        if (strcmp(step->params[i].key, key) == 0) {
            return step->params[i].value;
        }
    }

    return NULL;
}

/**
 * @brief Set pipeline step parameter
 */
bool yaml_set_step_param(yaml_pipeline_step_t *step, const char *key, const char *value) {
    if (!step || !key || !value || step->param_count >= 32) {
        return false;
    }

    strcpy(step->params[step->param_count].key, key);
    strcpy(step->params[step->param_count].value, value);
    step->param_count++;

    return true;
}
