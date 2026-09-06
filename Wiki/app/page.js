import Link from 'next/link';
import { listDocs } from '../lib/content';

export default function Home() {
  const docs = listDocs();
  const schemas = docs.filter((d) => d.slug[1] === 'schemas');
  const verbs = docs.filter((d) => d.slug[1] === 'verbs');
  const guides = docs.filter((d) => d.section === 'guide');

  return (
    <article className="doc">
      <div className="hero">
        <h1>AliceEngine-Singularity</h1>
        <p className="lede">
          콘텐츠는 코드가 아니라 텍스트 문서입니다.<br />
          게임플레이는 스크립트가 아니라 선언적 규칙입니다.<br />
          그리고 <strong>맞는지 확인하는 데 에디터를 켤 필요가 없습니다.</strong>
        </p>
      </div>

      <pre className="hero-code"><code>{`schema: alice/behavior/1
name: PlayerMovement

rules:
  - when: input.pressed("Jump") and physics.grounded
    do:
      - physics.impulse: { direction: [0, 1, 0], force: 6.5 }
      - audio.play:      { sound: sounds/jump.sound.yaml }`}</code></pre>

      <pre className="hero-code dim-block"><code>{`$ alice check Content/
  10개 문서 · 0개 오류 · 0개 경고  (73.90 ms)`}</code></pre>

      <div className="cards">
        <Link href="/guide/content-format/" className="card">
          <h3>콘텐츠 문서 문법</h3>
          <p>여기서 시작하십시오. 문서 하나로 콘텐츠를 만들 수 있습니다.</p>
        </Link>
        <Link href="/reference/expression-symbols/" className="card">
          <h3>조건식 심볼</h3>
          <p><code>when:</code> 에서 읽을 수 있는 이름 전부.</p>
        </Link>
        <Link href="/reference/diagnostics/" className="card">
          <h3>진단 코드</h3>
          <p>왜 그 규칙이 있는지. 각 코드는 안정 식별자입니다.</p>
        </Link>
        <Link href="/guide/engine-survey/" className="card">
          <h3>참고 엔진 조사</h3>
          <p>20개 엔진에서 무엇을 가져오고 무엇을 버렸는가.</p>
        </Link>
      </div>

      <h2>레퍼런스</h2>
      <p className="dim">
        아래 목록은 엔진 코드에서 생성됩니다. 문서 타입 {schemas.length}개, 동사 {verbs.length}개.
      </p>

      <div className="two-col">
        <div>
          <h3>문서 타입</h3>
          <ul className="tight">
            {schemas.map((d) => (
              <li key={d.href}><Link href={d.href}>{d.title}</Link></li>
            ))}
          </ul>
        </div>
        <div>
          <h3>동사</h3>
          <ul className="tight">
            {verbs.map((d) => (
              <li key={d.href}><Link href={d.href}><code>{d.title}</code></Link></li>
            ))}
          </ul>
        </div>
      </div>

      <h2>AI 를 위한 안내</h2>
      <p>이 사이트는 AI 가 읽을 것을 전제로 만들어졌습니다.</p>
      <table>
        <thead><tr><th>원하는 것</th><th>주소</th></tr></thead>
        <tbody>
          <tr><td>전체 구조 한 파일</td><td><a href="/llms.txt"><code>/llms.txt</code></a></td></tr>
          <tr><td>아무 페이지의 원문</td><td><code>&lt;그 페이지 주소&gt;.md</code></td></tr>
          <tr><td>문서 타입 목록</td><td><a href="/api/schemas.json"><code>/api/schemas.json</code></a></td></tr>
          <tr><td>동사 전체와 인자 스키마</td><td><a href="/api/verbs.json"><code>/api/verbs.json</code></a></td></tr>
          <tr><td>조건식 심볼</td><td><a href="/api/symbols.json"><code>/api/symbols.json</code></a></td></tr>
          <tr><td>진단 코드</td><td><a href="/api/diagnostics.json"><code>/api/diagnostics.json</code></a></td></tr>
        </tbody>
      </table>
      <p className="dim">
        엔진이 손에 있다면 웹을 거칠 필요가 없습니다. <code>alice verbs --json</code> 이 같은 것을 줍니다.
      </p>

      <h2>가이드</h2>
      <ul className="tight">
        {guides.map((d) => (
          <li key={d.href}>
            <Link href={d.href}>{d.title}</Link>
            {d.description && <span className="dim"> — {d.description}</span>}
          </li>
        ))}
      </ul>
    </article>
  );
}
