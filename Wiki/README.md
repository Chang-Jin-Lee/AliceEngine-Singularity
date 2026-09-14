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

Node.js 22+와 npm이 필요합니다. 저장소 루트에서 실행합니다.

```powershell
cd Wiki
npm ci
npm run build
npm start       # http://localhost:3000
```

`build`는 `out/`에 정적 사이트를 만듭니다. `start`와 `preview`는 그 결과를
127.0.0.1에서 제공합니다. 종료는 `Ctrl+C`, 다른 포트는 `npm start -- --port 3001`입니다.
편집할 때는 `npm run dev`로 변경 사항을 바로 확인할 수 있습니다.

현재 C++ 엔진은 문서 도구 CLI와 Null RHI까지 구현되어 있습니다.
게임 창과 에디터는 아직 없습니다. 위키의 `/guide/status/`에서 상태와 다음 작업을 확인하십시오.

`npm run dev` 와 `npm run build` 는 실행 전에 `scripts/generate.mjs` 를 돌립니다.
그 스크립트가 `alice` 를 호출해 레퍼런스 페이지를 새로 만듭니다.
실행 파일이 있으면 최신 엔진 데이터를 사용합니다. 없으면 커밋된 `Schemas/`와
`public/api/`의 스냅샷으로 전체 레퍼런스를 재생성합니다. 엔진 없이 위키만 빌드할 수 있으며,
엔진을 변경한 기여자는 바이너리를 다시 빌드하고 생성물도 함께 갱신해야 합니다.

```powershell
npm test        # 바이너리 없는 환경의 재생성, 정적 미리보기 HTTP 동작
```

## 배포

**공개 사이트는 아직 배포하지 않았습니다.** 로컬 미리보기 주소는 서버가 실행 중일 때만 열립니다.

Vercel에 GitHub 저장소를 연결하고 **Root Directory를 `Wiki`**로 지정합니다.
빌드 명령은 `npm run build`, 출력 디렉터리는 `out`입니다.
`ALICE_WIKI_URL=https://실제-도메인`을 설정하면 AI 문서 목록과 사이트맵에 배포 주소가 들어갑니다.

```bash
vercel --prod
```

또는 GitHub 저장소를 Vercel 에 연결하면 push 마다 자동 배포됩니다.
`vercel.json` 에 빌드 설정과 `.md` / `llms.txt` 의 Content-Type 헤더가 들어 있습니다.

기존 배포를 갱신하는 경우 생성된 레퍼런스와 `public/` 파일도 커밋에 포함하십시오.

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
