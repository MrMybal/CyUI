import {french} from './cyui-locales.js';

/** An independent English-first catalog for a web view. No browser global is required. */
export function createCyUILocale(initial = 'en') {
  const catalogs = new Map([['en', new Map()], ['fr', new Map(Object.entries(french))]]);
  let current = 'en';
  const normalize = code => typeof code === 'string' && /^[a-zA-Z0-9_-]{1,35}$/.test(code) ? code.replaceAll('_', '-').toLowerCase() : '';
  const api = {
    get locale() { return current; },
    setLocale(code) {
      code = normalize(code);
      if (!catalogs.has(code)) code = code.split('-')[0];
      if (!catalogs.has(code)) return false;
      current = code; return true;
    },
    registerTranslations(code, translations) {
      code = normalize(code);
      const entries = Object.entries(translations ?? {});
      if (!code || entries.some(([, value]) => typeof value !== 'string')) return false;
      if (!catalogs.has(code)) catalogs.set(code, new Map());
      for (const [key, value] of entries) catalogs.get(code).set(key, value);
      return true;
    },
    tr(key, englishFallback = key) {
      return catalogs.get(current)?.get(key) ?? catalogs.get(current.split('-')[0])?.get(key) ?? catalogs.get('en').get(key) ?? englishFallback;
    },
    apply(root = globalThis.document?.documentElement) {
      if (!root) return;
      root.lang = current;
      // Only explicitly marked interface labels are translated; document content is untouched.
      for (const element of root.querySelectorAll?.('[data-cyui-i18n]') ?? [])
        element.textContent = api.tr(element.getAttribute('data-cyui-i18n'));
      for (const element of root.querySelectorAll?.('[data-cyui-i18n-placeholder]') ?? [])
        element.setAttribute('placeholder', api.tr(element.getAttribute('data-cyui-i18n-placeholder')));
    },
  };
  api.setLocale(initial);
  return api;
}
