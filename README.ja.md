# ScarletNexus — UE5 戦闘・AIシステム リメイク

> バンダイナムコの **SCARLET NEXUS** をリファレンスに、Unreal Engine 5 + C++ で戦闘・AIシステムをゼロから設計・実装した個人ポートフォリオプロジェクトです。（Wanted PotenUp「AIエージェント&Unreal開発協業課程」受講中に制作）

**Language:** [English](README.md) · [한국어](README.ko.md) · 日本語

![Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?logo=unrealengine)
![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=cplusplus)
![AI](https://img.shields.io/badge/AI-StateTree-orange)
![Status](https://img.shields.io/badge/Status-Portfolio%20%2F%20Study-blue)

---

## 目次

- [概要](#概要)
- [主要システム](#主要システム)
- [技術スタック](#技術スタック)
- [アーキテクチャに関する補足](#アーキテクチャに関する補足)
- [プロジェクト構成](#プロジェクト構成)
- [起動方法](#起動方法)
- [デモ](#デモ)
- [免責事項](#免責事項)
- [Author](#author)

---

## 概要

本プロジェクトは、アクションRPGの核となる **ボス戦AI**、**パーティ（仲間）AI**、**サイコキネシス（PK）オブジェクトとのインタラクション**、**プレイヤーのコンボ戦闘** を、原作の構造を参考にしながら C++ でゼロから実装した技術学習用プロジェクトです。

目標は単に「見た目が似ているもの」を作ることではなく、**StateTreeベースのAIアーキテクチャ**、**インターフェースによって疎結合化された戦闘の規約**、**オブジェクトプーリング** といった、実務で求められる構造的な設計を自分の手で組み立てることにありました。プレイアブルキャラクターとしてユイト(Yuito)、カサネ(Kasane)、ハナビ(Hanabi)を実装しており、3キャラクターとも共通の `APlayerCharacterBase` を継承して移動・回避・サイコキネシス使用などの共通挙動を共有しつつ、カサネは専用コンポーネントで自身の浮遊ブレード戦闘を拡張しています。

## 主要システム

### ボスAI — StateTreeで構成した4フェーズ戦闘

- `UBossConfigDataAsset`（DataAsset）に各フェーズのHP閾値、解放条件、攻撃データ（予備動作モンタージュ・ダメージ・射程・クールダウン・選択ウェイト）をテーブル化し、デザインデータとロジックを分離しました。
- HPを基準に Phase 1（基本パターン4種）→ Phase 2（電撃弾の追加＋マップの色変化）→ Phase 2-2（念動力によるオブジェクト投げの追加）→ Phase 3（カットシーン後、Phase 2-2のパターンを継続）と自動遷移し、前フェーズのパターンは累積して使用可能になる構造です。
- `STTask_BossSelectAttack`、`STTask_BossAttackExecutor`、`STTask_BossPhaseTransition`、`STTask_BossStagger`、`STTask_BosshitReaction`、`STTask_BossTeleport`、`STTask_BossDeath` など、StateTreeのカスタムTask/Evaluatorで「パターン選択 → 実行 → 被弾リアクション → フェーズ遷移」の一連のループを構築しています。
- テレポートキック、3連続の分身突進（`ABossCloneActor`）、電撃弾のばら撒き、氷柱、念動力投げなど、フェーズごとの固有パターンを実装。`UBossSuperArmorComponent`（スーパーアーマー）と `IStaggerable`（スタッガーゲージ）インターフェースにより、互いに排他的な被弾判定を分離しています。
- 被弾リアクションは `EHitReactionType`（怯み／強打／ノックバック／浮き上がり）× `EHitDirection`（前後左右）の組み合わせで細分化しています。

### パーティAI — プレイヤー側と同一のStateTreeアーキテクチャ

- ボスだけでなく、パーティメンバー（`APartyMemberBase`）や雑魚敵（`AEnemyBase`）も**同一のStateTree構造**で統一して設計しました。`PartyEvaluator` / `EnemyEvaluator` が毎ティック最近接ターゲットと射程内判定を計算してTaskに渡し、`PCTask_Idle/Chase/Attack/PK` がその値を受けて行動を決定する構造です。
- `APartyAIController` は `AIPerception`（視覚センス）でプレイヤーを検知し、自動で追従・戦闘参加します。パーティメンバーも `PCTask_PK` と `PKTask_FindObject/LiftObject/ThrowObject` により、PKオブジェクトを自律的に探索・保持・投擲できます。
- 雑魚敵はデータテーブル駆動の `AEnemyManager` がウェーブ単位でスポーンを管理し、`OnWaveStarted` / `OnWaveCleared` / `OnAllEnemiesDefeated` デリゲートを通じて後続演出（次ウェーブ開始、ボス部屋の開放など）と疎結合に連携します。

### サイコキネシス（PK）システム

- `IPKInteractable` インターフェース（拾得可否／ピックアップ／解放／投擲）で「サイコキネシスで扱えるオブジェクト」の規約を定義し、`UPKComponent` がFOV（視野角のドット積判定）に基づくターゲット探索 → ホールド → 投擲／変形／搭乗までの一連の流れを処理します。このコンポーネントはプレイヤー・AI双方で共用しています。
- `APKObjectManager` がエントリごとのオブジェクトプール（`FPKPoolEntry`）を管理し、地面トレースによるスポーン位置補正、オブジェクト間の最小間隔維持、ランダム配置、使用後のリスポーン遅延までを一元管理する、完成度の高いプーリングマネージャーとして実装しています。

### プレイヤー戦闘 — コンボ・入力バッファ・アクションステートマシン

- `UActionManagerComponent` が Idle／Attacking／Dashing／Staggered／Jumping／Dead／PKHolding の各状態を一元管理し、移動・攻撃の可否を中央で判定します。
- `UInputBufferComponent` により短い有効時間内の入力をバッファリングしてコンボの操作感を向上させ、`UComboComponent` と `UComboAttackDataAsset` によってコンボ演出データをデータ駆動で管理しています。
- カサネは `UBladeHandlerComponent` が浮遊する `AKasaneBlade` を6本のオブジェクトプールとして管理し（常時3本を待機状態で展開）、AnimNotifyでブレードの発光演出とクリティカル判定の切り替えを制御します。
- ダッシュ／回避は `UDashSkillComponent` が担当し、ダメージ判定はすべて `IDamageable` インターフェースに統一することで、プレイヤー・パーティメンバー・ボス・雑魚敵が同一の規約でダメージを授受できるようにしています。

### オブジェクトプーリング — 複数システムへの横断的な適用

パフォーマンスを考慮し、繰り返しスポーンされるオブジェクトはすべてプーリングで処理しました。PKオブジェクト（`APKObjectManager`）、カサネの浮遊ブレード（`UBladeHandlerComponent`）、ダメージ数値ウィジェット（`UDamageAmountWidgetPoolComponent`）は、それぞれのドメインに合わせたプール戦略（固定スロット／待機数維持／必要時の動的生成）を持っています。

### UI・ViewModel

`BossHUDViewModel`、`PartyHUDViewModel`、`PlayerHUDViewModel`、`TargetingViewModel` などのViewModel層を分離し、ウィジェットがゲームロジックに直接依存しないようにしました。`FOnPKDamageDealt`、`FOnBladeDamageDealt`、`FOnBossHPChanged` といったデリゲートのブロードキャストにより、戦闘イベントとUIを疎結合に連携させています。

### レンダリング — トゥーンシェーダーの実験

セルシェーディングの表現を試すため、Lumen/Nanite対応のトゥーンシェーダープラグイン（サードパーティ製。クレジットは[免責事項](#免責事項)を参照）をプロジェクトに同梱しています。キャラクタープログラマーからグラフィックスプログラマーへとキャリアを広げたいという目標にも直結する部分です。

## 技術スタック

| 分野 | 内容 |
|---|---|
| エンジン | Unreal Engine 5.7 |
| 言語 | C++（ゲームプレイロジック全般）、一部データ／ブループリント資産 |
| AI | StateTree / GameplayStateTreeプラグイン、AIPerception |
| データ | DataAsset（ボス設定・コンボ）、DataTable（ウェーブ） |
| アニメーション | AnimNotify / AnimNotifyState（コンボ判定区間、ブレード発光など） |
| UI | UMG + ViewModelパターン |
| VFX | Niagara |
| レンダリング | カスタムトゥーンシェーダープラグイン（サードパーティ） |

## アーキテクチャに関する補足

- **インターフェースによる戦闘規約の分離**：`IDamageable`（ダメージ／HP）、`ICombatState`（攻撃中／スーパーアーマー状態）、`IStaggerable`（スタッガー）、`IPKInteractable`（PKインタラクション）——キャラクター種別によらず同一インターフェースで相互作用できるように設計しました。
- **ボス・パーティ・雑魚敵のAIをStateTreeに一本化**：ビヘイビアツリーを併用せず、Task/EvaluatorパターンをすべてのAIに一貫して適用。学習コストは高かったものの、構造的な一貫性を得られました。
- **データ駆動設計**：ボスの攻撃パターン、フェーズ遷移条件、コンボ、ウェーブ構成をコードに埋め込まず DataAsset/DataTable として公開し、デザインのイテレーションコストを下げています。

## プロジェクト構成

```
Source/ScarletNexus/
├─ Boss/          # ボスキャラクター、StateTree Task/Evaluator、スーパーアーマー、フェーズ/攻撃タイプ定義、分身・投射物アクター
├─ Enemy/         # 雑魚敵、ウェーブマネージャー、StateTree Task/Evaluator
├─ PartyAI/       # パーティメンバー（ハナビ等）AI、StateTree Task/Evaluator、PKタスク
├─ Player/        # プレイアブルキャラクター（ユイト/カサネ/ハナビ）、戦闘コンポーネント、ウィジェット/ViewModel
├─ PK/            # PKオブジェクト & プーリングマネージャー
├─ Interface/     # 戦闘インタラクション用インターフェース（IDamageable、IPKInteractable 等）
├─ Data/          # コンボデータアセット、攻撃タイプ定義
├─ FX/            # ディゾルブ、バリア等のエフェクトコンポーネント
└─ AnimNotify/    # コンボ判定区間、ブレード発光ノーティファイ
```

## 起動方法

1. Unreal Engine **5.7** をインストールします。
2. `ScarletNexus.uproject` を右クリック → *Generate Visual Studio project files* を実行します。
3. `.uproject` をダブルクリックするか、Visual Studioでビルドしてからエディタを起動します。
4. 必要なプラグイン（`StateTree`、`GameplayStateTree`）は `.uproject` に明記されており、エンジン起動時に自動で有効化されます。

## デモ

> プレイ映像・スクリーンショットは追って追加予定です。ご自身で撮影したクリップを `Content/Screenshots` や `Content/Movies` に追加した上で、以下のように埋め込んでください。
>
> ```md
> ![gameplay](docs/gameplay.gif)
> ```

## 免責事項

- 本プロジェクトは**非商用の学習・ポートフォリオ目的**で制作されたファンリメイク（二次創作）です。**SCARLET NEXUS** の世界観、キャラクター名（ユイト、カサネ、ハナビ 等）、原作リソースに関する権利は、各権利者（Bandai Namco Entertainment 等）に帰属します。本リポジトリは商業利用や原作アセットの無断配布を目的としたものではありません。
- 同梱しているUE5 Toon Shaderプラグイン（`Content/Plugins/ue5-toon-shader-plugin-master`）は、[Christopher Sims](https://twitter.com/csims314) 氏が公開しているサードパーティ製アセットであり、権利は制作者本人に帰属します。
- その他のマーケットプレイス／無料アセット（モデル、VFX、音声等）についても、各配布元のライセンスに従います。

## Author

**文景泰（ムン・ギョンテ / Moon Kyeongtae）** — Unity/C# クライアントエンジニア、現在UE5を学習中
GitHub: [@zpfhfh0124](https://github.com/zpfhfh0124)
