// Website-only checks for the Pulseboard SDK v3 install. The native executable is out of scope:
// it has no network access and does not load or embed anything checked here.
import assert from 'node:assert/strict';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
import vm from 'node:vm';

const root = new URL('../', import.meta.url);
const read = path => readFileSync(new URL(path, root), 'utf8').replaceAll('\r\n', '\n');
const COLLECTOR = 'https://pulseboard-observatory.commit-atlas.workers.dev';
const SITE = 'https://chris0jeky.github.io';
const ASSETS = ['release-page', 'release-list', 'source'];

// ---- The locked artifact ------------------------------------------------------------------------------

const lock = JSON.parse(read('observatory.lock.json'));
assert.equal(lock.sdk, '3.0.0', 'The lock must record SDK 3.0.0');
const entries = Object.entries(lock.installs ?? {});
assert.equal(entries.length, 1, 'The lock records exactly one installed artifact');
const [target, entry] = entries[0];
assert.equal(target, 'docs/pulseboard.js');
assert.equal(entry.project, 'idleharbor');
const code = read(target);
assert.equal(createHash('sha256').update(code).digest('hex'), entry.sha256, `${target} does not match the lock`);
const header = code.split('\n').slice(0, 3).join('\n');
assert.match(header, /pulseboard-sdk 3\.0\.0 for idleharbor\./, 'The header must name pulseboard-sdk 3.0.0');
const body = code.split('\n').slice(3).join('\n');
const bodyHash = /sha256 of the body below: ([0-9a-f]{64})/.exec(header)?.[1];
assert.equal(createHash('sha256').update(body).digest('hex'), bodyHash, 'The artifact was edited after it was built');
assert.ok(code.includes(`"collector":"${COLLECTOR}"`), 'The collector origin must be the Pulseboard Worker');
assert.ok(code.includes(`"origin":"${SITE}"`), 'The artifact must be built for the GitHub Pages origin');
assert.ok(!/MAX_BYTES|MAX_BATCH/.test(code), 'Server-only constants must not be published');
assert.equal(code.split('\n').find(line => /^(?:import|export)\b/.test(line)), undefined, 'The artifact must stay a plain script');

function fakeWindow({ origin = SITE, readyState = 'loading', webdriver = false } = {}) {
  const fetches = [];
  const listeners = {};
  const document = {
    readyState, body: null,
    addEventListener: (type, fn) => { (listeners[type] ||= []).push(fn); },
    querySelector: () => null,
  };
  const context = {
    document, fetches, listeners,
    location: { origin, protocol: new URL(origin).protocol, href: origin + '/IdleHarbor/' },
    navigator: { webdriver },
    fetch: (url) => { fetches.push(String(url)); throw new Error('Unexpected network before mount'); },
    setTimeout: () => 0, clearTimeout: () => {},
    addEventListener: () => {},
  };
  vm.createContext(context);
  return context;
}

{ // On the registered origin, loading defines a frozen API and makes no request before the DOM is ready.
  const w = fakeWindow();
  vm.runInContext(code, w);
  assert.equal(w.Pulseboard?.version, '3.0.0', 'window.Pulseboard must be defined');
  assert.equal(Object.isFrozen(w.Pulseboard), true);
  assert.deepEqual(w.fetches, [], 'No request may leave before mount');
  assert.equal(w.listeners.DOMContentLoaded?.length, 1, 'Mount waits for DOMContentLoaded');
  // A second copy of the script is ignored.
  const first = w.Pulseboard;
  vm.runInContext(code, w);
  assert.equal(w.Pulseboard, first);
}
for (const options of [{ origin: 'https://idleharbor.example' }, { webdriver: true }]) {
  // Off the registered origin (the Cloudflare mirror) or under automation the SDK stays inert.
  const w = fakeWindow({ ...options, readyState: 'complete' });
  vm.runInContext(code, w);
  assert.equal(w.Pulseboard.consent.get().blocked, true, `inert for ${JSON.stringify(options)}`);
  assert.equal(w.Pulseboard.track('download.requested', { asset: 'source', version: 'v0.2.0' }), false);
  assert.deepEqual(w.fetches, []);
}

// ---- The page -----------------------------------------------------------------------------------------

const html = read('docs/index.html');
assert.ok(!html.includes('observatory.js'), 'The retired observatory.js adapter is still referenced');
assert.match(html, /<body>\n<div data-pulseboard-bar style="height:2\.5rem"><\/div>\n/, 'The bar placeholder must be the first child of <body>');
assert.match(html, /<script defer src="pulseboard\.js"><\/script>\n<script defer src="site\.js"><\/script>\n<\/body>/,
  'The page must load pulseboard.js and then site.js, both deferred and same-origin');
assert.ok(!/<script(?![^>]*type="application\/ld\+json")[^>]*>[^<]/.test(html), 'No inline script: the CSP is script-src \'self\'');
const assets = [...html.matchAll(/data-download-asset="([^"]*)"/g)].map(m => m[1]);
assert.deepEqual([...assets].sort(), [...ASSETS].sort(), 'Each download asset appears exactly once');

// ---- The download hook, with and without the SDK ------------------------------------------------------

const siteCode = read('docs/site.js');
const ldJson = [...html.matchAll(/<script type="application\/ld\+json">([\s\S]*?)<\/script>/g)].map(m => m[1]);
function sitePage(pulseboard) {
  const links = ASSETS.map(asset => {
    const handlers = [];
    return { asset, handlers, getAttribute: name => (name === 'data-download-asset' ? asset : null),
      addEventListener: (type, fn) => { if (type === 'click') handlers.push(fn); } };
  });
  const document = {
    readyState: 'complete',
    addEventListener: () => { throw new Error('site.js must not wait when the DOM is ready'); },
    querySelectorAll: selector => (selector === 'script[type="application/ld+json"]' ? ldJson.map(textContent => ({ textContent }))
      : selector === 'a[data-download-asset]' ? links : []),
  };
  const context = { document };
  context.window = context;
  if (pulseboard !== undefined) context.Pulseboard = pulseboard;
  vm.createContext(context);
  vm.runInContext(siteCode, context);
  const click = asset => links.find(l => l.asset === asset).handlers.forEach(fn => fn({ type: 'click' }));
  return { click, links };
}

{ // Without the SDK (blocked, absent, or never loaded) every link still works and nothing throws.
  const page = sitePage(undefined);
  for (const link of page.links) assert.equal(link.handlers.length, 1);
  for (const asset of ASSETS) page.click(asset);
}
{ // An SDK that throws cannot break a link either.
  const page = sitePage({ count() { throw new Error('boom'); }, track() { throw new Error('boom'); } });
  for (const asset of ASSETS) page.click(asset);
}
{ // With the SDK, each click is one aggregate count and one event carrying closed-set props only.
  const calls = [];
  const page = sitePage({ count: (...args) => calls.push(['count', ...args]), track: (...args) => calls.push(['track', ...args]) });
  page.click('release-page');
  page.click('source');
  assert.deepEqual(JSON.parse(JSON.stringify(calls)), [
    ['count', 'download.requested'],
    ['track', 'download.requested', { asset: 'release-page', version: 'v0.2.0' }],
    ['count', 'download.requested'],
    ['track', 'download.requested', { asset: 'source', version: 'v0.2.0' }],
  ]);
}

// ---- The mirror's CSP --------------------------------------------------------------------------------

const csp = /^\s*Content-Security-Policy:\s*(.*)$/m.exec(read('docs/_headers'))?.[1] ?? '';
const directives = Object.fromEntries(csp.split(';').map(d => d.trim()).filter(Boolean).map(d => [d.split(/\s+/)[0], d]));
assert.equal(directives['connect-src'], `connect-src ${COLLECTOR}`);
assert.equal(directives['script-src'], "script-src 'self'");

console.log('Pulseboard SDK 3.0.0 artifact, page wiring, download hook and CSP checks passed.');
