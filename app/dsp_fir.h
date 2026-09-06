#ifndef DSP_FIR_H
#define DSP_FIR_H

#include <stddef.h>

/* Pure-C FIR filter. In the target firmware this still runs on the host for
 * cross-checking, so its behaviour is verifiable in CI without a board. */
typedef struct {
    size_t        taps;
    size_t        pos;
    float        *hist;    /* circular history, length == taps */
    const float  *coeffs;  /* length == taps, coeffs[0] applies to newest sample */
} fir_t;

void  fir_init(fir_t *f, float *hist, const float *coeffs, size_t taps);
float fir_process(fir_t *f, float x);

#endif /* DSP_FIR_H */

