import fs from 'node:fs';
import path from 'node:path';
import matter from 'gray-matter';

const CONTENT = path.join(process.cwd(), 'content');

const SECTIONS = [
  { dir: 'guide', title: '가이드', order: 1 },
  { dir: 'reference', title: '레퍼런스', order: 2 },
];

function walk(dir, base = []) {
  if (!fs.existsSync(dir)) return [];
  const out = [];
  for (const entry of fs.readdirSync(dir, { withFileTypes: true }).sort((a, b) => a.name.localeCompare(b.name))) {
    const full = path.join(dir, entry.name);
    if (entry.isDirectory()) {
      out.push(...walk(full, [...base, entry.name]));
    } else if (entry.name.endsWith('.md')) {
      out.push([...base, entry.name.replace(/\.md$/, '')]);
    }
  }
  return out;
}

/** content/ 아래의 모든 문서. slug 배열과 프론트매터를 함께 준다. */
export function listDocs() {
  return walk(CONTENT).map((slug) => {
    const file = path.join(CONTENT, ...slug) + '.md';
    const { data, content } = matter(fs.readFileSync(file, 'utf8'));
    return {
      slug,
      href: '/' + slug.join('/') + '/',
      mdHref: '/' + slug.join('/') + '.md',
      title: data.title || slug[slug.length - 1],
      description: data.description || '',
      section: slug[0],
      order: data.order ?? 999,
      generated: Boolean(data.generated),
      wordCount: content.split(/\s+/).length,
    };
  });
}

export function getDoc(slug) {
  const file = path.join(CONTENT, ...slug) + '.md';
  if (!fs.existsSync(file)) return null;
  const { data, content } = matter(fs.readFileSync(file, 'utf8'));
  return { meta: data, body: content, slug };
}

/** 사이드바 트리. 섹션 → 하위 그룹 → 문서. */
export function buildNav() {
  const docs = listDocs();
  return SECTIONS.map((section) => {
    const items = docs.filter((d) => d.section === section.dir);
    const groups = new Map();
    for (const doc of items) {
      const group = doc.slug.length > 2 ? doc.slug[1] : '';
      if (!groups.has(group)) groups.set(group, []);
      groups.get(group).push(doc);
    }
    for (const list of groups.values()) {
      list.sort((a, b) => a.order - b.order || a.title.localeCompare(b.title));
    }
    return {
      ...section,
      groups: [...groups.entries()]
        .sort(([a], [b]) => a.localeCompare(b))
        .map(([name, docs]) => ({ name, docs })),
    };
  }).filter((s) => s.groups.length > 0);
}
