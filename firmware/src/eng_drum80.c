/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM-80 Stage 1: silent registration scaffold.
 * No synthesis or per-voice state until the pad/editor/persistence integration.
 */
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
