# ScarletNexus — UE5 전투 · AI 시스템 리메이크

> 반다이남코의 **SCARLET NEXUS**를 레퍼런스로 삼아, Unreal Engine 5 + C++로 전투/AI 시스템을 직접 설계·구현한 개인 포트폴리오 프로젝트입니다. (원티드 포텐업 AI 에이전트 & 언리얼 개발 협업과정 진행 중 제작)

**Language:** [English](README.md) · 한국어 · [日本語](README.ja.md)

![Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?logo=unrealengine)
![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=cplusplus)
![AI](https://img.shields.io/badge/AI-StateTree-orange)
![Status](https://img.shields.io/badge/Status-Portfolio%20%2F%20Study-blue)

---

## 목차

- [소개](#소개)
- [핵심 시스템](#핵심-시스템)
- [기술 스택](#기술-스택)
- [아키텍처 노트](#아키텍처-노트)
- [프로젝트 구조](#프로젝트-구조)
- [실행 방법](#실행-방법)
- [데모](#데모)
- [고지 사항](#고지-사항)
- [Author](#author)

---

## 소개

이 프로젝트는 액션 RPG의 핵심이라 할 수 있는 **보스 전투 AI**, **파티(동료) AI**, **염력(Psychokinesis) 오브젝트 상호작용**, **플레이어 콤보 전투**를 원작 게임의 구조를 참고해 밑바닥부터 C++로 구현해본 기술 학습용 프로젝트입니다.

목표는 단순히 "비슷하게 움직이는" 결과물을 만드는 것이 아니라, **StateTree 기반 AI 아키텍처**, **인터페이스로 분리된 전투 규약**, **오브젝트 풀링**처럼 실무에서 요구되는 구조적인 설계를 스스로 세워보는 데 있었습니다. 플레이어블 캐릭터로 유이토(Yuito), 카사네(Kasane), 하나비(Hanabi)를 다루며, 세 캐릭터 모두 하나의 `APlayerCharacterBase`를 상속해 이동·회피·염력 사용 등 공통 동작을 공유하고, 카사네는 자신만의 부유형 블레이드 전투를 별도 컴포넌트로 확장합니다.

## 핵심 시스템

### 보스 AI — StateTree 기반 4페이즈 전투

- `UBossConfigDataAsset`(DataAsset)에 페이즈별 HP 임계값, 잠금 해제 패턴, 공격 데이터(선딜 몽타주/데미지/사거리/쿨다운/선택 가중치)를 테이블화하여 기획 데이터와 로직을 분리했습니다.
- Phase 1(기본 패턴 4종) → Phase 2(전류구 추가 + 맵 컬러 변경) → Phase 2-2(염동력 사물 던지기 추가) → Phase 3(컷씬 후 패턴 반복) 순으로 HP 기준 자동 전환되며, 이전 페이즈 패턴이 누적되는 구조입니다.
- `STTask_BossSelectAttack`, `STTask_BossAttackExecutor`, `STTask_BossPhaseTransition`, `STTask_BossStagger`, `STTask_BosshitReaction`, `STTask_BossTeleport`, `STTask_BossDeath` 등 StateTree 커스텀 Task/Evaluator로 패턴 선택 → 실행 → 피격 반응 → 페이즈 전환의 전체 루프를 구성했습니다.
- 텔레포트 킥, 3연속 분신 돌진(`ABossCloneActor`), 전류구 투척, 얼음가시, 염동력 투척 등 페이즈별 고유 패턴을 구현했고, `UBossSuperArmorComponent`(슈퍼아머)와 `IStaggerable`(경직 게이지) 인터페이스로 상호 배타적인 피격 판정을 분리했습니다.
- 피격 반응은 `EHitReactionType`(경직/강타/넉백/에어본) × `EHitDirection`(전후좌우) 조합으로 세분화했습니다.

### 파티 AI — 플레이어와 동일한 StateTree 아키텍처

- 보스뿐 아니라 파티원(`APartyMemberBase`)과 일반 몬스터(`AEnemyBase`)까지 **동일한 StateTree 구조**로 통일해서 설계했습니다. `PartyEvaluator` / `EnemyEvaluator`가 매 틱 최근접 타겟과 사거리 진입 여부를 계산해 Task에 데이터를 넘기고, `PCTask_Idle/Chase/Attack/PK`가 그 값을 받아 행동을 결정하는 구조입니다.
- `APartyAIController`는 `AIPerception`(시야 센스)으로 플레이어를 감지해 자동으로 추적·전투에 합류하며, 파티원도 `PCTask_PK` + `PKTask_FindObject/LiftObject/ThrowObject`를 통해 염력 오브젝트를 자율적으로 찾아 들고 던질 수 있습니다.
- 일반 몬스터는 데이터 테이블 기반 `AEnemyManager`가 웨이브 단위로 스폰을 관리하며, `OnWaveStarted` / `OnWaveCleared` / `OnAllEnemiesDefeated` 델리게이트로 이후 연출(다음 웨이브, 보스 룸 오픈 등)과 느슨하게 연결됩니다.

### 염력(Psychokinesis) 시스템

- `IPKInteractable` 인터페이스(집기 가능 여부/피킹/해제/던지기)로 "염력으로 다룰 수 있는 오브젝트"의 규약을 정의하고, `UPKComponent`가 시야각(FOV Dot Product) 기반 타겟 탐색 → 홀드 → 던지기/찌그러뜨리기/탑승의 흐름을 처리합니다. 이 컴포넌트는 플레이어와 AI 양쪽에서 공용으로 사용됩니다.
- `APKObjectManager`가 엔트리별 오브젝트 풀(`FPKPoolEntry`)을 관리하며, 지면 트레이스로 스폰 위치를 보정하고 오브젝트 간 최소 간격을 유지한 채 랜덤 배치 → 사용 후 리스폰 딜레이까지 담당하는 완전한 풀링 매니저로 구현했습니다.

### 플레이어 전투 — 콤보 · 입력 버퍼 · 액션 상태머신

- `UActionManagerComponent`가 Idle/Attacking/Dashing/Staggered/Jumping/Dead/PKHolding 상태를 관리하며 이동·공격 가능 여부를 중앙에서 판정합니다.
- `UInputBufferComponent`로 짧은 유효시간 내 입력을 버퍼링해 콤보 체감을 개선했고, `UComboComponent` + `UComboAttackDataAsset`으로 콤보 연출 데이터를 데이터 기반으로 관리했습니다.
- 카사네는 `UBladeHandlerComponent`가 부유하는 `AKasaneBlade` 6자루를 오브젝트 풀로 관리하며(대기 3자루 상시 노출), AnimNotify로 블레이드 발광(Glow) 연출과 크리티컬 판정 전환을 제어합니다.
- 대시/회피는 `UDashSkillComponent`가 담당하며, 모든 데미지 판정은 `IDamageable` 인터페이스로 통일해 플레이어·파티원·보스·일반 몬스터가 동일한 규약으로 데미지를 주고받습니다.

### 오브젝트 풀링 — 여러 시스템에 공통 적용

성능을 고려해 반복 스폰되는 오브젝트는 전부 풀링으로 처리했습니다: 염력 오브젝트(`APKObjectManager`), 카사네의 부유 블레이드(`UBladeHandlerComponent`), 데미지 폰트 위젯(`UDamageAmountWidgetPoolComponent`)이 각각 자신의 도메인에 맞춘 풀 전략(고정 슬롯/유휴 개수 유지/필요 시 동적 생성)을 갖습니다.

### UI/뷰모델

`BossHUDViewModel`, `PartyHUDViewModel`, `PlayerHUDViewModel`, `TargetingViewModel` 등 뷰모델 계층을 분리해 위젯이 게임 로직에 직접 의존하지 않도록 했고, 델리게이트 브로드캐스트(예: `FOnPKDamageDealt`, `FOnBladeDamageDealt`, `FOnBossHPChanged`)로 전투 이벤트와 UI를 느슨하게 연결했습니다.

### 렌더링 — 툰 셰이더 실험

셀 셰이딩 룩을 실험하기 위해 Lumen/Nanite 호환 툰 셰이더 플러그인(3rd-party, 서드파티 크레딧은 [고지 사항](#고지-사항) 참고)을 프로젝트에 포함했습니다. 캐릭터 프로그래머에서 그래픽스 프로그래머로 커리어를 확장하고 싶다는 목표와 맞닿아 있는 부분입니다.

## 기술 스택

| 분야 | 내용 |
|---|---|
| 엔진 | Unreal Engine 5.7 |
| 언어 | C++ (게임플레이 로직 전반), 일부 데이터/블루프린트 자산 |
| AI | StateTree / GameplayStateTree 플러그인, AIPerception |
| 데이터 | DataAsset(보스 설정/콤보), DataTable(웨이브) |
| 애니메이션 | AnimNotify / AnimNotifyState (콤보 판정, 블레이드 발광 등) |
| UI | UMG + ViewModel 패턴 |
| VFX | Niagara |
| 렌더링 | 커스텀 툰 셰이더 플러그인(서드파티) |

## 아키텍처 노트

- **인터페이스로 전투 규약 분리**: `IDamageable`(데미지/HP), `ICombatState`(공격 중/슈퍼아머 여부), `IStaggerable`(경직), `IPKInteractable`(염력 상호작용) — 캐릭터 타입에 상관없이 동일한 인터페이스로 상호작용하도록 설계했습니다.
- **보스 · 파티 · 일반 몹의 AI를 StateTree 하나로 통일**: 비헤이비어 트리를 섞어 쓰지 않고 StateTree Task/Evaluator 패턴을 전 AI에 일관 적용해, 러닝 커브는 있었지만 구조적 일관성을 얻었습니다.
- **데이터 기반 설계**: 보스 공격 패턴, 페이즈 전환 조건, 콤보, 웨이브 구성을 코드가 아닌 DataAsset/DataTable로 노출해 기획 반복(iteration) 비용을 낮췄습니다.

## 프로젝트 구조

```
Source/ScarletNexus/
├─ Boss/          # 보스 캐릭터, StateTree Task/Evaluator, 슈퍼아머, 페이즈/공격 타입 정의, 분신·투사체 액터
├─ Enemy/         # 일반 몬스터, 웨이브 매니저, StateTree Task/Evaluator
├─ PartyAI/       # 파티원(하나비 등) AI, StateTree Task/Evaluator, 염력 태스크
├─ Player/        # 플레이어블 캐릭터(유이토/카사네/하나비), 전투 컴포넌트, 위젯/뷰모델
├─ PK/            # 염력 오브젝트 & 풀링 매니저
├─ Interface/     # IDamageable, IPKInteractable 등 전투 상호작용 인터페이스
├─ Data/          # 콤보 데이터 에셋, 공격 타입 정의
├─ FX/            # 디졸브, 배리어 등 이펙트 컴포넌트
└─ AnimNotify/    # 콤보 판정 구간, 블레이드 발광 노티파이
```

## 실행 방법

1. Unreal Engine **5.7** 설치
2. `ScarletNexus.uproject` 우클릭 → *Generate Visual Studio project files*
3. `.uproject` 더블클릭 또는 Visual Studio에서 빌드 후 에디터 실행
4. 필요 플러그인(`StateTree`, `GameplayStateTree`)은 `.uproject`에 명시되어 있어 엔진 실행 시 자동 활성화됩니다.

## 데모

> 플레이 영상/스크린샷은 추후 추가 예정입니다. `Content/Screenshots` 또는 `Content/Movies`에 본인이 직접 캡처한 클립을 추가한 뒤, 아래처럼 삽입해 주세요.
>
> ```md
> ![gameplay](docs/gameplay.gif)
> ```

## 고지 사항

- 본 프로젝트는 **비상업적 학습·포트폴리오 목적**으로 제작된 팬 리메이크(2차 창작)입니다. **SCARLET NEXUS**의 세계관, 캐릭터명(유이토, 카사네, 하나비 등), 원작 리소스에 대한 권리는 각 원저작권자(Bandai Namco Entertainment 등)에게 있으며, 본 저장소는 상업적 이용이나 원작 자산의 무단 배포를 목적으로 하지 않습니다.
- 포함된 UE5 Toon Shader 플러그인(`Content/Plugins/ue5-toon-shader-plugin-master`)은 [Christopher Sims](https://twitter.com/csims314)가 공개한 서드파티 에셋이며, 해당 저작자에게 권리가 있습니다.
- 그 외 마켓플레이스/무료 에셋(모델, VFX, 사운드 등)은 각 배포처의 라이선스를 따릅니다.

## Author

**문경태 (Moon Kyeongtae)** — Unity/C# 클라이언트 개발자, UE5 학습 중
GitHub: [@zpfhfh0124](https://github.com/zpfhfh0124)
