// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"

class FBuildPackageChunkData
{
public:
	static bool PackageChunkData(const FString& ManifestFilePath, const FString& PrevManifestFilePath, const TArray<TSet<FString>>& TagSetArray, const FString& OutputFile, const FString& CloudDir, uint64 MaxOutputFileSize, const FString& ResultDataFilePath);
};
