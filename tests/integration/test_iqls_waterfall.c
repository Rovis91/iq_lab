/* Integration test for iqls waterfall */
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int rc = system("iqls --in data/kiwi.wav --format s16 --rate 12000 --fft 512 --hop 128 --avg 4 --logmag --waterfall --out out/iqls_wf");
    if (rc != 0) {
        printf("iqls command failed with rc=%d\n", rc);
        return 1;
    }
    FILE *f = fopen("out/iqls_wf_waterfall.png", "rb");
    if (!f) {
        printf("Waterfall image not found\n");
        return 1;
    }
    fclose(f);
    printf("iqls waterfall integration test passed\n");
    return 0;
}


