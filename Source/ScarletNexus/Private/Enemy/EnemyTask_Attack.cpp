// Fill out your copyright notice in the Description page of Project Settings.


#include "Enemy/EnemyTask_Attack.h"

#include "AIController.h"
#include "StateTreeExecutionContext.h"
#include "Enemy/EnemyAnimInstance.h"
#include "Enemy/EnemyBase.h"
#include "PartyAI/PartyMemberBase.h"
#include "Player/PlayerCharacterBase.h"

// Owner(Controller) -> EnemyBase 헬퍼
static AEnemyBase* GetEnemyFromContext_Attack(FStateTreeExecutionContext& Context)
{
	if (AAIController* aic = Cast<AAIController>(Context.GetOwner()))
	{
		return Cast<AEnemyBase>(aic->GetPawn());
	}
	return Cast<AEnemyBase>(Context.GetOwner());
}

EStateTreeRunStatus FEnemyTask_Attack::EnterState(FStateTreeExecutionContext& Context,
                                                  const FStateTreeTransitionResult& Transition) const
{
	auto& data = Context.GetInstanceData(*this);
	data.ElapsedTime = 0.0f;
	data.bDamageApplied = false;

	// 널 체크만으로는 부족함: 타겟이 이미 Destroy된(pending-kill) 액터일 수 있으므로
	// IsValid()로 검사해야 함 (raw pointer는 Destroy 직후에도 non-null일 수 있음)
	if (!IsValid(data.Target)) return EStateTreeRunStatus::Failed;

	// 공격 몽타주 재생
	AEnemyBase* enemy = GetEnemyFromContext_Attack(Context);
	if (enemy)
	{
		if (UEnemyAnimInstance* animInst = Cast<UEnemyAnimInstance>(enemy->GetMesh()->GetAnimInstance()))
		{
			animInst->PlayAttackMontage();
		}
	}
	
	return EStateTreeRunStatus::Running;
}

EStateTreeRunStatus FEnemyTask_Attack::Tick(FStateTreeExecutionContext& Context, const float DeltaTime) const
{
	auto& data = Context.GetInstanceData(*this);
	AEnemyBase* enemy = GetEnemyFromContext_Attack(Context);
	if (!IsValid(enemy) || !IsValid(data.Target)) return EStateTreeRunStatus::Failed;

	data.ElapsedTime += DeltaTime;

	// 쿨다운 중 특정 시점에 한 번 데미지 적용
	// 프레임 사이(공격 대기 중)에 타겟이 Destroy될 수 있으므로 여기서도 다시 확인
	if (!data.bDamageApplied && IsValid(data.Target) && data.ElapsedTime >= data.AttackCooldown * data.DamageTimingRatio)
	{
		const float dist = FVector::Dist(enemy->GetActorLocation(), data.Target->GetActorLocation());
		
		if (dist <= data.AttackRadius)
		{
			// 파티 맴버 대상
			if (APartyMemberBase* partyMember = Cast<APartyMemberBase>(data.Target))
			{
				partyMember->TakeDamage_Party(data.AtkDmg);
			}
			// 플레이어 대상 데미지 처리 추가
			else if (APlayerCharacterBase* player = Cast<APlayerCharacterBase>(data.Target))
			{
				FDamageInfo info;
				info.DamageAmount = data.AtkDmg;
				info.DamageCauser = enemy;
				player->ReceiveDamage_Implementation(info);
			}
		}
		
		data.bDamageApplied = true;
	}
	
	// 쿨다운 완료 -> Succeeded로 Combat 루프 재진입
	if (data.ElapsedTime >= data.AttackCooldown) return EStateTreeRunStatus::Succeeded;
	
	return EStateTreeRunStatus::Running;
}
