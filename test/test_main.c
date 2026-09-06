#include <stdio.h>
#include <string.h>
#include <math.h>

#include "ring_buffer.h"
#include "dsp_fir.h"

static int g_failures = 0;

#define CHECK(cond)                                                          \
    do {                                                                     \
        if (!(cond)) {                                                       \
            printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond);           \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

#define CHECK_NEAR(a, b, tol)                                                \
    do {                                                                     \
        float _a = (a), _b = (b);                                            \
        if (fabsf(_a - _b) > (tol)) {                                        \
            printf("FAIL %s:%d  %s = %.4f, expected %.4f\n",                 \
                   __FILE__, __LINE__, #a, (double)_a, (double)_b);          \
            g_failures++;                                                    \
        }                                                                    \
    } while (0)

static void test_ring_buffer(void)
{
    uint8_t storage[4];
    uint8_t out[8];
    ring_buffer_t rb;
    rb_init(&rb, storage, 4);

    CHECK(rb_used(&rb) == 0);
    CHECK(rb_free(&rb) == 4);

    const uint8_t a[3] = {1, 2, 3};
    CHECK(rb_write(&rb, a, 3) == 3);
    CHECK(rb_used(&rb) == 3);

    CHECK(rb_read(&rb, out, 2) == 2);
    CHECK(out[0] == 1 && out[1] == 2);
    CHECK(rb_used(&rb) == 1);

    /* write more than free space -> must cap at free */
    const uint8_t b[5] = {9, 9, 9, 9, 9};
    CHECK(rb_write(&rb, b, 5) == 3);      /* free was 3 */
    CHECK(rb_used(&rb) == 4);             /* full now */

    CHECK(rb_read(&rb, out, 8) == 4);
    CHECK(out[0] == 3 && out[1] == 9 && out[2] == 9 && out[3] == 9);
    CHECK(rb_used(&rb) == 0);

    CHECK(rb_read(&rb, out, 8) == 0);     /* empty */
}

static void test_fir_impulse(void)
{
    const float coeffs[3] = {1.0f, 2.0f, 3.0f};
    float hist[3];
    fir_t f;
    fir_init(&f, hist, coeffs, 3);

    CHECK_NEAR(fir_process(&f, 1.0f), 1.0f, 1e-5);
    CHECK_NEAR(fir_process(&f, 0.0f), 2.0f, 1e-5);
    CHECK_NEAR(fir_process(&f, 0.0f), 3.0f, 1e-5);
}

static void test_fir_average_converges_to_gain(void)
{
    const float coeffs[4] = {0.25f, 0.25f, 0.25f, 0.25f};
    float hist[4];
    fir_t f;
    fir_init(&f, hist, coeffs, 4);

    float last = 0.0f;
    for (int i = 0; i < 8; i++) {
        last = fir_process(&f, 1.0f);
    }
    /* DC gain equals sum(coeffs) = 1.0 */
    CHECK_NEAR(last, 1.0f, 1e-5);
}

int main(void)
{
    test_ring_buffer();
    test_fir_impulse();
    test_fir_average_converges_to_gain();

    if (g_failures == 0) {
        printf("ALL TESTS PASSED\n");
        return 0;
    }
    printf("%d TEST(S) FAILED\n", g_failures);
    return 1;
}

