/*
 * IQ Lab - SSB Demodulator Tool
 *
 * Purpose: Command-line SSB demodulator with BFO mixing
 * Author: IQ Lab Development Team
 *
 * This tool demodulates SSB (Single Sideband) signals from IQ data files
 * using professional BFO (Beat Frequency Oscillator) mixing techniques.
 * It supports both USB and LSB modes with configurable parameters.
 *
 * Key Features:
 * - BFO mixing for baseband conversion
 * - USB (Upper Sideband) and LSB (Lower Sideband) mode support
 * - Configurable BFO frequency and low-pass filtering
 * - Complex quadrature mixing with phase-continuous oscillators
 * - Automatic gain control with attack/release parameters
 * - High-quality audio resampling and filtering
 * - Support for s8/s16 IQ data formats
 *
 * Usage Examples:
 *   # Basic SSB demodulation (USB mode)
 *   iqdemod-ssb.exe --in capture.iq --out audio.wav
 *
 *   # LSB mode with custom BFO
 *   iqdemod-ssb.exe --in capture.iq --mode lsb --bfo 1700 --out audio.wav
 *
 *   # SSB with AGC and custom filtering
 *   iqdemod-ssb.exe --in capture.iq --agc --lpf-cutoff 2500 --out audio.wav
 *
 * Technical Details:
 * - Algorithm: Complex mixing with quadrature BFO + low-pass filtering
 * - BFO Range: Typically 1.5-3 kHz above/below carrier
 * - USB Mode: BFO frequency > carrier frequency
 * - LSB Mode: BFO frequency < carrier frequency
 * - Performance: O(1) per sample, real-time capable
 *
 * Input Formats:
 * - Raw IQ data: s8 (int8) or s16 (int16) interleaved I/Q samples
 * - Automatic format detection and sample rate handling
 * - SigMF metadata support for automatic parameter detection
 *
 * Output Format:
 * - 16-bit PCM WAV files (mono)
 * - Configurable sample rates with high-quality resampling
 * - Proper audio filtering and normalization
 *
 * Applications:
 * - Amateur radio SSB communications (HF bands)
 * - Professional SSB communications
 * - Military and government SSB systems
 * - Aeronautical SSB communications
 * - Any single-sideband modulated signal
 *
 * BFO Configuration Notes:
 * - USB: Set BFO 1.5-3 kHz above the suppressed carrier
 * - LSB: Set BFO 1.5-3 kHz below the suppressed carrier
 * - Fine tuning may be required for optimal reception
 * - BFO stability is critical for clean demodulation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <getopt.h>
#include <math.h>
#include "../src/iq_core/io_iq.h"
#include "../src/iq_core/io_sigmf.h"
#include "../src/demod/ssb.h"
#include "../src/demod/wave.h"
#include "../src/demod/agc.h"
#include "../src/iq_core/resample.h"

#define DEFAULT_AUDIO_RATE 48000
#define DEFAULT_BFO_FREQUENCY 1500.0f
#define DEFAULT_LPF_CUTOFF 3000.0f
#define DEFAULT_AGC_TARGET_DBFS -12.0f
#define DEFAULT_AGC_MAX_GAIN_DB 60.0f
#define BLOCK_SIZE 8192

// Command line arguments structure
typedef struct {
    const char *input_file;
    const char *meta_file;
    const char *output_file;
    int8_t format;  // 0=s8, 1=s16
    float sample_rate;
    float audio_rate;
    const char *mode_str;  // "usb" or "lsb"
    float bfo_frequency;
    float lpf_cutoff;
    bool enable_agc;
    float agc_target_dbfs;
    float agc_max_gain_db;
    bool verbose;
} args_t;

// Print usage information
void print_usage(const char *program_name) {
    printf("IQ Lab - SSB Demodulator Tool\n");
    printf("Demodulates SSB signals from IQ data to WAV audio\n\n");
    printf("Usage: %s [OPTIONS] --in <iq_file> --out <wav_file>\n\n", program_name);
    printf("Required Arguments:\n");
    printf("  --in <file>       Input IQ file (s8 or s16 interleaved)\n");
    printf("  --out <file>      Output WAV file\n");
    printf("\n");
    printf("Optional Arguments:\n");
    printf("  --meta <file>     SigMF metadata file (auto-detected if not specified)\n");
    printf("  --format {s8|s16} IQ data format (default: auto-detect)\n");
    printf("  --rate <Hz>       IQ sample rate (default: from metadata or 2000000)\n");
    printf("  --audio-rate <Hz> Output audio sample rate (default: %d)\n", DEFAULT_AUDIO_RATE);
    printf("  --mode {usb|lsb}  SSB mode (default: usb)\n");
    printf("  --bfo <Hz>        Beat Frequency Oscillator frequency (default: %.0f)\n", DEFAULT_BFO_FREQUENCY);
    printf("  --lpf-cutoff <Hz> Low-pass filter cutoff (default: %.0f)\n", DEFAULT_LPF_CUTOFF);
    printf("  --agc             Enable automatic gain control\n");
    printf("  --agc-target <dB> AGC target level (default: %.1f dBFS)\n", DEFAULT_AGC_TARGET_DBFS);
    printf("  --agc-max <dB>    AGC maximum gain (default: %.0f dB)\n", DEFAULT_AGC_MAX_GAIN_DB);
    printf("  --verbose         Verbose output\n");
    printf("  --help           Show this help message\n");
    printf("\n");
    printf("Examples:\n");
    printf("  %s --in capture.iq --out audio.wav\n", program_name);
    printf("  %s --in capture.iq --meta capture.sigmf-meta --out audio.wav --mode lsb --agc\n", program_name);
    printf("  %s --in capture.iq --rate 2000000 --mode usb --bfo 1700 --lpf-cutoff 2500 --out audio.wav\n", program_name);
}

// Parse SSB mode string
bool parse_ssb_mode(const char *mode_str, ssb_mode_t *mode) {
    if (strcmp(mode_str, "usb") == 0 || strcmp(mode_str, "USB") == 0) {
        *mode = SSB_MODE_USB;
        return true;
    } else if (strcmp(mode_str, "lsb") == 0 || strcmp(mode_str, "LSB") == 0) {
        *mode = SSB_MODE_LSB;
        return true;
    }
    return false;
}

// Parse command line arguments
bool parse_args(int argc, char *argv[], args_t *args) {
    // Initialize defaults
    memset(args, 0, sizeof(args_t));
    args->format = -1;  // Auto-detect
    args->sample_rate = 2000000.0f;  // Default if no metadata
    args->audio_rate = DEFAULT_AUDIO_RATE;
    args->mode_str = "usb";  // Default to USB
    args->bfo_frequency = DEFAULT_BFO_FREQUENCY;
    args->lpf_cutoff = DEFAULT_LPF_CUTOFF;
    args->enable_agc = false;
    args->agc_target_dbfs = DEFAULT_AGC_TARGET_DBFS;
    args->agc_max_gain_db = DEFAULT_AGC_MAX_GAIN_DB;
    args->verbose = false;

    struct option long_options[] = {
        {"in", required_argument, 0, 'i'},
        {"meta", required_argument, 0, 'm'},
        {"out", required_argument, 0, 'o'},
        {"format", required_argument, 0, 'f'},
        {"rate", required_argument, 0, 'r'},
        {"audio-rate", required_argument, 0, 'a'},
        {"mode", required_argument, 0, 'M'},
        {"bfo", required_argument, 0, 'b'},
        {"lpf-cutoff", required_argument, 0, 'l'},
        {"agc", no_argument, 0, 'g'},
        {"agc-target", required_argument, 0, 't'},
        {"agc-max", required_argument, 0, 'x'},
        {"verbose", no_argument, 0, 'v'},
        {"help", no_argument, 0, 'h'},
        {0, 0, 0, 0}
    };

    int opt;
    while ((opt = getopt_long(argc, argv, "i:m:o:f:r:a:M:b:l:gt:x:vh", long_options, NULL)) != -1) {
        switch (opt) {
            case 'i':
                args->input_file = optarg;
                break;
            case 'm':
                args->meta_file = optarg;
                break;
            case 'o':
                args->output_file = optarg;
                break;
            case 'f':
                if (strcmp(optarg, "s8") == 0) {
                    args->format = 0;
                } else if (strcmp(optarg, "s16") == 0) {
                    args->format = 1;
                } else {
                    fprintf(stderr, "Error: Invalid format '%s'. Use 's8' or 's16'\n", optarg);
                    return false;
                }
                break;
            case 'r':
                args->sample_rate = atof(optarg);
                break;
            case 'a':
                args->audio_rate = atof(optarg);
                break;
            case 'M':
                args->mode_str = optarg;
                break;
            case 'b':
                args->bfo_frequency = atof(optarg);
                break;
            case 'l':
                args->lpf_cutoff = atof(optarg);
                break;
            case 'g':
                args->enable_agc = true;
                break;
            case 't':
                args->agc_target_dbfs = atof(optarg);
                break;
            case 'x':
                args->agc_max_gain_db = atof(optarg);
                break;
            case 'v':
                args->verbose = true;
                break;
            case 'h':
                print_usage(argv[0]);
                exit(EXIT_SUCCESS);
            default:
                return false;
        }
    }

    // Validate required arguments
    if (!args->input_file) {
        fprintf(stderr, "Error: Input file is required (--in)\n");
        return false;
    }
    if (!args->output_file) {
        fprintf(stderr, "Error: Output file is required (--out)\n");
        return false;
    }

    return true;
}

// Load SigMF metadata and extract sample rate
bool load_metadata(const char *meta_file, float *sample_rate) {
    sigmf_metadata_t meta;
    sigmf_init_metadata(&meta);

    if (!sigmf_read_metadata(meta_file, &meta)) {
        fprintf(stderr, "Warning: Could not load metadata from '%s'\n", meta_file);
        sigmf_free_metadata(&meta);
        return false;
    }

    // Extract sample rate from metadata
    if (meta.global.sample_rate > 0) {
        *sample_rate = (float)meta.global.sample_rate;
        sigmf_free_metadata(&meta);
        return true;
    }

    fprintf(stderr, "Warning: No sample rate found in metadata\n");
    sigmf_free_metadata(&meta);
    return false;
}

// Main processing function
int process_ssb_demodulation(const args_t *args) {
    // Load IQ data
    iq_data_t iq_data;
    if (args->verbose) printf("Loading IQ data from '%s'...\n", args->input_file);

    if (!iq_load_file(args->input_file, &iq_data)) {
        fprintf(stderr, "Error: Failed to load IQ data from '%s'\n", args->input_file);
        return EXIT_FAILURE;
    }

    // Set sample rate (from command line, metadata, or default)
    uint32_t actual_sample_rate = args->sample_rate;
    if (args->meta_file) {
        float meta_rate;
        if (load_metadata(args->meta_file, &meta_rate)) {
            actual_sample_rate = (uint32_t)meta_rate;
            if (args->verbose) printf("Sample rate from metadata: %.0f Hz\n", meta_rate);
        }
    }

    // Override iq_data sample_rate with the determined rate
    iq_data.sample_rate = actual_sample_rate;

    if (args->verbose) {
        printf("Loaded %zu samples (%.2f seconds at %.0f Hz)\n",
               iq_data.num_samples, (double)iq_data.num_samples / actual_sample_rate, (double)actual_sample_rate);
    }

    // Parse SSB mode
    ssb_mode_t ssb_mode;
    if (!parse_ssb_mode(args->mode_str, &ssb_mode)) {
        fprintf(stderr, "Error: Invalid SSB mode '%s'. Use 'usb' or 'lsb'\n", args->mode_str);
        iq_free(&iq_data);
        return EXIT_FAILURE;
    }

    // Initialize SSB demodulator
    if (args->verbose) {
        printf("Initializing SSB demodulator...\n");
        printf("  Mode: %s\n", args->mode_str);
        printf("  BFO frequency: %.0f Hz\n", args->bfo_frequency);
        printf("  LPF cutoff: %.0f Hz\n", args->lpf_cutoff);
    }

    ssb_demod_t ssb;
    if (!ssb_demod_init_custom(&ssb, iq_data.sample_rate, ssb_mode,
                              args->bfo_frequency, args->lpf_cutoff)) {
        fprintf(stderr, "Error: Failed to initialize SSB demodulator\n");
        iq_free(&iq_data);
        return EXIT_FAILURE;
    }

    // Initialize AGC if requested
    agc_t agc;
    if (args->enable_agc) {
        if (args->verbose) {
            printf("Initializing AGC...\n");
            printf("  Target: %.1f dBFS\n", args->agc_target_dbfs);
            printf("  Max gain: %.0f dB\n", args->agc_max_gain_db);
        }

        // Convert dBFS to linear reference level
        float reference_level = powf(10.0f, args->agc_target_dbfs / 20.0f);

        if (!agc_init_custom(&agc, args->audio_rate, 0.01f, 0.1f,
                           reference_level, args->agc_max_gain_db, 0.5f)) {
            fprintf(stderr, "Error: Failed to initialize AGC\n");
            iq_free(&iq_data);
            return EXIT_FAILURE;
        }
    }

    // Initialize resampler if needed
    resample_t resampler;
    bool needs_resampling = fabsf((float)iq_data.sample_rate - args->audio_rate) > 1.0f;
    if (needs_resampling) {
        if (args->verbose) {
            printf("Initializing resampler: %.0f Hz -> %.0f Hz\n",
                   (double)iq_data.sample_rate, (double)args->audio_rate);
        }

        if (!resample_init(&resampler, iq_data.sample_rate, args->audio_rate)) {
            fprintf(stderr, "Error: Failed to initialize resampler\n");
            iq_free(&iq_data);
            return EXIT_FAILURE;
        }
    }

    // Initialize WAV writer
    if (args->verbose) printf("Initializing WAV writer...\n");

    wave_writer_t wav_writer;
    if (!wave_writer_init(&wav_writer, args->output_file, args->audio_rate, 1)) {  // Mono
        fprintf(stderr, "Error: Failed to initialize WAV writer\n");
        iq_free(&iq_data);
        if (needs_resampling) resample_free(&resampler);
        return EXIT_FAILURE;
    }

    // Process IQ data in blocks
    float *audio_buffer = malloc(BLOCK_SIZE * sizeof(float));
    if (!audio_buffer) {
        fprintf(stderr, "Error: Failed to allocate audio buffer\n");
        wave_writer_close(&wav_writer);
        iq_free(&iq_data);
        if (needs_resampling) resample_free(&resampler);
        return EXIT_FAILURE;
    }

    size_t total_samples_processed = 0;
    size_t total_audio_samples = 0;

    for (size_t offset = 0; offset < iq_data.num_samples; offset += BLOCK_SIZE) {
        size_t block_size = (offset + BLOCK_SIZE > iq_data.num_samples) ?
                           iq_data.num_samples - offset : BLOCK_SIZE;

        // Demodulate this block
        for (size_t i = 0; i < block_size; i++) {
            size_t idx = (offset + i) * 2;  // Interleaved: I,Q,I,Q,...
            float i_sample = iq_data.data[idx];
            float q_sample = iq_data.data[idx + 1];
            audio_buffer[i] = ssb_demod_process_sample(&ssb, i_sample, q_sample);
        }

        // Apply AGC if enabled
        if (args->enable_agc) {
            agc_process_buffer(&agc, audio_buffer, block_size);
        }

        // Resample if needed
        if (needs_resampling) {
            // Resample the block
            uint32_t output_samples = 0;
            float *resampled = malloc(block_size * 2 * sizeof(float));  // Allocate enough space
            if (resampled) {
                if (resample_process_buffer(&resampler, audio_buffer, block_size,
                                          resampled, block_size * 2, &output_samples)) {
                    // Convert float to int16 for WAV output
                    int16_t *wav_samples = malloc(output_samples * sizeof(int16_t));
                    if (wav_samples) {
                        for (uint32_t j = 0; j < output_samples; j++) {
                            // Clamp to [-1, 1] and convert to int16
                            float sample = resampled[j];
                            if (sample > 1.0f) sample = 1.0f;
                            if (sample < -1.0f) sample = -1.0f;
                            wav_samples[j] = (int16_t)(sample * 32767.0f);
                        }
                        wave_writer_write_samples(&wav_writer, wav_samples, output_samples);
                        total_audio_samples += output_samples;
                        free(wav_samples);
                    }
                }
                free(resampled);
            }
        } else {
            // Convert float to int16 for WAV output
            int16_t *wav_samples = malloc(block_size * sizeof(int16_t));
            if (wav_samples) {
                for (size_t i = 0; i < block_size; i++) {
                    // Clamp to [-1, 1] and convert to int16
                    float sample = audio_buffer[i];
                    if (sample > 1.0f) sample = 1.0f;
                    if (sample < -1.0f) sample = -1.0f;
                    wav_samples[i] = (int16_t)(sample * 32767.0f);
                }
                wave_writer_write_samples(&wav_writer, wav_samples, block_size);
                total_audio_samples += block_size;
                free(wav_samples);
            }
        }

        total_samples_processed += block_size;

        if (args->verbose && total_samples_processed % (BLOCK_SIZE * 10) == 0) {
            printf("Processed %zu/%zu samples (%.1f%%)\n",
                   total_samples_processed, iq_data.num_samples,
                   100.0f * total_samples_processed / iq_data.num_samples);
        }
    }

    // Clean up
    free(audio_buffer);
    wave_writer_close(&wav_writer);
    iq_free(&iq_data);
    if (needs_resampling) resample_free(&resampler);

    if (args->verbose) {
        printf("SSB demodulation completed successfully\n");
        printf("  Input samples: %zu\n", total_samples_processed);
        printf("  Output audio samples: %zu\n", total_audio_samples);
        printf("  Output file: %s\n", args->output_file);
    }

    return EXIT_SUCCESS;
}

int main(int argc, char *argv[]) {
    args_t args;

    if (!parse_args(argc, argv, &args)) {
        fprintf(stderr, "Error parsing command line arguments\n");
        print_usage(argv[0]);
        return EXIT_FAILURE;
    }

    if (args.verbose) {
        printf("IQ Lab SSB Demodulator\n");
        printf("=====================\n");
    }

    return process_ssb_demodulation(&args);
}
