# CHR-04 · alice CLI

```
상태: 완료
크기: L
선행: CHR-03
담당: -
```

## 무엇을

10개 명령. 전부 `--json` 지원.

## 왜

AI 가 엔진과 대화하는 주 통로다. 사람용 출력과 기계용 JSON 이
**같은 데이터에서** 나오므로 어긋날 수 없다.

## 완료 기준

- [x] check / fmt / convert / new / schema / verbs / explain / doctor / version / help
- [x] 모든 명령에 `--json`
- [x] 종료 코드: 0 성공 / 1 검증 실패 / 2 사용법 오류
- [x] 모르는 명령·타입·코드에 가장 가까운 것을 제안

## 건드리는 파일

```
Tools/AliceCLI/*
```

## 건드리면 안 되는 것

엔진 모듈 내부
