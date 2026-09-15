// SPDX-License-Identifier: MIT
//
// 엔진에서 레퍼런스 문서를 생성한다.
//
// 손으로 쓴 레퍼런스는 반드시 코드와 어긋난다. 스키마와 동사가 이미 설명·예시·흔한 실수를
// 들고 있으므로, 그걸 그대로 페이지로 만든다. 손으로 쓰는 것은 개념 설명과 튜토리얼뿐이다.
//
// alice 를 못 찾으면 저장소의 Schemas/*.json 으로 대체한다.
// 그러면 엔진을 빌드하지 않은 사람도 위키를 띄울 수 있다.

import { execFileSync } from 'node:child_process';
import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const WIKI = path.resolve(HERE, '..');
const REPO = path.resolve(WIKI, '..');
const OUT_REF = path.join(WIKI, 'content', 'reference');
const OUT_API = path.join(WIKI, 'public', 'api');
const GITHUB = 'https://github.com/Chang-Jin-Lee/AliceEngine-Singularity';
const docMap = {
  'Docs/STATUS.md': 'guide/status.md',
  'Docs/EDITOR.md': 'guide/editor.md',
  'Docs/ONBOARDING.md': 'guide/onboarding.md',
  'Docs/CONTENT_FORMAT.md': 'guide/content-format.md',
  'Docs/ARCHITECTURE.md': 'guide/architecture.md',
  'Docs/PERFORMANCE.md': 'guide/performance.md',
  'Docs/ASSET_PIPELINE.md': 'guide/asset-pipeline.md',
  'Docs/AI_BRIDGE.md': 'guide/ai-bridge.md',
  'Docs/ENGINE_SURVEY.md': 'guide/engine-survey.md',
};

function savedApi(name) {
  const file = path.join(OUT_API, name + '.json');
  if (!fs.existsSync(file)) throw new Error(`Missing ${file}. Build alice and run npm run generate first.`);
  return JSON.parse(fs.readFileSync(file, 'utf8'));
}

function wikiLinks(body, source) {
  return body.replace(/\]\(([^\s)]+)\)/g, (match, target) => {
    if (/^(?:[a-z]+:|\/|#)/i.test(target)) return match;
    const [file, ...fragment] = target.split('#');
    const resolved = path.posix.normalize(path.posix.join(path.posix.dirname(source), file));
    const anchor = fragment.length ? '#' + fragment.join('#') : '';
    if (docMap[resolved]) return `](/${docMap[resolved].replace(/\.md$/, '')}/${anchor})`;
    const local = path.join(REPO, resolved);
    const kind = fs.existsSync(local) && fs.statSync(local).isDirectory() ? 'tree' : 'blob';
    return `](${GITHUB}/${kind}/main/${resolved}${anchor})`;
  });
}

function findAlice() {
  const names = process.platform === 'win32' ? ['alice.exe'] : ['alice'];
  const roots = ['build/bin', 'build/Release/bin', 'build/RelWithDebInfo/bin', 'out/bin'];
  for (const root of roots) {
    for (const name of names) {
      const candidate = path.join(REPO, root, name);
      if (fs.existsSync(candidate)) return candidate;
    }
  }
  return null;
}

function runAlice(alice, args) {
  try {
    return JSON.parse(execFileSync(alice, [...args, '--json'], {
      encoding: 'utf8',
      maxBuffer: 64 * 1024 * 1024,
    }));
  } catch (err) {
    console.warn(`  alice ${args.join(' ')} 실패: ${err.message}`);
    return null;
  }
}

function write(file, text) {
  fs.mkdirSync(path.dirname(file), { recursive: true });
  fs.writeFileSync(file, text, 'utf8');
}

function slug(id) {
  return id.replace(/[/\\:]/g, '-');
}

function fence(lang, body) {
  return '```' + lang + '\n' + body + '\n```';
}

// ── 스키마 페이지 ────────────────────────────────────────────────────
function typeLabel(node) {
  if (!node) return 'any';
  if (node.$ref) return node.$ref.replace(/^\.\//, '').replace(/\.schema\.json$/, '');
  if (node.enum) return node.enum.join(' | ');
  if (node.anyOf) return node.anyOf.map(typeLabel).join(' | ');
  if (node.type === 'array') {
    const inner = typeLabel(node.items);
    if (node.minItems && node.minItems === node.maxItems) return `[${inner} x${node.minItems}]`;
    return `[${inner}]`;
  }
  return node.type || 'any';
}

function schemaPage(entry, jsonSchema) {
  const required = new Set(jsonSchema.required || []);
  let md = '---\n';
  md += `title: ${entry.title || entry.id}\n`;
  md += `description: ${(entry.description || '').replace(/\n/g, ' ')}\n`;
  md += `schemaId: ${entry.id}\n`;
  md += 'generated: true\n';
  md += '---\n\n';

  md += `# ${entry.title || entry.id}\n\n`;
  md += `> \`schema: ${entry.id}\`\n\n`;
  if (entry.description) md += `${entry.description}\n\n`;

  const props = jsonSchema.properties || {};
  const names = Object.keys(props);
  if (names.length) {
    md += '## 필드\n\n';
    md += '| 필드 | 타입 | 필수 | 기본값 | 설명 |\n|---|---|:---:|---|---|\n';
    for (const name of names) {
      const p = props[name];
      const def = p.default !== undefined ? '`' + JSON.stringify(p.default) + '`' : '';
      const desc = (p.description || '').replace(/\|/g, '\\|').replace(/\n/g, ' ');
      md += `| \`${name}\` | \`${typeLabel(p)}\` | ${required.has(name) ? '✓' : ''} | ${def} | ${desc} |\n`;
    }
    md += '\n';
  }

  if (jsonSchema.examples && jsonSchema.examples.length) {
    md += '## 예시\n\n';
    for (const ex of jsonSchema.examples) {
      md += fence('json', JSON.stringify(ex, null, 2)) + '\n\n';
    }
  }

  const pitfalls = jsonSchema['x-alice-pitfalls'];
  if (pitfalls && pitfalls.length) {
    md += '## 흔한 실수\n\n';
    for (const p of pitfalls) {
      md += `**${p.why}**\n\n`;
      md += fence('yaml', `# 이렇게 쓰면 안 된다\n${p.wrong}\n\n# 이렇게 쓴다\n${p.right}`) + '\n\n';
    }
  }

  md += '## 확인\n\n';
  md += fence('bash', `alice schema show ${entry.id}\nalice new ${entry.id.split('/').slice(-2, -1)[0]} MyThing`) + '\n';
  return md;
}

// ── 동사 페이지 ──────────────────────────────────────────────────────
function verbPage(verb) {
  let md = '---\n';
  md += `title: ${verb.id}\n`;
  md += `description: ${(verb.summary || '').replace(/\n/g, ' ')}\n`;
  md += `verbId: ${verb.id}\n`;
  md += 'generated: true\n';
  md += '---\n\n';

  md += `# \`${verb.id}\`\n\n`;
  md += `${verb.summary}\n\n`;
  if (verb.description) md += `${verb.description}\n\n`;

  md += `분류: ${(verb.tags || []).map((t) => '`' + t + '`').join(' · ')}`;
  md += ` · 비용: \`${verb.cost}\``;
  if (!verb.deterministic) md += ' · **비결정적**';
  md += '\n\n';

  if (!verb.deterministic) {
    md += '> 같은 입력이어도 결과가 다를 수 있습니다. 리플레이나 결정적 테스트에서 주의하십시오.\n\n';
  }

  const args = verb.args && verb.args.properties;
  if (args) {
    const required = new Set((verb.args.required || []));
    md += '## 인자\n\n';
    md += '| 인자 | 타입 | 필수 | 기본값 | 설명 |\n|---|---|:---:|---|---|\n';
    for (const name of Object.keys(args)) {
      const p = args[name];
      const def = p.default !== undefined ? '`' + JSON.stringify(p.default) + '`' : '';
      const desc = (p.description || '').replace(/\|/g, '\\|').replace(/\n/g, ' ');
      md += `| \`${name}\` | \`${typeLabel(p)}\` | ${required.has(name) ? '✓' : ''} | ${def} | ${desc} |\n`;
    }
    md += '\n';
  }

  if (verb.examples && verb.examples.length) {
    md += '## 예시\n\n';
    for (const ex of verb.examples) {
      md += fence('yaml', 'do:\n  - ' + JSON.stringify(ex).replace(/^\{|\}$/g, '')) + '\n\n';
    }
  }
  return md;
}

// ── 실행 ─────────────────────────────────────────────────────────────
console.log('레퍼런스 생성 중...');

const alice = findAlice();
if (alice) console.log(`  alice: ${path.relative(REPO, alice)}`);
else console.log('  alice 없음: 저장소의 Schemas/ 및 public/api/ 스냅샷으로 재생성한다.');

// Read saved inputs before replacing generated pages. Engine-free hosts must retain the full reference.
const verbs = alice ? runAlice(alice, ['verbs']) : savedApi('verbs');
const symbols = alice ? runAlice(alice, ['verbs', '--symbols']) : savedApi('symbols');
const codes = alice ? runAlice(alice, ['explain', '--list']) : savedApi('diagnostics');
if (!verbs?.verbs || !symbols?.symbols || !codes?.codes) {
  throw new Error('Incomplete engine reference data. Fix alice or restore the committed public/api snapshots.');
}

if (path.relative(WIKI, path.resolve(OUT_REF)) !== path.join('content', 'reference')) {
  throw new Error('Refusing to replace a reference directory outside Wiki/content/reference.');
}
fs.rmSync(OUT_REF, { recursive: true, force: true });
fs.mkdirSync(OUT_REF, { recursive: true });
fs.mkdirSync(OUT_API, { recursive: true });

// 스키마
let schemaIndex = null;
if (alice) schemaIndex = runAlice(alice, ['schema', 'list']);
if (!schemaIndex) {
  const indexPath = path.join(REPO, 'Schemas', 'index.json');
  if (fs.existsSync(indexPath)) {
    const raw = JSON.parse(fs.readFileSync(indexPath, 'utf8'));
    schemaIndex = { schemas: raw.schemas.map((s) => ({ ...s, short: s.id.split('/').slice(-2, -1)[0] })) };
  }
}

let schemaCount = 0;
if (schemaIndex) {
  for (const entry of schemaIndex.schemas) {
    let jsonSchema = null;
    if (alice) jsonSchema = runAlice(alice, ['schema', 'show', entry.id]);
    if (!jsonSchema) {
      const file = path.join(REPO, 'Schemas', slug(entry.id) + '.schema.json');
      if (fs.existsSync(file)) jsonSchema = JSON.parse(fs.readFileSync(file, 'utf8'));
    }
    if (!jsonSchema) continue;
    write(path.join(OUT_REF, 'schemas', slug(entry.id) + '.md'), schemaPage(entry, jsonSchema));
    schemaCount++;
  }
  write(path.join(OUT_API, 'schemas.json'), JSON.stringify(schemaIndex, null, 2));
}

// 동사
let verbCount = 0;
if (verbs && verbs.verbs) {
  for (const verb of verbs.verbs) {
    write(path.join(OUT_REF, 'verbs', slug(verb.id) + '.md'), verbPage(verb));
    verbCount++;
  }
  write(path.join(OUT_API, 'verbs.json'), JSON.stringify(verbs, null, 2));
}

// 식 심볼
if (symbols && symbols.symbols) {
  let md = '---\ntitle: 조건식 심볼\ndescription: when 에서 읽을 수 있는 이름 전부\ngenerated: true\n---\n\n';
  md += '# 조건식 심볼\n\n';
  md += '`when:` 에서 읽을 수 있는 이름 전부입니다. 여기 없는 이름을 쓰면 `alice check` 가 잡습니다.\n\n';
  md += '| 이름 | 종류 | 타입 | 설명 |\n|---|---|---|---|\n';
  for (const s of symbols.symbols) {
    const kind = s.kind === 'function' ? `함수(${s.minArgs}~${s.maxArgs})` : '값';
    md += `| \`${s.name}\` | ${kind} | \`${s.type}\` | ${(s.summary || '').replace(/\|/g, '\\|')} |\n`;
  }
  write(path.join(OUT_REF, 'expression-symbols.md'), md);
  write(path.join(OUT_API, 'symbols.json'), JSON.stringify(symbols, null, 2));
}

// 진단 코드
if (codes && codes.codes) {
  let md = '---\ntitle: 진단 코드\ndescription: 왜 그 규칙이 있는가\ngenerated: true\n---\n\n';
  md += '# 진단 코드\n\n';
  md += '각 코드는 **안정 식별자**입니다. 문구는 바뀌어도 코드는 바뀌지 않습니다.\n\n';
  for (const c of codes.codes) {
    md += `## \`${c.code}\`\n\n${c.summary}\n\n${c.why}\n\n`;
    md += fence('yaml', `# 이렇게 쓰면 안 된다\n${c.wrong}\n\n# 이렇게 쓴다\n${c.right}`) + '\n\n';
  }
  write(path.join(OUT_REF, 'diagnostics.md'), md);
  write(path.join(OUT_API, 'diagnostics.json'), JSON.stringify(codes, null, 2));
}

// 저장소 문서를 위키로 복사
for (const [src, dst] of Object.entries(docMap)) {
  const from = path.join(REPO, src);
  if (!fs.existsSync(from)) continue;
  const body = wikiLinks(fs.readFileSync(from, 'utf8'), src);
  const title = (body.match(/^#\s+(.+)$/m) || [, path.basename(dst, '.md')])[1];
  const order = src === 'Docs/STATUS.md' ? 'order: 1\n' : '';
  write(path.join(WIKI, 'content', dst), `---\ntitle: ${title}\nsynced: ${src}\n${order}---\n\n${body}`);
}

console.log(`  스키마 ${schemaCount} · 동사 ${verbCount}`);
console.log('  완료');
