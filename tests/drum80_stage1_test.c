/* SPDX-License-Identifier: GPL-3.0-only
 * DRUM-80 engine registration, independent pads, MIDI routing and choke regression tests.
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
    bad += ck("MIDI note lookup follows custom pad assignment",
              d80_pad_for_note(0, 99) == 2 &&
              d80_pad_for_note(0, 38) == -1 &&
              d80_pad_for_note(1, 38) == 2 &&
              d80_pad_for_note(0, 128) == -1);
    bad += ck("duplicate MIDI assignments select first matching pad",
              d80_note_set(0, 3, 99) && d80_pad_for_note(0, 99) == 2);
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
    bad += ck("DRUM-80 allocates an active synthesis voice",
              (trk_note_on(&trk[0], 36, 100), trk[0].v[0].active && d80_voice[0][0].active));
    d80_voice[0][1].active = 1u;
    d80_voice[0][1].pad = D80_OH;
    trk[0].v[2].note = 40u; /* factory closed hat */
    drum80_note_on(&trk[0], &trk[0].v[2]);
    bad += ck("closed hat chokes factory open hat on same track",
              !d80_voice[0][1].active && d80_voice[0][2].active);
    d80_voice[0][1].active = 1u;
    d80_voice[0][1].pad = D80_OH;
    trk[1].v[2].note = 40u;
    drum80_note_on(&trk[1], &trk[1].v[2]);
    bad += ck("closed hat does not choke another track",
              d80_voice[0][1].active);
    {   /* Exercise the actual fixed-point render path for all 12 factory
         * sounds; no sample data or UI state is needed. */
        vmod_t m = {0};
        m.amp0 = m.amp1 = 32767;
        uint32_t audible = 0, bounded = 1;
        for (uint32_t pad = 0; pad < D80_PADS; ++pad) {
            voice_t *v = &trk[0].v[0];
            int32_t out[CTL] = {0};
            d80_type_set(0, pad, pad);
            d80_note_set(0, pad, 36u + pad);
            v->note = (uint8_t)(36u + pad);
            v->vel = 127u;
            drum80_note_on(&trk[0], v);
            drum80_render(&trk[0], v, out, CTL, &m);
            uint32_t signal = 0;
            for (uint32_t i = 0; i < CTL; ++i) {
                signal |= out[i] != 0;
                bounded &= out[i] > -1048576 && out[i] < 1048576;
            }
            audible += signal;
        }
        bad += ck("all twelve types produce audio samples", audible == D80_PADS);
        bad += ck("initial output remains within bounded headroom", bounded);
    }
    printf("DRUM-80 STAGE 1 %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return !!bad;
}
