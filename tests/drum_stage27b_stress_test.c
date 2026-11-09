/* SPDX-License-Identifier: GPL-3.0-only
 * Stage 27B: bounded, repeatable live-drum host stress measurements.
 * Host speed is diagnostic only; it cannot predict FM1 core headroom. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define RUN_BLOCKS 690u /* 690 * CTL samples, roughly half a second */
static const uint8_t X_NOTE[12] = {36, 38, 39, 42, 46, 45, 37, 56, 41, 40, 43, 44};
static int bad;
static void check_stress(const char *label, int pass)
{
    printf("stage27b: %-60s %s\n", label, pass ? "ok" : "FAIL");
    bad += !pass;
}
/* mode: 0 LAB only, 1 X only, 2 mixed */
static void fresh_stress(uint32_t mode)
{
    memset(trk, 0, sizeof trk);
    memset(&song, 0, sizeof song);
    host_tracks_init();
    host_preset(&trk[0], mode ? ENGI_DRUM : ENGI_DRUMLAB, 0);
    if (mode == 2u) host_preset(&trk[1], ENGI_DRUMLAB, 0);
    drum_midi_custom[0] = 0;
    drumlab_midi_custom[mode == 2u ? 1u : 0u] = 0;
    drumlab_global_reset(mode == 2u ? 1u : 0u);
    load_tight = 0;
    gov_cap = NVOICE;
    sys_load = LOAD_FULL;
    for (uint32_t ti = 0; ti < 2u; ti++)
        for (uint32_t lane = 0; lane < 12u; lane++) {
            drum_retrigger[ti][lane] = 0;
            drum_perf_choke[ti][lane] = 0;
        }
    if (!mode) {
        drumlab_type_set(&trk[0], 10u, DLAB_CRASH);
        drumlab_patch_set(&trk[0], 10u, 2u, 127u);
    } else if (mode == 2u) {
        drumlab_type_set(&trk[1], 10u, DLAB_CRASH);
        drumlab_patch_set(&trk[1], 10u, 2u, 127u);
    }
}
static void benchmark(const char *label, uint32_t mode, uint32_t repeat)
{
    int32_t samples[CTL * 2u];
    uint32_t peak_busy = 0, peak_track[2] = {0, 0}, active_frames = 0, nonzero = 0, attempts = 0;
    uint64_t start, elapsed;
    fresh_stress(mode);
    start = now_ns();
    for (uint32_t b = 0; b < RUN_BLOCKS; b++) {
        /* Twelve pads are requested in both engines, exceeding the shared
         * eight-voice cap intentionally. Rapid mode adds repeated hits. */
        if (b == 0 || (repeat && !(b % 8u))) {
            for (uint32_t lane = 0; lane < 12u; lane++) {
                trk_note_on(&trk[0], mode ? X_NOTE[lane] : 36u + lane, 100u);
                attempts++;
                if (mode == 2u) { trk_note_on(&trk[1], 36u + lane, 100u); attempts++; }
            }
        }
        mix_block(samples, CTL);
        for (uint32_t s = 0; s < CTL * 2u; s++) nonzero += samples[s] != 0;
        uint32_t busy = voices_busy();
        if (busy > peak_busy) peak_busy = busy;
        for (uint32_t ti = 0; ti < (mode == 2u ? 2u : 1u); ti++) {
            uint32_t count = 0;
            for (uint32_t vi = 0; vi < NVOICE; vi++) count += trk[ti].v[vi].active != 0;
            if (count > peak_track[ti]) peak_track[ti] = count;
        }
        active_frames += busy > 0;
    }
    elapsed = now_ns() - start;
    printf("stage27b: %s: %u hits, peak %u/%u voices (tracks %u/%u), rendered %u frames, host %.1f ms (%.2fx real time)\n",
           label, attempts, peak_busy, voice_cap(), peak_track[0], peak_track[1], RUN_BLOCKS * CTL,
           (double)elapsed / 1000000.0,
           (double)elapsed * (double)FS / (1000000000.0 * RUN_BLOCKS * CTL));
    check_stress(label, attempts >= 12u && peak_busy > 0 && peak_busy <= voice_cap() &&
                 peak_track[0] > 0 && (mode != 2u || peak_track[1] > 0) &&
                 active_frames > 0 && nonzero > 0 && elapsed > 0);
}
int main(void)
{
    benchmark("DRUM-LAB 12-pad burst", 0u, 0u);
    benchmark("DRUM-LAB repeated 12-pad bursts", 0u, 1u);
    benchmark("DRUM-X 12-pad burst", 1u, 0u);
    benchmark("DRUM-X repeated 12-pad bursts", 1u, 1u);
    benchmark("DRUM-X plus DRUM-LAB 24-pad burst", 2u, 0u);
    benchmark("DRUM-X plus DRUM-LAB repeated bursts", 2u, 1u);
    printf("STAGE 27B STRESS %s (%d failures)\n", bad ? "FAILED" : "PASSED", bad);
    return bad != 0;
}
