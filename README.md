# ScarletNexus — UE5 Combat & AI Systems Remake

> A solo portfolio project that reimplements the combat and AI systems of Bandai Namco's **SCARLET NEXUS** from scratch in Unreal Engine 5 + C++. (Built during the Wanted PotenUp "AI Agent & Unreal Development" collaborative bootcamp.)

**Language:** English · [한국어](README.ko.md) · [日本語](README.ja.md)

![Engine](https://img.shields.io/badge/Unreal%20Engine-5.7-313131?logo=unrealengine)
![Language](https://img.shields.io/badge/Language-C%2B%2B-00599C?logo=cplusplus)
![AI](https://img.shields.io/badge/AI-StateTree-orange)
![Status](https://img.shields.io/badge/Status-Portfolio%20%2F%20Study-blue)

---

## Table of Contents

- [Overview](#overview)
- [Core Systems](#core-systems)
- [Tech Stack](#tech-stack)
- [Architecture Notes](#architecture-notes)
- [Project Structure](#project-structure)
- [Getting Started](#getting-started)
- [Demo](#demo)
- [Disclaimer](#disclaimer)
- [Author](#author)

---

## Overview

This project reimplements, from the ground up in C++, four systems that sit at the core of an action RPG: **boss combat AI**, **party (companion) AI**, **Psychokinesis (PK) object interaction**, and **player combo combat** — using the source game's design as a reference.

The goal wasn't just to make something that "looks similar," but to build out the kind of structural design that production work actually demands: a **StateTree-driven AI architecture**, **interface-decoupled combat contracts**, and **object pooling** throughout. The three playable characters — Yuito, Kasane, and Hanabi — all derive from a shared `APlayerCharacterBase` for common behavior (movement, dodging, Psychokinesis), while Kasane extends it with her own floating-blade combat through a dedicated component.

## Core Systems

### Boss AI — a 4-phase fight driven entirely by StateTree

- `UBossConfigDataAsset` tables out per-phase HP thresholds, unlock conditions, and attack data (wind-up montage, damage, range, cooldown, selection weight) as a `DataAsset`, keeping design data separate from logic.
- The fight auto-transitions on HP: Phase 1 (four base patterns) → Phase 2 (adds electric orbs + a map color shift) → Phase 2-2 (adds a telekinetic object-throw attack) → Phase 3 (a cutscene, then Phase 2-2 patterns repeat) — with each phase's patterns carrying forward into the next.
- Custom StateTree Tasks/Evaluators — `STTask_BossSelectAttack`, `STTask_BossAttackExecutor`, `STTask_BossPhaseTransition`, `STTask_BossStagger`, `STTask_BosshitReaction`, `STTask_BossTeleport`, `STTask_BossDeath`, and more — form the full loop: select pattern → execute → react to hits → transition phase.
- Implemented phase-specific patterns include a teleport kick, a triple clone-rush (`ABossCloneActor`), electric orb barrages, ice spikes, and telekinetic object throws. `UBossSuperArmorComponent` (super armor) and the `IStaggerable` interface (stagger gauge) separate out mutually exclusive hit-reaction states.
- Hit reactions are broken down by `EHitReactionType` (flinch / heavy hit / knockback / airborne) × `EHitDirection` (front / back / left / right).

### Party AI — the same StateTree architecture as the player's enemies

- Party members (`APartyMemberBase`) and regular enemies (`AEnemyBase`) share the exact same StateTree architecture as the boss, rather than mixing in a separate Behavior Tree system. A `PartyEvaluator` / `EnemyEvaluator` computes the nearest target and range checks every tick and feeds that data to tasks like `PCTask_Idle/Chase/Attack/PK`, which act on it.
- `APartyAIController` uses `AIPerception` (sight sense) to detect the player and automatically follow and join combat. Party members can also autonomously find, pick up, and throw PK objects via `PCTask_PK` combined with `PKTask_FindObject/LiftObject/ThrowObject`.
- Regular enemy spawning is handled by a data-table-driven `AEnemyManager` that manages wave-based spawns, loosely coupled to downstream effects (next wave, opening the boss room, etc.) via the `OnWaveStarted` / `OnWaveCleared` / `OnAllEnemiesDefeated` delegates.

### Psychokinesis (PK) System

- The `IPKInteractable` interface (pickup eligibility / pick up / release / throw) defines the contract for "objects that can be handled with Psychokinesis," while `UPKComponent` handles the full flow — FOV-dot-product-based target search, hold, and throw/crumple/ride — shared by both the player and AI.
- `APKObjectManager` manages per-entry object pools (`FPKPoolEntry`), snapping spawn positions to the ground via trace, keeping minimum spacing between objects, and handling randomized placement plus a respawn delay after use — a complete pooling manager in its own right.

### Player Combat — combos, input buffering, and an action state machine

- `UActionManagerComponent` centrally governs the Idle / Attacking / Dashing / Staggered / Jumping / Dead / PKHolding states and gates what movement and attacks are currently allowed.
- `UInputBufferComponent` buffers inputs within a short validity window to make combos feel responsive, while `UComboComponent` combined with `UComboAttackDataAsset` drives combo sequences from data rather than hardcoded logic.
- For Kasane, `UBladeHandlerComponent` manages a pool of six floating `AKasaneBlade` actors (three kept idle at all times), with AnimNotifies controlling the blade-glow VFX and critical-hit state toggling.
- Dashing/dodging is handled by `UDashSkillComponent`, and all damage resolution goes through the shared `IDamageable` interface so the player, party members, the boss, and regular enemies all deal and receive damage under the same contract.

### Object Pooling — applied consistently across systems

Every repeatedly-spawned object is pooled for performance: PK objects (`APKObjectManager`), Kasane's floating blades (`UBladeHandlerComponent`), and floating damage-number widgets (`UDamageAmountWidgetPoolComponent`) each implement a pooling strategy tailored to their own domain (fixed slots, a maintained idle count, or dynamic growth on demand).

### UI / ViewModels

A ViewModel layer — `BossHUDViewModel`, `PartyHUDViewModel`, `PlayerHUDViewModel`, `TargetingViewModel` — keeps widgets from depending directly on gameplay logic, with combat events loosely connected to the UI through delegate broadcasts such as `FOnPKDamageDealt`, `FOnBladeDamageDealt`, and `FOnBossHPChanged`.

### Rendering — toon shading experiments

A Lumen/Nanite-compatible toon shader plugin (third-party — see [Disclaimer](#disclaimer) for credit) is bundled in to experiment with a cel-shaded look, which ties directly into a longer-term goal of growing from a gameplay programmer into a graphics programmer.

## Tech Stack

| Area | Details |
|---|---|
| Engine | Unreal Engine 5.7 |
| Language | C++ (all core gameplay logic), with some data assets / Blueprint content |
| AI | StateTree / GameplayStateTree plugin, AIPerception |
| Data | DataAsset (boss config, combos), DataTable (enemy waves) |
| Animation | AnimNotify / AnimNotifyState (combo windows, blade glow, etc.) |
| UI | UMG + ViewModel pattern |
| VFX | Niagara |
| Rendering | Custom toon shader plugin (third-party) |

## Architecture Notes

- **Combat contracts decoupled through interfaces**: `IDamageable` (damage/HP), `ICombatState` (attacking / super-armor state), `IStaggerable` (stagger), and `IPKInteractable` (PK interaction) let any actor type interoperate through the same contract regardless of character type.
- **One StateTree architecture for boss, party, and regular enemy AI**: rather than mixing in a separate Behavior Tree, the same Task/Evaluator pattern was applied consistently across all AI — a steeper learning curve up front, in exchange for structural consistency.
- **Data-driven design**: boss attack patterns, phase-transition conditions, combos, and wave composition are all exposed through DataAssets/DataTables instead of hardcoded values, lowering the cost of design iteration.

## Project Structure

```
Source/ScarletNexus/
├─ Boss/          # Boss character, StateTree Tasks/Evaluators, super armor, phase/attack type definitions, clone & projectile actors
├─ Enemy/         # Regular enemies, wave manager, StateTree Tasks/Evaluators
├─ PartyAI/       # Party member (Hanabi, etc.) AI, StateTree Tasks/Evaluators, PK tasks
├─ Player/        # Playable characters (Yuito/Kasane/Hanabi), combat components, widgets/ViewModels
├─ PK/            # PK objects & the pooling manager
├─ Interface/     # Combat interaction interfaces (IDamageable, IPKInteractable, etc.)
├─ Data/          # Combo data assets, attack type definitions
├─ FX/            # Dissolve, barrier, and other effect components
└─ AnimNotify/    # Combo windows, blade-glow notifies
```

## Getting Started

1. Install Unreal Engine **5.7**.
2. Right-click `ScarletNexus.uproject` → *Generate Visual Studio project files*.
3. Double-click the `.uproject`, or build from Visual Studio and launch the editor.
4. Required plugins (`StateTree`, `GameplayStateTree`) are declared in the `.uproject` and enable automatically when the engine loads.

## Demo

> Gameplay footage and screenshots are coming soon. Once you add your own captured clips under `Content/Screenshots` or `Content/Movies`, embed them like this:
>
> ```md
> ![gameplay](docs/gameplay.gif)
> ```

## Disclaimer

- This project is a **non-commercial, educational/portfolio fan remake**. All rights to the **SCARLET NEXUS** setting, character names (Yuito, Kasane, Hanabi, and others), and original assets belong to their respective rights holders (Bandai Namco Entertainment and others). This repository is not intended for commercial use or for redistributing original assets.
- The bundled UE5 Toon Shader plugin (`Content/Plugins/ue5-toon-shader-plugin-master`) is a third-party asset released by [Christopher Sims](https://twitter.com/csims314); rights belong to its author.
- Any other marketplace/free assets (models, VFX, audio, etc.) remain subject to their respective distributors' licenses.

## Author

**Moon Kyeongtae (문경태)** — Unity/C# client developer, currently learning UE5
GitHub: [@zpfhfh0124](https://github.com/zpfhfh0124)
