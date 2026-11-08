/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 19A: independent fixed-point prototypes for electronic kicks and snares.
 * No engine registration, persistent data or existing DRUM-X behavior changes.
 * The caller supplies sine_i(phase), producing signed Q15, just as DRUM-X does.
 */
#ifndef FELUCCA_DRUMLAB_VOICE_C
#define FELUCCA_DRUMLAB_VOICE_C

enum { DLAB_KICK, DLAB_SNARE, DLAB_HEAVY_SNARE,
       DLAB_CLAP, DLAB_HAT_C, DLAB_HAT_O, DLAB_TOM, DLAB_METAL, DLAB_KIND_COUNT };
typedef struct {
    uint8_t kind, pitch, sweep, body, snap, noise, tone, drive;
    uint8_t wire, weight, metal; /* Heavy snare: wire length, sub-body and inharmonic ring. */
} dlab_patch_t;
typedef struct {
    uint32_t phase, phase2, phase3, seed, base_inc;
    uint32_t age_samples;
    int32_t body_env, snap_env, sweep_env, wire_env; /* Q24, initially 1.0 */
    int32_t noise_lp, wire_lp;                   /* Q15 one-pole states */
    int32_t body_fast_lp, body_slow_lp;          /* Stage 23C snare low-mid band */
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

/* The legacy denominator above wraps in 32 bits for tau > ~97 ms. Stage
 * 21B uses the algebraically equivalent x = nsamples * (1e6 * 65536 / 44100)
 * / tau_us, with the constant rounded to 1486077. The 32-sample audio blocks
 * fit in uint32_t; no unsupported 64-bit divide is pulled into the firmware.
 * Preserve the legacy rendering for previously approved patches. */
static uint32_t dlab_decay_long(uint32_t nsamples, uint32_t tau_us)
{
    uint32_t x = (nsamples * 1486077u) / tau_us;
    return x >= 65536u ? 0u : (uint32_t)(((uint64_t)65536u * 65536u) / (65536u + x));
}

/* Stage 23A: x*x*span wrapped uint32_t before the /16129, making BODY and
 * WIRES cycle through shorter decays as their displayed values increased.
 * Divide the fixed span first: q*sq + (r*sq)/16129 is EXACTLY the same
 * integer result as (sq*span)/16129, without a 64-bit divide or overflow. */
#define DLAB_CURVE_US(v, span) \
    (((uint32_t)(v) * (v)) * ((span) / 16129u) + \
     (((uint32_t)(v) * (v)) * ((span) % 16129u)) / 16129u)

static void dlab_trigger(dlab_voice_t *v, const dlab_patch_t *p, uint32_t seed)
{
    uint32_t note = 24u + (uint32_t)p->pitch * 60u / 127u;
    memset(v, 0, sizeof *v);
    v->seed = seed ? seed : 0x6D2B79F5u;
    v->base_inc = pitch_inc(note * 16u);
    v->body_env = v->snap_env = v->sweep_env = v->wire_env = 1 << 24;
    v->active = 1;
}

/* One-shot synthesis, mono Q15 samples. KICK is a swept sine with a short
 * noise click; SNARE uses two inharmonic body oscillators and independently
 * decaying filtered noise. Both modes have a drive knee, not hard clipping. */
static void dlab_run(dlab_voice_t *v, const dlab_patch_t *p, int32_t *out, uint32_t n)
{
    uint32_t i, kb, kn, kp, ph = v->phase, ph2 = v->phase2, ph3 = v->phase3, rng = v->seed;
    int32_t ba = v->body_env, na = v->snap_env, pe = v->sweep_env, lp = v->noise_lp;
    int32_t b1, n1, p1, db, dn, inc, inc1, di;
    uint32_t body_us = 14000u + DLAB_CURVE_US(p->body, 850000u);
    uint32_t snap_us = 1500u + (uint32_t)p->snap * p->snap * 240000u / 16129u;
    uint32_t sweep_us = 1800u + (uint32_t)p->sweep * 150u;
    int32_t noise_mix = p->noise, tone_mix = 127 - noise_mix;
    int32_t lp_k = 1024 + (int32_t)p->tone * 220;
    /* Stage 21A revision: the original kick's fundamental can be around 50 Hz;
     * a sub-octave is then almost inaudible. METAL 64..127 instead adds a
     * tuneful second harmonic (punch around 100 Hz), with a longer body tail.
     * WEIGHT sets its gain. 0..63 stays bit-for-bit like Stage 20B. */
    uint32_t punch_mix = p->kind == DLAB_KICK && p->metal > 63u ?
                         ((uint32_t)p->metal - 63u) * (64u + p->weight) : 0u;
    /* The last part of the range adds a pronounced, industrial edge: a third
     * harmonic and a short filtered-noise strike. Lower settings are identical
     * to the previous Stage 21A audition. */
    uint32_t edge_mix = p->kind == DLAB_KICK && p->metal > 111u ?
                        (uint32_t)p->metal - 111u : 0u;
    if (punch_mix) body_us += ((uint32_t)p->metal - 63u) * 6000u;
    if (!v->active) {
        for (i = 0; i < n; i++) out[i] = 0;
        return;
    }
    kb = dlab_decay_long(n, body_us);
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
            if (punch_mix) {
                x += (int32_t)(((int64_t)sine_i(ph2) * ba * punch_mix) >> 38);
                ph2 += (uint32_t)inc * 2u;
            }
            if (edge_mix) {
                x += (int32_t)(((int64_t)sine_i(ph3) * ba * edge_mix) >> 29);
                x += (int32_t)(((int64_t)noise * na * edge_mix) >> 30);
                ph3 += (uint32_t)inc * 3u;
            }
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
    v->phase3 = ph3;
    v->seed = rng;
    v->noise_lp = lp;
    v->body_env = b1;
    v->snap_env = n1;
    v->sweep_env = p1;
    if (b1 < 100 && n1 < 100)
        v->active = 0;
}

/* Stage 19B: three-layer snare model. The drum membrane, attack crack, and
 * wire rattle have INDEPENDENT envelopes. Optional inharmonic modes add metal
 * ringing. Q15 signal paths, no floats, allocation or tables at render time.
 * Deliberately separate from the original DRUM-X and Stage 19A voices. */
static void dlab_run_heavy_snare(dlab_voice_t *v, const dlab_patch_t *p,
                                 int32_t *out, uint32_t n)
{
    uint32_t i, ph = v->phase, ph2 = v->phase2, ph3 = v->phase3, rng = v->seed;
    /* Stage 23C: BODY shapes a compact non-tonal membrane, not a long sine.
     * SNAP and WIRES retain independent, smoothly increasing durations. */
    uint32_t body_us = 35000u + DLAB_CURVE_US(p->body, 185000u);
    uint32_t wire_us = 16000u + DLAB_CURVE_US(p->wire, 310000u);
    uint32_t snap_us = 9000u + DLAB_CURVE_US(p->snap, 65000u);
    uint32_t sweep_us = 3000u + (uint32_t)p->sweep * 85u;
    int32_t be = v->body_env, we = v->wire_env, se = v->snap_env, pe = v->sweep_env;
    int32_t be_end, we_end, se_end, pe_end, db, dw, ds, dp;
    int32_t lp = v->noise_lp, wire_lp = v->wire_lp;
    int32_t body_fast_lp = v->body_fast_lp, body_slow_lp = v->body_slow_lp;
    int32_t wire_k = 1800 + (int32_t)p->tone * 170;
    uint32_t base = v->base_inc;
    if (!v->active) {
        for (i = 0; i < n; i++) out[i] = 0;
        return;
    }
    be_end = (int32_t)(((int64_t)be * dlab_decay_long(n, body_us)) >> 16);
    we_end = (int32_t)(((int64_t)we * dlab_decay_long(n, wire_us)) >> 16);
    se_end = (int32_t)(((int64_t)se * dlab_decay_long(n, snap_us)) >> 16);
    pe_end = (int32_t)(((int64_t)pe * dlab_decay_long(n, sweep_us)) >> 16);
    db = (be_end - be) / (int32_t)n;
    dw = (we_end - we) / (int32_t)n;
    ds = (se_end - se) / (int32_t)n;
    dp = (pe_end - pe) / (int32_t)n;
    for (i = 0; i < n; i++, be += db, we += dw, se += ds, pe += dp) {
        uint32_t inc = base + (uint32_t)(((uint64_t)base * p->sweep * (uint32_t)(pe >> 12)) >> 19);
        int32_t white = (int32_t)(dlab_rand(&rng) >> 16) - 32768;
        int32_t membrane = sine_i(ph);
        int32_t second = sine_i(ph2);
        int32_t ring = sine_i(ph3);
        int32_t crack, wires, raw, y;
        /* Fast high-pass transient plus lower, gently damped wire rattle. */
        lp += (int32_t)(((int64_t)(white - lp) * 3100) >> 15);
        wire_lp += (int32_t)(((int64_t)(white - wire_lp) * wire_k) >> 15);
        crack = white - lp;
        wires = (white + wire_lp) / 2;
        /* Stage 23C: the broad low-mid band IS the body. TONE moves the
         * filters gradually, BODY controls weight and duration, while the
         * audible oscillator impact dies away in a few milliseconds. */
        body_fast_lp += (int32_t)(((int64_t)(white - body_fast_lp) * (3000 + p->tone * 28)) >> 15);
        body_slow_lp += (int32_t)(((int64_t)(white - body_slow_lp) * (450 + p->tone * 3)) >> 15);
        int32_t membrane_noise = body_fast_lp - body_slow_lp;
        raw = (int32_t)(((int64_t)membrane_noise * be * (180 + p->weight)) >> 32);
        raw += (int32_t)(((int64_t)(membrane + second / 3) * pe) >> 24);
        raw += (int32_t)(((int64_t)ring * pe * p->metal) >> 34);
        raw += (int32_t)(((int64_t)(crack + wire_lp) * se * (50 + p->snap)) >> 32);
        /* WIRES is a controlled, independently decaying rattle, not the body. */
        int32_t rattle = (we >> 12) * (we >> 12);
        raw += (int32_t)(((int64_t)wires * rattle * p->noise) >> 32);
        /* Strong soft saturation: sustained shell remains substantial while
         * the attack is compressed rather than cut off at a hard threshold. */
        y = (int32_t)(((int64_t)raw * (170 + p->drive * 5u)) >> 8);
        if (y > 14000) y = 14000 + (y - 14000) / 5;
        if (y < -14000) y = -14000 + (y + 14000) / 5;
        if (y > 31000) y = 31000 + (y - 31000) / 32;
        if (y < -31000) y = -31000 + (y + 31000) / 32;
        out[i] = y > 32767 ? 32767 : y < -32768 ? -32768 : y;
        ph += inc;
        ph2 += (uint32_t)(((uint64_t)inc * 6021u) >> 12);
        ph3 += (uint32_t)(((uint64_t)inc * (7300u + p->metal * 27u)) >> 12);
    }
    v->phase = ph;
    v->phase2 = ph2;
    v->phase3 = ph3;
    v->seed = rng;
    v->noise_lp = lp;
    v->wire_lp = wire_lp;
    v->body_fast_lp = body_fast_lp;
    v->body_slow_lp = body_slow_lp;
    v->body_env = be_end;
    v->wire_env = we_end;
    v->snap_env = se_end;
    v->sweep_env = pe_end;
    if (be_end < 100 && we_end < 100 && se_end < 100) v->active = 0;
}

/* Stage 24B: five editable electronic/industrial percussion characters.
 * HAT C/O share exactly one noise-based synthesis path; no pitched component
 * reaches the hats. Existing kick and snare renderers are not changed. */
static void dlab_run_extra(dlab_voice_t *v, const dlab_patch_t *p,
                           int32_t *out, uint32_t n)
{
    uint32_t i, kind = p->kind, at = v->age_samples;
    uint32_t tau = 18000u + DLAB_CURVE_US(p->body, 300000u);
    int32_t env = v->body_env, lp = v->noise_lp, fast = v->wire_lp;
    uint32_t phase = v->phase, phase2 = v->phase2, phase3 = v->phase3, rng = v->seed;
    if (!v->active) { for (i = 0; i < n; i++) out[i] = 0; return; }
    if (kind == DLAB_CLAP) tau = 28000u + DLAB_CURVE_US(p->body, 160000u);
    if (kind == DLAB_HAT_C) tau = 10000u + DLAB_CURVE_US(p->body, 95000u);
    if (kind == DLAB_HAT_O) tau = 60000u + DLAB_CURVE_US(p->body, 420000u);
    if (kind == DLAB_TOM) tau = 35000u + DLAB_CURVE_US(p->body, 400000u);
    int32_t end = (int32_t)(((int64_t)env * dlab_decay_long(n, tau)) >> 16);
    int32_t step = (end - env) / (int32_t)n;
    for (i = 0; i < n; i++, env += step, at++) {
        int32_t white = (int32_t)(dlab_rand(&rng) >> 16) - 32768;
        int32_t s = sine_i(phase), s2 = sine_i(phase2), s3 = sine_i(phase3), raw = 0;
        if (kind == DLAB_CLAP) {
            lp += (int32_t)(((int64_t)(white - lp) * (1100 + p->tone * 32u)) >> 15);
            uint32_t d = at % 410u;
            int32_t burst = at < 1230u ? (int32_t)(410u - d) : 0;
            raw = (int32_t)(((int64_t)(white - lp) * env * (45 + burst / 4 + p->snap / 3)) >> 31);
        } else if (kind == DLAB_HAT_C || kind == DLAB_HAT_O) {
            lp += (int32_t)(((int64_t)(white - lp) * (1700 + p->tone * 16u)) >> 15);
            fast += (int32_t)(((int64_t)(white - fast) * (7200 + p->tone * 32u)) >> 15);
            int32_t top = white - fast, texture = fast - lp;
            int32_t blend = kind == DLAB_HAT_C ? (top * 3 + texture) / 4 :
                                                        (top * 2 + texture * 2) / 4;
            raw = (int32_t)(((int64_t)blend * env * (100 + p->noise / 2)) >> 31);
        } else if (kind == DLAB_TOM) {
            uint32_t sweep = at < 3500u ? 3500u - at : 0u;
            uint32_t inc = v->base_inc + (uint32_t)(((uint64_t)v->base_inc * sweep * (20u + p->sweep)) >> 20);
            raw = (int32_t)(((int64_t)(s * 3 + s2) * env * (64 + p->weight)) >> 34);
            phase += inc;
            phase2 += inc * 2u;
        } else { /* DLAB_METAL */
            raw = (int32_t)(((int64_t)(s + s2 + s3) * env * (80u + p->metal)) >> 32);
            if (at < 2500u) raw += (int32_t)(((int64_t)white * env * p->noise) >> 32);
        }
        if (kind != DLAB_TOM) {
            phase += v->base_inc;
            phase2 += (uint32_t)(((uint64_t)v->base_inc * 6021u) >> 12);
        }
        phase3 += (uint32_t)(((uint64_t)v->base_inc * (7800u + p->metal * 12u)) >> 12);
        raw = (int32_t)(((int64_t)raw * (120 + p->drive * 2u)) >> 8);
        if (raw > 18000) raw = 18000 + (raw - 18000) / 5;
        if (raw < -18000) raw = -18000 + (raw + 18000) / 5;
        out[i] = raw > 32767 ? 32767 : raw < -32768 ? -32768 : raw;
    }
    v->body_env = end; v->noise_lp = lp; v->wire_lp = fast;
    v->phase = phase; v->phase2 = phase2; v->phase3 = phase3;
    v->seed = rng; v->age_samples = at;
    if (end < 100) v->active = 0;
}
#endif /* FELUCCA_DRUMLAB_VOICE_C */
