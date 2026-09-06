// SPDX-License-Identifier: MIT
//
// content/ 를 public/ 으로 미러링하고 /llms.txt 를 만든다.
//
// 왜 이게 필요한가:
//   AI 가 문서 사이트를 읽으려면 보통 HTML 을 파싱해야 한다. 사이드바, 네비게이션,
//   각주가 전부 섞여 들어오고, 사이트마다 구조가 다르다.
//
//   여기서는 어떤 페이지든 URL 끝에 `.md` 를 붙이면 원문이 나온다.
//     /reference/schemas/alice-actor-1/      ← 사람
//     /reference/schemas/alice-actor-1.md    ← AI
//
//   그리고 /llms.txt 하나에 전체 지도가 있다. (llmstxt.org 규약)
//   크롤링할 필요가 없다.

import fs from 'node:fs';
import path from 'node:path';
import { fileURLToPath } from 'node:url';
import matter from 'gray-matter';

const HERE = path.dirname(fileURLToPath(import.meta.url));
const WIKI = path.resolve(HERE, '..');
const CONTENT = path.join(WIKI, 'content');
const PUBLIC = path.join(WIKI, 'public');

const SITE = process.env.ALICE_WIKI_URL || '';

function walk(dir, base = []) {
  if (!fs.existsSync(dir)) return [];
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) out.push(...walk(full, [...base, entry.name]));
    else if (entry.name.endsWith('.md')) out.push([...base, entry.name.replace(/\.md$/, '')]);
  }
  return out;
}

const slugs = walk(CONTENT);
const docs = slugs.map((slug) => {
  const file = path.join(CONTENT, ...slug) + '.md';
  const raw = fs.readFileSync(file, 'utf8');
  const { data, content } = matter(raw);
  return {
    slug,
    urlPath: '/' + slug.join('/') + '/',
    mdPath: '/' + slug.join('/') + '.md',
    title: data.title || slug[slug.length - 1],
    description: data.description || '',
    section: slug[0],
    group: slug.length > 2 ? slug[1] : '',
    generated: Boolean(data.generated),
    body: content,
    raw,
  };
});

// ── 1. .md 원문 미러 ────────────────────────────────────────────────
let mirrored = 0;
for (const doc of docs) {
  const target = path.join(PUBLIC, ...doc.slug) + '.md';
  fs.mkdirSync(path.dirname(target), { recursive: true });
  // 프론트매터를 벗기고 본문만 낸다. AI 에게는 본문이 필요하고,
  // 제목과 설명은 이미 llms.txt 에 있다.
  fs.writeFileSync(target, doc.body.trimStart() + '\n', 'utf8');
  mirrored++;
}

// ── 2. llms.txt ────────────────────────────────────────────────────
// llmstxt.org 규약. AI 가 사이트를 이해하기 위해 읽는 단 하나의 파일.
const sections = [
  { key: 'guide', title: '가이드' },
  { key: 'reference', title: '레퍼런스' },
];

let llms = '# AliceEngine-Singularity\n\n';
llms += '> AI가 다룰 수 있게 처음부터 설계한 게임 엔진.\n';
llms += '> 콘텐츠는 코드가 아니라 스키마로 검증되는 텍스트 문서이고,\n';
llms += '> 게임플레이는 스크립트가 아니라 선언적 규칙이다.\n\n';

llms += '이 사이트의 모든 페이지는 URL 끝에 `.md` 를 붙이면 마크다운 원문이 나온다.\n';
llms += 'HTML 을 파싱할 필요가 없다.\n\n';

llms += '## 기계용 데이터\n\n';
llms += '엔진이 손에 있다면 웹을 거칠 필요가 없다. 괄호 안이 같은 것을 주는 로컬 명령이다.\n\n';
llms += `- [/api/schemas.json](${SITE}/api/schemas.json): 만들 수 있는 문서 타입 전부 (\`alice schema list --json\`)\n`;
llms += `- [/api/verbs.json](${SITE}/api/verbs.json): 콘텐츠가 쓸 수 있는 동사 전부와 인자 스키마 (\`alice verbs --json\`)\n`;
llms += `- [/api/symbols.json](${SITE}/api/symbols.json): 조건식에서 읽을 수 있는 이름 (\`alice verbs --symbols --json\`)\n`;
llms += `- [/api/diagnostics.json](${SITE}/api/diagnostics.json): 진단 코드와 설명 (\`alice explain --list --json\`)\n\n`;

for (const section of sections) {
  const items = docs.filter((d) => d.section === section.key);
  if (!items.length) continue;
  llms += `## ${section.title}\n\n`;

  const groups = new Map();
  for (const doc of items) {
    if (!groups.has(doc.group)) groups.set(doc.group, []);
    groups.get(doc.group).push(doc);
  }

  for (const [group, list] of [...groups.entries()].sort(([a], [b]) => a.localeCompare(b))) {
    if (group) llms += `### ${group}\n\n`;
    for (const doc of list) {
      const desc = doc.description ? `: ${doc.description}` : '';
      llms += `- [${doc.title}](${SITE}${doc.mdPath})${desc}\n`;
    }
    llms += '\n';
  }
}

llms += '## 시작하는 법\n\n';
llms += '엔진을 클론했다면 이 순서로 물어보라. 추측할 필요가 없다.\n\n';
llms += '```bash\n';
llms += 'alice schema list --json      # 만들 수 있는 문서 타입\n';
llms += 'alice verbs --json            # 할 수 있는 동작\n';
llms += 'alice verbs --symbols --json  # 조건식에서 읽을 수 있는 이름\n';
llms += 'alice new <타입> <이름>        # 올바른 뼈대\n';
llms += 'alice check <경로> --json     # 맞는지 확인\n';
llms += 'alice explain <진단코드>       # 왜 그 규칙이 있는지\n';
llms += '```\n';

fs.mkdirSync(PUBLIC, { recursive: true });
fs.writeFileSync(path.join(PUBLIC, 'llms.txt'), llms, 'utf8');

// ── 3. llms-full.txt — 전문을 한 파일에 ────────────────────────────
// 컨텍스트가 넉넉한 AI 는 이 하나만 읽으면 사이트 전체를 안다.
let full = llms + '\n\n---\n\n';
for (const doc of docs) {
  full += `\n\n<!-- ${doc.mdPath} -->\n\n`;
  full += doc.body.trimStart() + '\n';
}
fs.writeFileSync(path.join(PUBLIC, 'llms-full.txt'), full, 'utf8');

// ── 4. robots.txt ──────────────────────────────────────────────────
fs.writeFileSync(
  path.join(PUBLIC, 'robots.txt'),
  [
    '# AI 크롤러를 환영한다. 그러라고 만든 사이트다.',
    'User-agent: *',
    'Allow: /',
    '',
    SITE ? `Sitemap: ${SITE}/sitemap.xml` : '# Sitemap: <ALICE_WIKI_URL 을 설정하면 여기 채워진다>',
    '',
  ].join('\n'),
  'utf8',
);

// ── 5. sitemap.xml ─────────────────────────────────────────────────
let sitemap = '<?xml version="1.0" encoding="UTF-8"?>\n';
sitemap += '<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9">\n';
sitemap += `  <url><loc>${SITE}/</loc></url>\n`;
for (const doc of docs) {
  sitemap += `  <url><loc>${SITE}${doc.urlPath}</loc></url>\n`;
}
sitemap += '</urlset>\n';
fs.writeFileSync(path.join(PUBLIC, 'sitemap.xml'), sitemap, 'utf8');

console.log(`  .md 미러 ${mirrored} · llms.txt (${(llms.length / 1024).toFixed(1)}KB) · llms-full.txt (${(full.length / 1024).toFixed(1)}KB)`);
