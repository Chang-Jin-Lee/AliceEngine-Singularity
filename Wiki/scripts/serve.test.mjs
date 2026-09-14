import assert from 'node:assert/strict';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

test('static preview serves exported routes, raw Markdown, JSON and a real 404', async (t) => {
  const moduleUrl = new URL('./serve.mjs', import.meta.url);
  assert.ok(fs.existsSync(fileURLToPath(moduleUrl)), 'A static preview server is needed for output: export');
  const { createPreviewServer } = await import(moduleUrl);
  const fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'alice-preview-test-'));
  t.after(() => {
    assert.equal(path.dirname(path.resolve(fixture)), path.resolve(os.tmpdir()));
    assert.ok(path.basename(fixture).startsWith('alice-preview-test-'));
    fs.rmSync(fixture, { recursive: true, force: true });
  });
  fs.mkdirSync(path.join(fixture, 'guide/start'), { recursive: true });
  fs.mkdirSync(path.join(fixture, 'api'));
  fs.writeFileSync(path.join(fixture, 'index.html'), '<h1>Home</h1>');
  fs.writeFileSync(path.join(fixture, 'guide/start/index.html'), '<h1>Start</h1>');
  fs.writeFileSync(path.join(fixture, 'guide/start.md'), '# Start');
  fs.writeFileSync(path.join(fixture, 'api/verbs.json'), '{"verbs":[]}');
  fs.writeFileSync(path.join(fixture, '404.html'), '<h1>Missing</h1>');
  const server = createPreviewServer(fixture);
  await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
  t.after(() => new Promise((resolve) => server.close(resolve)));
  const base = `http://127.0.0.1:${server.address().port}`;
  for (const [route, type, content] of [
    ['/', 'text/html', '<h1>Home</h1>'],
    ['/guide/start/', 'text/html', '<h1>Start</h1>'],
    ['/guide/start.md', 'text/markdown', '# Start'],
    ['/api/verbs.json', 'application/json', '{"verbs":[]}'],
  ]) {
    const response = await fetch(base + route);
    assert.equal(response.status, 200, route);
    assert.ok(response.headers.get('content-type').startsWith(type), route);
    assert.equal(await response.text(), content);
  }
  assert.equal((await fetch(base + '/guide/start', { redirect: 'manual' })).status, 308);
  const redirected = await fetch(base + '/guide/start?test=1', { redirect: 'manual' });
  assert.equal(redirected.headers.get('location'), '/guide/start/?test=1');
  const missing = await fetch(base + '/missing/');
  assert.equal(missing.status, 404);
  assert.equal(await missing.text(), '<h1>Missing</h1>');
  assert.equal((await fetch(base + '/%2e%2e%5cpackage.json')).status, 403);
  assert.equal((await fetch(base + '/%E0%A4%A')).status, 400);
  assert.equal((await fetch(base, { method: 'POST' })).status, 405);
  const head = await fetch(base, { method: 'HEAD' });
  assert.equal(head.status, 200);
  assert.equal(await head.text(), '');
});
