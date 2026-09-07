import assert from 'node:assert/strict';
import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import os from 'node:os';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import test from 'node:test';

const wiki = path.resolve(path.dirname(fileURLToPath(import.meta.url)), '..');

test('an engine-free checkout regenerates verb, symbol and diagnostic pages from saved API data', (t) => {
  const fixture = fs.mkdtempSync(path.join(os.tmpdir(), 'alice-wiki-test-'));
  t.after(() => {
    assert.equal(path.dirname(path.resolve(fixture)), path.resolve(os.tmpdir()));
    assert.ok(path.basename(fixture).startsWith('alice-wiki-test-'));
    fs.rmSync(fixture, { recursive: true, force: true });
  });
  const fixtureWiki = path.join(fixture, 'Wiki');
  fs.mkdirSync(path.join(fixtureWiki, 'scripts'), { recursive: true });
  fs.copyFileSync(path.join(wiki, 'scripts/generate.mjs'), path.join(fixtureWiki, 'scripts/generate.mjs'));
  fs.cpSync(path.join(wiki, 'public/api'), path.join(fixtureWiki, 'public/api'), { recursive: true });
  fs.cpSync(path.join(wiki, '../Schemas'), path.join(fixture, 'Schemas'), { recursive: true });
  fs.mkdirSync(path.join(fixture, 'Docs'));
  fs.writeFileSync(path.join(fixture, 'README.md'), '# Engine\n');
  fs.writeFileSync(path.join(fixture, 'Docs/ONBOARDING.md'), '# Start\n\n[Status](STATUS.md) [README](../README.md#build)\n');
  fs.writeFileSync(path.join(fixture, 'Docs/STATUS.md'), '# Status\n');

  execFileSync(process.execPath, [path.join(fixtureWiki, 'scripts/generate.mjs')]);

  for (const file of ['verbs/audio.play.md', 'expression-symbols.md', 'diagnostics.md', 'schemas/alice-actor-1.md']) {
    assert.ok(fs.existsSync(path.join(fixtureWiki, 'content/reference', file)), `Missing generated ${file}`);
  }
  const onboarding = fs.readFileSync(path.join(fixtureWiki, 'content/guide/onboarding.md'), 'utf8');
  assert.ok(onboarding.includes('[Status](/guide/status/)'));
  assert.ok(onboarding.includes('[README](https://github.com/Chang-Jin-Lee/AliceEngine-Singularity/blob/main/README.md#build)'));
});
