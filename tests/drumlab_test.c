/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 19A: host-only DSP experiment. Optional argument writes a four-hit WAV. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/drumlab_voice.c"

static const dlab_patch_t LAB_PRESETS[4] = {
    {DLAB_KICK, 16, 100, 75, 12, 10, 90, 18},  /* sub kick */
    {DLAB_KICK, 34, 127, 34, 5, 65, 120, 95}, /* industrial kick */
    {DLAB_SNARE, 43, 8, 47, 46, 92, 75, 35},  /* shell + wire */
    {DLAB_SNARE, 58, 70, 21, 12, 127, 20, 110},/* electronic burst */
};

static int check(const char *name, int yes)
{
    printf("drumlab: %-68s %s\n", name, yes ? "ok" : "FAIL");
    return !yes;
}

int main(int argc, char **argv)
{
    enum { BLOCK = 32, HIT_SAMPLES = 44100, SILENCE_SAMPLES = 4410 };
    FILE *wav = argc > 1 ? fopen(argv[1], "wb") : 0;
    dlab_voice_t v;
    int32_t out[BLOCK];
    int bad = 0;
    if (argc > 1 && !wav) return 2;
    if (wav) wav_hdr(wav, 4u * (HIT_SAMPLES + SILENCE_SAMPLES));
    for (uint32_t k = 0; k < 4u; k++) {
        static const char *const NAMES[4] = {"sub kick", "industrial kick", "resonant snare", "electronic snare"};
        char label[80];
        uint64_t early = 0, late = 0;
        int bounded = 1;
        uint32_t clipped = 0;
        dlab_trigger(&v, &LAB_PRESETS[k], 0x13579BDFu);
        for (uint32_t i = 0; i < HIT_SAMPLES; i += BLOCK) {
            uint32_t n = HIT_SAMPLES - i < BLOCK ? HIT_SAMPLES - i : BLOCK;
            dlab_run(&v, &LAB_PRESETS[k], out, n);
            for (uint32_t j = 0; j < n; j++) {
                int32_t a = out[j] < 0 ? -out[j] : out[j];
                bounded &= out[j] >= -32768 && out[j] <= 32767;
                clipped += out[j] == 32767 || out[j] == -32768;
                if (i + j < 2205u) early += (uint32_t)a;
                if (i + j >= 22050u && i + j < 24255u) late += (uint32_t)a;
                if (wav) wav_put(wav, out[j], out[j]);
            }
        }
        snprintf(label, sizeof label, "%s: bounded, no hard clipping, transient then decay", NAMES[k]);
        bad += check(label,
                     bounded && !clipped && early > late * 2u && early > 10000u);
        if (wav)
            for (uint32_t i = 0; i < SILENCE_SAMPLES; i++) wav_put(wav, 0, 0);
    }
    {
        dlab_voice_t a, b;
        int32_t x[32], y[32];
        dlab_trigger(&a, &LAB_PRESETS[2], 7);
        dlab_trigger(&b, &LAB_PRESETS[2], 7);
        dlab_run(&a, &LAB_PRESETS[2], x, 32);
        dlab_run(&b, &LAB_PRESETS[2], y, 32);
        bad += check("identical preset and seed reproduce samples exactly", !memcmp(x, y, sizeof x));
    }
    {   /* Stage 23A: the old 32-bit square*span wrapped multiple times. */
        uint32_t last_k = 0, last_s = 0, last_w = 0, last_decay = 0;
        int monotonic = 1;
        for (uint32_t value = 0; value <= 127u; value++) {
            uint32_t kick = 14000u + DLAB_CURVE_US(value, 850000u);
            uint32_t shell = 70000u + DLAB_CURVE_US(value, 900000u);
            uint32_t wires = 35000u + DLAB_CURVE_US(value, 1200000u);
            uint32_t decay = dlab_decay_long(32u, shell);
            if (value) monotonic &= kick > last_k && shell > last_s && wires > last_w && decay >= last_decay;
            last_k = kick; last_s = shell; last_w = wires; last_decay = decay;
        }
        bad += check("Stage 23A: BODY and WIRES durations/decays rise monotonically 0..127", monotonic);
        bad += check("Stage 23A: full-scale BODY and WIRES retain their intended durations",
                     last_k == 864000u && last_s == 970000u && last_w == 1235000u);
    }
    if (wav) fclose(wav);
    printf("DRUMLAB DSP TESTS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return !!bad;
}
