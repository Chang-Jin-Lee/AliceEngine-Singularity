import './globals.css';
import Link from 'next/link';
import { buildNav } from '../lib/content';

export const metadata = {
  title: { default: 'AliceEngine-Singularity 문서', template: '%s · AliceEngine' },
  description: 'AI가 다룰 수 있게 처음부터 설계한 게임 엔진의 문서',
};

const GROUP_LABEL = {
  '': '',
  schemas: '문서 타입',
  verbs: '동사',
};

export default function RootLayout({ children }) {
  const nav = buildNav();

  return (
    <html lang="ko">
      <body>
        <a className="skip" href="#main">본문으로</a>

        <header className="topbar">
          <Link href="/" className="brand">
            <span className="brand-mark">◆</span>
            <span>AliceEngine<span className="brand-dim">-Singularity</span></span>
          </Link>
          <nav className="topnav">
            <a href="/llms.txt">llms.txt</a>
            <a href="/api/verbs.json">API</a>
            <a href="https://github.com/">GitHub</a>
          </nav>
        </header>

        <div className="shell">
          <aside className="sidebar">
            {nav.map((section) => (
              <section key={section.dir}>
                <h2>{section.title}</h2>
                {section.groups.map((group) => (
                  <div key={group.name} className="navgroup">
                    {group.name && GROUP_LABEL[group.name] !== '' && (
                      <h3>{GROUP_LABEL[group.name] || group.name}</h3>
                    )}
                    <ul>
                      {group.docs.map((doc) => (
                        <li key={doc.href}>
                          <Link href={doc.href}>{doc.title}</Link>
                        </li>
                      ))}
                    </ul>
                  </div>
                ))}
              </section>
            ))}
          </aside>

          <main id="main">{children}</main>
        </div>

        <footer className="footer">
          <p>
            이 사이트의 레퍼런스는 엔진 코드에서 <strong>생성</strong>됩니다. 손으로 쓰지 않으므로 어긋나지 않습니다.
          </p>
          <p className="dim">
            AI 라면 <a href="/llms.txt">/llms.txt</a> 부터 읽으십시오. 어떤 페이지든 URL 끝에
            <code>.md</code> 를 붙이면 원문이 나옵니다.
          </p>
        </footer>
      </body>
    </html>
  );
}
