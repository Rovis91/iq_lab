/*
 * IQ Lab - Detection Clustering Header
 *
 * Purpose: Temporal clustering and hysteresis for signal event formation
 * Author: IQ Lab Development Team
 *
 * This module provides temporal clustering algorithms to group individual
 * signal detections into coherent events. It implements hysteresis-based
 * clustering with configurable temporal and frequency gap tolerances.
 *
 * Key Features:
 * - Temporal hysteresis for stable event formation
 * - Frequency domain merging of overlapping detections
 * - Configurable gap tolerances for time and frequency
 * - Event consolidation and feature aggregation
 * - Memory-efficient tracking of active clusters
 *
 * Usage:
 *   cluster_t cluster;
 *   cluster_init(&cluster, max_time_gap_ms, max_freq_gap_hz);
 *   cluster_add_detection(&cluster, detection, frame_time);
 *   cluster_get_events(&cluster, events, max_events);
 *
 * Algorithm Overview:
 * 1. Maintain active signal clusters with temporal hysteresis
 * 2. Add new detections to existing clusters or create new ones
 * 3. Merge clusters that become spatially/temporally adjacent
 * 4. Close clusters after configurable gap periods
 * 5. Extract completed events with aggregated features
 *
 * Technical Details:
 * - Active Clusters: Dynamic list of ongoing signal events
 * - Gap Tolerance: Maximum time/frequency separation for merging
 * - Hysteresis: Prevents cluster fragmentation from brief gaps
 * - Feature Aggregation: SNR, bandwidth, center frequency averaging
 *
 * Performance:
 * - O(N) per detection addition (N = active clusters)
 * - Memory usage scales with number of active clusters
 * - Suitable for real-time processing with moderate cluster counts
 *
 * Applications:
 * - Signal event formation from raw detections
 * - Burst signal analysis and characterization
 * - Interference monitoring and classification
 * - Communications signal detection and tracking
 *
 * Dependencies: stdlib.h, stdbool.h, stdint.h, math.h
 * Thread Safety: Not thread-safe (single cluster instance per thread)
 * Performance: O(N) per detection, suitable for real-time SDR processing
 */

#ifndef IQ_LAB_CLUSTER_H
#define IQ_LAB_CLUSTER_H

#include <stdint.h>
#include <stdbool.h>
#include "cfar_os.h"  // Include for cfar_detection_t definition

// Event structure representing a completed signal cluster
typedef struct {
    // Temporal bounds
    double start_time_s;        // Start time of the event (seconds)
    double end_time_s;          // End time of the event (seconds)
    double duration_s;          // Total duration (end - start)

    // Frequency bounds
    uint32_t min_bin;           // Minimum FFT bin index
    uint32_t max_bin;           // Maximum FFT bin index
    double center_freq_hz;      // Center frequency (Hz)
    double bandwidth_hz;        // Bandwidth (Hz)

    // Signal quality metrics
    double peak_snr_db;         // Peak SNR across the event
    double avg_snr_db;          // Average SNR across the event
    double peak_power_dbfs;     // Peak power in dBFS

    // Detection statistics
    uint32_t num_detections;    // Number of detections in this event
    double confidence;          // Overall event confidence (0.0 to 1.0)

    // Modulation hints
    double modulation_confidence; // Confidence in modulation classification
    const char *modulation_guess;  // Modulation type hint
} cluster_event_t;

// Active cluster structure for tracking ongoing events
typedef struct {
    // Temporal tracking
    double start_time_s;        // First detection time
    double last_update_s;       // Last detection time
    uint32_t frame_count;       // Number of frames with detections

    // Frequency bounds
    uint32_t min_bin;           // Minimum bin across all detections
    uint32_t max_bin;           // Maximum bin across all detections
    double center_freq_sum;     // Sum of center frequencies (for averaging)
    double bandwidth_sum;       // Sum of bandwidths (for averaging)

    // Signal quality aggregation
    double peak_snr_db;         // Peak SNR in this cluster
    double snr_sum;             // Sum of SNR values (for averaging)
    double peak_power_dbfs;     // Peak power in dBFS

    // Detection count
    uint32_t num_detections;    // Total detections in this cluster
} active_cluster_t;

// Main clustering context
typedef struct {
    // Configuration parameters
    double max_time_gap_s;      // Maximum time gap for cluster extension (seconds)
    double max_freq_gap_hz;     // Maximum frequency gap for merging (Hz)
    uint32_t max_clusters;      // Maximum number of active clusters
    double sample_rate_hz;      // Sample rate for frequency calculations

    // Internal state
    active_cluster_t *clusters; // Array of active clusters
    uint32_t num_clusters;      // Current number of active clusters
    uint32_t next_cluster_id;   // Next available cluster ID

    // Working buffers
    bool initialized;           // Whether clustering is properly initialized
} cluster_t;

// Public API functions

/*
 * Initialize clustering context with specified parameters
 *
 * Parameters:
 *   cluster          - Pointer to cluster context to initialize
 *   max_time_gap_ms  - Maximum time gap for cluster extension (milliseconds)
 *   max_freq_gap_hz  - Maximum frequency gap for cluster merging (Hz)
 *   max_clusters     - Maximum number of active clusters to track
 *   sample_rate_hz   - Sample rate for frequency calculations
 *
 * Returns:
 *   true on success, false on parameter error or allocation failure
 */
bool cluster_init(cluster_t *cluster, double max_time_gap_ms, double max_freq_gap_hz,
                  uint32_t max_clusters, double sample_rate_hz);

/*
 * Add a detection to the clustering system
 *
 * Parameters:
 *   cluster     - Pointer to initialized cluster context
 *   detection   - CFAR detection to add
 *   frame_time  - Time of the detection frame (seconds)
 *
 * Returns:
 *   true on success, false on error or if cluster is full
 */
bool cluster_add_detection(cluster_t *cluster, const cfar_detection_t *detection, double frame_time);

/*
 * Extract completed events from the clustering system
 *
 * Parameters:
 *   cluster     - Pointer to cluster context
 *   events      - Output array to store completed events
 *   max_events  - Maximum number of events to return
 *   current_time - Current processing time (for gap detection)
 *
 * Returns:
 *   Number of completed events extracted (0 to max_events)
 */
uint32_t cluster_get_events(cluster_t *cluster, cluster_event_t *events, uint32_t max_events, double current_time);

/*
 * Reset clustering state (clear all active clusters)
 *
 * Parameters:
 *   cluster - Pointer to cluster context to reset
 */
void cluster_reset(cluster_t *cluster);

/*
 * Free clustering resources and reset to uninitialized state
 *
 * Parameters:
 *   cluster - Pointer to cluster context to free
 */
void cluster_free(cluster_t *cluster);

/*
 * Get current number of active clusters
 *
 * Parameters:
 *   cluster - Pointer to cluster context
 *
 * Returns:
 *   Number of currently active clusters
 */
uint32_t cluster_get_active_count(const cluster_t *cluster);

/*
 * Get clustering configuration as human-readable string
 *
 * Parameters:
 *   cluster     - Pointer to cluster context
 *   buffer      - Output buffer for configuration string
 *   buffer_size - Size of output buffer
 *
 * Returns:
 *   true on success, false if buffer too small
 */
bool cluster_get_config_string(const cluster_t *cluster, char *buffer, uint32_t buffer_size);

/*
 * Validate clustering parameters without creating cluster
 *
 * Parameters:
 *   max_time_gap_ms  - Maximum time gap for cluster extension
 *   max_freq_gap_hz  - Maximum frequency gap for cluster merging
 *   max_clusters     - Maximum number of active clusters
 *   sample_rate_hz   - Sample rate for frequency calculations
 *
 * Returns:
 *   true if parameters are valid, false otherwise
 */
bool cluster_validate_params(double max_time_gap_ms, double max_freq_gap_hz,
                             uint32_t max_clusters, double sample_rate_hz);

#endif /* IQ_LAB_CLUSTER_H */
