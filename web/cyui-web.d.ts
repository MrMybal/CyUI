export interface CyUIMessage<T = unknown> { protocol: 'cyui.web'; version: 1; type: string; payload: T; requestId?: string }
export interface CyUITransport { postMessage(message: CyUIMessage): void; addEventListener(type: 'message', callback: (event: {data: CyUIMessage}) => void): void; removeEventListener?(type: 'message', callback: (event: {data: CyUIMessage}) => void): void }
export interface CyUITheme { scale: number; colors: Record<string, string>; locale?: string }
export interface CyUILocale { readonly locale: string; setLocale(code: string): boolean; registerTranslations(code: string, translations: Record<string, string>): boolean; tr(key: string, englishFallback?: string): string; apply(root?: HTMLElement): void }
export function createCyUILocale(initial?: string): CyUILocale;
export function createCyUIBridge(transport?: CyUITransport): { i18n: CyUILocale; send(type: string, payload?: unknown, requestId?: string): void; on<T = unknown>(type: string, callback: (payload: T, message: CyUIMessage<T>) => void): () => void; close(): void };
export function applyCyUITheme(theme: CyUITheme, root?: HTMLElement): void;
export function mountCyMD(container: HTMLElement, url: string, options?: {onReady?(): void; onSave?(request: {name: string; file: Blob}): Promise<void>; onState?(state: unknown): void; onError?(message: string): void}): {frame: HTMLIFrameElement; open(name: string, file: Blob, locale?: 'fr' | 'en'): void; save(): void; close(): void};
