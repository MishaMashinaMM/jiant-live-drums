# JLD v0.3 — experimental global EQ

Branch: `feature/global-eq`, created **directly from `main`**, not from `feature/track-eq`. The existing per-track EQ experiment remains untouched.

## Controls

Open **GLO → MIX → EQ**.

- **LOW:** 50 Hz, broad shelf, −15 to +15 dB.
- **MID:** 700 Hz, Q=1 peak, −15 to +15 dB.
- **HIGH:** 10 kHz, broad shelf, −15 to +15 dB.
- **ON:** global bypass (gain slews toward zero rather than switching abruptly).

The page controls one **shared** EQ, not the selected track. All values default to flat (0 dB); ON defaults to enabled.

**Preview limitation:** EQ control values currently live in RAM and are **not stored in project files or device settings**. Do not assume they survive restart or project changes. Persistence and editor support should be implemented and tested before a release.

## Actual signal flow

Tracks (including drum stereo image and per-pad sends) are mixed first, then the global EQ processes the shared pre-effects buses, **once per audio block**:

1. Stereo dry L/R
2. Chorus, delay and reverb *input sends*
3. Stereo input to the SLICER

The global chorus/delay/reverb processors consume the EQ-filtered sends on the following block (the original pipeline already has a one-block bus handoff). The EQ is placed **before effects returns**, not on the final stereo output: wet reverb/delay/chorus tails are not filtered a second time. The SLICER's input is filtered before the SLICER processes it.

This costs up to 7 streams × 3 bands = **21 biquad paths per sample**, regardless of whether 1, 3 or 6 tracks are sounding. Completely silent streams with settled filter histories are skipped. This is a path count, not a CPU percentage or cycle benchmark.

The DSP coefficients and precision-preserving Q28 LOW feedback are reused from the working per-track EQ experiment. Global EQ works in place on shared buses and does not allocate per-track audio scratch arrays.

## Validation and risks

- Standalone frequency, fractional-feedback and seven-stream tests: `cc -O2 -I firmware/src tests/global_eq_test.c firmware/src/track_eq_dsp.c -lm -o /tmp/jld-global-eq-test && /tmp/jld-global-eq-test`
- Build hardware firmware: `bash build.sh` (`FELUCCA_NO_SEQUENCER=1` by default for JLD Drums).
- **Not yet hardware tested.** Do not describe this branch as production-ready until the hardware build, UI, emulator, existing audio tests and an FM1 multitrack stress test pass.
- The existing stem-export code collects per-track audio *before* the shared EQ. Consequently, stems are not guaranteed to sum to the EQ-filtered master mix in this preview. Fix/test this before claiming full stems support.
- A global EQ necessarily changes how effects inputs interact with overlapping tracks versus independent per-track filters; compare sound and gain staging before release.
- A freeze/reset under excessive workload was observed with **the different per-track EQ branch**. A global EQ is expected to use less CPU, but its stability and actual CPU savings require new measurements.
