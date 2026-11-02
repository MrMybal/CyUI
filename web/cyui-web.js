import {createCyUILocale} from './cyui-i18n.js';
export {createCyUILocale} from './cyui-i18n.js';
/** CyUI browser bridge. Works with CEF or an explicitly supplied transport. */
export function createCyUIBridge(transport = globalThis.cyuiNative) {
  if (!transport?.postMessage || !transport?.addEventListener) throw new Error('CyUI transport unavailable');
  const listeners = new Map();
  const i18n = createCyUILocale();
  let closed = false;
  const receive = event => {
    const message = event.data;
    if (!message || message.protocol !== 'cyui.web' || message.version !== 1 || typeof message.type !== 'string') return;
    if (message.type === 'theme') applyCyUITheme(message.payload);
    if (message.type === 'theme' && typeof message.payload?.locale === 'string') {
      i18n.setLocale(message.payload.locale); i18n.apply();
    }
    if (message.type === 'locale' && typeof message.payload?.locale === 'string') {
      i18n.setLocale(message.payload.locale); i18n.apply();
    }
    for (const callback of listeners.get(message.type) || []) callback(message.payload, message);
  };
  transport.addEventListener('message', receive);
  return {
    i18n,
    send(type, payload = null, requestId) {
      if (closed || typeof type !== 'string' || type.length > 128) throw new Error('Invalid CyUI message');
      const message = {protocol: 'cyui.web', version: 1, type, payload};
      if (requestId !== undefined) message.requestId = requestId;
      if (new TextEncoder().encode(JSON.stringify(message)).byteLength > 1024 * 1024) throw new Error('CyUI message exceeds 1 MiB');
      transport.postMessage(message);
    },
    on(type, callback) {
      if (!listeners.has(type)) listeners.set(type, new Set());
      listeners.get(type).add(callback);
      return () => listeners.get(type)?.delete(callback);
    },
    close() { closed = true; transport.removeEventListener?.('message', receive); listeners.clear(); },
  };
}

export function applyCyUITheme(theme, root = globalThis.document?.documentElement) {
  if (!root || !theme || typeof theme.colors !== 'object') return;
  const names = new Set(['bg0', 'bg1', 'bg2', 'bg3', 'text', 'textDim', 'textFaint', 'accent', 'selection', 'line', 'success', 'warning', 'error', 'info']);
  for (const [name, value] of Object.entries(theme.colors)) {
    if (names.has(name) && typeof value === 'string' && /^rgba\([\d., ]+\)$/.test(value)) root.style.setProperty('--cyui-'+name, value);
  }
  if (Number.isFinite(theme.scale) && theme.scale > 0) root.style.setProperty('--cyui-scale', String(theme.scale));
}

/** Existing CyMD iframe protocol, version 1. No Electron API is required. */
export function mountCyMD(container, url, {onReady = () => {}, onSave = async () => {throw new Error('Save handler required');}, onState = () => {}, onError = () => {}} = {}) {
  const target = new URL(url, location.href);
  if (!['http:', 'https:'].includes(target.protocol)) throw new Error('CyMD requires an HTTP(S) URL');
  const session = crypto.randomUUID();
  target.searchParams.set('integration', '1');
  target.searchParams.set('parentOrigin', location.origin);
  target.searchParams.set('session', session);
  const frame = document.createElement('iframe');
  frame.title = 'CyMD'; frame.src = target.href;
  frame.style.cssText = 'width:100%;height:100%;border:0';
  container.append(frame);
  let closed = false;
  const send = (type, data = {}) => frame.contentWindow?.postMessage({protocol: 'cymd.integration', version: 1, session, source: 'host', type, ...data}, target.origin);
  const receive = async event => {
    const m = event.data;
    if (closed || event.source !== frame.contentWindow || event.origin !== target.origin || m?.protocol !== 'cymd.integration' || m.version !== 1 || m.session !== session || m.source !== 'cymd') return;
    if (m.type === 'ready') onReady();
    else if (m.type === 'state') onState(m);
    else if (m.type === 'error') onError(m.message);
    else if (m.type === 'save' && typeof m.requestId === 'string') {
      if (!(m.file instanceof Blob) || m.file.size > 50*1024*1024 || typeof m.name !== 'string' || !/\.(cymd|md|markdown)$/i.test(m.name)) {
        send('save-result', {requestId: m.requestId, ok: false, error: 'Invalid save request'}); return;
      }
      try { await onSave({name: m.name.replace(/^.*[\\/]/, ''), file: m.file}); send('save-result', {requestId: m.requestId, ok: true}); }
      catch (error) { send('save-result', {requestId: m.requestId, ok: false, error: String(error)}); }
    }
  };
  window.addEventListener('message', receive);
  return {
    frame,
    open(name, file, locale = 'en') {
      if (!(file instanceof Blob) || file.size > 50*1024*1024 || !/\.(cymd|md|markdown)$/i.test(name)) throw new Error('Invalid CyMD document');
      send('open', {name, file, locale});
    },
    save() { send('command', {command: 'save'}); },
    close() { closed = true; window.removeEventListener('message', receive); frame.remove(); },
  };
}
