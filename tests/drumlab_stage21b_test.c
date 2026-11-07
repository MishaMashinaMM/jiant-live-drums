/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 21B: three snare characters (original / hard / extreme) for audition.
 *   build/host/drumlab_stage21b_test FILE.wav
 */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/drumlab_voice.c"

static int chk(const char *label, int yes)
{
    printf("drumlab 21B: %-62s %s\n", label, yes ? "ok" : "FAIL");
    return !yes;
}

int main(int argc, char **argv)
{
    enum { BLOCK = 32, HIT = 66150, PAUSE = 8820 };
    const dlab_patch_t original = {DLAB_HEAVY_SNARE, 34, 24, 96, 115, 95, 55, 118, 74, 127, 20};
    const uint8_t tone[] = {55, 121, 127};
    int32_t samples[BLOCK];
    FILE *wav = argc > 1 ? fopen(argv[1], "wb") : NULL;
    int bad = 0;
    if (argc > 1 && !wav) return 2;
    if (wav) wav_hdr(wav, 3u * (HIT + PAUSE));
    for (uint32_t k = 0; k < 3; k++) {
        dlab_patch_t p = original;
        dlab_voice_t v;
        uint64_t energy = 0;
        uint32_t clipped = 0, audible_tail = 0;
        p.tone = tone[k];
        dlab_trigger(&v, &p, 0x13579BDFu);
        for (uint32_t i = 0; i < HIT; i += BLOCK) {
            uint32_t n = HIT - i < BLOCK ? HIT - i : BLOCK;
            dlab_run_heavy_snare(&v, &p, samples, n);
            for (uint32_t j = 0; j < n; j++) {
                int32_t a = samples[j] < 0 ? -samples[j] : samples[j];
                energy += (uint32_t)a;
                if (i + j >= 22050u && a > 30) audible_tail++;
                clipped += samples[j] == 32767 || samples[j] == -32768;
                if (wav) wav_put(wav, samples[j], samples[j]);
            }
        }
        printf("drumlab 21B: tone %u: energy %llu, tail %u samples, clipped %u samples\n",
               (unsigned)p.tone, (unsigned long long)energy, audible_tail, clipped);
        bad += chk(k == 0 ? "original industrial snare sustained and unclipped" :
                   k == 1 ? "hard shell snare sustained and unclipped" :
                            "extreme crack snare sustained and unclipped",
                   energy > 100000u && audible_tail > 100u && !clipped);
        if (wav) for (uint32_t i = 0; i < PAUSE; i++) wav_put(wav, 0, 0);
    }
    if (wav) fclose(wav);
    printf("DRUMLAB STAGE 21B %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return bad != 0;
}
