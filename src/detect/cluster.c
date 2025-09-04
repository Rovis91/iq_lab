/*
 * IQ Lab - Detection Clustering Implementation
 *
 * This file implements temporal clustering algorithms for grouping individual
 * signal detections into coherent events. The implementation uses hysteresis
 * to prevent cluster fragmentation and provides configurable temporal and
 * frequency gap tolerances for robust event formation.
 *
 * Algorithm Details:
 * 1. Active Clusters: Maintain a list of ongoing signal events
 * 2. Detection Addition: Add new detections to existing or create new clusters
 * 3. Cluster Merging: Merge clusters that become spatially/temporally adjacent
 * 4. Gap Detection: Close clusters after configurable time gaps
 * 5. Event Extraction: Generate completed events with aggregated features
 *
 * Clustering Strategy:
 * - Temporal Hysteresis: Extend clusters within time gap tolerance
 * - Frequency Merging: Merge clusters within frequency gap tolerance
 * - Feature Aggregation: Track SNR, bandwidth, and power statistics
 * - Gap Closure: Complete events after configurable inactivity periods
 *
 * Performance Considerations:
 * - O(N) complexity per detection (N = active clusters)
 * - Memory usage scales with max_clusters parameter
 * - Suitable for real-time processing with moderate cluster counts
 *
 * Edge Cases Handled:
 * - Empty detection streams
 * - Maximum cluster limits
 * - Boundary conditions in time/frequency domains
 * - Memory allocation failures
 * - Parameter validation and error recovery
 */

#include "cluster.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>

// Internal helper functions

/*
 * Check if two clusters should be merged based on temporal and frequency proximity
 */
static bool clusters_should_merge(const active_cluster_t *c1, const active_cluster_t *c2,
                                  double max_time_gap_s, double max_freq_gap_hz,
                                  double sample_rate_hz) {
    (void)sample_rate_hz; // Currently not used but kept for future frequency calculations

    // Check temporal overlap/gap
    double time_gap = fabs(c1->last_update_s - c2->last_update_s);
    if (time_gap > max_time_gap_s) {
        return false; // Too much time gap
    }

    // Check frequency proximity using bin indices
    double c1_center_bin = c1->center_freq_sum / c1->num_detections;
    double c2_center_bin = c2->center_freq_sum / c2->num_detections;
    double freq_gap_bins = fabs(c1_center_bin - c2_center_bin);

    // Convert to approximate frequency gap
    double approx_fft_size = sample_rate_hz; // Rough estimate
    double freq_gap = freq_gap_bins * (sample_rate_hz / approx_fft_size);

    if (freq_gap > max_freq_gap_hz) {
        return false; // Too much frequency gap
    }

    return true;
}

/*
 * Merge two active clusters into the first one
 */
static void merge_clusters(active_cluster_t *dest, const active_cluster_t *src) {
    // Update temporal bounds
    if (src->start_time_s < dest->start_time_s) {
        dest->start_time_s = src->start_time_s;
    }
    if (src->last_update_s > dest->last_update_s) {
        dest->last_update_s = src->last_update_s;
    }
    dest->frame_count += src->frame_count;

    // Update frequency bounds
    if (src->min_bin < dest->min_bin) dest->min_bin = src->min_bin;
    if (src->max_bin > dest->max_bin) dest->max_bin = src->max_bin;

    // Aggregate center frequency and bandwidth
    dest->center_freq_sum += src->center_freq_sum;
    dest->bandwidth_sum += src->bandwidth_sum;

    // Update signal quality metrics
    if (src->peak_snr_db > dest->peak_snr_db) {
        dest->peak_snr_db = src->peak_snr_db;
    }
    if (src->peak_power_dbfs > dest->peak_power_dbfs) {
        dest->peak_power_dbfs = src->peak_power_dbfs;
    }

    dest->snr_sum += src->snr_sum;
    dest->num_detections += src->num_detections;
}

/*
 * Convert cluster to completed event
 */
static void cluster_to_event(const active_cluster_t *cluster, cluster_event_t *event,
                            double sample_rate_hz, uint32_t fft_size) {

    // Temporal information
    event->start_time_s = cluster->start_time_s;
    event->end_time_s = cluster->last_update_s;
    event->duration_s = event->end_time_s - event->start_time_s;

    // Frequency bounds
    event->min_bin = cluster->min_bin;
    event->max_bin = cluster->max_bin;

    // Calculate center frequency and bandwidth
    double center_freq_norm = cluster->center_freq_sum / cluster->num_detections;
    event->center_freq_hz = (center_freq_norm / fft_size - 0.5) * sample_rate_hz;
    event->bandwidth_hz = cluster->bandwidth_sum / cluster->num_detections;

    // Signal quality metrics
    event->peak_snr_db = cluster->peak_snr_db;
    event->avg_snr_db = cluster->snr_sum / cluster->num_detections;
    event->peak_power_dbfs = cluster->peak_power_dbfs;

    // Detection statistics
    event->num_detections = cluster->num_detections;

    // Calculate confidence based on SNR and duration
    double snr_factor = fmin(event->avg_snr_db / 20.0, 1.0); // Cap at 20dB SNR
    double duration_factor = fmin(event->duration_s / 1.0, 1.0); // Cap at 1 second
    event->confidence = sqrt(snr_factor * duration_factor); // Geometric mean

    // Basic modulation classification hints
    if (event->bandwidth_hz < 5000) {
        event->modulation_guess = "narrowband";
        event->modulation_confidence = 0.7;
    } else if (event->bandwidth_hz < 20000) {
        event->modulation_guess = "wideband";
        event->modulation_confidence = 0.6;
    } else {
        event->modulation_guess = "unknown";
        event->modulation_confidence = 0.3;
    }
}

/*
 * Find the best cluster to add a detection to, or return -1 if none suitable
 */
static int32_t find_best_cluster(const cluster_t *cluster, const cfar_detection_t *detection,
                                double frame_time) {

    int32_t best_cluster_idx = -1;
    double best_score = -1.0;

    for (uint32_t i = 0; i < cluster->num_clusters; i++) {
        const active_cluster_t *c = &cluster->clusters[i];

        // Check temporal proximity
        double time_gap = frame_time - c->last_update_s;
        if (time_gap > cluster->max_time_gap_s) {
            continue; // Too old
        }

        // Check frequency proximity using bin indices
        double cluster_center_bin = c->center_freq_sum / c->num_detections;
        double freq_gap_bins = fabs((double)detection->bin_index - cluster_center_bin);

        // Convert bin gap to approximate frequency gap (assuming FFT size ~ sample_rate)
        // This is a rough approximation for the frequency gap check
        double approx_fft_size = cluster->sample_rate_hz; // Rough estimate
        double freq_gap_hz = freq_gap_bins * (cluster->sample_rate_hz / approx_fft_size);

        if (freq_gap_hz > cluster->max_freq_gap_hz) {
            continue; // Too far in frequency
        }

        // Calculate score (prefer closer in time and frequency)
        double time_score = 1.0 / (1.0 + time_gap);
        double freq_score = 1.0 / (1.0 + freq_gap_hz / 1000.0); // Normalize to kHz
        double score = time_score * freq_score;

        if (score > best_score) {
            best_score = score;
            best_cluster_idx = (int32_t)i;
        }
    }

    return best_cluster_idx;
}

// Public API implementation

bool cluster_init(cluster_t *cluster, double max_time_gap_ms, double max_freq_gap_hz,
                  uint32_t max_clusters, double sample_rate_hz) {

    if (!cluster || max_clusters == 0) {
        return false;
    }

    // Validate parameters
    if (!cluster_validate_params(max_time_gap_ms, max_freq_gap_hz, max_clusters, sample_rate_hz)) {
        return false;
    }

    // Initialize basic parameters
    cluster->max_time_gap_s = max_time_gap_ms / 1000.0; // Convert to seconds
    cluster->max_freq_gap_hz = max_freq_gap_hz;
    cluster->max_clusters = max_clusters;
    cluster->sample_rate_hz = sample_rate_hz;

    // Allocate cluster array
    cluster->clusters = calloc(max_clusters, sizeof(active_cluster_t));
    if (!cluster->clusters) {
        return false;
    }

    // Initialize state
    cluster->num_clusters = 0;
    cluster->next_cluster_id = 0;
    cluster->initialized = true;

    return true;
}

bool cluster_add_detection(cluster_t *cluster, const cfar_detection_t *detection, double frame_time) {

    if (!cluster || !cluster->initialized || !detection) {
        return false;
    }

    // Check if we have room for a new cluster
    if (cluster->num_clusters >= cluster->max_clusters) {
        // Try to find a cluster to merge with
        int32_t best_idx = find_best_cluster(cluster, detection, frame_time);
        if (best_idx >= 0) {
            // Update existing cluster
            active_cluster_t *c = &cluster->clusters[best_idx];
            c->last_update_s = frame_time;
            c->frame_count++;

            // Update frequency bounds
            if (detection->bin_index < c->min_bin) c->min_bin = detection->bin_index;
            if (detection->bin_index > c->max_bin) c->max_bin = detection->bin_index;

            // Aggregate center frequency and bandwidth
            double detection_freq_hz = (detection->bin_index / (double)cluster->sample_rate_hz - 0.5) * cluster->sample_rate_hz;
            c->center_freq_sum += detection_freq_hz;
            c->bandwidth_sum += 1000.0; // Rough bandwidth estimate

            // Update signal quality
            if (detection->snr_estimate > c->peak_snr_db) {
                c->peak_snr_db = detection->snr_estimate;
            }
            if (detection->signal_power > c->peak_power_dbfs) {
                c->peak_power_dbfs = detection->signal_power;
            }

            c->snr_sum += detection->snr_estimate;
            c->num_detections++;

            return true;
        } else {
            // No suitable cluster found and we're at capacity
            return false;
        }
    }

    // Find best existing cluster to add to
    int32_t best_idx = find_best_cluster(cluster, detection, frame_time);
    if (best_idx >= 0) {
        // Update existing cluster
        active_cluster_t *c = &cluster->clusters[best_idx];
        c->last_update_s = frame_time;
        c->frame_count++;

        // Update frequency bounds
        if (detection->bin_index < c->min_bin) c->min_bin = detection->bin_index;
        if (detection->bin_index > c->max_bin) c->max_bin = detection->bin_index;

        // Aggregate center frequency and bandwidth
        // Store bin index directly for frequency comparison
        c->center_freq_sum += (double)detection->bin_index;
        c->bandwidth_sum += 1000.0; // Rough bandwidth estimate

        // Update signal quality
        if (detection->snr_estimate > c->peak_snr_db) {
            c->peak_snr_db = detection->snr_estimate;
        }
        if (detection->signal_power > c->peak_power_dbfs) {
            c->peak_power_dbfs = detection->signal_power;
        }

        c->snr_sum += detection->snr_estimate;
        c->num_detections++;

    } else {
        // Create new cluster
        active_cluster_t *c = &cluster->clusters[cluster->num_clusters];

        c->start_time_s = frame_time;
        c->last_update_s = frame_time;
        c->frame_count = 1;

        c->min_bin = detection->bin_index;
        c->max_bin = detection->bin_index;

        // Store bin index directly for frequency comparison
        c->center_freq_sum = (double)detection->bin_index;
        c->bandwidth_sum = 1000.0; // Rough bandwidth estimate

        c->peak_snr_db = detection->snr_estimate;
        c->snr_sum = detection->snr_estimate;
        c->peak_power_dbfs = detection->signal_power;
        c->num_detections = 1;

        cluster->num_clusters++;
    }

    // Check for cluster merging after adding the detection
    // This is a simple O(N²) merge - could be optimized for large cluster counts
    for (uint32_t i = 0; i < cluster->num_clusters; i++) {
        for (uint32_t j = i + 1; j < cluster->num_clusters; j++) {
            if (clusters_should_merge(&cluster->clusters[i], &cluster->clusters[j],
                                    cluster->max_time_gap_s, cluster->max_freq_gap_hz,
                                    cluster->sample_rate_hz)) {
                // Merge j into i
                merge_clusters(&cluster->clusters[i], &cluster->clusters[j]);

                // Remove cluster j by shifting remaining clusters
                for (uint32_t k = j; k < cluster->num_clusters - 1; k++) {
                    cluster->clusters[k] = cluster->clusters[k + 1];
                }
                cluster->num_clusters--;
                j--; // Adjust index since we removed an element
            }
        }
    }

    return true;
}

uint32_t cluster_get_events(cluster_t *cluster, cluster_event_t *events, uint32_t max_events, double current_time) {

    if (!cluster || !cluster->initialized || !events || max_events == 0) {
        return 0;
    }

    uint32_t num_events = 0;

    // Check each cluster for completion
    for (uint32_t i = 0; i < cluster->num_clusters && num_events < max_events; ) {
        active_cluster_t *c = &cluster->clusters[i];

        // Check if cluster should be completed (gap timeout)
        double time_gap = current_time - c->last_update_s;
        if (time_gap > cluster->max_time_gap_s && c->num_detections >= 3) {
            // Convert cluster to event
            cluster_to_event(c, &events[num_events], cluster->sample_rate_hz, (uint32_t)(cluster->sample_rate_hz * 2)); // Rough FFT size estimate
            num_events++;

            // Remove completed cluster by shifting remaining clusters
            for (uint32_t j = i; j < cluster->num_clusters - 1; j++) {
                cluster->clusters[j] = cluster->clusters[j + 1];
            }
            cluster->num_clusters--;
            // Don't increment i since we removed an element
        } else {
            i++; // Move to next cluster
        }
    }

    return num_events;
}

void cluster_reset(cluster_t *cluster) {
    if (!cluster) return;

    if (cluster->clusters) {
        memset(cluster->clusters, 0, cluster->max_clusters * sizeof(active_cluster_t));
    }

    cluster->num_clusters = 0;
    cluster->next_cluster_id = 0;
}

void cluster_free(cluster_t *cluster) {
    if (!cluster) return;

    if (cluster->clusters) {
        free(cluster->clusters);
        cluster->clusters = NULL;
    }

    memset(cluster, 0, sizeof(cluster_t));
    cluster->initialized = false;
}

uint32_t cluster_get_active_count(const cluster_t *cluster) {
    return cluster ? cluster->num_clusters : 0;
}

bool cluster_get_config_string(const cluster_t *cluster, char *buffer, uint32_t buffer_size) {
    if (!cluster || !buffer || buffer_size == 0) {
        return false;
    }

    int written = snprintf(buffer, buffer_size,
                          "Clustering: MaxTimeGap=%.1fms, MaxFreqGap=%.0fHz, MaxClusters=%u, SampleRate=%.0fHz",
                          cluster->max_time_gap_s * 1000.0, cluster->max_freq_gap_hz,
                          cluster->max_clusters, cluster->sample_rate_hz);

    return (written > 0 && (uint32_t)written < buffer_size);
}

bool cluster_validate_params(double max_time_gap_ms, double max_freq_gap_hz,
                             uint32_t max_clusters, double sample_rate_hz) {

    // Basic parameter validation
    if (max_time_gap_ms <= 0.0 || max_time_gap_ms > 10000.0) { // 0-10 seconds
        return false;
    }

    if (max_freq_gap_hz <= 0.0 || max_freq_gap_hz > sample_rate_hz / 2.0) { // Max Nyquist
        return false;
    }

    if (max_clusters == 0 || max_clusters > 1000) { // Reasonable upper limit
        return false;
    }

    if (sample_rate_hz <= 0.0 || sample_rate_hz > 1e9) { // Reasonable sample rate range
        return false;
    }

    return true;
}
