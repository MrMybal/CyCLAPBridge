// SPDX-License-Identifier: MIT
import { copyFile, mkdir } from 'node:fs/promises';
import { dirname, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';
const root = dirname(fileURLToPath(import.meta.url));
const out = resolve(root, '../Documentation/DocData/licenses');
await mkdir(out, { recursive: true });
for (const [name, source] of Object.entries({
  CyDoc: 'cydoc/LICENSE', marked: 'marked/LICENSE.md', PrismJS: 'prismjs/LICENSE', esbuild: 'esbuild/LICENSE.md'
})) await copyFile(resolve(root, 'node_modules', source), resolve(out, name + '.txt'));
