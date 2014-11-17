// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"
#include "BlackboardKeyType_Object.generated.h"

UCLASS(EditInlineNew, MinimalAPI, meta=(DisplayName="Object"))
class UBlackboardKeyType_Object : public UBlackboardKeyType
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Category=Blackboard, EditDefaultsOnly, meta=(AllowAbstract="1"))
	UClass* BaseClass;

	static UObject* GetValue(const uint8* RawData);
	static bool SetValue(uint8* RawData, UObject* Value);

	virtual FString DescribeValue(const uint8* RawData) const override;
	virtual FString DescribeSelf() const override;
	virtual bool IsAllowedByFilter(UBlackboardKeyType* FilterOb) const override;
	virtual bool GetLocation(const uint8* RawData, FVector& Location) const override;
	virtual bool GetRotation(const uint8* MemoryBlock, FRotator& Rotation) const override;
	virtual EBlackboardCompare::Type Compare(const uint8* MemoryBlockA, const uint8* MemoryBlockB) const override;
	virtual bool TestBasicOperation(const uint8* MemoryBlock, EBasicKeyOperation::Type Op) const override;
};