/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 21A: optional mid-bass kick punch and longer body; three sounds for audition.
 *   build/host/drumlab_stage21_test build/drumlab_stage21-kicks.wav
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/drumlab_voice.c"

static int check21(const char *label, int yes)
{
    printf("drumlab 21A: %-63s %s\n", label, yes ? "ok" : "FAIL");
    return !yes;
}

int main(int argc, char **argv)
{
    enum { BLOCK = 32, HIT = 44100, PAUSE = 11025 };
    const dlab_patch_t base = {DLAB_KICK, 16, 100, 96, 32, 18, 55, 80, 60, 120, 20};
    const uint8_t metal[] = {20, 96, 127};
    int32_t audio[BLOCK], baseline[BLOCK], silent[BLOCK];
    FILE *wav = argc > 1 ? fopen(argv[1], "wb") : NULL;
    int bad = 0;
    if (argc > 1 && !wav) return 2;
    if (wav) wav_hdr(wav, 3u * (HIT + PAUSE));

    /* The original sound is unchanged across the entire inactive knob range. */
    {
        dlab_patch_t a = base, b = base;
        dlab_voice_t va, vb;
        b.metal = 63;
        dlab_trigger(&va, &a, 1234);
        dlab_trigger(&vb, &b, 1234);
        dlab_run(&va, &a, baseline, BLOCK);
        dlab_run(&vb, &b, silent, BLOCK);
        bad += check21("original kick is bit-identical for METAL 20 and 63",
                       !memcmp(baseline, silent, sizeof baseline));
    }

    for (uint32_t k = 0; k < 3; k++) {
        dlab_patch_t p = base;
        dlab_voice_t v;
        uint64_t energy = 0;
        int32_t peak = 0;
        p.metal = metal[k];
        dlab_trigger(&v, &p, 1234);
        for (uint32_t i = 0; i < HIT; i += BLOCK) {
            uint32_t n = HIT - i < BLOCK ? HIT - i : BLOCK;
            dlab_run(&v, &p, audio, n);
            for (uint32_t j = 0; j < n; j++) {
                int32_t a = audio[j] < 0 ? -audio[j] : audio[j];
                if (a > peak) peak = a;
                energy += (uint32_t)a;
                if (wav) wav_put(wav, audio[j], audio[j]);
            }
        }
        bad += check21(k == 0 ? "original kick audible, without clipping" :
                       k == 1 ? "moderate punch kick audible, without clipping" :
                                "heavy punch kick audible, without clipping",
                       energy > 100000u && peak > 100u && peak < 32767);
        if (wav) for (uint32_t i = 0; i < PAUSE; i++) wav_put(wav, 0, 0);
    }
    if (wav) fclose(wav);
    printf("DRUMLAB STAGE 21A %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return bad != 0;
}
