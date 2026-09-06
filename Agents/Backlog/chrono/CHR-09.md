# CHR-09 · MCP 서버

```
상태: 대기
크기: M
선행: CHR-01
담당: -
```

## 무엇을

외부 AI(Claude Desktop, Cursor 등)가 이 엔진을 도구로 쓸 수 있게 한다.

## 왜

`alice --json` 이 이미 기계용 인터페이스지만, MCP 를 지원하면 AI 도구들이
**설정 없이** 엔진에 붙는다.

SpartanEngine 이 이미 이걸 했다 (`source/mcp/`, TCP 47777).
구조는 참고할 만하다: 서버 스레드가 요청을 받고, 큐가 메인 스레드로 마샬링하고,
핸들러가 등록제로 붙는다.

## 완료 기준

- [ ] MCP 서버 (stdio + TCP)
- [ ] 도구: check, schema, verbs, dump, apply, logs, profile
- [ ] Claude Desktop 설정 예시 제공
- [ ] 엔진이 안 떠 있어도 문서 도구는 동작한다

## 건드리는 파일

```
Engine/Bridge/Mcp/*
Tools/
```

## 건드리면 안 되는 것

CLI 명령의 의미
