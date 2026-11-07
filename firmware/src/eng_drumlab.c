/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 20A: electronic percussion engine. Stage 19B's fixed-point synthesis
 * runs in Jiant's existing voice/mixer path; DRUM-X is entirely unaffected.
 * First milestone: MIDI notes 36 (kick), 38 (snare), and neighboring octave
 * keys play independent one-shots. Per-sound editing follows in Stage 20B.
 */
#include "drumlab_voice.c"

static dlab_voice_t drumlab_state[NPART][NVOICE];
/* Stage 20B work-in-progress: one independent 11-parameter patch for each
 * chromatic note C2..B2, per track. RAM-only until project serialization and
 * the per-lane editor are added. Never share DRUM-X's kit storage. */
#define DLAB_LANES 12u
static dlab_patch_t drumlab_kit[NPART][DLAB_LANES];
static uint8_t drumlab_kit_ready[NPART];

static uint32_t drumlab_lane(uint32_t note)
{
    return (note + 120u - 36u) % DLAB_LANES;
}

static uint32_t drumlab_slot(const track_t *t, const voice_t *v)
{
    return (uint32_t)(v - t->v);
}

/* MIDI 36..47: alternate bass and snare voices. All voices are independent,
 * retriggering the same note restarts its existing voice. The sound controls
 * affect the entire prototype kit until per-lane patches are introduced. */
static uint32_t drumlab_is_snare(uint32_t note)
{
    static const uint8_t SNARE[12] = {0, 1, 1, 1, 0, 0, 1, 0, 1, 0, 1, 0};
    return SNARE[drumlab_lane(note)];
}

static dlab_patch_t drumlab_default_patch(const track_t *t, uint32_t note)
{
    const int16_t *p = t->p;
    uint32_t snare = drumlab_is_snare(note);
    dlab_patch_t patch = {
        (uint8_t)(snare ? DLAB_HEAVY_SNARE : DLAB_KICK),
        (uint8_t)clamp(p[P_E0], 0, 127),
        (uint8_t)clamp(p[P_E1], 0, 127),
        (uint8_t)clamp(p[P_E2], 0, 127),
        (uint8_t)clamp(p[P_E3], 0, 127),
        (uint8_t)clamp(p[P_E4], 0, 127),
        (uint8_t)clamp(p[P_E5], 0, 127),
        (uint8_t)clamp(p[P_E6], 0, 127),
        (uint8_t)clamp(p[P_E7], 0, 127),
        110, 20,
    };
    return patch;
}

static void drumlab_kit_ensure(track_t *t)
{
    uint32_t ti = (uint32_t)(t - trk), lane;
    if (ti >= NPART || drumlab_kit_ready[ti]) return;
    for (lane = 0; lane < DLAB_LANES; lane++) {
        dlab_patch_t patch = drumlab_default_patch(t, 36u + lane);
        /* Individual starting characters; each can later be edited. */
        if (lane == 0u) { patch.pitch = 16; patch.body = 75; patch.drive = 30; }
        if (lane == 2u) { patch.pitch = 34; patch.body = 96; patch.drive = 118; patch.wire = 74; }
        if (lane == 4u) { patch.pitch = 34; patch.sweep = 127; patch.body = 34; patch.drive = 95; }
        if (lane == 7u) { patch.pitch = 23; patch.body = 120; patch.wire = 105; }
        drumlab_kit[ti][lane] = patch;
    }
    drumlab_kit_ready[ti] = 1;
}

/* Stage 20B editor/persistence integration can call this without exposing
 * the bank layout. Bounds checking also protects MIDI-invalid notes. */
static dlab_patch_t *drumlab_patch_of(track_t *t, uint32_t note)
{
    uint32_t ti = (uint32_t)(t - trk);
    if (ti >= NPART) return 0;
    drumlab_kit_ensure(t);
    return &drumlab_kit[ti][drumlab_lane(note)];
}

static int drumlab_patch_set(track_t *t, uint32_t lane, uint32_t field, uint32_t value)
{
    dlab_patch_t *p;
    uint8_t *parameters;
    if (lane >= DLAB_LANES || field >= 10u || value > 127u) return 0;
    p = drumlab_patch_of(t, 36u + lane);
    if (!p) return 0;
    parameters = &p->pitch; /* pitch..metal are ten adjacent 7-bit controls */
    parameters[field] = (uint8_t)value;
    return 1;
}

static void drumlab_note_on(track_t *t, voice_t *v)
{
    dlab_patch_t *patch = drumlab_patch_of(t, v->note);
    uint32_t slot = drumlab_slot(t, v);
    if (slot >= NVOICE || !patch) return;
    dlab_trigger(&drumlab_state[t - trk][slot], patch,
                 0x13579BDFu ^ ((uint32_t)v->note << 16) ^ (uint32_t)v->age);
}

static int32_t drumlab_amp(track_t *t, voice_t *v, int32_t adsr)
{
    uint32_t slot = drumlab_slot(t, v);
    if (slot >= NVOICE || !drumlab_state[t - trk][slot].active) {
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    if (v->stage == 4u) return adsr;
    v->env = 1 << 24;
    return 32767;
}

static void drumlab_render(track_t *t, voice_t *v, int32_t *out,
                           uint32_t n, const vmod_t *m)
{
    int32_t hit[CTL];
    dlab_patch_t *patch = drumlab_patch_of(t, v->note);
    uint32_t i, slot = drumlab_slot(t, v);
    if (slot >= NVOICE || !patch) return;
    dlab_voice_t *state = &drumlab_state[t - trk][slot];
    if (patch->kind == DLAB_HEAVY_SNARE)
        dlab_run_heavy_snare(state, patch, hit, n);
    else
        dlab_run(state, patch, hit, n);
    for (i = 0; i < n; i++)
        out[i] += voice_amp(hit[i], m, i);
}

static const preset_t DRUMLAB_PRESETS[] = {
    {"LAB KIT", {34, 24, 96, 115, 95, 55, 118, 74}, {0, 127, 127, 127}, 0, TAG(DRUM)},
    {"SUB LAB", {16, 100, 75, 12, 10, 90, 18, 50}, {0, 127, 127, 127}, 0, TAG(DRUM)},
    {"IND LAB", {34, 127, 80, 100, 100, 90, 118, 74}, {0, 127, 127, 127}, 0, TAG(DRUM)},
};

static const engine_t ENG_DRUMLAB = {
    .name = "DRUM-LAB",
    .page_title = {"STRIKE", "BODY"},
    .edit = {
        {"PITCH", F_PCT, 0, 127, 34, 0, 0},
        {"SWEEP", F_PCT, 0, 127, 24, 0, 0},
        {"BODY", F_PCT, 0, 127, 96, 0, 0},
        {"SNAP", F_PCT, 0, 127, 115, 0, 0},
        {"NOISE", F_PCT, 0, 127, 95, 0, 0},
        {"TONE", F_PCT, 0, 127, 55, 0, 0},
        {"DRIVE", F_PCT, 0, 127, 118, 0, 0},
        {"WIRES", F_PCT, 0, 127, 74, 0, 0},
    },
    .presets = DRUMLAB_PRESETS,
    .npresets = NELEM(DRUMLAB_PRESETS),
    .note_on = drumlab_note_on,
    .render = drumlab_render,
    .amp = drumlab_amp,
    .knob = {P_E0, P_E2, P_E4, P_E6},
    .poly = NVOICE,
    .oneshot = 1,
};
