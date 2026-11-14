# JLD v0.3 — global EQ technical notes

Originally developed on `feature/global-eq` directly from `main`, independently of the experimental per-track EQ branch.

## Controls

Open **GLO → MIX → EQ**.

- **LOW:** 50 Hz, broad shelf, −15 to +15 dB.
- **MID:** 700 Hz, Q=1 peak, −15 to +15 dB.
- **HIGH:** 10 kHz, broad shelf, −15 to +15 dB.
- **ON:** global bypass (gain slews toward zero rather than switching abruptly).

The page controls one **shared** EQ, not the selected track. All values default to flat (0 dB); ON defaults to enabled.

**Project persistence:** LOW/MID/HIGH/ON are now written to an optional six-byte `EQ` block in the existing packed project format. Loading a project without that block resets the EQ to flat (0 dB) and ON, preserving legacy sound. Project-format regression tests cover round-trip and absent/invalid blocks. **EQ is not a device-wide setting**: values are saved with projects, not independently across power cycles; unsaved changes are lost. The web editor does not yet expose these controls.

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
- Additional 4,096-block seven-stream stress test under ASan/UBSan: `tests/global_eq_stress_test.c`, automated in GitHub Actions.
- Project save/load format regression: `tests/project_test.c`, automated by the live test workflow.
- Build hardware firmware: `bash build.sh` (`FELUCCA_NO_SEQUENCER=1` by default for JLD Drums).
- **Physical FM1 flash confirmed November 14, 2026.** The owner reported that the Global EQ build flashed successfully and tested functions worked well. Extended six-track hardware CPU-load measurements and long-duration stability testing remain outstanding.
- The existing stem-export code collects per-track audio *before* the shared EQ. Consequently, stems are not guaranteed to sum to the EQ-filtered master mix in this preview. Fix/test this before claiming full stems support.
- A global EQ necessarily changes how effects inputs interact with overlapping tracks versus independent per-track filters; compare sound and gain staging before release.
- A freeze/reset under excessive workload was observed with **the different per-track EQ branch**. A global EQ is expected to use less CPU, but its stability and actual CPU savings require new measurements.
