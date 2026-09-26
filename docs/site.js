/* SPDX-License-Identifier: GPL-3.0-only
 * Website-only download-intent hook for the Pulseboard SDK (docs/pulseboard.js).
 * The native IdleHarbor executable sends nothing; this file runs only on the project website.
 * Every call is guarded: the page works the same when the SDK is absent, blocked or declined. */
(function () {
  'use strict';
  // Closed set: which link was chosen, never where it points or anything the visitor typed.
  var ASSETS = ['release-page', 'release-list', 'source'];

  function releaseTag(doc) {
    // The release tag the page advertises, read from its own JSON-LD (for example "v0.2.0").
    try {
      var blocks = doc.querySelectorAll('script[type="application/ld+json"]');
      for (var i = 0; i < blocks.length; i += 1) {
        var data = JSON.parse(blocks[i].textContent);
        var graph = data && data['@graph'] ? data['@graph'] : [data];
        for (var j = 0; j < graph.length; j += 1) {
          var version = graph[j] && graph[j].softwareVersion;
          if (typeof version === 'string' && /^\d+\.\d+\.\d+$/.test(version)) return 'v' + version;
        }
      }
    } catch (_) { /* fall through */ }
    return 'unknown';
  }

  function requested(asset, version, win) {
    try {
      var sdk = win.Pulseboard;
      if (!sdk || ASSETS.indexOf(asset) === -1) return;
      // The aggregate count (Usage counts) carries no properties; the event with its closed-set
      // properties is sent only when Journeys is on. A request is intent, not a completed download.
      if (typeof sdk.count === 'function') sdk.count('download.requested');
      if (typeof sdk.track === 'function') sdk.track('download.requested', { asset: asset, version: version });
    } catch (_) { /* the SDK never throws, but the page must not depend on that */ }
  }

  function install(win) {
    var doc = win.document;
    var version = releaseTag(doc);
    var links = doc.querySelectorAll('a[data-download-asset]');
    for (var i = 0; i < links.length; i += 1) {
      (function (link) {
        link.addEventListener('click', function () {
          requested(link.getAttribute('data-download-asset'), version, win);
        });
      })(links[i]);
    }
  }

  if (typeof window === 'undefined' || !window.document) return;
  if (window.document.readyState === 'loading') {
    window.document.addEventListener('DOMContentLoaded', function () { install(window); });
  } else {
    install(window);
  }
})();
