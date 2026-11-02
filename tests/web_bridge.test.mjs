import test from 'node:test';
import assert from 'node:assert/strict';
import {createCyUIBridge, applyCyUITheme} from '../web/cyui-web.js';

test('versioned transport and disposal', () => {
  const sent = []; let receive;
  const transport = {postMessage: m => sent.push(m), addEventListener: (_, fn) => {receive = fn;}, removeEventListener: () => {receive = undefined;}};
  const bridge = createCyUIBridge(transport); let result;
  const off = bridge.on('answer', payload => {result = payload;});
  bridge.send('question', {text: '音楽'}, 'request-1');
  assert.equal(sent[0].requestId, 'request-1');
  receive({data: {protocol: 'other', version: 1, type: 'answer', payload: 0}});
  assert.equal(result, undefined);
  receive({data: {protocol: 'cyui.web', version: 2, type: 'answer', payload: 0}});
  assert.equal(result, undefined);
  receive({data: {protocol: 'cyui.web', version: 1, type: 'answer', payload: 42}});
  assert.equal(result, 42); off();
  assert.throws(() => bridge.send('large', 'x'.repeat(1024*1024)));
  bridge.close(); assert.equal(receive, undefined);
  assert.throws(() => bridge.send('closed'));
});

test('theme writes only known CSS roles', () => {
  const properties = new Map(); const root = {style: {setProperty: (key, value) => properties.set(key, value)}};
  applyCyUITheme({scale: 2, colors: {accent: 'rgba(1,2,3,1)', bg0: 'url(evil)', unknown: 'rgba(0,0,0,1)'}}, root);
  assert.deepEqual([...properties], [['--cyui-accent', 'rgba(1,2,3,1)'], ['--cyui-scale', '2']]);
});
