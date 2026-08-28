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
│   ├── windows/
│   ├── macos/
│   └── linux/
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

## 경로 규칙

- CMake와 소스 코드에 개발자 PC의 절대경로를 넣지 않는다.
- 프로젝트 내부 경로는 `${CMAKE_CURRENT_SOURCE_DIR}` 또는 `${PROJECT_SOURCE_DIR}`를 기준으로 구성한다.
- 형제 프로젝트와 SDK는 프로젝트 루트 기준 상대경로로 참조한다.
- 기본 Vulkan SDK 경로는 `../VulkanSdk/<platform>` 형식을 사용한다.
- 외부 경로가 필요한 경우 CMake cache 변수로 재정의할 수 있게 하되, 기본값은 상대경로로 둔다.
- 런타임 리소스는 현재 작업 디렉터리가 아니라 실행 파일 또는 설치 리소스 루트를 기준으로 찾는다.
- `C:/...`, `D:/...`, `/Users/...`, `/home/...` 형태의 개인 환경 경로를 커밋하지 않는다.

## 크로스플랫폼 규칙

- Windows, macOS, Linux에서 동일한 공개 API와 Widget 동작을 제공한다.
- 공통 코드는 운영체제별 `#ifdef`로 흩뜨리지 않고 platform 및 renderer 구현 계층으로 분리한다.
- CMake는 `WIN32`, `APPLE`, `UNIX`별 설정을 제공하고 어느 한 플랫폼의 경로 또는 라이브러리를 공통 타깃에 강제하지 않는다.
- Vulkan SDK 기본 경로는 프로젝트 기준 `../VulkanSdk/Win`, `../VulkanSdk/Apple`, `../VulkanSdk/Linux`처럼 선택한다.
- macOS에서는 Vulkan 호환 계층으로 MoltenVK를 사용한다.
- Vulkan 및 플랫폼별 CMake 구성은 `../3dEngine`을 참고할 수 있지만 코드를 직접 의존하거나 링크하지 않는다.
- LotUI는 `3dEngine` 없이도 독립적으로 구성, 빌드, 설치할 수 있어야 한다.

## 기술 선택

- 언어: C++17 이상
- 빌드: CMake
- 플랫폼 및 입력: OS별 네이티브 백엔드(Win32, Cocoa, X11/Wayland)
- 기본 렌더러: Vulkan
- 글리프 생성: FreeType
- 문자 shaping: HarfBuzz
- 리소스 관리: RAII
- 수동 new/delete 사용 금지

## 의존성 규칙

- core는 Vulkan과 OS 네이티브 타입을 알지 못한다.
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

1. OS별 네이티브 창 생성
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

## VulkanCAD 직접 연동 목표

LotUI는 독립 프로젝트로 유지하면서, 실제 VulkanCAD 엔진에 외부 라이브러리로 직접 연결해 검증한다.

- LotUI 저장소는 VulkanCAD 클래스나 소스 코드에 직접 의존하지 않는다.
- VulkanCAD 전용 접착 코드와 어댑터는 VulkanCAD 저장소 쪽에 둔다.
- VulkanCAD가 이미 소유한 `VkInstance`, `VkPhysicalDevice`, `VkDevice`, 그래픽 큐, RenderPass 또는 동적 렌더링 정보와 `VkCommandBuffer`를 LotUI 임베디드 렌더러에 전달한다.
- 임베디드 렌더러는 VulkanCAD의 Surface, Swapchain, Vulkan 객체를 생성하거나 파괴하지 않고 `Present`도 호출하지 않는다.
- LotUI가 처리한 입력과 CAD 뷰포트로 전달할 입력을 명확히 구분한다.
- CAD 뷰포트 영역, UI 패널, 팝업 및 대화상자를 같은 프레임 안에서 합성한다.
- 논리 좌표와 Swapchain 물리 픽셀 사이의 DPI 스케일을 Windows, macOS, Linux에서 검증한다.

## VulkanCAD 통합 시험

직접 연결 가능 여부는 설계 검토만으로 판정하지 않고 실제 엔진 빌드와 실행으로 확인한다.

- LotUI 내부의 외부 Vulkan 컨텍스트 계약 테스트는 `tests/`에 둔다.
- 실제 엔진 연동 예제는 형제 프로젝트 `../3dEngine/samples/lotui_smoke/`에 둔다.
- 통합 시험 실행 파일의 CMake target 이름은 `VulkanAppLotGUI`로 하고 Windows 결과물은 `VulkanAppLotGUI.exe`로 만든다.
- 기존 `VulkanApp`은 비교와 안전한 폴백을 위해 연동 시험 중 변경 없이 실행 가능해야 한다.
- 첫 연동 화면에는 실제 CAD 뷰포트 위의 LotUI Button과 Modal/Modeless Dialog를 포함한다.
- 클릭 입력 분배, 창 크기 변경, DPI, 한글 IME, 포커스, 종료 시 Vulkan 객체 수명을 확인한다.
- 통합 시험이 안정되면 `VulkanAppLotGUI`의 구성을 기본 `VulkanApp`에 단계적으로 적용한다.

## 라이브러리 배포 및 연결 순서

1. VulkanCAD는 형제 저장소의 LotGUI를 상대경로 `add_subdirectory`로 가져와 정적으로 연결한다.
2. `VulkanAppLotGUI` 통합 시험도 정적 연결을 사용하며 LotUI DLL을 VulkanCAD 배포 필수 파일로 만들지 않는다.
3. LotGUI의 `.cpp` 목록을 VulkanCAD CMake에 직접 복사하지 않고 항상 `LotUI::` namespace target을 연결한다.
4. 외부 개발자용 배포에서는 소스/CMake 방식과 정적 라이브러리를 제공한다.
5. 외부 개발자용 공유 라이브러리는 Windows의 `LotUI.dll`, macOS의 `libLotUI.dylib`, Linux의 `libLotUI.so`로 별도 consumer smoke test를 수행한다.
6. 설치 가능한 CMake package를 제공해 `find_package(LotUI CONFIG REQUIRED)`로 사용할 수 있게 한다.
7. CMake 기본 경로는 형제 저장소 기준 상대경로로 유지하고 사용자가 cache 변수로 재정의할 수 있게 한다.

## 위젯 개발 완료 기준

새 위젯과 UI 기능은 다음 조건을 함께 만족해야 완료로 본다.

- LotUI 단독 예제에서 동작한다.
- core와 widgets 공개 API에 VulkanCAD 또는 OS 네이티브 타입이 노출되지 않는다.
- 독립형 Vulkan 렌더러와 외부 CommandBuffer를 사용하는 임베디드 렌더러에서 표현 가능하다.
- UI가 처리하지 않은 포인터와 키보드 입력을 VulkanCAD가 계속 받을 수 있다.
- Windows, macOS, Linux의 빌드 구성을 깨뜨리지 않는다.
- 가능해지는 시점부터 `VulkanAppLotGUI`에서 실제 CAD 사용 흐름으로 검증한다.

## 초기 비목표

- Qt 전체 기능 대체
- 웹 브라우저 수준 CSS
- HTML 파서
- 복잡한 애니메이션 시스템
- 모바일 플랫폼
- 완전한 접근성 지원

이 기능들은 기본 구조가 안정된 후 검토한다.
