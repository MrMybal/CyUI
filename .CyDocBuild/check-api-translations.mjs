// CyDoc 0.1 cannot record sourceRevision tags in extracted comments. Track the
// reviewed pair here; never label an untranslated source change as current.
import { readdir, readFile, writeFile } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { extractComments } from 'cydoc/extract';

const root = new URL('../include/cyui/', import.meta.url);
async function scan(url) {
  const entries = await readdir(url, { withFileTypes: true });
  return (await Promise.all(entries.map(async entry => {
    const file = new URL(entry.name + (entry.isDirectory() ? '/' : ''), url);
    if (entry.isDirectory()) return scan(file);
    return entry.name.endsWith('.h') ? extractComments(await readFile(file, 'utf8'), entry.name) : [];
  }))).flat();
}
const lock = {};
for (const page of await scan(root)) {
  lock[`${page.id}:${page.locale}`] = createHash('sha256').update(JSON.stringify({
    title: page.title, symbol: page.symbol, markdown: page.markdown
  })).digest('hex');
}
const ordered = Object.fromEntries(Object.entries(lock).sort(([a], [b]) => a.localeCompare(b, 'en')));
const text = JSON.stringify(ordered, null, 2) + '\n';
const path = new URL('./api-translations.json', import.meta.url);
if (process.argv.includes('--record')) await writeFile(path, text);
else if (await readFile(path, 'utf8') !== text) throw new Error('Public API annotations changed. Review EN/FR together, then run node .CyDocBuild/check-api-translations.mjs --record.');
for (const key of Object.keys(lock)) {
  const [id] = key.split(':');
  if (!lock[`${id}:en`] || !lock[`${id}:fr`]) throw new Error(`Incomplete API translation: ${id}`);
}
