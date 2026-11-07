/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 20A integration checks: registered engine, real voice path and MIDI. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/favorites.c"

static int check_lab(const char *name, int ok)
{
    printf("drumlab engine: %-62s %s\n", name, ok ? "ok" : "FAIL");
    return !ok;
}

static uint64_t render_energy(track_t *t, uint32_t blocks)
{
    uint64_t sum = 0;
    int32_t out[CTL];
    for (uint32_t k = 0; k < blocks; k++) {
        track_render(t, out, CTL);
        for (uint32_t i = 0; i < CTL; i++)
            sum += (uint32_t)(out[i] < 0 ? -(int64_t)out[i] : out[i]);
    }
    return sum;
}

int main(void)
{
    int bad = 0;
    track_t *t = &trk[0];
    host_tracks_init();
    bad += check_lab("new append-only engine ID 15", ENGI_DRUMLAB == 15u &&
                     ENGINES[ENGI_DRUMLAB] == &ENG_DRUMLAB);
    bad += check_lab("existing DRUM-X ID 10 preserved", ENGINES[ENGI_DRUM] == &ENG_DRUM);
    bad += check_lab("DRUM-LAB selectable and separate", eng_ok(ENGI_DRUMLAB) &&
                     ENG_DRUMLAB.oneshot && ENG_DRUMLAB.npresets >= 3u);
    {
        uint8_t metadata[32];
        memcpy(metadata, favorites.factory[15], sizeof metadata);
        bad += check_lab("legacy settings slot 15 protected from favorites",
                         !favorite_set(ENGI_DRUMLAB, 0, 1) &&
                         !favorite_has(ENGI_DRUMLAB, 0) &&
                         !memcmp(metadata, favorites.factory[15], sizeof metadata));
    }
    host_preset(t, ENGI_DRUMLAB, 0);
    t->p[P_ATK] = 0;
    t->p[P_SUS] = 127;
    trk_note_on(t, 36, 110);
    bad += check_lab("MIDI 36: kick sounds through track renderer", render_energy(t, 50) > 100000u);
    trk_note_on(t, 38, 110);
    bad += check_lab("MIDI 38: heavy snare sounds through track renderer", render_energy(t, 50) > 100000u);
    bad += check_lab("two independent one-shot voices", t->v[0].active && t->v[1].active);
    printf("DRUMLAB ENGINE TESTS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return !!bad;
}
