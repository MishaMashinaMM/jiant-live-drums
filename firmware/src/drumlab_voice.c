/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 19A: independent fixed-point prototypes for electronic kicks and snares.
 * No engine registration, persistent data or existing DRUM-X behavior changes.
 * The caller supplies sine_i(phase), producing signed Q15, just as DRUM-X does.
 */
#ifndef FELUCCA_DRUMLAB_VOICE_C
#define FELUCCA_DRUMLAB_VOICE_C

enum { DLAB_KICK, DLAB_SNARE, DLAB_HEAVY_SNARE };
typedef struct {
    uint8_t kind, pitch, sweep, body, snap, noise, tone, drive;
    uint8_t wire, weight, metal; /* Heavy snare: wire length, sub-body and inharmonic ring. */
} dlab_patch_t;
typedef struct {
    uint32_t phase, phase2, phase3, seed, base_inc;
    int32_t body_env, snap_env, sweep_env, wire_env; /* Q24, initially 1.0 */
    int32_t noise_lp, wire_lp;                   /* Q15 one-pole states */
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

/* Long snare envelopes need a 64-bit denominator. The legacy approximation
 * above multiplied 44100 * tau_us as 32-bit, wrapping for tau > ~97 ms.
 * Keep legacy rendering untouched; Stage 21B's opt-in harder-snare mode uses
 * this corrected decay so its shell/wire tails stay smooth and continuous. */
static uint32_t dlab_decay_long(uint32_t nsamples, uint32_t tau_us)
{
    uint64_t x = ((uint64_t)nsamples * 1000000u * 65536u) /
                 ((uint64_t)44100u * tau_us);
    return x >= 65536u ? 0u : (uint32_t)(((uint64_t)65536u * 65536u) / (65536u + x));
}

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
    uint32_t body_us = 14000u + (uint32_t)p->body * p->body * 850000u / 16129u;
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
    uint32_t body_us = 70000u + (uint32_t)p->body * p->body * 900000u / 16129u;
    uint32_t wire_us = 35000u + (uint32_t)p->wire * p->wire * 1200000u / 16129u;
    uint32_t snap_us = 900u + (uint32_t)p->snap * 130u;
    uint32_t sweep_us = 1200u + (uint32_t)p->sweep * 220u;
    /* Stage 21B: TONE above 116 adds an optional harder shell/crack voice.
     * The three approved Stage 19B snare patches all have TONE <= 113,
     * so their original audio remains bit-for-bit unchanged. */
    uint32_t edge = p->tone > 116u ? (uint32_t)p->tone - 116u : 0u;
    if (edge) {
        body_us += edge * 24000u;
        wire_us += edge * 12000u;
    }
    int32_t be = v->body_env, we = v->wire_env, se = v->snap_env, pe = v->sweep_env;
    int32_t be_end, we_end, se_end, pe_end, db, dw, ds, dp;
    int32_t lp = v->noise_lp, wire_lp = v->wire_lp;
    int32_t wire_k = 1800 + (int32_t)p->tone * 170;
    uint32_t base = v->base_inc;
    if (!v->active) {
        for (i = 0; i < n; i++) out[i] = 0;
        return;
    }
    be_end = (int32_t)(((int64_t)be * (edge ? dlab_decay_long(n, body_us) : dlab_decay(n, body_us))) >> 16);
    we_end = (int32_t)(((int64_t)we * (edge ? dlab_decay_long(n, wire_us) : dlab_decay(n, wire_us))) >> 16);
    se_end = (int32_t)(((int64_t)se * (edge ? dlab_decay_long(n, snap_us) : dlab_decay(n, snap_us))) >> 16);
    pe_end = (int32_t)(((int64_t)pe * (edge ? dlab_decay_long(n, sweep_us) : dlab_decay(n, sweep_us))) >> 16);
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
        int32_t crack, wires, shell, raw, y;
        /* Fast high-pass transient plus lower, gently damped wire rattle. */
        lp += (int32_t)(((int64_t)(white - lp) * 3100) >> 15);
        wire_lp += (int32_t)(((int64_t)(white - wire_lp) * wire_k) >> 15);
        crack = white - lp;
        wires = (white + wire_lp) / 2;
        shell = (membrane * (180 + p->weight) + second * 85) / 392;
        shell += (ring * p->metal) / 340;
        raw = (int32_t)(((int64_t)shell * be * (100 + p->weight)) >> 31);
        raw += (int32_t)(((int64_t)wires * we * p->noise) >> 31);
        raw += (int32_t)(((int64_t)crack * se * (90 + p->snap)) >> 32);
        if (edge) {
            /* A pitched, inharmonic body plus a separate short crack. */
            raw += (int32_t)(((int64_t)second * be * edge) >> 30);
            raw += (int32_t)(((int64_t)crack * se * edge) >> 30);
        }
        /* Strong soft saturation: sustained shell remains substantial while
         * the attack is compressed rather than cut off at a hard threshold. */
        y = (int32_t)(((int64_t)raw * (170 + p->drive * 5u)) >> 8);
        if (y > 14000) y = 14000 + (y - 14000) / 5;
        if (y < -14000) y = -14000 + (y + 14000) / 5;
        if (y > 31000) y = 31000 + (y - 31000) / 32;
        if (y < -31000) y = -31000 + (y + 31000) / 32;
        if (edge) {  /* extra soft knee for the new crack layer; legacy snares untouched */
            if (y > 30000) y = 30000 + (y - 30000) / 4;
            if (y < -30000) y = -30000 + (y + 30000) / 4;
        }
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
    v->body_env = be_end;
    v->wire_env = we_end;
    v->snap_env = se_end;
    v->sweep_env = pe_end;
    if (be_end < 100 && we_end < 100 && se_end < 100) v->active = 0;
}
#endif /* FELUCCA_DRUMLAB_VOICE_C */
