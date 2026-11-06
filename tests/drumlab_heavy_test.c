/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 19B: three heavier, independently sustained synthesized snares. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/drumlab_voice.c"

static const dlab_patch_t HEAVY[3] = {
    /* kind, pitch, sweep, body, snap, noise, tone, drive, wire, weight, metal */
    {DLAB_HEAVY_SNARE, 34, 24, 96, 115, 95, 55, 118, 74, 127, 20},
    {DLAB_HEAVY_SNARE, 23, 8, 120, 65, 80, 20, 80, 105, 120, 3},
    {DLAB_HEAVY_SNARE, 46, 62, 65, 127, 110, 113, 125, 118, 90, 127},
};

static int check(const char *s, int yes)
{
    printf("drumlab heavy: %-62s %s\n", s, yes ? "ok" : "FAIL");
    return !yes;
}

int main(int argc, char **argv)
{
    enum { BLOCK = 32, HIT = 88200, PAUSE = 8820 };
    static const char *const names[] = {"industrial", "808-style", "experimental metal"};
    FILE *wav = argc > 1 ? fopen(argv[1], "wb") : NULL;
    int bad = 0;
    int32_t out[BLOCK];
    if (argc > 1 && !wav) return 2;
    if (wav) wav_hdr(wav, 3u * (HIT + PAUSE));
    for (uint32_t k = 0; k < 3; k++) {
        dlab_voice_t v;
        uint64_t attack = 0, sustain = 0, very_late = 0;
        uint32_t clipped = 0;
        char label[120];
        dlab_trigger(&v, &HEAVY[k], 0x13579BDFu + k);
        for (uint32_t i = 0; i < HIT; i += BLOCK) {
            uint32_t n = HIT - i < BLOCK ? HIT - i : BLOCK;
            dlab_run_heavy_snare(&v, &HEAVY[k], out, n);
            for (uint32_t j = 0; j < n; j++) {
                uint32_t t = i + j;
                int32_t a = out[j] < 0 ? -out[j] : out[j];
                clipped += out[j] == 32767 || out[j] == -32768;
                if (t < 2205u) attack += (uint32_t)a;
                if (t >= 13230u && t < 15435u) sustain += (uint32_t)a;
                if (t >= 66150u && t < 68355u) very_late += (uint32_t)a;
                if (wav) wav_put(wav, out[j], out[j]);
            }
        }
        snprintf(label, sizeof label, "%s: sustained at 300ms, decays by 1.5s, no clipping", names[k]);
        bad += check(label, attack > 50000 && sustain > 100000 &&
                     sustain > very_late && !clipped);
        if (wav) for (uint32_t i = 0; i < PAUSE; i++) wav_put(wav, 0, 0);
    }
    {
        dlab_voice_t a, b;
        int32_t x[32], y[32];
        dlab_trigger(&a, &HEAVY[1], 7);
        dlab_trigger(&b, &HEAVY[1], 7);
        dlab_run_heavy_snare(&a, &HEAVY[1], x, 32);
        dlab_run_heavy_snare(&b, &HEAVY[1], y, 32);
        bad += check("repeatable fixed-point rendering", !memcmp(x, y, sizeof x));
    }
    if (wav) fclose(wav);
    printf("DRUMLAB HEAVY TESTS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return !!bad;
}
