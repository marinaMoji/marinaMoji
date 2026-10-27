// Copyright 2026 marinaMoji contributors.
// SPDX-License-Identifier: GPL-3.0-or-later
//
// Regenerates kage_test_data.tsv and kage_reference.tsv, the fixtures of
// kage_engine_test.cc, with the JavaScript engine the C++ port follows:
//
//   node make_reference.mjs <kageCore.json> <path to @kurgm/kage-engine/lib/esm/index.js>
//
// kageCore.json is Grognard's bundle of the GlyphWiki dump (name -> KAGE data).
// Each case below is rendered by the JavaScript engine; the test checks that the
// C++ engine produces the same polygons.

import fs from 'node:fs';
import { pathToFileURL } from 'node:url';

const [corePath, enginePath] = process.argv.slice(2);
const { Kage, Polygons } = await import(pathToFileURL(enginePath).href);
const core = JSON.parse(fs.readFileSync(corePath, 'utf8'));

const strip = (d) => d.split('$').map((r) => {
  const f = r.split(':');
  if (f[0] === '99' && f.length >= 8) f[7] = f[7].split('@')[0];
  return f.join(':');
}).join('$');

const refs = (d) => d.split('$').map((r) => r.split(':'))
  .filter((f) => f[0] === '99' && f.length >= 8).map((f) => f[7].split('@')[0]);

const chars = ['木', '巻', '中', '国', '永', '龍', '手', '扌', '言', '女', '心', '必', '辶', '鳥'];
const need = new Set();
const add = (n) => { if (need.has(n) || !(n in core)) return; need.add(n); refs(core[n]).forEach(add); };
for (const c of chars) add('u' + c.codePointAt(0).toString(16));

const cases = [];
for (const c of chars) cases.push({ name: 'u' + c.codePointAt(0).toString(16), data: null });
// Compositions as the composer lays them out (Grognard's kageCompose boxes).
cases.push({ name: 'lr_kimaki', data: '99:0:0:0:0:108:200:u6728$99:0:0:92:0:200:200:u5dfb' });
cases.push({ name: 'tb_onna_kokoro', data: '99:0:0:0:0:200:108:u5973$99:0:0:0:92:200:200:u5fc3' });
cases.push({ name: 'enclose_kuni', data: '99:0:0:0:0:200:200:u56d7$99:0:0:40:40:160:160:u7389' });
cases.push({ name: 'lower_left_shin', data: '99:0:0:0:0:200:200:u8fb6$99:0:0:60:0:200:140:u6728' });

const kage = new Kage();
for (const n of need) kage.kBuhin.push(n, strip(core[n]));

const out = {};
for (const c of cases) {
  const polygons = new Polygons();
  if (c.data) kage.makeGlyph2(polygons, strip(c.data)); else kage.makeGlyph(polygons, c.name);
  out[c.name] = { data: c.data, polygons: polygons.array.map((p) => p.array.map(({ x, y }) => [x, y])) };
}
// One line per case ("D", its data if it is not a named glyph) and per polygon
// ("P", its points as x,y;x,y;...), so the C++ test needs no JSON parser.
const lines = [];
for (const [name, { data, polygons }] of Object.entries(out)) {
  if (data) lines.push(`D\t${name}\t${strip(data)}`);
  lines.push(`C\t${name}\t${polygons.length}`);
  for (const poly of polygons) lines.push(`P\t${name}\t${poly.map(([x, y]) => `${x},${y}`).join(';')}`);
}
fs.writeFileSync('kage_reference.tsv', lines.join('\n') + '\n');
fs.writeFileSync('kage_test_data.tsv',
  [...need].sort().map((n) => `${n}\t${strip(core[n])}`).join('\n') + '\n');
console.log(`${need.size} glyphs, ${cases.length} cases`);
