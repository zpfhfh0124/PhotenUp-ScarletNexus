// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyTask_Chase.h"

#include "AIController.h"
#include "StateTreeExecutionContext.h"

EStateTreeRunStatus FEnemyTask_Chase::EnterState(FStateTreeExecutionContext& Context,
                                                 const FStateTreeTransitionResult& Transition) const
{
	auto& data = Context.GetInstanceData(*this);
	data.ElapsedSinceRepath = 0.f;

	// IsValid()로 검사: Destroy된 직후의 pending-kill 액터는 raw pointer가 non-null일 수 있음
	if (!IsValid(data.ChaseTarget)) return EStateTreeRunStatus::Failed;
	
	// owner -> pawn -> AIController 경로로 MoveTo 요청
	AAIController* aic = Cast<AAIController>(Context.GetOwner());
	if (!aic || !aic->GetPawn()) return EStateTreeRunStatus::Failed;
	
	aic->MoveToActor(data.ChaseTarget, data.AcceptanceRadius);
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyTask_Chase::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	auto& data = Context.GetInstanceData(*this);
	if (!IsValid(data.ChaseTarget)) return EStateTreeRunStatus::Failed;

	AAIController* aic = Cast<AAIController>(Context.GetOwner());
	if (!aic) return EStateTreeRunStatus::Failed;

	APawn* pawn = aic->GetPawn();
	if (!IsValid(pawn)) return EStateTreeRunStatus::Failed;
	
	// AcceptanceRadius 이내에 도달했으면 성공
	const float dist = FVector::Dist(pawn->GetActorLocation(), data.ChaseTarget->GetActorLocation());
	if (dist <= data.AcceptanceRadius)
	{
		aic->StopMovement();
		return EStateTreeRunStatus::Succeeded;
	}
	
	// 일정 주기마다 경로 재갱신 (타겟이 움직이므로)
	data.ElapsedSinceRepath += DeltaTime;
	if (data.ElapsedSinceRepath > data.RepathInterval)
	{
		data.ElapsedSinceRepath = 0.f;
		aic->MoveToActor(data.ChaseTarget, data.AcceptanceRadius);
	}
	
	return EStateTreeRunStatus::Running;
}

void FEnemyTask_Chase::ExitState(FStateTreeExecutionContext& Context,
	const FStateTreeTransitionResult& Transition) const
{
	APawn* pawn = Cast<APawn>(Context.GetOwner());
	if (!pawn) return;
	
	if (AAIController* aic = Cast<AAIController>(pawn->GetController()))
	{
		aic->StopMovement();
	}
}
