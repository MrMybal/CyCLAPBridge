# Manual generation

The C++ build consumes the checked-in `../Documentation/` export. It does not run Node.js or download a documentation toolchain.

With Node.js 22+ and npm, from this directory:

```sh
npm ci --ignore-scripts --legacy-peer-deps
npm run build
npm run check
```

Both CyDoc commands generate the export. Edit `manual/en` first and update the complete `manual/fr` translation with matching IDs, explicit anchors and revision numbers. `notices.mjs` copies notices after generation; do not edit generated bundles or manifests. Open `../Documentation/Documentation.html` directly to inspect navigation, search and both languages.

`vendor/cydoc-0.1.0.tgz` is packed from the local CyDoc 0.1.0 checkout at revision `93f64486d61dae27a94ae1e5a05a20f6edd99914`, using `npm pack --ignore-scripts`. Archive SHA-256: `1095ac2afb5f958c22e56cc187e4a60fb4d5fed79a6c0d2b639bc8b9b5586193`. This MIT package is separate generation tooling, not an audio runtime or an unrelated npm package. The lockfile pins its dependencies; the optional Three.js peer is unused.

Keep working notes in ignored `internal/`, outside the human pages. Do not add audit logs, verification histories or agent instructions to the manual. Application compilation is documented in the root README.

## GitHub Pages publication

`npm run site` regenerates/checks the manual locally and prepares a fresh static
site under the ignored `build/` directory. `build/pages-site-root.txt` records
that output directory. The site contains the existing CyDoc reader, its complete
DocData assets/notices, an identical index.html entry and .nojekyll. The source
workspace and separate agent guide are outside this publication.

The Pages source is the root of the `codex/pages` branch. Publish only the
prepared site files to that branch, keeping its history. GitHub performs the
Pages deployment; the application is not compiled by this deployment. Confirm
repository visibility and Pages availability before activating the site.
