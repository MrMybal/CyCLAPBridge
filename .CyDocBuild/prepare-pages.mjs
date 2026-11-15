// SPDX-License-Identifier: MIT
import { cp, copyFile, mkdir, mkdtemp, readFile, writeFile } from 'node:fs/promises';
import { dirname, join, resolve } from 'node:path';
import { fileURLToPath } from 'node:url';

const workspace=dirname(fileURLToPath(import.meta.url));
const root=resolve(workspace,'..');
const documentation=join(root,'Documentation');
const manifest=JSON.parse(await readFile(join(documentation,'DocData/manifest.json'),'utf8'));
if(manifest.name!=='CyCLAPBridge'||manifest.defaultLocale!=='en')throw new Error('Unexpected manual export');
if(!['en','fr'].every(locale=>manifest.locales.some(item=>item.code===locale)))throw new Error('Both manual languages are required');
const english=manifest.pages.filter(page=>page.locale==='en').map(page=>page.id).sort();
const french=manifest.pages.filter(page=>page.locale==='fr').map(page=>page.id).sort();
if(!english.length||JSON.stringify(english)!==JSON.stringify(french))throw new Error('The complete manual must be available in both languages');
const build=join(root,'build');await mkdir(build,{recursive:true});
const preview=await mkdtemp(join(build,'pages-'));
const site=join(preview,'CyCLAPBridge');await mkdir(site);
await cp(join(documentation,'DocData'),join(site,'DocData'),{recursive:true});
await copyFile(join(documentation,'Documentation.html'),join(site,'index.html'));
await copyFile(join(documentation,'Documentation.html'),join(site,'Documentation.html'));
await copyFile(join(root,'LICENSE'),join(site,'LICENSE'));
await writeFile(join(site,'.nojekyll'),'');
await writeFile(join(build,'pages-site-root.txt'),site+'\n');
console.log(`Static CyDoc site: ${site}`);
console.log(`${manifest.pages.length} pages, English/French; index and existing Documentation.html entry preserved.`);
