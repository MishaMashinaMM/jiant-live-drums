// SPDX-License-Identifier: GPL-3.0-only
// Smoke-test the actual live-only browser build, without the inherited
// SAMPLE-bank assumptions (there is no playable SAMPLE engine in Drums).
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';

const wasm = readFileSync(process.argv[2] || 'build/emu/felucca.wasm');
const { instance } = await WebAssembly.instantiate(wasm, {});
const ex = instance.exports;
ex._initialize();
ex.web_nor_erase();
ex.web_boot();
assert.equal(ex.web_live_only(), 1, 'the published emulator must disable the sequencer');

function render(ms) {
  let peak = 0;
  const frames = Math.round(ms * 44.1);
  for (let done = 0; done < frames; done += 128) {
    const count = Math.min(128, frames - done);
    ex.web_render(count);
    const left = new Float32Array(ex.memory.buffer, ex.web_out_l(), count);
    const right = new Float32Array(ex.memory.buffer, ex.web_out_r(), count);
    for (let i = 0; i < count; i++)
      peak = Math.max(peak, Math.abs(left[i]), Math.abs(right[i]));
  }
  return peak;
}

render(2500); // Finish the startup splash and enter the live HOME screen.
assert.equal(ex.web_playing(), 0);
// The physical key bypasses configurable external MIDI RX routing. Host
// midi_rx_test and drum_midi_map_test separately verify per-track MIDI input.
ex.web_keys(1 << 7); // C4 on the FM1 keybed, as in the upstream emulator test.
const peak = render(300);
ex.web_keys(0);
render(300);
assert.ok(peak > 0.001, `physical key must produce audio (peak ${peak})`);

// PLAY and SEQ have live edit pages, not transport functions.
for (const button of [10, 9]) {
  ex.web_buttons(1 << button);
  render(80);
  ex.web_buttons(0);
  render(80);
  assert.equal(ex.web_playing(), 0, 'PLAY/SEQ must not start the sequencer');
}
console.log('PASS: live-only emulator boots, keybed sounds, SEQ/PLAY do not start transport');
