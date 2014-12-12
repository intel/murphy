#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>

#include <murphy/common/mask.h>

int main(int argc, char *argv[])
{
    uint64_t   bits;
    int        i, j, prev, set, n, cnt, clr, bit;
    mrp_mask_t m = MRP_MASK_EMPTY, m1;
    int        b[] = { 0, 1, 5, 16, 32, 48, 97, 112, 113, 114, 295, 313, -1 };

    cnt = argc > 1 ? strtoul(argv[1], NULL, 10) : 100;

    srand((unsigned int)time(NULL) ^ (unsigned int)getpid());

    bits = 0x17;
    bits <<= 35;
    n = mrp_ffsll(bits);
    printf("ffsl(0x%lx) = %d\n", bits, n);

    for (i = 0; i < cnt; i++) {
        bits = (unsigned long)rand();
        n    = mrp_ffsll(bits);
        clr  = ~((((unsigned long)-1) >> (n - 1)) << (n - 1));

        if (n > 1) {
            if ((bits & clr) != 0) {
            fail:
                printf("ffs(0x%lx) = %d: FAIL\n", bits, n);
                exit(1);
            }
            else
                printf("ffs(0x%lx) = %d: OK\n", bits, n);
        }

        if (n != __builtin_ffsl(bits))
            goto fail;

    }

    for (i = 0; b[i] != -1; i++) {
        printf("setting bit %d...\n", b[i]);
        mrp_mask_set(&m, b[i]);
        if (!mrp_mask_test(&m, b[i])) {
            printf("testing bit %d: FAILED\n", b[i]);
            exit(1);
        }
    }


    prev = 0;
    for (i = 0; b[i] != -1; i++) {
        for (j = prev + 1; j < b[i]; j++) {
            set = mrp_mask_test(&m, j);
            if (set) {
                printf("negative mask_test(%d): FAILED\n", j);
                exit(1);
            }
        }

        set = mrp_mask_test(&m, b[i]);

        if (!set) {
            printf("mask_test(%d): FAILED\n", b[i]);
            exit(1);
        }

        prev = b[i];
    }

    printf("mask tests: OK\n");

    MRP_MASK_FOREACH_SET(&m, bit, 0) {
        printf("next bit set: %d\n", bit);
    }

    MRP_MASK_FOREACH_CLEAR(&m, bit, 150) {
        printf("next bit clear: %d\n", bit);
    }

    mrp_mask_neg(&m);
    MRP_MASK_FOREACH_CLEAR(&m, bit, 150) {
        printf("next bit clear: %d\n", bit);
    }

    MRP_MASK_FOREACH_CLEAR(&m, bit, 0) {
        printf("next bit clear (negated): %d\n", bit);
    }

    mrp_mask_neg(&m);

    mrp_mask_copy(&m1, &m);
    mrp_mask_neg(&m1);
    mrp_mask_or(&m1, &m);

    MRP_MASK_FOREACH_SET(&m1, bit, 0) {
        printf("next bit set (or'd): %d\n", bit);
    }

    mrp_mask_copy(&m1, &m);
    mrp_mask_neg(&m1);
    mrp_mask_xor(&m1, &m);

    MRP_MASK_FOREACH_SET(&m1, bit, 0) {
        printf("next bit set (neg'd+xor'd): %d\n", bit);
    }

    mrp_mask_copy(&m1, &m);
    mrp_mask_neg(&m1);
    mrp_mask_and(&m1, &m);

    MRP_MASK_FOREACH_SET(&m1, bit, 0) {
        printf("next bit set (neg'd+and'd): %d\n", bit);
    }

    mrp_mask_copy(&m1, &m);
    mrp_mask_and(&m1, &m);

    MRP_MASK_FOREACH_SET(&m1, bit, 0) {
        printf("next bit set (and'd): %d\n", bit);
    }

    mrp_mask_reset(&m);

    return 0;
}
