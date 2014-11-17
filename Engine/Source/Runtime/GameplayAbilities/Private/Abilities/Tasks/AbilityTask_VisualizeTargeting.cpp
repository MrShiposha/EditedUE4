
#include "AbilitySystemPrivatePCH.h"
#include "GameplayAbilityTargetActor.h"
#include "AbilitySystemComponent.h"
#include "Abilities/Tasks/AbilityTask_VisualizeTargeting.h"

UAbilityTask_VisualizeTargeting::UAbilityTask_VisualizeTargeting(const class FPostConstructInitializeProperties& PCIP)
	: Super(PCIP)
{

}

UAbilityTask_VisualizeTargeting* UAbilityTask_VisualizeTargeting::VisualizeTargeting(UObject* WorldContextObject, TSubclassOf<AGameplayAbilityTargetActor> InTargetClass, FName TaskInstanceName, float Duration)
{
	auto MyObj = NewTask<UAbilityTask_VisualizeTargeting>(WorldContextObject, TaskInstanceName);		//Register for task list here, providing a given FName as a key
	MyObj->TargetClass = InTargetClass;
	if (Duration > 0.0f)
	{
		MyObj->GetWorld()->GetTimerManager().SetTimer(MyObj, &UAbilityTask_VisualizeTargeting::OnTimeElapsed, Duration, false);
	}
	return MyObj;
}

// ---------------------------------------------------------------------------------------

bool UAbilityTask_VisualizeTargeting::BeginSpawningActor(UObject* WorldContextObject, TSubclassOf<AGameplayAbilityTargetActor> TargetClass, AGameplayAbilityTargetActor*& SpawnedActor)
{
	SpawnedActor = nullptr;

	UGameplayAbility* MyAbility = Ability.Get();
	if (MyAbility)
	{
		const AGameplayAbilityTargetActor* CDO = CastChecked<AGameplayAbilityTargetActor>(TargetClass->GetDefaultObject());
		MyTargetActor = CDO;

		bool Replicates = CDO->GetReplicates();
		bool StaticFunc = CDO->StaticTargetFunction;
		bool IsLocallyControlled = MyAbility->GetCurrentActorInfo()->IsLocallyControlled();

		if (Replicates && StaticFunc)
		{
			// We can't replicate a staticFunc target actor, since we are just calling a static function and not spawning an actor at all!
			ABILITY_LOG(Fatal, TEXT("AbilityTargetActor class %s can't be Replicating and Static"), *TargetClass->GetName());
			Replicates = false;
		}

		// Spawn the actor if this is a locally controlled ability (always) or if this is a replicating targeting mode.
		// (E.g., server will spawn this target actor to replicate to all non owning clients)
		if (Replicates || IsLocallyControlled)
		{
			UClass* Class = *TargetClass;
			if (Class != NULL)
			{
				UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject);
				SpawnedActor = World->SpawnActorDeferred<AGameplayAbilityTargetActor>(Class, FVector::ZeroVector, FRotator::ZeroRotator, NULL, NULL, true);
			}

			MyTargetActor = SpawnedActor;

			AGameplayAbilityTargetActor* TargetActor = CastChecked<AGameplayAbilityTargetActor>(SpawnedActor);
			if (TargetActor)
			{
				TargetActor->MasterPC = MyAbility->GetCurrentActorInfo()->PlayerController.Get();
			}
		}
	}
	return (SpawnedActor != nullptr);
}

void UAbilityTask_VisualizeTargeting::FinishSpawningActor(UObject* WorldContextObject, AGameplayAbilityTargetActor* SpawnedActor)
{
	if (SpawnedActor)
	{
		check(MyTargetActor == SpawnedActor);

		const FTransform SpawnTransform = AbilitySystemComponent->GetOwner()->GetTransform();

		SpawnedActor->FinishSpawning(SpawnTransform);
		AbilitySystemComponent->SpawnedTargetActors.Push(SpawnedActor);
		SpawnedActor->StartTargeting(Ability.Get());
	}
}

// ---------------------------------------------------------------------------------------

void UAbilityTask_VisualizeTargeting::OnDestroy(bool AbilityEnded)
{
	if (MyTargetActor.IsValid() && !MyTargetActor->HasAnyFlags(RF_ClassDefaultObject))
	{
		MyTargetActor->Destroy();
	}
	GetWorld()->GetTimerManager().ClearTimer(this, &UAbilityTask_VisualizeTargeting::OnTimeElapsed);

	Super::OnDestroy(AbilityEnded);
}


// --------------------------------------------------------------------------------------

void UAbilityTask_VisualizeTargeting::OnTimeElapsed()
{
	TimeElapsed.Broadcast();
	EndTask();
}