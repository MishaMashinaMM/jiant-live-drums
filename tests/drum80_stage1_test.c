/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM-80 Stage 1: silent engine and independent pad model regression tests.
 */
#define main hostsim_main
#include "hostsim.c"
#undef main

static int ck(const char *name, int yes)
{
    printf("DRUM-80: %-56s %s\n", name, yes ? "ok" : "FAIL");
    return !yes;
}
int main(void)
{
    int bad = 0;
    host_tracks_init();
    bad += ck("engine 16 appended; legacy engine IDs unchanged",
              NENGINES == 17 && ENGI_DRUM80 == 16u &&
              ENGINES[ENGI_DRUM80] == &ENG_DRUM80 &&
              ENGINES[ENGI_DRUMLAB] == &ENG_DRUMLAB &&
              ENGINES[ENGI_DRUM] == &ENG_DRUM);
    bad += ck("engine selectable; WHEEL retired",
              eng_ok(ENGI_DRUM80) && !eng_ok(ENGI_WHEEL));
    d80_ensure(0);
    d80_ensure(1);
    for (uint32_t i = 0; i < D80_PADS; ++i) {
        bad += ck("factory pad type and MIDI note",
                  d80_kit[0][i].type == i && d80_kit[0][i].note == 36u + i);
    }
    bad += ck("type change resets parameters, keeps assigned MIDI note",
              d80_note_set(0, 2, 99) && d80_param_set(0, 2, 0, 123) &&
              d80_type_set(0, 2, D80_BD) &&
              d80_kit[0][2].type == D80_BD &&
              d80_kit[0][2].note == 99u &&
              d80_kit[0][2].param[0] == d80_defaults[D80_BD][0]);
    bad += ck("duplicate types on independent pads",
              d80_type_set(0, 3, D80_BD) &&
              d80_kit[0][3].type == d80_kit[0][2].type &&
              d80_param_set(0, 3, 1, 127) &&
              d80_kit[0][2].param[1] != d80_kit[0][3].param[1]);
    bad += ck("track isolation",
              d80_kit[1][2].type == D80_CP && d80_kit[1][2].note == 38u);
    bad += ck("reject invalid pad, type, note, parameter and value",
              !d80_type_set(0, 12, 0) && !d80_type_set(0, 0, 12) &&
              !d80_note_set(0, 0, 128) && !d80_param_set(0, 0, 12, 0) &&
              !d80_param_set(0, 0, 0, 128) && !d80_type_set(NPART, 0, 0));
    host_preset(&trk[0], ENGI_DRUM80, 0);
    bad += ck("silent engine does not retain active note voices",
              (trk_note_on(&trk[0], 36, 100), !trk[0].v[0].active));
    printf("DRUM-80 STAGE 1 %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return !!bad;
}
