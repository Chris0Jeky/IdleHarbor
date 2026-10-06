import { readFileSync } from 'node:fs';
import { resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

function exactlyOne(matches, label) {
  if (matches.length !== 1) throw new Error(`Expected exactly one ${label}; found ${matches.length}.`);
  return matches[0];
}

/** Validate the site's own published-release consensus, not the build version. */
export function validateSiteRelease(source) {
  if (typeof source !== 'string') throw new TypeError('Site source must be text.');
  const html = source.replace(/<!--[\s\S]*?-->/g, '');
  const applications = [];
  function visit(node) {
    if (Array.isArray(node)) { node.forEach(visit); return; }
    if (!node || typeof node !== 'object') return;
    if (node['@type'] === 'SoftwareApplication') applications.push(node);
    if (node['@graph']) visit(node['@graph']);
  }
  for (const block of html.matchAll(/<script\b[^>]*\btype\s*=\s*["']application\/ld\+json["'][^>]*>([\s\S]*?)<\/script\s*>/gi)) {
    let data;
    try { data = JSON.parse(block[1]); }
    catch (cause) { throw new Error('Invalid JSON-LD in site source.', { cause }); }
    visit(data);
  }
  const application = exactlyOne(applications, 'SoftwareApplication JSON-LD record');
  const version = application.softwareVersion;
  if (typeof version !== 'string' || !/^(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$/.test(version)) {
    throw new Error('SoftwareApplication softwareVersion must be a stable release version.');
  }
  const releaseUrl = `https://github.com/Chris0Jeky/IdleHarbor/releases/tag/v${version}`;
  for (const field of ['downloadUrl', 'installUrl']) {
    if (application[field] !== releaseUrl) throw new Error(`${field} disagrees with release version ${version}.`);
  }
  const button = exactlyOne([...html.matchAll(/<a\b[^>]*\bhref\s*=\s*["']#download["'][^>]*>\s*Download\s+v([^\s<]+)\s*<\/a\s*>/gi)], 'versioned download button');
  if (button[1] !== version) throw new Error(`Download button disagrees with release version ${version}.`);

  const hash = exactlyOne([...html.matchAll(/Get-FileHash[ \t]+\.\\(IdleHarbor-[^\s<]+)[ \t]+-Algorithm[ \t]+SHA256\b/gi)], 'checksum command');
  const attestation = exactlyOne([...html.matchAll(/gh[ \t]+attestation[ \t]+verify[ \t]+\.\\(IdleHarbor-[^\s<]+)[ \t]+--repo[ \t]+Chris0Jeky\/IdleHarbor\b/gi)], 'attestation command');
  for (const [label, match] of [['checksum', hash], ['attestation', attestation]]) {
    const archive = /^IdleHarbor-(\d+\.\d+\.\d+)-windows-(x64|arm64)-portable\.zip$/.exec(match[1]);
    if (!archive || archive[1] !== version) throw new Error(`${label} archive disagrees with release version ${version}.`);
  }
  if (hash[1] !== attestation[1]) throw new Error('Checksum and attestation commands must verify the same archive.');
  return version;
}

if (process.argv[1] && resolve(process.argv[1]) === fileURLToPath(import.meta.url)) {
  try {
    const path = process.argv[2] ?? new URL('../docs/index.html', import.meta.url);
    const version = validateSiteRelease(readFileSync(path, 'utf8'));
    console.log(`Site release contract passed: six references agree on v${version}.`);
  } catch (error) {
    console.error(error.message);
    process.exitCode = 1;
  }
}
