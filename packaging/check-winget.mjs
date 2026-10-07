import { readFileSync, readdirSync } from 'node:fs';
import { join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
import { createHash } from 'node:crypto';

const identifier = 'Chris0Jeky.IdleHarbor';
const repository = 'https://github.com/Chris0Jeky/IdleHarbor';
const stable = /^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/;
function requireEqual(actual, expected, label) {
  if (actual !== expected) throw new Error(`${label}: expected ${expected}, got ${actual}.`);
}
function readManifest(path) {
  const text = readFileSync(path, 'utf8').replace(/^\uFEFF/, '').replace(/\r\n/g, '\n')
    .split('\n').filter(line => line.trim() && !line.trimStart().startsWith('#')).join('\n');
  const fields = Object.create(null);
  for (const line of text.split('\n')) {
    if (/^\s/.test(line)) continue;
    const match = /^([A-Za-z][A-Za-z0-9]*):(?: (.*))?$/.exec(line);
    if (!match) throw new Error(`Unsupported top-level syntax in ${path}: ${line}`);
    const [, key, value = ''] = match;
    if (Object.hasOwn(fields, key)) throw new Error(`Duplicate ${key} in ${path}.`);
    fields[key] = value;
  }
  return { text, fields };
}

/** A fail-closed contract for the tracked plain-scalar manifest layout, not a
 * general YAML parser or a replacement for winget validate's schema validation. */
export function validateWingetManifests(root = new URL('./winget/', import.meta.url)) {
  if (root instanceof URL) root = fileURLToPath(root);
  const versions = readdirSync(root, { withFileTypes: true }).filter(entry => entry.isDirectory());
  if (!versions.length) throw new Error('No WinGet version directories found.');
  const records = [];
  for (const entry of versions.sort((a, b) => a.name.localeCompare(b.name))) {
    const version = entry.name;
    if (!stable.test(version)) throw new Error(`Invalid WinGet version directory: ${version}`);
    const dir = join(root, version);
    const expectedFiles = [`${identifier}.yaml`, `${identifier}.installer.yaml`, `${identifier}.locale.en-US.yaml`];
    const actualFiles = readdirSync(dir, { withFileTypes: true });
    if (actualFiles.length !== 3 || actualFiles.some(file => !file.isFile() || !expectedFiles.includes(file.name))) {
      throw new Error(`${version}: expected exactly the version, installer and en-US locale manifests.`);
    }
    const [versionDoc, installer, locale] = expectedFiles.map(name => readManifest(join(dir, name)));
    const schema = versionDoc.fields.ManifestVersion;
    if (!stable.test(schema ?? '')) throw new Error(`${version}: invalid ManifestVersion.`);
    for (const [doc, kind] of [[versionDoc, 'version'], [installer, 'installer'], [locale, 'defaultLocale']]) {
      for (const [key, expected] of Object.entries({ PackageIdentifier: identifier, PackageVersion: version, ManifestType: kind, ManifestVersion: schema })) {
        requireEqual(doc.fields[key], expected, `${version}/${kind}/${key}`);
      }
    }
    requireEqual(versionDoc.fields.DefaultLocale, 'en-US', 'DefaultLocale');
    requireEqual(locale.fields.PackageLocale, 'en-US', 'PackageLocale');
    requireEqual(locale.fields.License, 'GPL-3.0-only', 'License');
    requireEqual(locale.fields.LicenseUrl, `${repository}/blob/v${version}/LICENSE`, 'LicenseUrl');
    requireEqual(locale.fields.ReleaseNotesUrl, `${repository}/releases/tag/v${version}`, 'ReleaseNotesUrl');
    requireEqual(installer.fields.InstallerType, 'zip', 'InstallerType');
    requireEqual(installer.fields.NestedInstallerType, 'portable', 'NestedInstallerType');
    requireEqual(installer.fields.Installers, '', 'Installers block');
    const lines = installer.text.split('\n');
    const start = lines.indexOf('Installers:');
    let end = start + 1;
    while (end < lines.length && /^\s/.test(lines[end])) end++;
    const block = lines.slice(start + 1, end).join('\n');
    const stanzas = block.split(/(?=^  - Architecture: )/m).filter(Boolean);
    const architectures = new Set();
    for (const stanza of stanzas) {
      const match = /^  - Architecture: (x64|arm64)\n    InstallerUrl: (\S+)\n    InstallerSha256: ([0-9A-Fa-f]{64})\n    NestedInstallerFiles:\n      - RelativeFilePath: (\S+)\n        PortableCommandAlias: idleharbor\n?$/.exec(stanza);
      if (!match) throw new Error(`${version}: unsupported, incomplete or duplicate installer fields.`);
      const [, architecture, url, sha256, relativePath] = match;
      if (architectures.has(architecture)) throw new Error(`${version}: duplicate architecture ${architecture}.`);
      architectures.add(architecture);
      const name = `IdleHarbor-${version}-windows-${architecture}-portable`;
      requireEqual(url, `${repository}/releases/download/v${version}/${name}.zip`, 'InstallerUrl');
      requireEqual(relativePath, `${name}\\IdleHarbor.exe`, 'RelativeFilePath');
      records.push({ version, architecture, url, sha256: sha256.toLowerCase() });
    }
    if (architectures.size !== 2) throw new Error(`${version}: exactly x64 and arm64 installers are required.`);
  }
  return records;
}

export async function verifyArchive(record, { fetchImpl = fetch, maxBytes = 32 * 1024 * 1024 } = {}) {
  const response = await fetchImpl(record.url, { signal: AbortSignal.timeout(45000) });
  if (!response.ok) throw new Error(`HTTP ${response.status} while downloading ${record.url}.`);
  if (!response.body) throw new Error(`Missing archive body: ${record.url}`);
  const hash = createHash('sha256');
  let bytes = 0;
  for await (const chunk of response.body) {
    bytes += chunk.byteLength;
    if (bytes > maxBytes) throw new Error(`Archive exceeds size limit (${maxBytes} bytes): ${record.url}`);
    hash.update(chunk);
  }
  const sha256 = hash.digest('hex');
  if (sha256 !== record.sha256.toLowerCase()) throw new Error(`Archive checksum mismatch for ${record.url}: ${sha256}`);
  return { bytes, sha256 };
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const args = process.argv.slice(2);
    if (args.some(arg => arg !== '--verify-published-checksum') || args.length > 1) throw new Error('Usage: node packaging/check-winget.mjs [--verify-published-checksum]');
    const records = validateWingetManifests();
    console.log(`WinGet metadata contracts passed: ${records.length} architecture-specific archives.`);
    if (args.includes('--verify-published-checksum')) {
      for (const record of records) {
        const verified = await verifyArchive(record);
        console.log(`Verified ${record.version}/${record.architecture}: ${verified.bytes} bytes, SHA256 ${verified.sha256}`);
      }
    }
  } catch (error) { console.error(error.message); process.exitCode = 1; }
}
