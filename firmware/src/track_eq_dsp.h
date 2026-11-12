/* GPL-3.0-only: standalone JLD per-track, three-band audio EQ. */
#ifndef JLD_TRACK_EQ_DSP_H
#define JLD_TRACK_EQ_DSP_H
#include <stdint.h>
#define EQ_BANDS 3u
#define EQ_STREAMS 5u /* stereo dry + chorus, delay, reverb sends */
typedef struct { int32_t x1, x2, y1, y2; } eq_hist_t;
typedef struct {
    eq_hist_t hist[EQ_STREAMS][EQ_BANDS];
    int8_t gain[EQ_BANDS];
    uint8_t on;
    uint8_t was_active; /* one neutral processing block on bypass transition */
    /* LOW's fractional output feedback in Q28 (two previous outputs per bus).
     * Retains low-frequency state without storing double-precision samples. */
    int32_t low_frac[EQ_STREAMS][2];
#if defined(EQ_DIAG_LOW_DOUBLE) && EQ_DIAG_LOW_DOUBLE
    /* Emulator-only high-precision LOW history: x1, x2, y1, y2 for each bus.
     * Normal hardware builds do not allocate this state. */
    double low_hist[EQ_STREAMS][4];
#endif
} eq_dsp_t;
void eq_dsp_init(eq_dsp_t *s);
void eq_dsp_tick(eq_dsp_t *s, const int8_t gain[EQ_BANDS], uint8_t on);
void eq_dsp_process(eq_dsp_t *s, unsigned stream, int32_t *buf, unsigned n);
#endif
