import { build } from 'esbuild';
import { readFile, readdir } from 'node:fs/promises';
import { createHash } from 'node:crypto';
import { fileURLToPath } from 'node:url';

const output = new URL('./.cache/export/DocData/', import.meta.url);
const versions = {};
for (const name of await readdir(output)) {
  if (name.startsWith('media-')) versions[name] = createHash('sha256')
    .update(await readFile(new URL(name, output))).digest('hex').slice(0, 12);
}
await build({
  entryPoints: [fileURLToPath(new URL('./reader.mjs', import.meta.url))],
  outfile: fileURLToPath(new URL('viewer.js', output)),
  bundle: true, format: 'iife', target: 'es2020', minify: true,
  define: { RESOURCE_VERSIONS: JSON.stringify(versions) }
});
