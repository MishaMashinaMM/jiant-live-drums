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
    printf("DRUM MIDI MAP TESTS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return bad != 0;
}
