/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM-80 Stage 1: silent registration scaffold.
 * No synthesis or per-voice state until the pad/editor/persistence integration.
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

static void drum80_note_on(track_t *t, voice_t *v)
{
    (void)t;
    /* Silent placeholder: do not leave an allocated voice sounding indefinitely. */
    v->active = 0;
    v->gate = 0;
}
static void drum80_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    (void)t; (void)v; (void)out; (void)n; (void)m;
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
    .render = drum80_render,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = NVOICE,
    .oneshot = 1,
};
