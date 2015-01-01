#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <sys/types.h>

#include <murphy/common/mask.h>
#include <murphy/common/debug.h>

enum {
    V_FATAL = 0,
    V_ERROR,
    V_PROGRESS,
    V_INFO
};

#define INFO(fmt, args...)  do {                                \
        if (test.verbosity >= V_INFO) {                         \
            printf("[%s] "fmt"\n" , __FUNCTION__ , ## args);    \
            fflush(stdout);                                     \
        }                                                       \
    } while (0)

#define PROGRESS(fmt, args...) do {                             \
        if (test.verbosity >= V_PROGRESS) {                     \
            printf("[%s] "fmt"\n" , __FUNCTION__ , ## args);    \
            fflush(stdout);                                     \
        }                                                       \
    } while (0)

#define ERROR(fmt, args...) do {                                        \
        if (test.verbosity >= V_ERROR) {                                \
            printf("[%s] error: "fmt"\n",                               \
                   __FUNCTION__, ## args);                              \
            fflush(stdout);                                             \
        }                                                               \
    } while (0)

#define FATAL(fmt, args...) do {                                        \
        printf("[%s] fatal error: "fmt"\n",                             \
               __FUNCTION__, ## args);                                  \
        fflush(stdout);                                                 \
        exit(1);                                                        \
    } while (0)


typedef struct {
    unsigned int seed;
    int          cnt;

    int          range;
    int          verbosity;
} test_t;


test_t test;


static void range_tests(void)
{
    mrp_mask_t m, c;
    int        i, offs, width, b;

    if (!test.range)
        return;

    mrp_mask_init(&m);
    mrp_mask_grow(&m, 1500);

    for (offs = 0; offs < 1500; offs++) {
        PROGRESS("range-set test %.2f %%", (100.0 * offs) / 1500);

        for (width = 1; width < 256; width++) {
            mrp_mask_reset(&m);
            mrp_mask_grow(&m, 1500);

            mrp_mask_set_range(&m, offs, offs + width);
            for (i = 0; i < 1500; i++) {
                b = mrp_mask_test(&m, i);

                if ((b && (i < offs && i > offs + width)) ||
                    (!b && (i >= offs && i <= offs + width && i <= 1500)))
                    FATAL("range set %d@%d [%d-%d] test for bit #%d: FAILED",
                          width, offs, offs, offs + width, i);
                else
                    INFO("range set %d@%d [%d-%d] test for bit #%d: OK",
                         width, offs, offs, offs + width, i);
            }
        }
    }

    for (offs = 0; offs < 1500; offs++) {
        PROGRESS("range-clear test %.2f %%", (100.0 * offs) / 1500);

        for (width = 1; width < 256; width++) {
            mrp_mask_reset(&m);
            mrp_mask_grow(&m, 1500);
            mrp_mask_copy(&c, &m);
            mrp_mask_not(&c, NULL);
            mrp_mask_or(&m, &c);

            mrp_mask_clear_range(&m, offs, offs + width);
            for (i = 0; i < 1500; i++) {
                b = mrp_mask_test(&m, i);

                if ((!b && (i < offs && i > offs + width)) ||
                    (b && (i >= offs && i <= offs + width && i <= 1500)))
                    FATAL("range clear %d@%d [%d-%d] test for bit #%d: FAILED",
                          width, offs, offs, offs + width, i);
                else
                    INFO("range clear %d@%d [%d-%d] test for bit #%d: OK",
                         width, offs, offs, offs + width, i);
            }
        }
    }

}


static void basic_tests(void)
{
    _mask_t    bits, clr;
    int        i, n, b;

    if (!test.seed)
        test.seed = (unsigned int)time(NULL) ^ (unsigned int)getpid();

    srand(test.seed);

    if (test.cnt <= 0)
        test.cnt = 256 + (rand() & 1023);

    for (i = 0; i < test.cnt; i++) {
        bits = ((_mask_t)rand() << (i & 0xf));
        n    = mrp_ffs(bits);
        clr  = MRP_MASK_BELOW(n);

        if ((bits & clr) != 0 || !(bits & MRP_MASK_BIT(n))) {
            FATAL("mrp_ffs(0x%lx): FAILED (n:%d, masked:0x%lx, bit:%d)",
                  (unsigned long)bits, n,
                  (unsigned long)bits & clr,
                  !!((unsigned long)bits & MRP_MASK_BIT(n)));
        }
        else {
            INFO("#%d/%d mrp_ffs(0x%lx) = %d: OK", i + 1, test.cnt,
                 (unsigned long)bits, n);
        }
    }

    bits = MRP_MASK_BELOW(17);
    for (i = 0; i < _BITS_PER_WORD; i++) {
        b = !!(bits & MRP_MASK_BIT(i));

        if ((b && i >= 17) || (!b && i < 17))
            FATAL("MRP_MASK_BELOW(17) for bit #%d: FAILED (%d)", i, b);
        else
            INFO("MRP_MASK_BELOW(17) for bit #%d: OK (%d)", i, b);
    }

    bits = MRP_MASK_ABOVE(7);
    for (i = 0; i < _BITS_PER_WORD; i++) {
        b = !!(bits & MRP_MASK_BIT(i));

        if ((b && i <= 7) || (!b && i > 7))
            FATAL("MRP_MASK_ABOVE(7) for bit #%d: FAILED (%d)", i, b);
        else
            INFO("MRP_MASK_ABOVE(7) for bit #%d: OK (%d)", i, b);
    }
}


static void mask_tests(void)
{
    int bits[] = {
        0, 1, 2, 5, 11, 19, 31, 32, 63, 64, 65, 66, 126, 127, 128, 129,
        213, 345, 452, 517, 1029,
        -1
    };
    char buf[16 * 1024];
    mrp_mask_t m, c, r;
    int        b, i, max;

    mrp_mask_init(&m);
    mrp_mask_init(&c);
    mrp_mask_init(&r);

    for (i = max = 0; (b = bits[i]) >= 0; i++) {
        if (b > max)
            max = b;

        mrp_mask_set(&m, b);

        if (!mrp_mask_test(&m, b))
            FATAL("set/test for bit %d: FAILED", b);
        else
            PROGRESS("set/test for bit %d: OK", b);
    }

    mrp_mask_not(&c, &m);
    PROGRESS("mask: %s", mrp_mask_dump(buf, sizeof(buf), &m));
    PROGRESS("negated mask: %s", mrp_mask_dump(buf, sizeof(buf), &c));

    /* test ~m against m */
    for (i = 0; i <= max; i++) {
        b = mrp_mask_test(&m, i);

        if ((b && mrp_mask_test(&c, i)) || (!b && !mrp_mask_test(&c, i)))
            FATAL("set/negated test for bit #%d: FAILED", i);
        else
            PROGRESS("set/negated test for bit #%d: OK", i);
    }

    mrp_mask_not(&c, NULL);
    PROGRESS("doubly negated mask: %s",
         mrp_mask_dump(buf, sizeof(buf), mrp_mask_not(&c, NULL)));

    /* test m | ~m */
    mrp_mask_copy(&c, &m);
    mrp_mask_copy(&r, &c);
    mrp_mask_neg(&c);
    mrp_mask_or(&r, &c);

    for (i = 0; i <= max; i++) {
        if (!mrp_mask_test(&r, i))
            FATAL("m | ~m test for bit #%d: FAILED", i);
        else
            PROGRESS("m | ~m test for bit #%d: OK", i);
    }

    /* test m & ~m */
    mrp_mask_copy(&c, &m);
    mrp_mask_copy(&r, &c);
    mrp_mask_neg(&c);
    mrp_mask_and(&r, &c);

    for (i = 0; i <= max; i++) {
        if (mrp_mask_test(&r, i))
            FATAL("m & ~m test for bit #%d: FAILED", i);
        else
            PROGRESS("m & ~m test for bit #%d: OK", i);
    }

    /* test clearing and setting operations */
    mrp_mask_neg(&r);                             /* now all ones */
    mrp_mask_clear_below(&r, 100);

    PROGRESS("cleared below 100 %s", mrp_mask_dump(buf, sizeof(buf), &r));

    for (i = 0; i <= max; i++) {
        b = mrp_mask_test(&r, i);

        if ((b && i < 100) || (!b && i >= 100))
            FATAL("clear below 100 for bit #%d: FAILED (%d)", i, b);
        else
            PROGRESS("clear below 100 for bit #%d: OK (%d)", i, b);
    }

    mrp_mask_clear_above(&r, 500);

    PROGRESS("cleared below 100/above 500 %s",
             mrp_mask_dump(buf, sizeof(buf), &r));

    for (i = 0; i <= max; i++) {
        b = mrp_mask_test(&r, i);

        if ((b && (i < 100 || i > 500)) || (!b && (100 <= i && i <= 500)))
            FATAL("clear below 100/above 500 for bit #%d: FAILED (%d)", i, b);
        else
            PROGRESS("clear below 100/above 500 for bit #%d: OK (%d)", i, b);
    }

    /* test range clear */
    mrp_mask_copy(&c, &r);
    mrp_mask_neg(&c);
    mrp_mask_or(&r, &c);                 /* now all ones */

    mrp_mask_clear_range(&r, 100, 350);

    PROGRESS("cleared 100-350 %s", mrp_mask_dump(buf, sizeof(buf), &r));

    for (i = 0; i <= max; i++) {
        b = mrp_mask_test(&r, i);

        if ((b && (100 <= i && i <= 350)) || (!b && (i < 100 || i > 350)))
            FATAL("range clear [100-350] for bit #%d: FAILED (%d)", i, b);
        else
            PROGRESS("range clear [100-350] for bit #%d: OK (%d)", i, b);
    }

    mrp_mask_copy(&c, &r);
    mrp_mask_neg(&c);
    mrp_mask_or(&r, &c);                 /* now all ones */

    mrp_mask_clear_range(&r, 200, 250);

    PROGRESS("cleared 200-250 %s", mrp_mask_dump(buf, sizeof(buf), &r));

    for (i = 0; i <= max; i++) {
        b = mrp_mask_test(&r, i);

        if ((b && (200 <= i && i <= 250)) || (!b && (i < 200 || i > 250)))
            FATAL("range clear [200-250] for bit #%d: FAILED (%d)", i, b);
        else
            PROGRESS("range clear [200-250] for bit #%d: OK (%d)", i, b);
    }

    /* test range set */
    mrp_mask_copy(&c, &r);
    mrp_mask_neg(&c);
    mrp_mask_and(&r, &c);                 /* now all zeros */

    mrp_mask_set_below(&r, 100);

    PROGRESS("set below 100 %s", mrp_mask_dump(buf, sizeof(buf), &r));

    for (i = 0; i <= max; i++) {
        b = mrp_mask_test(&r, i);

        if ((b && (i >= 100)) || (!b && i < 100))
            FATAL("set below 100 for bit #%d: FAILED (%d)", i, b);
        else
            PROGRESS("set below 100 for bit #%d: OK (%d)", i, b);
    }

    mrp_mask_set_above(&r, 500);

    PROGRESS("set below 100/above 500 %s", mrp_mask_dump(buf, sizeof(buf), &r));

    for (i = 0; i <= max; i++) {
        b = mrp_mask_test(&r, i);

        if ((b && (100 < i && i <= 500)) || (!b && (i < 100 || i > 500)))
            FATAL("set below 100/above 500 for bit #%d: FAILED (%d)", i, b);
        else
            PROGRESS("set below 100/above 500 for bit #%d: OK (%d)", i, b);
    }

    /* test range set */
    mrp_mask_copy(&c, &r);
    mrp_mask_neg(&c);
    mrp_mask_and(&r, &c);                 /* now all zeros */

    mrp_mask_set_range(&r, 200, 250);
    PROGRESS("set range [200-250] %s", mrp_mask_dump(buf, sizeof(buf), &r));

    for (i = 0; i <= max; i++) {
        b = mrp_mask_test(&r, i);

        if ((b && !(200 <= i && i <= 250)) || (!b && (200 <= i && i <= 250)))
            FATAL("range set [200-250] for bit #%d: FAILED (%d)", i, b);
        else
            PROGRESS("range set [200-250] for bit #%d: OK (%d)", i, b);
    }
}


void iter_tests(void)
{
    int bits[] = {
        0, 1, 2, 5, 11, 19, 31, 32, 63, 64, 65, 66, 126, 127, 128, 129,
        213, 345, 452, 509, 510, 511, 512, 513, 514, 515, 517, 1001, 1029,
        -1
    };
    mrp_mask_t m;
    int        b, i, max;

    mrp_mask_init(&m);

    for (i = max = 0; (b = bits[i]) >= 0; i++) {
        if (b > max)
            max = b;

        mrp_mask_set(&m, b);

        if (!mrp_mask_test(&m, b))
            FATAL("set/test for bit %d: FAILED", b);
        else
            PROGRESS("set/test for bit %d: OK", b);
    }

    MRP_MASK_FOREACH_SET(&m, b, 0) {
        if (!mrp_mask_test(&m, b))
            FATAL("iterator gave unset bit %d", b);
        else
            PROGRESS("next bit set: %d", b);
    }
}


void alloc_tests(void)
{
    mrp_mask_t m;
    int i, j;

    mrp_mask_init(&m);

    for (i = 0; i < 1024; i++) {
        mrp_mask_grow(&m, i + 1);
        j = mrp_mask_alloc(&m);
        if (j < 0)
            FATAL("failed to allocate bit #%d", i);
        else
            PROGRESS("allocated bit #%d: %d", i, j);
    }

    mrp_mask_reset(&m);
    mrp_mask_lock(&m, 256);

    for (i = 0; i < 1024; i++) {
        j = mrp_mask_alloc(&m);
        if ((j < 0 && i < 256) || (j >= 0 && i >= 256))
            FATAL("failed to allocate bit #%d", i);
        else
            PROGRESS("alloc-test bit #%d: OK (%d)", i, j);
    }

}


int main(int argc, char *argv[])
{
    int i;

    test.verbosity = V_ERROR;
    test.seed      = 0;
    test.cnt       = 0;
    test.range     = 0;

    for (i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-d")) {
            mrp_debug_enable(true);
            mrp_debug_set("@mask.h");
        }
        else if (!strcmp(argv[i], "-v"))
            test.verbosity++;
        else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--range"))
            test.range = 1;
    }

    basic_tests();
    range_tests();
    mask_tests();
    alloc_tests();
    iter_tests();

    return 0;
}
