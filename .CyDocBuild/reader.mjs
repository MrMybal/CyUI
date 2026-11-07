// CyUI bootstrap for the unmodified CyDoc reader. Bundled as a classic script
// for file://; providers follow CyDoc's generated offline-export contract.
import { mountViewer } from 'cydoc/viewer';

const options = globalThis.CYDOC_OPTIONS;
const resources = RESOURCE_VERSIONS;
const local = name => `DocData/${name}?v=${resources[name] || '1'}`;
const pending = new Map();
function loadAsset(src) {
  if (pending.has(src)) return pending.get(src);
  const name = options.assets?.[src];
  if (!name) return Promise.reject(new Error(`Local asset is not packaged: ${src}`));
  const request = new Promise((resolve, reject) => {
    const script = document.createElement('script');
    script.src = local(name);
    const cleanup = () => { script.remove(); pending.delete(src); };
    script.onload = () => {
      const asset = globalThis.CYDOC_ASSETS?.[src];
      if (globalThis.CYDOC_ASSETS) delete globalThis.CYDOC_ASSETS[src];
      cleanup();
      asset ? resolve(asset) : reject(new Error(`Invalid local asset: ${src}`));
    };
    script.onerror = () => { cleanup(); reject(new Error(`Cannot load local asset: ${src}`)); };
    document.head.append(script);
  });
  pending.set(src, request);
  return request;
}
const modules = Object.fromEntries(options.modules.map(name => [name, () => new Promise((resolve, reject) => {
  const script = document.createElement('script');
  script.src = local(`media-${name}.js`);
  script.onload = () => resolve(globalThis[`CyDocMedia_${name}`].default);
  script.onerror = () => { script.remove(); reject(new Error(`Cannot load CyDoc module ${name}`)); };
  document.head.append(script);
})]));
let theme = 'dark';
try { theme = localStorage.getItem('cydoc:theme') || theme; } catch { /* File origins may deny storage. */ }
mountViewer(document.querySelector('#app'), globalThis.CYDOC_DATA, {
  homePage: options.homePage,
  initialTheme: theme,
  storageId: 'CyUI',
  modules,
  loadAsset,
  assetBase: 'DocData/'
});
