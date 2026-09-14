# Wiki/ 에서 일할 때

> 루트의 [`AGENTS.md`](../AGENTS.md) 를 먼저 읽으십시오.

## 손대면 안 되는 것

```
content/reference/     ← 생성물. alice 를 호출해 만든다
public/api/            ← 생성물
public/**/*.md         ← 생성물 (content/ 의 미러)
public/llms*.txt       ← 생성물
public/sitemap.xml     ← 생성물
```

여기를 손으로 고치면 다음 `npm run generate` 에서 사라집니다.
**레퍼런스를 고치려면 엔진 코드(`CoreSchemas.cpp`, `CoreVerbs.cpp`)를 고치십시오.**

## 손으로 쓰는 것

```
content/guide/         개념 설명, 튜토리얼
app/                   레이아웃, 스타일
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

`Docs/` 의 문서 여덟 개는 `scripts/generate.mjs` 가 `content/guide/` 로 동기화합니다.
그것들도 손대지 말고 `Docs/` 원본을 고치십시오.

## 왜 .md 미러와 llms.txt 가 있는가

AI 가 문서 사이트를 읽으려면 보통 HTML 을 파싱해야 합니다.
사이드바, 네비게이션, 각주가 섞여 들어오고 사이트마다 구조가 다릅니다.

여기서는:

```
/reference/schemas/alice-actor-1/      ← 사람
/reference/schemas/alice-actor-1.md    ← AI (원문)
/llms.txt                              ← 전체 지도 (5KB)
/llms-full.txt                         ← 전문 (64KB)
/api/verbs.json                        ← 기계용 덤프
```

**이 구조를 깨뜨리는 변경은 하지 마십시오.** 이 사이트의 존재 이유입니다.

## 작업 흐름

```bash
cd Wiki
npm ci
npm run dev        # generate 후 http://localhost:3000
npm run build      # generate 후 정적 빌드 → out/
npm start          # out/ 로컬 미리보기 → http://localhost:3000
npm test           # 생성·미리보기 회귀 검사
```

`generate` 는 `../build/bin/alice` 를 찾아 호출합니다.
없으면 저장소의 `Schemas/` 및 `public/api/` 스냅샷으로 전체 레퍼런스를 재생성합니다.
엔진을 변경했다면 **엔진을 먼저 빌드하십시오.** 위키만 보는 환경은 스냅샷으로 빌드할 수 있습니다.

## 커밋 전

```bash
npm run generate
git status Wiki/content/reference Wiki/public   # 변경이 있으면 함께 커밋
```

CI 가 생성물이 최신인지 검사합니다.
