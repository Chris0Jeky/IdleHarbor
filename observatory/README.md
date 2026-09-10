# Observatory integration

Shared collector/dashboard: [Pulseboard #15](https://github.com/Chris0Jeky/Pulseboard/pull/15), source commit `8d92fff11f581d600c357e402cd521426665f318`.

Only the public website loads this local adapter. Its empty endpoint keeps reporting and consent storage off. The native C++ executable, release pipeline, INI settings and no-telemetry/no-network promise are unchanged.

Run `node observatory/check.mjs`, then the existing website checks and a browser smoke test on the published subpath. Shared kit: 58 local tests passed. The complete website and native build checks have not been executed here.

Activation is a separate reviewed change: deploy the collector, distinguish optional website analytics from the native app's no-telemetry promise in the notice, review CSP and regenerate the locked adapter. Verify consent and withdrawal. Baseline active events are page views and content-free error occurrences. `download.requested` is reserved for a later link hook and must not be described as a completed download, installation or active user. Release asset download counts need a separate server-side source.
