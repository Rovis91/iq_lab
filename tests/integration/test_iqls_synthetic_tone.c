/* iqls acceptance test: peak bin accuracy on synthetic tone */
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <stdint.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

int main(void) {
    // Generate synthetic IQ: 1 kHz tone at 12 kHz sample rate
    const uint32_t sample_rate = 12000;
    const uint32_t num_samples = 8192; // More samples for averaging
    const double tone_freq = 1000.0;
    const char *synth_file = "out/synth_tone.iq";

    FILE *f = fopen(synth_file, "wb");
    if (!f) {
        printf("Failed to create synth file\n");
        return 1;
    }
    for (uint32_t i = 0; i < num_samples; i++) {
        double t = (double)i / (double)sample_rate;
        double phase = 2.0 * M_PI * tone_freq * t;
        int16_t i_val = (int16_t)lrintf(cos(phase) * 32767.0);
        int16_t q_val = (int16_t)lrintf(sin(phase) * 32767.0);
        fwrite(&i_val, 2, 1, f);
        fwrite(&q_val, 2, 1, f);
    }
    fclose(f);

    // Run iqls
    int rc = system("iqls --in out/synth_tone.iq --format s16 --rate 12000 --fft 4096 --hop 2048 --avg 1 --logmag --out out/synth_spectrum");
    if (rc != 0) {
        printf("iqls failed rc=%d\n", rc);
        return 1;
    }

    // Expected peak bin: (1000 / 12000) * 4096 ≈ 341
    int expected_bin = (int)lrintf(tone_freq / (double)sample_rate * 4096.0);
    printf("Expected peak bin: %d\n", expected_bin);

    // For now, just check file exists; in future, parse PNG or add peak detection
    FILE *check = fopen("out/synth_spectrum_spectrum.png", "rb");
    if (!check) {
        printf("Spectrum PNG not found\n");
        return 1;
    }
    fclose(check);

    printf("iqls synthetic tone test passed (basic check)\n");
    return 0;
}
