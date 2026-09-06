# AliceEngine-Singularity

Multiple Engines. One AI-Native Engine. 여러 자체엔진을 하나로 통합하고 AI 친화된 하나의 엔진으로 만드는 파멸적인 프로젝트

## AI Agents

| Agent      | 역할         | 책임                                              |
| ---------- | ---------- | ----------------------------------------------- |
| **Alice**  | Architect  | 인터페이스, 모듈 경계, 의존성 규칙 결정                         |
| **Sidney** | Runtime    | ECS/GameObject, Scene, Memory, Serialization 담당 |
| **Monday** | Graphics   | DX11 Renderer, Material, Shader, RenderGraph 담당 |
| **Chrono** | Tools & AI | Tool API, JSON/Schema, 명령 인터페이스, 자동화 담당         |
| **Seeho**  | Review     | 코드 리뷰, 테스트, 빌드, 성능 Regression 검증                |
