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
 * SHAPE: DRIVE PUNCH VEL LEVEL. Initial neutral factory settings. Sound-specific factory tuning is pending. */
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

/* Kit-wide EDIT controls P_E0..P_E7 are independent multipliers of each
 * pad's first eight parameters. 100 is neutral. MIDI CC/macros already
 * target these track parameters, so no separate MIDI or storage path is needed.
 * Work on a copy: never overwrite an individual pad's saved settings. */
static void d80_effective(const track_t *t, const d80_pad_t *source, d80_pad_t *out)
{
    *out = *source;
    for (uint32_t i = 0; i < 8u; ++i) {
        int32_t global = clamp(t->p[P_E0 + i], 0, 127);
        out->param[i] = (uint8_t)clamp((int32_t)source->param[i] * global / 100, 0, 127);
    }
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
/* Experimental original 12 kHz signed 8-bit PCM attacks (32 ms each).
 * Generated mathematically, not copied from commercial drum-machine ROMs.
 * Flash cost: 768 bytes total. 48 kHz renderer reads every fourth frame. */
static const int8_t d80_pcm_bd[384] = {
11,0,17,35,40,37,58,72,79,65,75,76,99,80,91,103,100,103,101,86,84,97,81,90,90,86,78,75,73,60,53,60,52,48,41,36,30,28,23,14,9,8,-1,-7,-12,-17,-20,-25,-27,-33,-33,-36,-42,-44,-46,-47,-51,-52,-57,-58,-57,-59,-60,-64,-63,-64,-64,-64,-65,-64,-63,-63,-63,-62,-60,-60,-59,-58,-56,-56,-53,-51,-50,-48,-46,-44,-41,-40,-38,-35,-34,-31,-29,-26,-24,-21,-19,-17,-15,-12,-10,-8,-6,-3,-1,1,3,5,7,9,11,13,14,16,18,19,21,22,24,25,26,28,29,30,31,32,33,34,34,35,36,36,37,37,37,38,38,38,38,38,38,38,38,38,38,38,37,37,37,36,36,35,34,34,33,33,32,31,30,30,29,28,27,26,25,24,23,22,21,20,19,18,17,16,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0,-1,-2,-2,-3,-4,-5,-6,-7,-7,-8,-9,-10,-10,-11,-12,-12,-13,-13,-14,-15,-15,-16,-16,-16,-17,-17,-18,-18,-18,-19,-19,-19,-19,-19,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-20,-19,-19,-19,-19,-19,-18,-18,-18,-18,-17,-17,-17,-16,-16,-16,-15,-15,-15,-14,-14,-13,-13,-12,-12,-12,-11,-11,-10,-10,-9,-9,-8,-8,-7,-7,-6,-6,-6,-5,-5,-4,-4,-3,-3,-2,-2,-1,-1,0,0,0,1,1,2,2,2,3,3,4,4,4,5,5,5,6,6,6,6,7,7,7,7,8,8,8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,10,9,9,9,9,9,9,9,9,9,8,8,8,8,8,8,7,7,7,7,7,6,6,6,6,6,5,5,5,5,4,4,4,4,4,3,3,3,3,2,2,2,2,2,1
};
static const int8_t d80_pcm_sd[384] = {
14,-42,-4,36,34,4,59,93,103,31,56,44,123,31,67,109,87,95,85,12,-1,64,-16,38,45,36,3,4,11,-47,-71,-3,-24,-25,-43,-45,-51,-31,-25,-53,-56,-25,-46,-56,-57,-56,-45,-42,-23,-36,-2,12,-13,6,15,33,22,39,14,30,59,47,50,22,44,38,39,43,24,38,49,35,34,25,37,21,14,12,9,-10,8,2,-17,-10,-18,-25,-11,-30,-36,-18,-40,-43,-33,-32,-37,-21,-25,-27,-33,-21,-19,-32,-27,-11,-19,-11,-14,-11,-7,-9,-2,6,8,15,14,15,14,13,25,26,24,26,19,29,30,23,28,29,19,25,24,19,20,19,13,13,4,6,2,5,2,-4,-7,-3,-9,-13,-10,-10,-12,-17,-16,-16,-17,-19,-22,-19,-18,-21,-18,-16,-14,-13,-17,-11,-14,-9,-9,-7,-7,-4,-4,-3,3,0,5,4,6,7,10,10,9,11,11,11,14,13,16,14,15,15,13,12,14,13,10,11,9,7,8,6,6,4,1,1,0,-2,-3,-3,-5,-6,-7,-7,-7,-9,-10,-10,-11,-10,-10,-10,-11,-10,-11,-10,-10,-9,-8,-9,-8,-7,-5,-6,-4,-3,-2,-1,-1,1,1,2,3,3,5,5,6,6,7,7,7,7,7,8,8,9,8,7,7,7,7,7,6,6,5,4,4,3,3,2,2,1,0,-1,-1,-2,-2,-3,-3,-4,-4,-5,-5,-5,-5,-6,-6,-6,-6,-6,-6,-6,-6,-5,-5,-5,-5,-4,-4,-3,-3,-2,-2,-1,-1,0,0,1,1,2,2,3,3,3,3,4,4,4,4,4,5,5,4,5,4,4,4,4,4,3,3,3,3,2,2,2,1,1,0,0,0,-1,-1,-1,-1,-2,-2,-2,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-3,-2,-2,-2,-2,-1,-1,-1,-1,0,0,0,0,1,1,1,1,2,2,2,2,2,2
};

static uint32_t d80_decay(uint32_t p, uint32_t base, uint32_t range)
{
    /* Q24 envelope multiplier, based on the requested tail length. */
    uint32_t samples = (base + p * range / 127u) * 48u; /* ms -> samples at 48 kHz */
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
    d80_pad_t effective;
    d80_effective(t, &d80_kit[ti][pad], &effective);
    const d80_pad_t *p = &effective;
    /* Per-type pitch registers: kick/toms stay low, snare/clap/metallic
     * percussion start progressively higher. TUNE spans a useful range. */
    static const uint8_t base[D80_TYPES] = {
        28, 42, 52, 52, 62, 62, 33, 42, 62, 53, 43, 50
    };
    uint32_t pitch = (uint32_t)clamp((int32_t)base[p->type] +
                                    (int32_t)p->param[0] * 30 / 127, 0, 127);
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
    d80_pad_t effective;
    d80_effective(t, &d80_kit[ti][d->pad], &effective);
    const d80_pad_t *p = &effective;
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
        /* For BD/SD, a short PCM attack supplies the non-melodic drum impact.
         * Existing oscillators/noise continue as editable tails; no RAM bank. */
        if (type == D80_BD || type == D80_SD) {
            uint32_t frame = d->age >> 2; /* 48 kHz -> 12 kHz */
            int32_t attack = 0;
            if (frame < 384u)
                attack = (type == D80_BD ? d80_pcm_bd[frame] : d80_pcm_sd[frame]) * 120;
            tone = tone / (type == D80_BD ? 5 : 3);
            x = (int32_t)(((int64_t)tone * d->body * osc_mix) >> 31);
            x += attack * (32 + p->param[2]) / 159;
        } else {
            x = (int32_t)(((int64_t)tone * d->body * osc_mix) >> 31);
        }
        x += (int32_t)(((int64_t)hiss * d->noise * mix) >> 31);
        if (d->age < click_samples && p->param[9]) {
            uint32_t left = click_samples - d->age;
            int32_t click = (hiss * (int32_t)p->param[9] / 127) * (int32_t)left / (int32_t)click_samples / 5;
            x += click;
        }
        x = (x * level / 127) * velocity_gain / 127;
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
/* Match DRUM-X's local keyboard layout: C2 (MIDI 36) on key 7 at OCT 0.
 * External MIDI note assignments remain exact and unaffected by octave controls. */
static int32_t drum80_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return clamp(29 + (int32_t)k, 0, 127);
}
static const preset_t DRUM80_PRESETS[] = {
    {"80s KIT", {100, 100, 100, 100, 100, 100, 100, 100}, {0, 127, 127, 127}, 0, TAG(DRUM)},
};
static const engine_t ENG_DRUM80 = {
    .name = "DRUM-80",
    .page_title = {"BODY", "ATTACK"},
    .edit = {
        {"TUNE", F_PCT, 0, 127, 100, 0, 0},
        {"SWEEP", F_PCT, 0, 127, 100, 0, 0},
        {"BODY", F_PCT, 0, 127, 100, 0, 0},
        {"DECAY", F_PCT, 0, 127, 100, 0, 0},
        {"ATTACK", F_PCT, 0, 127, 100, 0, 0},
        {"NOISE", F_PCT, 0, 127, 100, 0, 0},
        {"BRIGHT", F_PCT, 0, 127, 100, 0, 0},
        {"TAIL", F_PCT, 0, 127, 100, 0, 0},
    },
    .presets = DRUM80_PRESETS,
    .npresets = NELEM(DRUM80_PRESETS),
    .note_on = drum80_note_on,
    .amp = drum80_amp,
    .render = drum80_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = NVOICE,
    .oneshot = 1,
    .keys = drum80_keys,
};
