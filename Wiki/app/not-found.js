import Link from 'next/link';

export default function NotFound() {
  return (
    <article className="doc">
      <h1>없는 페이지입니다</h1>
      <p>
        주소를 확인하거나 <Link href="/">처음으로</Link> 돌아가십시오.
      </p>
      <p className="dim">
        AI 라면 <a href="/llms.txt">/llms.txt</a> 에 전체 목록이 있습니다.
      </p>
    </article>
  );
}
