/** @type {import('next').NextConfig} */
const nextConfig = {
  // 정적 내보내기. Vercel 은 물론 GitHub Pages, S3, 어디든 올라간다.
  // 문서 사이트에 서버가 필요할 이유가 없다.
  output: 'export',
  trailingSlash: true,
  images: { unoptimized: true },
};

export default nextConfig;
