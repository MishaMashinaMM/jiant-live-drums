/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 19A: independent fixed-point prototypes for electronic kicks and snares.
 * No engine registration, persistent data or existing DRUM-X behavior changes.
 * The caller supplies sine_i(phase), producing signed Q15, just as DRUM-X does.
 */

enum { DLAB_KICK, DLAB_SNARE };
typedef struct {
    uint8_t kind, pitch, sweep, body, snap, noise, tone, drive;
} dlab_patch_t;
typedef struct {
    uint32_t phase, phase2, seed, base_inc;
    int32_t body_env, snap_env, sweep_env; /* Q24, initially 1.0 */
    int32_t noise_lp;                      /* Q15 one-pole state */
    uint8_t active;
} dlab_voice_t;

static uint32_t dlab_rand(uint32_t *seed)
{
    uint32_t x = *seed;
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    *seed = x;
    return x;
}

/* Q16 exponential envelope for one audio block. This rational first-order
 * approximation is monotonic, bounded, and uses no floats or per-sample divides. */
static uint32_t dlab_decay(uint32_t nsamples, uint32_t tau_us)
{
    uint32_t x = (uint32_t)(((uint64_t)nsamples * 1000000u * 65536u) /
                            (44100u * tau_us));
    return x >= 65536u ? 0u : (uint32_t)(((uint64_t)65536u * 65536u) / (65536u + x));
}

static void dlab_trigger(dlab_voice_t *v, const dlab_patch_t *p, uint32_t seed)
{
    uint32_t note = 24u + (uint32_t)p->pitch * 60u / 127u;
    memset(v, 0, sizeof *v);
    v->seed = seed ? seed : 0x6D2B79F5u;
    v->base_inc = pitch_inc(note * 16u);
    v->body_env = v->snap_env = v->sweep_env = 1 << 24;
    v->active = 1;
}

/* One-shot synthesis, mono Q15 samples. KICK is a swept sine with a short
 * noise click; SNARE uses two inharmonic body oscillators and independently
 * decaying filtered noise. Both modes have a drive knee, not hard clipping. */
static void dlab_run(dlab_voice_t *v, const dlab_patch_t *p, int32_t *out, uint32_t n)
{
    uint32_t i, kb, kn, kp, ph = v->phase, ph2 = v->phase2, rng = v->seed;
    int32_t ba = v->body_env, na = v->snap_env, pe = v->sweep_env, lp = v->noise_lp;
    int32_t b1, n1, p1, db, dn, inc, inc1, di;
    uint32_t body_us = 14000u + (uint32_t)p->body * p->body * 850000u / 16129u;
    uint32_t snap_us = 1500u + (uint32_t)p->snap * p->snap * 240000u / 16129u;
    uint32_t sweep_us = 1800u + (uint32_t)p->sweep * 150u;
    int32_t noise_mix = p->noise, tone_mix = 127 - noise_mix;
    int32_t lp_k = 1024 + (int32_t)p->tone * 220;
    if (!v->active) {
        for (i = 0; i < n; i++) out[i] = 0;
        return;
    }
    kb = dlab_decay(n, body_us);
    kn = dlab_decay(n, snap_us);
    kp = dlab_decay(n, sweep_us);
    b1 = (int32_t)(((int64_t)ba * kb) >> 16);
    n1 = (int32_t)(((int64_t)na * kn) >> 16);
    p1 = (int32_t)(((int64_t)pe * kp) >> 16);
    db = (b1 - ba) / (int32_t)n;
    dn = (n1 - na) / (int32_t)n;
    /* Sweep: up to about +2.3 octaves at the strike, returning to the base. */
    inc = (int32_t)(v->base_inc + ((uint64_t)v->base_inc * p->sweep * (uint32_t)(pe >> 12) >> 17));
    inc1 = (int32_t)(v->base_inc + ((uint64_t)v->base_inc * p->sweep * (uint32_t)(p1 >> 12) >> 17));
    di = (inc1 - inc) / (int32_t)n;
    for (i = 0; i < n; i++, ba += db, na += dn, inc += di) {
        int32_t a = sine_i(ph), b = 0, white, noise, y, x;
        white = (int32_t)(dlab_rand(&rng) >> 16) - 32768;
        lp += (int32_t)(((int64_t)(white - lp) * lp_k) >> 15);
        noise = white - lp;
        if (p->kind == DLAB_SNARE) {
            b = sine_i(ph2);
            a = (a * 3 + b * 2) / 5;
            ph2 += (uint32_t)(((uint64_t)(uint32_t)inc * 6021u) >> 12);
        }
        ph += (uint32_t)inc;
        if (p->kind == DLAB_KICK) {
            x = (int32_t)(((int64_t)a * ba) >> 24);
            x += (int32_t)(((int64_t)noise * na * (12 + noise_mix)) >> 32);
        } else {
            x = (int32_t)(((int64_t)a * ba * tone_mix) >> 31);
            x += (int32_t)(((int64_t)noise * na * noise_mix) >> 31);
        }
        /* Fold-free saturator: preserve the small-signal gain, compress peaks. */
        y = (int32_t)(((int64_t)x * (128u + p->drive * 3u)) >> 7);
        if (y > 16384) {
            y = 16384 + (y - 16384) / 6;
            if (y > 30000) y = 30000 + (y - 30000) / 16;
        }
        if (y < -16384) {
            y = -16384 + (y + 16384) / 6;
            if (y < -30000) y = -30000 + (y + 30000) / 16;
        }
        out[i] = y > 32767 ? 32767 : y < -32768 ? -32768 : y;
    }
    v->phase = ph;
    v->phase2 = ph2;
    v->seed = rng;
    v->noise_lp = lp;
    v->body_env = b1;
    v->snap_env = n1;
    v->sweep_env = p1;
    if (b1 < 100 && n1 < 100)
        v->active = 0;
}
