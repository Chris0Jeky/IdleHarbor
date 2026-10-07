import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import test from 'node:test';
import { validateSiteRelease } from '../tools/check-site-release.mjs';

const html = readFileSync(new URL('../docs/index.html', import.meta.url), 'utf8');
const version = JSON.parse(html.match(/<script type="application\/ld\+json">([\s\S]*?)<\/script>/)[1])
  ['@graph'].find(node => node['@type'] === 'SoftwareApplication').softwareVersion;
const next = '9.8.7';
function change(pattern, replacement) {
  const changed = html.replace(pattern, replacement);
  assert.notEqual(changed, html, 'Fixture mutation must change the source.');
  return changed;
}

test('the checked-in site has six agreeing release references', () => {
  assert.equal(validateSiteRelease(html), version);
});
test('a coordinated site repoint does not depend on the CMake version', () => {
  assert.equal(validateSiteRelease(html.replaceAll(version, next)), next);
});
for (const field of ['softwareVersion', 'downloadUrl', 'installUrl']) {
  test(`reject independent ${field} drift`, () => {
    const changed = change(new RegExp(`("${field}"\\s*:\\s*")([^"]+)(")`),
      (_, before, value, after) => before + value.replace(version, next) + after);
    assert.throws(() => validateSiteRelease(changed), /release|version/i);
  });
}
for (const [name, pattern] of [
  ['download button', /Download v\d+\.\d+\.\d+/],
  ['checksum command', /Get-FileHash [^\r\n<]+/],
  ['attestation command', /gh attestation verify [^\r\n<]+/],
]) {
  test(`reject independent ${name} drift`, () => {
    assert.throws(() => validateSiteRelease(change(pattern, text => text.replace(version, next))), /release|version/i);
  });
  test(`reject missing ${name}`, () => {
    assert.throws(() => validateSiteRelease(change(pattern, 'removed')), /exactly one|missing/i);
  });
}
test('reject a release URL for another repository', () => {
  assert.throws(() => validateSiteRelease(change(/("downloadUrl"\s*:\s*")[^"]+/, '$1https://github.com/other/repo/releases/tag/v' + version)), /release/i);
});
test('reject mismatching archive architectures in verification commands', () => {
  assert.throws(() => validateSiteRelease(change(/Get-FileHash [^\r\n<]+/, text => text.replace('x64', 'arm64'))), /archive/i);
});
test('reject malformed JSON-LD', () => {
  assert.throws(() => validateSiteRelease(change(/"softwareVersion":/, 'invalid:')), /JSON/i);
});
test('reject duplicate download buttons instead of accepting the first', () => {
  assert.throws(() => validateSiteRelease(html + `<a href="#download">Download v${version}</a>`), /exactly one/i);
});
test('ignore commented-out release examples', () => {
  assert.equal(validateSiteRelease(html + '\n<!-- Get-FileHash .\\IdleHarbor-9.8.7-windows-x64-portable.zip -->'), version);
});
test('inert hosting plan names the canonical-routing and release authorities', async () => {
  const { existsSync } = await import('node:fs');
  const plan = JSON.parse(readFileSync(new URL('../.hosting/manifest.json', import.meta.url), 'utf8'));
  const required = ['docs/index.html', 'docs/sitemap.xml', 'docs/robots.txt', 'docs/_headers', 'docs/_redirects',
    'packaging/cloudflare/wrangler.jsonc', 'packaging/cloudflare/README.md',
    'packaging/Test-CloudflareSite.ps1', 'packaging/Submit-IndexNow.ps1', 'tools/check-site-release.mjs'];
  for (const path of required) {
    assert.ok(plan.source_paths.includes(path), `Hosting source_paths omits ${path}.`);
    assert.ok(existsSync(new URL('../' + path, import.meta.url)), `Hosting source ${path} is missing.`);
  }
  assert.equal(plan.activation_authorized, false);
  assert.equal(plan.runtime_class, 'local');
  assert.equal(plan.public_origin, 'https://chris0jeky.github.io/IdleHarbor/');
});
