// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "AbilitySystemPrivatePCH.h"
#include "GameplayEffectStackingExtension.h"

UGameplayEffectStackingExtension::UGameplayEffectStackingExtension(const class FPostConstructInitializeProperties& PCIP)
: Super(PCIP)
{
	
}

FGameplayEffectSpec* UGameplayEffectStackingExtension::StackCustomEffects(TArray<FGameplayEffectSpec*>& CustomGameplayEffects)
{
	return NULL;
}