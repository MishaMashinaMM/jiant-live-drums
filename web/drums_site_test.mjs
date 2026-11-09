// SPDX-License-Identifier: GPL-3.0-only
// Jiant Live Drums site parity and publication guardrails (no firmware flashing).
import fs from 'node:fs';
import assert from 'node:assert/strict';

const read = p => fs.readFileSync(p, 'utf8');
const home = read('web/drums-home.html');
const installer = read('web/index_pkg.html');
const editor = read('web/editor.html');
const emulator = read('web/emu/index.html');
const workflow = read('.github/workflows/emulator.yml');
const readme = read('README.md');

for (const [name, text] of [['home', home], ['installer', installer],
  ['editor', editor], ['emulator', emulator], ['README', readme]]) {
  assert.match(text, /JIANT LIVE DRUMS/i, `${name} must name the Drums fork`);
}
for (const href of ['emu/', 'webapp/installer/', 'webapp/editor/', 'manual/']) {
  assert.ok(home.includes(`href="${href}"`), `homepage must link to ${href}`);
}
assert.match(home, /physical FM1/i, 'site must disclose hardware-test status');
assert.match(installer, /JIANT LIVE DRUMS v0\.2: DRUM-X and DRUM-LAB/, 'installer must explain both engines');
assert.match(editor, /id="backup-export"/, 'editor must retain complete backup export');
assert.match(editor, /id="backup-import"/, 'editor must retain complete backup restore');
assert.doesNotMatch(editor, /<button role="tab" data-tab="sequencer"/, 'Drums editor must not offer disabled sequencer');
assert.doesNotMatch(editor, /const TABS = \[[^\n]*"sequencer"/, 'disabled sequencer must not be navigable');
assert.doesNotMatch(emulator, /id="sample-bank-controls"|id="sample-play"|id="sample-bank"/, 'Drums emulator must not offer sample kits');
assert.doesNotMatch(emulator, /withSampleBank|encodeSampleBank|sampleKit:|factory-voices\.json/, 'Drums emulator must not load sample kits');
assert.match(workflow, /FELUCCA_NO_SEQUENCER=1 python3 tools\/build\.py/, 'site must package live-only firmware');
assert.match(workflow, /FELUCCA_NO_SEQUENCER=1 sh web\/emu\/build\.sh/, 'site must emulate live-only firmware');
assert.doesNotMatch(workflow, /\bweb\/(?:STABLE|PREVIEW|CLASSIC)\b/, 'site must not publish pinned upstream firmware');
assert.match(readme, /562,972 bytes/, 'README must document tested firmware size');
console.log('PASS: Jiant Live Drums site pages, links, metadata and build source');
