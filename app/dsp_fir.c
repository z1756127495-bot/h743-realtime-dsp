#include "dsp_fir.h"

void fir_init(fir_t *f, float *hist, const float *coeffs, size_t taps)
{
    f->taps   = taps;
    f->pos    = 0;
    f->hist   = hist;
    f->coeffs = coeffs;
    for (size_t i = 0; i < taps; i++) {
        hist[i] = 0.0f;
    }
}

float fir_process(fir_t *f, float x)
{
    f->hist[f->pos] = x;

    float acc = 0.0f;
    for (size_t k = 0; k < f->taps; k++) {
        size_t idx = (f->pos + f->taps - k) % f->taps; /* k-th oldest sample */
        acc += f->coeffs[k] * f->hist[idx];
    }
    f->pos = (f->pos + 1u) % f->taps;
    return acc;
}

