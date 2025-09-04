/* Integration test for iqls spectrum-only */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {
    int rc = system("iqls --in data/kiwi.wav --format s16 --rate 12000 --fft 1024 --hop 256 --avg 8 --logmag --out out/iqls_test");
    if (rc != 0) {
        printf("iqls command failed with rc=%d\n", rc);
        return 1;
    }
    FILE *f = fopen("out/iqls_test_spectrum.png", "rb");
    if (!f) {
        printf("Output spectrum not found\n");
        return 1;
    }
    fclose(f);
    printf("iqls spectrum integration test passed\n");
    return 0;
}


