// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyManager.h"

#include "AudioMixerBlueprintLibrary.h"
#include "ScarletNexus.h"
#include "Chaos/ChaosPerfTest.h"
#include "Components/CapsuleComponent.h"
#include "Components/WidgetComponent.h"
#include "Enemy/EnemyBase.h"
#include "GameFramework/CharacterMovementComponent.h"


AEnemyManager::AEnemyManager()
{
	PrimaryActorTick.bCanEverTick = false;
}

void AEnemyManager::BeginPlay()
{
	Super::BeginPlay();
	
	LoadWaveRows();
	InitPool();
	
	if (bAutoStartFirstWave && WaveRows.Num() > 0)
	{
		StartWave(0);
	}

	// ** 치트 단축키 바인딩
	if (APlayerController* pc = GetWorld()->GetFirstPlayerController())
	{
		pc->InputComponent->BindAction("CheatEnemyToggle", IE_Pressed, this, &AEnemyManager::Cheat_ToggleAllEnemiesVisible);
	}
}

void AEnemyManager::Tick(float DeltaTime)
{
	Super::Tick(DeltaTime);
}

void AEnemyManager::StartNextWave()
{
	if (CurrentWaveIndex < WaveRows.Num())
	{
		StartWave(CurrentWaveIndex);
	}
}

void AEnemyManager::StartWave(int32 WaveIndex)
{
	if (!WaveRows.IsValidIndex(WaveIndex)) return;
	
	CurrentWaveIndex = WaveIndex;
	const FEnemyWave* wave = WaveRows[WaveIndex];
	
	if (wave->DelayBeforeWave > 0.f)
	{
		// 딜레이 후 스폰
		GetWorldTimerManager().SetTimer(
			WaveDelayTimerHandle,
			[this, WaveIndex]()
			{
				if (WaveRows.IsValidIndex(WaveIndex))
				{
					OnWaveStarted.Broadcast(WaveIndex);
					SpawnWaveEnemies(*WaveRows[WaveIndex]);
				}
			},
			wave->DelayBeforeWave,
			false);
	}
	else
	{
		OnWaveStarted.Broadcast(WaveIndex);
		SpawnWaveEnemies(*wave);
	}
}

void AEnemyManager::OnEnemyDied(AEnemyBase* Enemy)
{
	if (!Enemy) return;
	
	ReleaseAttackToken(Enemy);
	AliveEnemies.Remove(Enemy);
	
	TWeakObjectPtr<AEnemyBase> weakEnemy = Enemy;  // ← weak로 캡처
	
	// 풀로 반환 (딜레이 후 DestroyDelay와 맞춤
	FTimerHandle returnHandle;
	GetWorldTimerManager().SetTimer(
		returnHandle,
		[this, weakEnemy]()
		{
			if (AEnemyBase* enemy = weakEnemy.Get())
			ReturnToPool(enemy);
		},
		Enemy->DestroyDelay,
		false);
	
	// 현재 웨이브 클리어 체크
	if (AliveEnemies.Num() == 0)
	{
		OnWaveCleared.Broadcast(CurrentWaveIndex);
		CurrentWaveIndex++;
		
		if (CurrentWaveIndex >= WaveRows.Num())
		{
			// 전체 웨이브 클리어
			OnAllEnemiesDefeated.Broadcast();
		}
		else
		{
			// 다음 웨이브
			StartNextWave();
		}
	}
}

void AEnemyManager::Cheat_ToggleAllEnemiesVisible()
{
	bEnemyVisible = !bEnemyVisible;
	for (AEnemyBase* enemy : AliveEnemies)
	{
		if (enemy)
		{
			enemy->SetActorHiddenInGame(!bEnemyVisible);
		}
	}
}

void AEnemyManager::LoadWaveRows()
{
	WaveRows.Empty();
	
	if (!WaveDataTable) return;
	
	TArray<FEnemyWave*> rows;
	WaveDataTable->GetAllRows<FEnemyWave>(TEXT("EnemyManager"), rows);
	
	// Row 이름 순서대로 들어오므로 그대로 사용
	// (DataTable에서 Row 이름을 Wave_00, Wave_01... 으로 지으면 순서 보장)
	WaveRows = rows;
}

void AEnemyManager::InitPool()
{
	// 웨이브에 등장하는 모든 클래스에 대해 풀 미리 생성
	TSet<TSubclassOf<AEnemyBase>> UniqueClasses;
	
	for (const FEnemyWave* wave : WaveRows)
	{
		for (const FEnemyWaveEntry& entry : wave->Entries)
		{
			if (entry.EnemyClass)
			{
				UniqueClasses.Add(entry.EnemyClass);
			}
		}
	}
	
	for (const TSubclassOf<AEnemyBase>& enemyClass : UniqueClasses)
	{
		// 참조(&)로 받아야 함: 값으로 복사하면 아래 pool.Add()가 로컬 복사본에만 쌓이고
		// EnemyPool 맵의 실제 배열은 항상 비어있게 되어(GetFromPool이 매번 풀을 "비어있음"으로 판단),
		// 사전 워밍한 개체들이 재사용되지 못한 채 버려지는 버그가 있었음
		TArray<AEnemyBase*>& pool = EnemyPool.FindOrAdd(enemyClass);
		
		for (int32 i = 0; i < PoolSizePerClass; ++i)
		{
			FActorSpawnParameters spawnParams;
			spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
			
			AEnemyBase* enemy = GetWorld()->SpawnActor<AEnemyBase>
			(
				enemyClass,
				FVector(0.0f, 0.0f, -5000.0f), // 스크린 밖에 생성
				FRotator::ZeroRotator,
				spawnParams
				);
			
			if (enemy)
			{
				DeactivateEnemy(enemy);
				pool.Add(enemy);
			}
		}
	}
}

AEnemyBase* AEnemyManager::GetFromPool(TSubclassOf<AEnemyBase> EnemyClass)
{
	TArray<AEnemyBase*>* pool = EnemyPool.Find(EnemyClass);
	if (pool && pool->Num() > 0)
	{
		AEnemyBase* enemy = pool->Pop();
		return enemy;
	}
	
	// 풀이 비었으면 새로 스폰
	FActorSpawnParameters spawnParams;
	spawnParams.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
	
	AEnemyBase* enemy = GetWorld()->SpawnActor<AEnemyBase>(
		EnemyClass,
		FVector(0.0f, 0.0f, -5000.0f),
		FRotator::ZeroRotator,
		spawnParams
		);
	
	return enemy;
}

void AEnemyManager::ReturnToPool(AEnemyBase* Enemy)
{
	if (!Enemy) return;
	
	DeactivateEnemy(Enemy);
	
	TSubclassOf<AEnemyBase> enemyClass = Enemy->GetClass();
	TArray<AEnemyBase*>& pool = EnemyPool.FindOrAdd(enemyClass);
	pool.Add(Enemy);
}

void AEnemyManager::ActivateEnemy(AEnemyBase* Enemy, const FVector& Location, const FRotator& Rotaion)
{
	if (!Enemy) return;
	
	Enemy->SetActorLocation(Location);
	Enemy->SetActorRotation(Rotaion);
	Enemy->SetActorHiddenInGame(false);
	Enemy->SetActorEnableCollision(true);
	Enemy->SetActorTickEnabled(true);
	
	// HP 초기화
	Enemy->CurrHP = Enemy->MaxHP;
	Enemy->bIsDie = false;
	Enemy->bIsStunned = false;
	Enemy->OwningManager = this;
	
	// 체력바 표시
	if (Enemy->HealthBarComp)
	{
		Enemy->HealthBarComp->SetVisibility(true);
	}
	
	// 캡슐 충돌 복원
	if (UCapsuleComponent* capsule = Enemy->GetCapsuleComponent())
	{
		capsule->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
	}
	
	// 이동 복원
	if (UCharacterMovementComponent* moveComp = Enemy->GetCharacterMovement())
	{
		moveComp->SetMovementMode(MOVE_Walking);
	}
	
	// AI 재시작 - Controller 재빙의
	Enemy->SpawnDefaultController();
}

void AEnemyManager::DeactivateEnemy(AEnemyBase* Enemy)
{
	if (!Enemy || !IsValid(Enemy)) return;
	
	// AI 정지
	AController* controller = Enemy->GetController();
	if (IsValid(controller))
	{
		controller->UnPossess();
	}
	
	// Ragdoll 리셋
	if (USkeletalMeshComponent* meshComp = Enemy->GetMesh())
	{
		// Mesh 체크도 추가
		if (IsValid(meshComp))
		{
			meshComp->SetSimulatePhysics(false);
			meshComp->SetAllBodiesSimulatePhysics(false);
			meshComp->SetCollisionProfileName(TEXT("CharacterMesh"));
            
			if (USceneComponent* root = Enemy->GetRootComponent())
			{
				meshComp->AttachToComponent(root, FAttachmentTransformRules::SnapToTargetNotIncludingScale);
			}
		}
	}
	
	// 체력바 숨기기
	if (Enemy->HealthBarComp)
	{
		Enemy->HealthBarComp->SetVisibility(false);
	}
	
	Enemy->SetActorHiddenInGame(true);
	Enemy->SetActorEnableCollision(false);
	Enemy->SetActorTickEnabled(false);
	Enemy->SetActorLocation(FVector(0.0f, 0.0f, -5000.0f));
}

FTransform AEnemyManager::GetRandomSpawnTransform() const
{
	const FVector origin = GetActorLocation();

	const float angle = FMath::FRandRange(0.f, 2.f * PI);
	const float dist = FMath::FRandRange(SpawnRadius * 0.3f, SpawnRadius);
	FVector location = origin + FVector(FMath::Cos(angle) * dist, FMath::Sin(angle) * dist, 0.f);

	FRotator rotation = FRotator(0.f, FMath::FRandRange(0.f, 360.f), 0.f);

	return FTransform(rotation, location);
}

void AEnemyManager::SpawnWaveEnemies(const FEnemyWave& Wave)
{
	for (const FEnemyWaveEntry& entry : Wave.Entries)
	{
		if (!entry.EnemyClass) continue;
		
		for (int32 i = 0; i < entry.Count; ++i)
		{
			AEnemyBase* enemy = GetFromPool(entry.EnemyClass);
			if (!enemy) continue;
			
			const FTransform spawnTransform = GetRandomSpawnTransform();
			ActivateEnemy(enemy, spawnTransform.GetLocation(), spawnTransform.GetRotation().Rotator());
			
			AliveEnemies.Add(enemy);
		}
	}
}

bool AEnemyManager::RequestAttackToken(AEnemyBase* Enemy)
{
	if (!Enemy) return false;
	
	// 이미 토큰 보유 중
	if (CurrentAttackers.Contains(Enemy)) return true;
	
	// 슬롯 남아있으면 허용
	if (CurrentAttackers.Num() < MaxAttackers)
	{
		CurrentAttackers.Add(Enemy);
		return true;
	}
	
	return false;
}

void AEnemyManager::ReleaseAttackToken(AEnemyBase* Enemy)
{
	CurrentAttackers.Remove(Enemy);
}
