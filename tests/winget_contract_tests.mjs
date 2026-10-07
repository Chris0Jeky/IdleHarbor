import assert from 'node:assert/strict';
import { mkdtempSync, cpSync, readFileSync, writeFileSync, rmSync } from 'node:fs';
import { tmpdir } from 'node:os';
import { join } from 'node:path';
import test from 'node:test';
import { validateWingetManifests, verifyArchive } from '../packaging/check-winget.mjs';

const source = new URL('../packaging/winget/', import.meta.url);
function fixture(action) {
  const root = mkdtempSync(join(tmpdir(), 'idleharbor-winget-'));
  try { cpSync(source, root, { recursive: true }); return action(root); }
  finally { rmSync(root, { recursive: true, force: true }); }
}
const prefix = 'Chris0Jeky.IdleHarbor';
function change(root, file, from, to) {
  const path = join(root, '0.2.0', `${prefix}${file}.yaml`);
  const original = readFileSync(path, 'utf8');
  assert.ok(original.includes(from), `Mutation source missing: ${from}`);
  writeFileSync(path, original.replace(from, to));
}
test('both tracked versions contain exactly their two architecture-specific archives', () => {
  const records = validateWingetManifests(source);
  assert.equal(records.length, 4);
  assert.deepEqual(records.map(r => `${r.version}/${r.architecture}`).sort(), ['0.1.0/arm64','0.1.0/x64','0.2.0/arm64','0.2.0/x64']);
});
const mutations = [
  ['version identity', '', 'PackageIdentifier: Chris0Jeky.IdleHarbor', 'PackageIdentifier: Other.App'],
  ['installer version', '.installer', 'PackageVersion: 0.2.0', 'PackageVersion: 0.1.0'],
  ['locale version', '.locale.en-US', 'PackageVersion: 0.2.0', 'PackageVersion: 0.1.0'],
  ['duplicate identity', '', 'PackageVersion: 0.2.0', 'PackageVersion: 0.2.0\nPackageVersion: 0.1.0'],
  ['schema mismatch', '', 'ManifestVersion: 1.12.0', 'ManifestVersion: 1.11.0'],
  ['default locale', '', 'DefaultLocale: en-US', 'DefaultLocale: de-DE'],
  ['wrong manifest kind', '.installer', 'ManifestType: installer', 'ManifestType: singleton'],
  ['nonportable type', '.installer', 'NestedInstallerType: portable', 'NestedInstallerType: exe'],
  ['wrong architecture', '.installer', 'Architecture: x64', 'Architecture: x86'],
  ['duplicate architecture', '.installer', 'Architecture: arm64', 'Architecture: x64'],
  ['archive version', '.installer', '/v0.2.0/IdleHarbor-0.2.0', '/v0.1.0/IdleHarbor-0.1.0'],
  ['archive architecture', '.installer', 'windows-x64-portable.zip', 'windows-arm64-portable.zip'],
  ['foreign repository', '.installer', 'github.com/Chris0Jeky/IdleHarbor/releases', 'github.com/other/repo/releases'],
  ['bad digest', '.installer', '18B4A517EF767C005A6D01BA53C13A79FF50E4A956BD7C43F3635B17B80C75F7', 'not-a-sha256'],
  ['path traversal', '.installer', 'RelativeFilePath: IdleHarbor-0.2.0-windows-x64-portable\\IdleHarbor.exe', 'RelativeFilePath: ..\\IdleHarbor.exe'],
  ['wrong command alias', '.installer', 'PortableCommandAlias: idleharbor', 'PortableCommandAlias: other'],
  ['wrong license pin', '.locale.en-US', '/blob/v0.2.0/LICENSE', '/blob/main/LICENSE'],
  ['wrong release notes', '.locale.en-US', '/tag/v0.2.0', '/tag/v0.1.0'],
  ['missing nested files', '.installer', '    NestedInstallerFiles:\n', ''],
  ['duplicate digest', '.installer', '    NestedInstallerFiles:\n', '    InstallerSha256: ' + 'a'.repeat(64) + '\n    NestedInstallerFiles:\n'],
];
for (const [name, file, from, to] of mutations) {
  test(`reject ${name}`, () => fixture(root => {
    change(root, file, from, to);
    assert.throws(() => validateWingetManifests(root));
  }));
}
test('reject missing manifests', () => fixture(root => {
  rmSync(join(root, '0.2.0', `${prefix}.yaml`));
  assert.throws(() => validateWingetManifests(root));
}));
test('reject an empty manifest root', () => {
  const root = mkdtempSync(join(tmpdir(), 'idleharbor-empty-'));
  try { assert.throws(() => validateWingetManifests(root)); } finally { rmSync(root, { recursive: true }); }
});
test('accept CRLF checkouts', () => fixture(root => {
  for (const file of ['', '.installer', '.locale.en-US']) {
    const path = join(root, '0.2.0', `${prefix}${file}.yaml`);
    writeFileSync(path, readFileSync(path, 'utf8').replace(/\r?\n/g, '\r\n'));
  }
  assert.equal(validateWingetManifests(root).length, 4);
}));
// Response doubles supply bytes, not a predetermined hash verdict: production
// hashing, byte limits and failure handling execute in every case below.
const sample = { url: 'https://example.invalid/archive.zip', sha256: 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad' };
test('verify actual streamed bytes against a matching SHA256', async () => {
  const result = await verifyArchive(sample, { fetchImpl: async () => new Response('abc') });
  assert.equal(result.bytes, 3);
  assert.equal(result.sha256, sample.sha256);
});
test('reject downloaded bytes with the wrong hash', async () => {
  await assert.rejects(verifyArchive(sample, { fetchImpl: async () => new Response('abd') }), /checksum/i);
});
test('fail HTTP errors rather than reporting verification success', async () => {
  await assert.rejects(verifyArchive(sample, { fetchImpl: async () => new Response('missing', { status: 404 }) }), /HTTP 404/);
});
test('bound bytes consumed even with no Content-Length', async () => {
  await assert.rejects(verifyArchive(sample, { maxBytes: 2, fetchImpl: async () => new Response('abc') }), /size limit/i);
});
