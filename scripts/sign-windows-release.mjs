#!/usr/bin/env node
// Authenticode-sign the Windows MSIs of a published GitHub release.
//
// Signing changes the MSI bytes, so this also rewrites the two MSI lines in
// SHA256SUMS. The in-app updater compares the downloaded MSI with GitHub's
// asset `digest`, which GitHub recomputes when an asset is replaced, so no
// other metadata needs touching. Signing itself is delegated to the
// maintainer's `sign-windows` command (Certum SimplySign via PKCS#11; see
// SECURITY.md). SimplySign Desktop must be running and logged in.
//
// Usage: node scripts/sign-windows-release.mjs <tag> [--upload] [--work-dir DIR]
//
// Without --upload nothing on GitHub is modified: everything is prepared and
// verified in <work-dir>/new for review. With --upload the changed assets are
// replaced in the order MSIs, SHA256SUMS, then re-downloaded and checked
// against the staged files, the signatures, and GitHub's per-asset digests.
// Originals are kept in <work-dir>/orig.

import { execFileSync } from 'node:child_process';
import { createHash } from 'node:crypto';
import { mkdirSync, mkdtempSync, readdirSync, readFileSync, writeFileSync } from 'node:fs';
import { tmpdir } from 'node:os';
import path from 'node:path';
import process from 'node:process';

const REPO = 'marinaMoji/marinaMoji';

const args = process.argv.slice(2);
const upload = args.includes('--upload');
const workFlag = args.indexOf('--work-dir');
const tag = args.find((arg, i) => !arg.startsWith('--') && args[i - 1] !== '--work-dir');
if (!tag || !/^v\d/.test(tag)) {
  throw new Error('Usage: sign-windows-release.mjs <tag> [--upload] [--work-dir DIR]');
}
const workDir =
  workFlag >= 0 ? path.resolve(args[workFlag + 1]) : mkdtempSync(path.join(tmpdir(), 'sign-win-'));
const origDir = path.join(workDir, 'orig');
const newDir = path.join(workDir, 'new');
mkdirSync(origDir, { recursive: true });
mkdirSync(newDir, { recursive: true });

const run = (cmd, cmdArgs, options = {}) =>
  execFileSync(cmd, cmdArgs, {
    encoding: 'utf8',
    stdio: ['ignore', 'pipe', 'inherit'],
    ...options,
  });
const gh = (...ghArgs) => run('gh', [...ghArgs, '-R', REPO]);
const sha256 = (file) => createHash('sha256').update(readFileSync(file)).digest('hex');
const log = (message) => process.stdout.write(`${message}\n`);
const bundle = path.join(process.env.HOME, '.config/sign-windows/ca/verify-bundle.pem');
const verifySignature = (file) =>
  run('osslsigncode', ['verify', '-CAfile', bundle, '-TSA-CAfile', bundle, '-in', file]);

for (const tool of ['gh', 'sign-windows', 'osslsigncode']) {
  try {
    run('which', [tool]);
  } catch {
    throw new Error(`Required tool not found on PATH: ${tool}`);
  }
}

log(`Downloading ${tag} Windows assets to ${origDir}`);
gh(
  'release',
  'download',
  tag,
  '--dir',
  origDir,
  '--clobber',
  '-p',
  '*.msi',
  '-p',
  'SHA256SUMS',
);

const msis = readdirSync(origDir)
  .filter((n) => n.endsWith('.msi'))
  .sort();
if (msis.length !== 2) throw new Error(`Expected 2 MSIs, found ${msis.length}`);
for (const name of msis) {
  if (!name.includes(tag)) throw new Error(`${name} does not match tag ${tag}`);
}

// Refuse to continue if the release does not match its own checksums, or if
// an MSI is already signed (re-signing would stack signatures).
const sums = readFileSync(path.join(origDir, 'SHA256SUMS'), 'utf8').trimEnd().split('\n');
const listed = new Map(sums.map((line) => line.split(/\s+/)).map(([hash, name]) => [name, hash]));
for (const name of msis) {
  if (listed.get(name) !== sha256(path.join(origDir, name))) {
    throw new Error(`${name} does not match SHA256SUMS; refusing to continue`);
  }
}
for (const name of msis) {
  let signed = false;
  try {
    run('osslsigncode', [
      'extract-signature',
      '-in',
      path.join(origDir, name),
      '-out',
      path.join(workDir, 'probe.sig'),
    ]);
    signed = true;
  } catch {
    // No signature present: expected.
  }
  if (signed) throw new Error(`${name} is already signed; nothing to do`);
}

for (const name of msis) {
  log(`\nSigning ${name}`);
  run(
    'sign-windows',
    ['--force', '-n', 'marinaMoji', '-o', path.join(newDir, name), path.join(origDir, name)],
    { stdio: 'inherit' },
  );
  verifySignature(path.join(newDir, name));
}

// SHA256SUMS: rewrite only the MSI lines.
writeFileSync(
  path.join(newDir, 'SHA256SUMS'),
  `${sums
    .map((line) => {
      const [, name] = line.split(/\s+/);
      return msis.includes(name) ? `${sha256(path.join(newDir, name))}  ${name}` : line;
    })
    .join('\n')}\n`,
);

log(`\nChanges prepared in ${newDir}`);
try {
  run('diff', ['-u', path.join(origDir, 'SHA256SUMS'), path.join(newDir, 'SHA256SUMS')]);
} catch (error) {
  log(error.stdout);
}
log('Staged signatures verified.');

if (!upload) {
  log(
    `\nDry run: nothing uploaded. Re-run with --upload to publish (originals are in ${origDir}).`,
  );
  process.exit(0);
}

const order = [...msis, 'SHA256SUMS'];
for (const name of order) {
  log(`Uploading ${name}`);
  gh('release', 'upload', tag, path.join(newDir, name), '--clobber');
}

const liveDir = path.join(workDir, 'live');
mkdirSync(liveDir, { recursive: true });
gh('release', 'download', tag, '--dir', liveDir, '--clobber', ...order.flatMap((n) => ['-p', n]));
const digests = new Map(
  JSON.parse(gh('release', 'view', tag, '--json', 'assets')).assets.map((a) => [a.name, a.digest]),
);
for (const name of order) {
  const live = path.join(liveDir, name);
  if (sha256(live) !== sha256(path.join(newDir, name))) {
    throw new Error(`Live ${name} differs from the staged file`);
  }
  if (digests.get(name) !== `sha256:${sha256(live)}`) {
    throw new Error(`GitHub digest for ${name} does not match the live file`);
  }
  if (name.endsWith('.msi')) verifySignature(live);
}
const liveSums = new Map(
  readFileSync(path.join(liveDir, 'SHA256SUMS'), 'utf8')
    .trimEnd()
    .split('\n')
    .map((line) => line.split(/\s+/).reverse()),
);
for (const name of msis) {
  if (liveSums.get(name) !== sha256(path.join(liveDir, name))) {
    throw new Error(`Live SHA256SUMS does not match live ${name}`);
  }
}
log(`\nUploaded and verified. Originals kept in ${origDir}.`);
