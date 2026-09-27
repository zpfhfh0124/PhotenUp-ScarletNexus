// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyEvaluator.h"

#include "AIController.h"
#include "ScarletNexus.h"
#include "StateTreeExecutionContext.h"
#include "Enemy/EnemyBase.h"
#include "Enemy/EnemyManager.h"
#include "Engine/OverlapResult.h"
#include "PartyAI/PartyMemberBase.h"
#include "Player/PlayerCharacterBase.h"

void FEnemyEvaluator::TreeStart(FStateTreeExecutionContext& Context) const
{
	FStateTreeEvaluatorBase::TreeStart(Context);
}

void FEnemyEvaluator::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	auto& data = Context.GetInstanceData(*this);

	AEnemyBase* enemy = nullptr;
	AActor* owner = Cast<AActor>(Context.GetOwner());
	if (!owner) return;

	enemy = Cast<AEnemyBase>(owner);
	if (!owner)
	{
		PRINTLOG_GT(TEXT("owner가 Null이다"));
		return;
	}
	
	if (!enemy)
	{
		if (AAIController* aic = Cast<AAIController>(owner))
		{
			APawn* pawn = aic->GetPawn();
			//PRINTLOG_GT(TEXT("Controller Pawn: %s"), pawn ? *pawn->GetName() : TEXT("Null"));
			enemy = Cast<AEnemyBase>(pawn);
		}
	}

	if (!enemy)
	{
		PRINTLOG_GT(TEXT("EnemyBase가 Null이다"));
		return;
	}
	
	// 상태 체크
	data.bIsStunned = enemy->bIsStunned;
	data.bIsDie = enemy->bIsDie;

	// Die/Stun 시 타겟 탐색 스킵
	if (data.bIsStunned || data.bIsDie) return;
	
	const FVector pawnLocation = enemy->GetActorLocation();
	
	TArray<FOverlapResult> overlaps;
	FCollisionQueryParams params;
	params.AddIgnoredActor(enemy);
	
	enemy->GetWorld()->OverlapMultiByChannel(
		overlaps,
		pawnLocation,
		FQuat::Identity,
		ECC_Pawn,
		FCollisionShape::MakeSphere(data.DetectRadius),
		params);
	
	AActor* bestTarget = nullptr;
	float bestDist = FLT_MAX;
	
	for (const FOverlapResult& hit : overlaps)
	{
		AActor* actor = hit.GetActor();
		if (!IsValid(actor)) continue;
		
		// 플레이어 캐릭터 or 파티 맴버를 타겟으로 인식
		bool bIsTarget = false;
		if (Cast<APlayerCharacterBase>(actor))
		{
			bIsTarget = true;
		}
		else if (APartyMemberBase* partyMember = Cast<APartyMemberBase>(actor))
		{
			bIsTarget = partyMember->IsAlive();
		}
		
		if (bIsTarget)
		{
			const float dist = FVector::DistSquared(pawnLocation, actor->GetActorLocation());
			if (dist < bestDist)
			{
				bestDist = dist;
				bestTarget = actor;
			}
		}
	}
	
	data.NearestTarget = bestTarget;

	if (IsValid(bestTarget))
	{
		data.DistanceToTarget = FVector::Dist(pawnLocation, bestTarget->GetActorLocation());
		data.bInAttackRange = data.DistanceToTarget <= data.AttackRange;
		
		// 공격 토큰 체크
		if (data.bInAttackRange && enemy->OwningManager)
		{
			data.bCanAttack = enemy->OwningManager->RequestAttackToken(enemy);
		}
		else
		{
			if (enemy->OwningManager)
			{
				enemy->OwningManager->ReleaseAttackToken(enemy);
			}
			data.bCanAttack = false;
		}
	}
	else
	{
		data.DistanceToTarget = 0.f;
		data.bInAttackRange = false;
		
		// 타겟이 없으면 토큰 반환
		if (enemy->OwningManager)
		{
			enemy->OwningManager->ReleaseAttackToken(enemy);
		}
		data.bCanAttack = false;
	}
}
