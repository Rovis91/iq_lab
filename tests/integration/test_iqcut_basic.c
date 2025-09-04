/* Integration test for iqcut: translate+decimate+cut */
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    int rc = system("iqcut --in data/kiwi.wav --rate 12000 --f_center 0 --bw 3000 --t_start 0.1 --t_end 0.3 --out out/cut_test");
    if (rc != 0) {
        printf("iqcut failed rc=%d\n", rc);
        return 1;
    }
    FILE *f1 = fopen("out/cut_test.iq", "rb");
    FILE *f2 = fopen("out/cut_test.sigmf-meta", "rb");
    if (!f1 || !f2) {
        printf("iqcut outputs missing\n");
        return 1;
    }
    fclose(f1); fclose(f2);
    printf("iqcut basic integration test passed\n");
    return 0;
}


