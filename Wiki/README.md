# 위키

사람이 읽는 문서 사이트이자, **AI 가 크롤링하는 데이터 소스**입니다.

## 다른 위키와 다른 점

| | 보통의 문서 사이트 | 여기 |
|---|---|---|
| AI 가 읽으려면 | HTML 을 파싱해야 한다 | 같은 URL 에 `.md` 를 붙이면 원문이 나온다 |
| 전체 구조 파악 | 사이트맵을 크롤링 | `/llms.txt` 한 파일 |
| 레퍼런스 정확성 | 손으로 쓴다 → 코드와 어긋난다 | **엔진에서 생성한다** |
| 기계용 데이터 | 없다 | `/api/schemas.json`, `/api/verbs.json` |

```
https://<도메인>/reference/schemas/alice-actor-1/        ← 사람
https://<도메인>/reference/schemas/alice-actor-1.md      ← AI (원문)
https://<도메인>/llms.txt                                 ← 전체 지도
https://<도메인>/api/verbs.json                           ← 기계용 덤프
```

## 로컬 실행

```bash
cd Wiki
npm install
npm run dev      # http://localhost:3000
```

`npm run dev` 와 `npm run build` 는 실행 전에 `scripts/generate.mjs` 를 돌립니다.
그 스크립트가 `alice` 를 호출해 레퍼런스 페이지를 새로 만듭니다.
그래서 **레퍼런스가 코드와 어긋날 수 없습니다.**

## 배포

```bash
vercel --prod
```

또는 GitHub 저장소를 Vercel 에 연결하면 push 마다 자동 배포됩니다.
`vercel.json` 에 빌드 설정과 `.md` / `llms.txt` 의 Content-Type 헤더가 들어 있습니다.

## 내용을 추가하려면

```
content/
  guide/          손으로 쓴다 — 개념, 튜토리얼
  reference/      자동 생성 — 손대지 마라 (덮어쓰인다)
```

`content/guide/*.md` 에 파일을 만들면 자동으로 페이지가 됩니다.

```markdown
---
title: 제목
description: 한 줄 설명
order: 3
---

본문
```
