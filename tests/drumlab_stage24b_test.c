/* Stage 24B: new electronic percussion sounds stay bounded and deterministic. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include "../firmware/src/drumlab_voice.c"

static uint64_t render(const dlab_patch_t *p, uint32_t *clipped, uint64_t *energy)
{
    dlab_voice_t v;
    int32_t out[32];
    uint64_t hash = 1469598103934665603ull;
    dlab_trigger(&v, p, 0x13579BDFu);
    for (unsigned i = 0; i < 22048u; i += 32u) {
        dlab_run_extra(&v, p, out, 32u);
        for (unsigned j = 0; j < 32u; j++) {
            int32_t s = out[j];
            *clipped += s == 32767 || s == -32768;
            *energy += (uint32_t)(s < 0 ? -s : s);
            hash = (hash ^ (uint32_t)s) * 1099511628211ull;
        }
    }
    return hash;
}

int main(void)
{
    static const char *const names[] = {"CLAP", "CLOSED HAT", "OPEN HAT", "TOM", "METAL"};
    static const uint8_t pitches[] = {65, 85, 85, 51, 93};
    unsigned bad = 0;
    for (unsigned k = 0; k < 5u; k++) {
        dlab_patch_t p = {(uint8_t)(DLAB_CLAP + k), pitches[k], 35, 70, 80, 75, 65, 45, 60, 100, 70};
        uint32_t clipped = 0;
        uint64_t energy = 0;
        uint64_t a = render(&p, &clipped, &energy);
        uint64_t energy2 = 0;
        uint64_t b = render(&p, &clipped, &energy2);
        int ok = a == b && energy == energy2 && energy > 1000000u && !clipped;
        printf("drumlab 24B: %-12s audible, deterministic, no clipping %s\n", names[k], ok ? "ok" : "FAIL");
        bad += !ok;
        if (p.kind == DLAB_CLAP || p.kind == DLAB_HAT_C || p.kind == DLAB_HAT_O) {
            p.pitch = (uint8_t)(pitches[k] > 40u ? pitches[k] - 40u : pitches[k] + 40u);
            uint64_t different_energy = 0;
            uint64_t shifted = render(&p, &clipped, &different_energy);
            ok = shifted != a && !clipped;
            printf("drumlab 24B: %-12s PITCH tunes noise spectrum %s\n", names[k], ok ? "ok" : "FAIL");
            bad += !ok;
        }
    }
    printf("DRUMLAB STAGE 24B %s (%u failures)\n", bad ? "FAILED" : "PASSED", bad);
    return bad != 0;
}
