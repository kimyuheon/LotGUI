# AGENTS.md - LotUI

## 프로젝트 목표

C++ 기반 크로스플랫폼 GUI 프레임워크를 개발한다.

- Windows, macOS, Linux 우선 지원
- Vulkan을 첫 번째 렌더링 백엔드로 사용
- 향후 Metal, Direct3D, OpenGL 백엔드 추가 가능
- Vulkan CAD 엔진과 독립된 별도 프로젝트
- 완성 후 CAD 엔진에서 외부 라이브러리로 사용

## 핵심 구조

UI는 Retained Mode로 관리하고, 매 프레임 PaintCommand를 생성해 렌더링한다.

Widget Tree
→ Layout
→ Input / Focus
→ Paint Commands
→ Renderer

## 모듈 구조

LotUI/
├── core/
│   ├── widget
│   ├── layout
│   ├── event
│   ├── focus
│   ├── style
│   └── paint_command
├── platform/
│   └── sdl3/
├── renderer/
│   └── vulkan/
├── text/
│   ├── freetype
│   └── harfbuzz
├── widgets/
│   ├── label
│   ├── button
│   ├── checkbox
│   ├── slider
│   ├── text_field
│   ├── dialog
│   ├── popup
│   ├── scroll_view
│   └── tree_view
├── examples/
└── tests/

## 기술 선택

- 언어: C++17 이상
- 빌드: CMake
- 플랫폼 및 입력: SDL3
- 기본 렌더러: Vulkan
- 글리프 생성: FreeType
- 문자 shaping: HarfBuzz
- 리소스 관리: RAII
- 수동 new/delete 사용 금지

## 의존성 규칙

- core는 Vulkan, SDL 타입을 알지 못한다.
- platform 계층은 창, 입력, DPI, 클립보드, IME를 담당한다.
- renderer 계층은 PaintCommand만 입력받는다.
- widgets는 특정 렌더링 API에 의존하지 않는다.
- CAD 엔진 클래스와 코드를 직접 가져오지 않는다.

## 기본 렌더 명령

struct PaintCommand {
    Rect bounds;
    Rect clip;
    Color color;
    TextureId texture;
    float cornerRadius;
};

- 사각형은 인스턴싱으로 렌더링
- clip은 Vulkan scissor 사용
- 문자는 glyph atlas 사용
- texture 또는 clip 변경을 기준으로 batch 처리

## 개발 순서

1. SDL3 창 생성
2. Vulkan Surface와 Swapchain
3. 사각형 렌더링
4. PaintCommand와 clipping
5. 마우스 hit test와 pointer capture
6. Row/Column 레이아웃
7. Label, Button, Checkbox, Slider
8. 포커스와 키보드 탐색
9. TextField와 한글 IME
10. Dialog, Popup, ScrollView
11. TreeView와 PropertyGrid
12. Docking
13. Vulkan CAD 엔진 연동 예제

## 첫 번째 마일스톤

Windows/macOS/Linux에서 동일하게 실행되는 다음 기능을 구현한다.

- 창
- Label
- Button
- 숫자 입력
- Checkbox
- Modal Dialog
- 한글 입력
- DPI Scaling

## 초기 비목표

- Qt 전체 기능 대체
- 웹 브라우저 수준 CSS
- HTML 파서
- 복잡한 애니메이션 시스템
- 모바일 플랫폼
- 완전한 접근성 지원

이 기능들은 기본 구조가 안정된 후 검토한다.
