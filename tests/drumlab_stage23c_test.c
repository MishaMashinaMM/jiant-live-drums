/* Stage 23C: every heavy-snare sound control must affect the new body. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/drumlab_voice.c"

static uint64_t signature(const dlab_patch_t *p, uint32_t *clipped)
{
    dlab_voice_t v;
    int32_t out[32];
    uint64_t sig = 0;
    dlab_trigger(&v, p, 0x13579BDFu);
    for (unsigned i = 0; i < 8192; i += 32) {
        dlab_run_heavy_snare(&v, p, out, 32);
        for (unsigned j = 0; j < 32; ++j) {
            *clipped += out[j] == 32767 || out[j] == -32768;
            sig = sig * 1099511628211ull + (uint32_t)out[j];
        }
    }
    return sig;
}

int main(void)
{
    dlab_patch_t p = {DLAB_HEAVY_SNARE, 49, 30, 65, 94, 46, 52, 30, 25, 107, 20};
    uint32_t clipped = 0;
    uint64_t baseline = signature(&p, &clipped);
    const char *names[] = {"PITCH", "SWEEP", "BODY", "SNAP", "NOISE",
                           "TONE", "DRIVE", "WIRES", "WEIGHT", "METAL"};
    uint8_t *controls[] = {&p.pitch, &p.sweep, &p.body, &p.snap, &p.noise,
                           &p.tone, &p.drive, &p.wire, &p.weight, &p.metal};
    unsigned bad = 0;
    for (unsigned k = 0; k < 10; ++k) {
        uint8_t saved = *controls[k];
        *controls[k] = saved > 63 ? 20 : 110;
        uint64_t changed = signature(&p, &clipped);
        printf("drumlab 23C: %-8s changes rendered snare %s\n",
               names[k], changed != baseline ? "ok" : "FAIL");
        bad += changed == baseline;
        *controls[k] = saved;
    }
    printf("drumlab 23C: no hard clipping %s (%u samples)\n", clipped ? "FAIL" : "ok", clipped);
    bad += clipped != 0;
    printf("DRUMLAB STAGE 23C %s (%u failures)\n", bad ? "FAILED" : "PASSED", bad);
    return bad != 0;
}
