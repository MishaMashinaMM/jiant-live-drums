/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM-80: independent 12-pad 1980s drum engine.
 * Fixed-point oscillator/noise synthesis with per-pad controls and MIDI map.
 */
#define D80_PADS 12u
#define D80_PARAMS 12u
typedef enum {
    D80_BD, D80_SD, D80_CP, D80_RS, D80_CH, D80_OH,
    D80_LT, D80_HT, D80_CB, D80_CL, D80_PC1, D80_PC2,
    D80_TYPES
} d80_type_t;
typedef struct {
    uint8_t type;
    uint8_t param[D80_PARAMS];
    uint8_t note;
} d80_pad_t;
static d80_pad_t d80_kit[NPART][D80_PADS];
static uint8_t d80_ready[NPART];
static const char *const d80_names[D80_TYPES] = {
    "BD", "SD", "CP", "RS", "CH", "OH", "LT", "HT", "CB", "CL", "PC1", "PC2"
};
/* BODY: TUNE SWEEP BODY DECAY; ATTACK: ATTACK NOISE BRIGHT TAIL;
 * SHAPE: DRIVE PUNCH VEL LEVEL. Initial neutral factory settings.
 * Sound-specific factory tuning belongs to Stage 2, not this silent scaffold. */
static const uint8_t d80_defaults[D80_TYPES][D80_PARAMS] = {
    {50,85,110,65,75,0,40,0,0,85,64,100},  /* BD */
    {70,10,85,50,90,95,95,55,15,90,64,100}, /* SD */
    {64,0,0,15,100,110,85,75,10,90,64,100}, /* CP */
    {75,0,90,15,110,5,80,10,5,90,64,100},   /* RS */
    {80,0,45,10,90,110,115,12,5,85,64,100}, /* CH */
    {80,0,45,10,90,110,115,75,5,80,64,100}, /* OH */
    {40,85,110,70,60,0,35,0,10,80,64,100},  /* LT */
    {85,75,105,50,65,0,45,0,10,80,64,100},  /* HT */
    {80,0,110,55,70,0,100,0,10,80,64,100},  /* CB */
    {90,0,100,12,90,0,70,0,0,90,64,100},    /* CL */
    {65,50,85,55,80,50,80,40,10,80,64,100},/* PC1 */
    {65,50,85,55,80,50,80,40,10,80,64,100} /* PC2 */
};
static void d80_pad_defaults(d80_pad_t *p, uint32_t type)
{
    uint32_t j;
    p->type = (uint8_t)type;
    for (j = 0; j < D80_PARAMS; ++j) p->param[j] = d80_defaults[type][j];
}
static void d80_ensure(uint32_t ti)
{
    uint32_t i;
    if (ti >= NPART || d80_ready[ti]) return;
    for (i = 0; i < D80_PADS; ++i) {
        d80_pad_defaults(&d80_kit[ti][i], i);
        d80_kit[ti][i].note = (uint8_t)(36u + i);
    }
    d80_ready[ti] = 1u;
}
static int d80_type_set(uint32_t ti, uint32_t pad, uint32_t type)
{
    uint8_t note;
    if (ti >= NPART || pad >= D80_PADS || type >= D80_TYPES) return 0;
    d80_ensure(ti);
    note = d80_kit[ti][pad].note;
    d80_pad_defaults(&d80_kit[ti][pad], type);
    d80_kit[ti][pad].note = note;
    return 1;
}
static int d80_param_set(uint32_t ti, uint32_t pad, uint32_t param, uint32_t value)
{
    if (ti >= NPART || pad >= D80_PADS || param >= D80_PARAMS || value > 127u) return 0;
    d80_ensure(ti);
    d80_kit[ti][pad].param[param] = (uint8_t)value;
    return 1;
}
static int d80_note_set(uint32_t ti, uint32_t pad, uint32_t note)
{
    if (ti >= NPART || pad >= D80_PADS || note > 127u) return 0;
    d80_ensure(ti);
    d80_kit[ti][pad].note = (uint8_t)note;
    return 1;
}

/* Exact note matching, including duplicate note assignments.
 * First matching pad wins until multi-pad triggering is explicitly supported.
 * No modulo/octave wrap: user-defined note maps must be deterministic. */
static int32_t d80_pad_for_note(uint32_t ti, uint32_t note)
{
    if (ti >= NPART || note > 127u) return -1;
    d80_ensure(ti);
    for (uint32_t pad = 0; pad < D80_PADS; ++pad)
        if (d80_kit[ti][pad].note == note) return (int32_t)pad;
    return -1;
}
/* Fixed-point, sample-free 1980s-style synthesis. One state per allocated
 * shared voice; no per-sample allocation or floating-point operations. */
typedef struct {
    uint32_t phase, phase2, rng, age;
    int32_t body, noise, sweep, lp;
    uint32_t inc;
    uint8_t pad, active;
} d80_voice_t;
static d80_voice_t d80_voice[NPART][NVOICE];
static uint32_t d80_decay(uint32_t p, uint32_t base, uint32_t range)
{
    /* Q24 envelope multiplier, based on the requested tail length. */
    uint32_t samples = (base + p * range / 127u) * 48u / 1000u;
    return samples ? 16777216u / samples : 16777216u;
}
static void drum80_note_on(track_t *t, voice_t *v)
{
    uint32_t ti = (uint32_t)(t - trk), slot = (uint32_t)(v - t->v);
    int32_t pad = d80_pad_for_note(ti, v->note);
    if (ti >= NPART || slot >= NVOICE || pad < 0) {
        v->active = v->gate = 0;
        return;
    }
    /* Factory closed hat chokes the factory open hat on the same track.
     * Reassigned/duplicated pad types remain independent. */
    if (pad == D80_CH && d80_kit[ti][pad].type == D80_CH &&
        d80_kit[ti][D80_OH].type == D80_OH)
        for (uint32_t other = 0; other < NVOICE; ++other)
            if (d80_voice[ti][other].active && d80_voice[ti][other].pad == D80_OH)
                d80_voice[ti][other].active = 0u;
    d80_voice_t *d = &d80_voice[ti][slot];
    const d80_pad_t *p = &d80_kit[ti][pad];
    uint32_t pitch = (uint32_t)clamp(30 + (int32_t)p->param[0] * 55 / 127, 0, 127);
    memset(d, 0, sizeof *d);
    d->pad = (uint8_t)pad;
    d->active = 1u;
    d->body = d->noise = d->sweep = 1 << 24;
    d->inc = pitch_inc(pitch * 16u);
    d->rng = 0x9E3779B9u ^ (v->age * 2654435761u) ^ ((pad + 1u) * 127u);
}
static int32_t drum80_amp(track_t *t, voice_t *v, int32_t adsr)
{
    uint32_t ti = (uint32_t)(t - trk), slot = (uint32_t)(v - t->v);
    if (ti >= NPART || slot >= NVOICE || !d80_voice[ti][slot].active) {
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    if (v->stage == 4u) return adsr;
    v->env = 1 << 24;
    return 32767;
}
static void drum80_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    uint32_t ti = (uint32_t)(t - trk), slot = (uint32_t)(v - t->v);
    if (ti >= NPART || slot >= NVOICE) return;
    d80_voice_t *d = &d80_voice[ti][slot];
    if (!d->active) return;
    const d80_pad_t *p = &d80_kit[ti][d->pad];
    uint32_t type = p->type, i;
    uint32_t bd = d80_decay(p->param[3], 30u, 750u);
    uint32_t nd = d80_decay(p->param[7], 6u, 350u);
    uint32_t sd = d80_decay(p->param[1], 8u, 130u);
    int32_t mix = p->param[5], brightness = 1024 + (int32_t)p->param[6] * 230;
    int32_t level = p->param[11], drive = p->param[8];
    int32_t osc_mix = (type == D80_CP || type == D80_CH || type == D80_OH) ? 15 :
                      (type == D80_SD ? 65 : 115);
    if (type == D80_CB || type == D80_PC2) osc_mix = 110;
    if (type == D80_RS) osc_mix = 75;
    if (type == D80_CL) osc_mix = 120;
    /* BODY controls oscillator weight; VEL blends fixed and note velocity.
     * ATTACK controls transient duration, independently of PUNCH amount. */
    osc_mix = osc_mix * (32 + (int32_t)p->param[2]) / 159;
    uint32_t click_samples = 240u + (127u - p->param[4]) * 9u;
    int32_t velocity_gain = 127 - (int32_t)p->param[10] +
                            (int32_t)p->param[10] * v->vel / 127;

    if (type == D80_CH) nd *= 4u;
    if (type == D80_OH) nd /= 2u;
    if (type == D80_CP) bd *= 5u;
    for (i = 0; i < n; ++i) {
        int32_t tone, hiss, x, inc;
        if (!d->active) break;
        d->rng ^= d->rng << 13; d->rng ^= d->rng >> 17; d->rng ^= d->rng << 5;
        hiss = (int32_t)(d->rng >> 16) - 32768;
        d->lp += (int32_t)(((int64_t)(hiss - d->lp) * brightness) >> 15);
        hiss = d->lp;
        inc = (int32_t)d->inc + (int32_t)(((int64_t)d->inc * d->sweep * p->param[1]) >> 29);
        d->phase += (uint32_t)inc;
        d->phase2 += (uint32_t)(inc + (type == D80_CB || type == D80_PC2 ? inc * 37 / 100 : inc / 2));
        tone = sine_i(d->phase);
        if (type == D80_SD || type == D80_CB || type == D80_PC2 || type == D80_CH || type == D80_OH)
            tone = (tone * 2 + sine_i(d->phase2)) / 3;
        if (type == D80_CH || type == D80_OH) tone = (tone > 0 ? 18000 : -18000);
        x = (int32_t)(((int64_t)tone * d->body * osc_mix) >> 31);
        x += (int32_t)(((int64_t)hiss * d->noise * mix) >> 31);
        if (d->age < click_samples && p->param[9]) {
            uint32_t left = click_samples - d->age;
            int32_t click = (int32_t)(((int64_t)hiss * p->param[9] * left) / ((int64_t)click_samples * 127u * 5u));
            x += click;
        }
        x = (int32_t)(((int64_t)x * level * velocity_gain) / (127u * 127u));
        if (drive) {
            int32_t y = x * (128 + drive) / 128;
            x = clamp(y, -32768, 32767);
        }
        out[i] += voice_amp(x, m, i);
        d->body -= (int32_t)(((int64_t)d->body * bd) >> 24);
        d->noise -= (int32_t)(((int64_t)d->noise * nd) >> 24);
        d->sweep -= (int32_t)(((int64_t)d->sweep * sd) >> 24);
        d->age++;
        if (d->body < 256 && d->noise < 256) d->active = 0u;
    }
}
static const preset_t DRUM80_PRESETS[] = {
    {"80s KIT", {64, 36, 100, 70, 75, 60, 80, 55}, {0, 127, 127, 127}, 0, TAG(DRUM)},
};
static const engine_t ENG_DRUM80 = {
    .name = "DRUM-80",
    .page_title = {"BODY", "ATTACK"},
    .edit = {
        {"TUNE", F_PCT, 0, 127, 64, 0, 0},
        {"SWEEP", F_PCT, 0, 127, 36, 0, 0},
        {"BODY", F_PCT, 0, 127, 100, 0, 0},
        {"DECAY", F_PCT, 0, 127, 70, 0, 0},
        {"ATTACK", F_PCT, 0, 127, 75, 0, 0},
        {"NOISE", F_PCT, 0, 127, 60, 0, 0},
        {"BRIGHT", F_PCT, 0, 127, 80, 0, 0},
        {"TAIL", F_PCT, 0, 127, 55, 0, 0},
    },
    .presets = DRUM80_PRESETS,
    .npresets = NELEM(DRUM80_PRESETS),
    .note_on = drum80_note_on,
    .amp = drum80_amp,
    .render = drum80_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = NVOICE,
    .oneshot = 1,
};
