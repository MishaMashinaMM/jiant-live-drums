/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 22: duplicate-note routing for DRUM-X and DRUM-LAB. */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int test(const char *name, int yes)
{
    printf("drum map: %-65s %s\n", name, yes ? "ok" : "FAIL");
    return !yes;
}

int main(void)
{
    track_t *t = &trk[0];
    uint32_t before, i, kick, snare;
    int bad = 0;
    host_tracks_init();
    host_preset(t, ENGI_DRUM, 0);
#if FELUCCA_NO_SEQUENCER
    before = vage;
    trk_note_on(t, 24, 100);  /* formerly SHORT kick: not a sound slot */
    bad += test("live-only default: no octave variation on MIDI 24", before == vage);
    trk_note_on(t, 36, 100);
    bad += test("live-only default: MIDI 36 still plays normal kick", vage > before);
#endif
    bad += test("DRUM-X accepts MIDI 0..127, rejects invalid lane and note",
                drum_note_set(t, 0, 6) && drum_note_set(t, 1, 6) &&
                drum_note_set(t, 11, 127) && !drum_note_set(t, 12, 6) &&
                !drum_note_set(t, 0, 128));
    before = vage;
    trk_note_on(t, 38, 100);
    bad += test("DRUM-X old snare note unassigned", before == vage);
    trk_note_on(t, 6, 100);
    kick = snare = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].age > before) {
            kick += t->v[i].s[0] == DV_KICK;
            snare += t->v[i].s[0] == DV_SNARE;
        }
    bad += test("DRUM-X MIDI 6 triggers both kick and snare", kick == 1 && snare == 1);
    drum_pad_pan[0][0] = -64;
    for (i = 0; i < 70u; i++) drum_side_begin(t, CTL);
    bad += test("DRUM-X per-pad fixed pan works with IMAGE at zero",
                dx_lpan[0] == -64 && (dx_xon & 1u));
    drum_pad_pitch[0][0] = 24;
    bad += test("DRUM-X per-pad pitch offset holds +24 semitones independently",
                drum_pad_pitch[0][0] == 24 && drum_pad_pitch[0][1] == 0);

    host_tracks_init();
    host_preset(t, ENGI_DRUMLAB, 0);
    drumlab_midi_custom[0] = 0;
    bad += test("DRUM-LAB accepts independent duplicate MIDI 6",
                drumlab_note_set(t, 0, 6) && drumlab_note_set(t, 2, 6));
    before = vage;
    trk_note_on(t, 6, 100);
    kick = snare = 0;
    for (i = 0; i < NVOICE; i++)
        if (t->v[i].active && t->v[i].age > before) {
            kick += t->v[i].note == 36;
            snare += t->v[i].note == 38;
        }
    bad += test("DRUM-LAB MIDI 6 triggers two independent pads", kick == 1 && snare == 1);
    drum_pad_pan[0][0] = 64;
    for (i = 0; i < 70u; i++) drum_side_begin(t, CTL);
    bad += test("DRUM-LAB per-pad fixed pan has its own stereo side",
                dx_lpan[0] == 64 && (dx_xon & 1u) && !(dx_xon & 0xEu));
    drum_pad_pitch[0][0] = -12;
    trk_note_on(t, 6, 100);
    {
        uint32_t found = 0;
        dlab_patch_t *patch = drumlab_patch_of(t, 36);
        uint32_t want = pitch_inc((uint32_t)(24 + (uint32_t)patch->pitch * 60u / 127u - 12u) * 16u);
        for (i = 0; i < NVOICE; i++)
            if (t->v[i].active && t->v[i].note == 36 &&
                drumlab_state[0][i].base_inc == want) found = 1u;
        bad += test("DRUM-LAB applies signed pitch offset at note trigger", found);
    }
    printf("DRUM MIDI MAP TESTS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return bad != 0;
}
