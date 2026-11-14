# DRUM-80 — frozen design and Stage 1 plan

Status: design approved; implementation NOT started. Branch: feature/drum80-stage1 (based on feature/remove-wheel). No changes to main.

## Purpose
A deterministic, tight, hybrid 1980s synthpop/new-wave drum synthesizer with analog-style tonal synthesis and LinnDrum-inspired transient precision, **without samples**. A third independent engine alongside DRUM-X and DRUM-LAB.

## Identity and compatibility
- Engine ID 16 (append-only); WHEEL ID 7 remains retired/reserved. Do not renumber existing engines.
- Twelve independent pads; each pad can select any of twelve instrument types, including duplicates.
- Default order: BD, SD, CP, RS, CH, OH, LT, HT, CB, CL, PC1, PC2.
- Pad note assignment independent of TYPE. On TYPE change, load that type's factory parameter defaults while preserving MIDI note and routing.
- Reuse existing eight-voice pool, MIDI track routing, global EQ, effects, and CC architecture. Existing engines unchanged.
- Old projects and editor protocol must remain compatible. New engine needs explicit project serialization and version/migration review.

## Sound synthesis (Stage 2 onward)
- BD: sine, falling pitch envelope, click.
- SD: two tonal oscillators, filtered noise, click.
- CP: band-pass noise, three/four timed bursts and tail.
- RS: resonant oscillator, click.
- CH/OH: two inharmonic pulse oscillators plus high-pass noise; CH chokes OH, not other pads.
- LT/HT: one pitched oscillator and falling pitch envelope.
- CB: two inharmonic pulse oscillators.
- CL: one short tonal oscillator plus click.
- PC1: one oscillator plus noise.
- PC2: two oscillators plus noise.
- Only instantiate second oscillator where needed. Independent tone/noise/transient envelopes; sample-accurate timing.
- No samples, oversampling, convolution, lookahead, per-sample transcendental operations or compressor.

## Per-pad edit UI
Three pages, four controls each; all displayed 0..127:
- BODY: TUNE, SWEEP, BODY, DECAY
- ATTACK: ATTACK, NOISE, BRIGHT, TAIL
- SHAPE: DRIVE, PUNCH, VEL, LEVEL
TYPE belongs to pad selection, not one of the twelve controls. Inapplicable controls display inactive rather than silently changing meaning.

Proposed mappings for later tuning: TUNE logarithmic and type-specific; SWEEP 0..48 semitones; DECAY 10..2000 ms logarithmic; TAIL 5..2000 ms logarithmic; BRIGHT ~500 Hz..16 kHz; LEVEL and per-type loudness calibrated.

PUNCH is transient emphasis, **not compression**: a bounded short-lived attack emphasis (~5..30 ms type-specific) with gain compensation. Zero leaves native envelope unchanged.

## Stage 1 — framework only, NO DSP
1. Register engine 16 in append-only table, show in selection and editor; preserve ID 7 retirement.
2. Implement independent 12-pad TYPE and twelve parameters with default values and reset-on-TYPE-change behavior.
3. Reuse the existing drum navigation pattern, without changing DRUM-X or DRUM-LAB.
4. Integrate pad MIDI mapping and engine-specific project serialization (including defaults and old-project migration).
5. Provide silent note-on/render placeholders: no synthesis yet and no additional per-sample CPU.
6. Add tests: engine ordering and ID preservation, twelve pad/type independence and duplicates, note mapping retained after type change, parameter round trips, save/load, legacy projects, and editor compatibility.
7. Run host tests, emulator build, firmware build; do not merge until validated. Physical FM1 flash in later stages.

## Initial repository findings
- firmware/src/core.h currently declares NENGINES 16 and NENG_SHOWN based on five retired engines. With WHEEL retired, the count must be adjusted **and tested** when adding engine 16.
- firmware/src/engines.c has append-only ENGINES and ENGINE_ORDER arrays, eng_ok and eng_live fallback.
- firmware/src/eng_drumlab.c provides twelve-pad and TYPE patterns, but copying it wholesale is not appropriate.
- firmware/src/project.c explicitly handles ENGI_DRUMLAB in numerous serialization paths; DRUM-80 requires deliberate integration, not just registration.
- The feature/remove-wheel branch has not yet passed full build/regression verification.

## Explicitly deferred
- New sound algorithms, audio tuning, CPU benchmarking, firmware flashing, release/merge.
