import { marked } from 'marked';
import { getDoc, listDocs } from '../../lib/content';
import { notFound } from 'next/navigation';

marked.setOptions({ gfm: true, breaks: false });

export function generateStaticParams() {
  return listDocs().map((doc) => ({ slug: doc.slug }));
}

export async function generateMetadata({ params }) {
  const { slug } = await params;
  const doc = getDoc(slug);
  if (!doc) return {};
  return {
    title: doc.meta.title,
    description: doc.meta.description,
  };
}

export default async function DocPage({ params }) {
  const { slug } = await params;
  const doc = getDoc(slug);
  if (!doc) notFound();

  const mdHref = '/' + slug.join('/') + '.md';
  const html = marked.parse(doc.body);

  return (
    <article className="doc">
      <div className="doc-tools">
        <a href={mdHref} className="pill" title="이 페이지의 마크다운 원문">
          .md 원문
        </a>
        {doc.meta.generated && (
          <span className="pill pill-generated" title="엔진 코드에서 생성된 페이지입니다. 손으로 고치지 마십시오.">
            자동 생성
          </span>
        )}
        {doc.meta.synced && (
          <span className="pill pill-dim" title={`저장소의 ${doc.meta.synced} 와 동기화됩니다`}>
            {doc.meta.synced}
          </span>
        )}
      </div>

      <div dangerouslySetInnerHTML={{ __html: html }} />
    </article>
  );
}
