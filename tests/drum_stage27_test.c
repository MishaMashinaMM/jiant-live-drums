/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 27: real-time multi-pad and retrigger smoke tests for live kits.
 * Uses the production voice allocator and mixer, not isolated DSP patches. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int failures;
static void expect(const char *name, int ok)
{
    printf("stage27: %-65s %s\n", name, ok ? "ok" : "FAIL");
    failures += !ok;
}
static uint32_t active_note(track_t *t, uint32_t note)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < NVOICE; i++)
        count += t->v[i].active && t->v[i].note == note;
    return count;
}
static uint32_t active_total(track_t *t)
{
    uint32_t count = 0;
    for (uint32_t i = 0; i < NVOICE; i++) count += t->v[i].active != 0;
    return count;
}
static void fresh_kit(uint32_t engine)
{
    host_tracks_init();
    host_preset(&trk[0], engine, 0);
    for (uint32_t i = 0; i < NVOICE; i++) trk[0].v[i].active = 0;
    for (uint32_t i = 0; i < 12u; i++) {
        drum_retrigger[0][i] = 0;
        drum_perf_choke[0][i] = 0;
    }
    if (engine == ENGI_DRUMLAB) {
        drumlab_midi_custom[0] = 0;
        drumlab_global_reset(0);
    } else drum_midi_custom[0] = 0;
}
static void exercise(uint32_t engine, uint32_t a, uint32_t b, uint32_t c)
{
    track_t *t = &trk[0];
    int32_t out[2u * CTL];
    uint32_t nonzero = 0;
    fresh_kit(engine);
    trk_note_on(t, a, 110);
    trk_note_on(t, b, 105);
    trk_note_on(t, c, 100);
    expect(engine == ENGI_DRUM ? "DRUM-X: simultaneous three different pads" :
           "DRUM-LAB: simultaneous three different pads",
           active_note(t, a) == 1 && active_note(t, b) == 1 && active_note(t, c) == 1);
    for (uint32_t i = 0; i < 16u; i++) {
        mix_block(out, CTL);
        for (uint32_t s = 0; s < 2u * CTL; s++) nonzero += out[s] != 0;
    }
    expect(engine == ENGI_DRUM ? "DRUM-X: three-pad mix processes audio" :
           "DRUM-LAB: three-pad mix processes audio", active_total(t) > 0 && nonzero > 0);
    trk_note_on(t, a, 110);
    trk_note_on(t, a, 110);
    expect(engine == ENGI_DRUM ? "DRUM-X: rapid OVERLAP preserves multiple tails" :
           "DRUM-LAB: rapid OVERLAP preserves multiple tails", active_note(t, a) == 3);
    drum_retrigger[0][0] = 1;
    trk_note_on(t, a, 110);
    expect(engine == ENGI_DRUM ? "DRUM-X: RESTART cuts earlier same-pad tails only" :
           "DRUM-LAB: RESTART cuts earlier same-pad tails only",
           active_note(t, a) == 1 && active_note(t, b) == 1 && active_note(t, c) == 1);
    for (uint32_t i = 0; i < 32u; i++) mix_block(out, CTL);
    expect(engine == ENGI_DRUM ? "DRUM-X: retrigger and mix remain stable" :
           "DRUM-LAB: retrigger and mix remain stable", active_total(t) > 0);
}
int main(void)
{
    exercise(ENGI_DRUM, 36, 38, 46);
    exercise(ENGI_DRUMLAB, 36, 38, 46);
    {
        track_t *t = &trk[0];
        int32_t out[2u * CTL];
        fresh_kit(ENGI_DRUMLAB);
        drumlab_type_set(t, 10u, DLAB_CRASH); /* note 46: long cymbal */
        drumlab_patch_set(t, 10u, 2u, 127u); /* BODY: longest tail */
        trk_note_on(t, 46u, 120u);
        for (uint32_t i = 0; i < 32u; i++) mix_block(out, CTL);
        trk_note_on(t, 36u, 110u);
        trk_note_on(t, 38u, 110u);
        expect("DRUM-LAB: long crash remains active after kick and snare",
               active_note(t, 46u) && active_note(t, 36u) && active_note(t, 38u));
    }
    printf("STAGE 27 LIVE PERFORMANCE %s (%d failures)\n",
           failures ? "FAILED" : "PASSED", failures);
    return failures != 0;
}
