/*
 * IQ Lab - Cluster Unit Tests
 *
 * Tests for the temporal clustering and hysteresis module.
 * These tests validate the clustering algorithm, event formation,
 * and parameter handling to ensure robust signal event detection.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <assert.h>
#include "../../src/detect/cluster.h"

// Test data generation helpers
static cfar_detection_t *create_test_detection(uint32_t bin_index, double snr_db,
                                             double signal_power_db, uint32_t frame_idx) {
    (void)frame_idx; // Suppress unused parameter warning

    cfar_detection_t *detection = malloc(sizeof(cfar_detection_t));
    if (!detection) return NULL;

    detection->bin_index = bin_index;
    detection->snr_estimate = snr_db;
    detection->signal_power = signal_power_db;
    detection->threshold = signal_power_db - snr_db; // Calculate threshold
    detection->confidence = 0.8; // Default confidence

    return detection;
}

// Test functions
static void test_cluster_initialization(void) {
    printf("Testing cluster initialization...\n");

    cluster_t cluster;
    bool result;

    // Test valid initialization
    result = cluster_init(&cluster, 50.0, 10000.0, 100, 2000000.0);
    assert(result == true);
    assert(cluster.initialized == true);
    assert(cluster.max_time_gap_s == 0.05); // 50ms converted to seconds
    assert(cluster.max_freq_gap_hz == 10000.0);
    assert(cluster.max_clusters == 100);
    assert(cluster.sample_rate_hz == 2000000.0);
    cluster_free(&cluster);

    // Test invalid parameters
    result = cluster_init(&cluster, 0.0, 10000.0, 100, 2000000.0); // Invalid time gap
    assert(result == false);

    result = cluster_init(&cluster, 50.0, 0.0, 100, 2000000.0); // Invalid freq gap
    assert(result == false);

    result = cluster_init(&cluster, 50.0, 10000.0, 0, 2000000.0); // Invalid max clusters
    assert(result == false);

    printf("✓ Cluster initialization tests passed\n");
}

static void test_cluster_parameter_validation(void) {
    printf("Testing cluster parameter validation...\n");

    // Test valid parameters
    bool result = cluster_validate_params(50.0, 10000.0, 100, 2000000.0);
    assert(result == true);

    // Test invalid parameters
    result = cluster_validate_params(0.0, 10000.0, 100, 2000000.0);
    assert(result == false);

    result = cluster_validate_params(50.0, 0.0, 100, 2000000.0);
    assert(result == false);

    result = cluster_validate_params(50.0, 10000.0, 0, 2000000.0);
    assert(result == false);

    result = cluster_validate_params(50.0, 10000.0, 100, 0.0);
    assert(result == false);

    printf("✓ Cluster parameter validation tests passed\n");
}

static void test_cluster_single_detection(void) {
    printf("Testing cluster single detection...\n");

    cluster_t cluster;
    bool result = cluster_init(&cluster, 50.0, 10000.0, 10, 2000000.0);
    assert(result == true);

    // Add a single detection
    cfar_detection_t *detection = create_test_detection(512, 15.0, -30.0, 0);
    assert(detection != NULL);

    result = cluster_add_detection(&cluster, detection, 1.0);
    assert(result == true);
    assert(cluster_get_active_count(&cluster) == 1);

    free(detection);
    cluster_free(&cluster);

    printf("✓ Cluster single detection test passed\n");
}

static void test_cluster_multiple_detections(void) {
    printf("Testing cluster multiple detections...\n");

    cluster_t cluster;
    bool result = cluster_init(&cluster, 50.0, 10000.0, 10, 2000000.0);
    assert(result == true);

    // Add multiple detections to the same cluster
    for (int i = 0; i < 5; i++) {
        cfar_detection_t *detection = create_test_detection(512, 15.0, -30.0, i);
        assert(detection != NULL);

        result = cluster_add_detection(&cluster, detection, (double)i * 0.01); // 10ms apart
        assert(result == true);

        free(detection);
    }

    assert(cluster_get_active_count(&cluster) == 1);

    cluster_free(&cluster);

    printf("✓ Cluster multiple detections test passed\n");
}

static void test_cluster_event_extraction(void) {
    printf("Testing cluster event extraction...\n");

    cluster_t cluster;
    bool result = cluster_init(&cluster, 50.0, 10000.0, 10, 2000000.0);
    assert(result == true);

    // Add detections to form a cluster
    for (int i = 0; i < 5; i++) {
        cfar_detection_t *detection = create_test_detection(512, 15.0, -30.0, i);
        assert(detection != NULL);

        result = cluster_add_detection(&cluster, detection, (double)i * 0.01);
        assert(result == true);

        free(detection);
    }

    // Extract events (should get one event after timeout)
    cluster_event_t events[5];
    uint32_t num_events = cluster_get_events(&cluster, events, 5, 1.0); // 1 second later
    assert(num_events == 1);

    // Verify event properties
    cluster_event_t *event = &events[0];
    assert(event->num_detections == 5);
    assert(event->start_time_s == 0.0);
    assert(event->end_time_s == 0.04);
    assert(event->duration_s == 0.04);
    assert(event->peak_snr_db == 15.0);

    cluster_free(&cluster);

    printf("✓ Cluster event extraction test passed\n");
}

static void test_cluster_frequency_separation(void) {
    printf("Testing cluster frequency separation...\n");

    cluster_t cluster;
    bool result = cluster_init(&cluster, 50.0, 100.0, 10, 2000000.0); // Very small freq gap (100Hz)
    assert(result == true);

    // Add detections at very different frequencies (bins far apart)
    cfar_detection_t *detection1 = create_test_detection(100, 15.0, -30.0, 0);   // Bin 100
    cfar_detection_t *detection2 = create_test_detection(900, 15.0, -30.0, 0);   // Bin 900 (far apart)

    result = cluster_add_detection(&cluster, detection1, 0.0);
    assert(result == true);
    printf("  Added detection1 (bin 100), active clusters: %u\n", cluster_get_active_count(&cluster));

    result = cluster_add_detection(&cluster, detection2, 0.01);
    assert(result == true);
    printf("  Added detection2 (bin 900), active clusters: %u\n", cluster_get_active_count(&cluster));

    uint32_t active_count = cluster_get_active_count(&cluster);
    printf("  Final active clusters: %u (expected: 2)\n", active_count);

    // For debugging, let's check if they should be separate
    if (active_count != 2) {
        printf("  Debug: max_freq_gap_hz = %.1f\n", cluster.max_freq_gap_hz);
        printf("  Debug: detection1 bin = %u, detection2 bin = %u\n", detection1->bin_index, detection2->bin_index);
    }

    // Verify that detections are in separate clusters
    assert(cluster_get_active_count(&cluster) == 2);

    free(detection1);
    free(detection2);
    cluster_free(&cluster);

    printf("✓ Cluster frequency separation test completed\n");
}

static void test_cluster_config_string(void) {
    printf("Testing cluster configuration string...\n");

    cluster_t cluster;
    bool result = cluster_init(&cluster, 100.0, 15000.0, 50, 1000000.0);
    assert(result == true);

    char buffer[256];
    result = cluster_get_config_string(&cluster, buffer, sizeof(buffer));
    assert(result == true);

    // Check that configuration string contains expected parameters
    assert(strstr(buffer, "MaxTimeGap=") != NULL);
    assert(strstr(buffer, "MaxFreqGap=15000") != NULL);
    assert(strstr(buffer, "MaxClusters=50") != NULL);
    assert(strstr(buffer, "SampleRate=1000000") != NULL);

    cluster_free(&cluster);

    printf("✓ Cluster configuration string test passed\n");
}

static void test_cluster_reset(void) {
    printf("Testing cluster reset functionality...\n");

    cluster_t cluster;
    bool result = cluster_init(&cluster, 50.0, 10000.0, 10, 2000000.0);
    assert(result == true);

    // Add some detections
    cfar_detection_t *detection = create_test_detection(512, 15.0, -30.0, 0);
    result = cluster_add_detection(&cluster, detection, 0.0);
    assert(result == true);
    free(detection);

    assert(cluster_get_active_count(&cluster) == 1);

    // Reset cluster
    cluster_reset(&cluster);
    assert(cluster_get_active_count(&cluster) == 0);

    // Verify cluster is still functional after reset
    detection = create_test_detection(256, 10.0, -35.0, 0);
    result = cluster_add_detection(&cluster, detection, 0.1);
    assert(result == true);
    assert(cluster_get_active_count(&cluster) == 1);

    free(detection);
    cluster_free(&cluster);

    printf("✓ Cluster reset test passed\n");
}

// Main test runner
int main(int argc, char **argv) {
    (void)argc; // Suppress unused parameter warning
    (void)argv; // Suppress unused parameter warning

    printf("Running Cluster Unit Tests\n");
    printf("==========================\n\n");

    test_cluster_initialization();
    test_cluster_parameter_validation();
    test_cluster_single_detection();
    test_cluster_multiple_detections();
    test_cluster_event_extraction();
    test_cluster_frequency_separation();
    test_cluster_config_string();
    test_cluster_reset();

    printf("\n==========================\n");
    printf("All cluster tests passed! ✓\n");
    printf("==========================\n");

    return 0;
}
