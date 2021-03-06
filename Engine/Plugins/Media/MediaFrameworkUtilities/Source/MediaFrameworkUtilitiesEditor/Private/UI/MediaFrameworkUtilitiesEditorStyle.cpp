// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "UI/MediaFrameworkUtilitiesEditorStyle.h"

#include "Brushes/SlateImageBrush.h"
#include "Styling/SlateStyle.h"
#include "Styling/SlateStyleRegistry.h"

namespace MediaFrameworkUtilitiesStyle
{
	const FVector2D Icon16x16(16.0f, 16.0f);
	const FVector2D Icon20x20(20.0f, 20.0f);
	const FVector2D Icon40x40(40.0f, 40.0f);
	const FVector2D Icon64x64(64.0f, 64.0f);

	const FName NAME_StyleName(TEXT("MediaBundleStyle"));
	const FName NAME_ContextName(TEXT("MediaBundle"));

	static TUniquePtr<FSlateStyleSet> StyleInstance;
}

#define IMAGE_BRUSH(RelativePath, ...) FSlateImageBrush(MediaFrameworkUtilitiesStyle::StyleInstance->RootToContentDir(RelativePath, TEXT(".png")), __VA_ARGS__)

void FMediaFrameworkUtilitiesEditorStyle::Register()
{
	MediaFrameworkUtilitiesStyle::StyleInstance = MakeUnique<FSlateStyleSet>(MediaFrameworkUtilitiesStyle::NAME_StyleName);
	MediaFrameworkUtilitiesStyle::StyleInstance->SetContentRoot(FPaths::EnginePluginsDir() / TEXT("Media/MediaFrameworkUtilities/Content/Editor/Icons/"));

	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ClassThumbnail.MediaBundle", new IMAGE_BRUSH("MediaBundle_64x", MediaFrameworkUtilitiesStyle::Icon64x64));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ClassIcon.MediaBundle", new IMAGE_BRUSH("MediaBundle_20x", MediaFrameworkUtilitiesStyle::Icon20x20));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ClassThumbnail.ProxyMediaOutput", new IMAGE_BRUSH("ProxyMediaOutput_64x", MediaFrameworkUtilitiesStyle::Icon64x64));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ClassIcon.ProxyMediaOutput", new IMAGE_BRUSH("ProxyMediaOutput_16x", MediaFrameworkUtilitiesStyle::Icon16x16));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ClassThumbnail.ProxyMediaSource", new IMAGE_BRUSH("ProxyMediaSource_64x", MediaFrameworkUtilitiesStyle::Icon64x64));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ClassIcon.ProxyMediaSource", new IMAGE_BRUSH("ProxyMediaSource_16x", MediaFrameworkUtilitiesStyle::Icon16x16));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ClassThumbnail.MediaProfile", new IMAGE_BRUSH("MediaProfile_64x", MediaFrameworkUtilitiesStyle::Icon64x64));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ClassIcon.MediaProfile", new IMAGE_BRUSH("MediaProfile_20x", MediaFrameworkUtilitiesStyle::Icon20x20));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ToolbarIcon.MediaProfile", new IMAGE_BRUSH("MediaProfile_Color_40x", MediaFrameworkUtilitiesStyle::Icon40x40));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("ToolbarIcon.Apply", new IMAGE_BRUSH("Apply_40x", MediaFrameworkUtilitiesStyle::Icon40x40));

	MediaFrameworkUtilitiesStyle::StyleInstance->Set("CaptureCameraViewport_Capture", new IMAGE_BRUSH("CaptureCameraViewport_Capture_40x", MediaFrameworkUtilitiesStyle::Icon40x40));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("CaptureCameraViewport_Capture.Small", new IMAGE_BRUSH("CaptureCameraViewport_Capture_16x", MediaFrameworkUtilitiesStyle::Icon16x16));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("CaptureCameraViewport_Stop", new IMAGE_BRUSH("CaptureCameraViewport_Stop_40x", MediaFrameworkUtilitiesStyle::Icon40x40));

	//Use existing icon of MaterialInstanceConstant from Engine Editor content
	const FString EngineContentPath = FPaths::EngineContentDir() / TEXT("Editor/Slate/Icons/AssetIcons/MaterialInstanceConstant_64x.png");
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("MaterialEditor", new FSlateImageBrush(EngineContentPath, MediaFrameworkUtilitiesStyle::Icon40x40));
	MediaFrameworkUtilitiesStyle::StyleInstance->Set("MaterialEditor.Small", new FSlateImageBrush(EngineContentPath, MediaFrameworkUtilitiesStyle::Icon20x20));


	FSlateStyleRegistry::RegisterSlateStyle(*MediaFrameworkUtilitiesStyle::StyleInstance.Get());
}

void FMediaFrameworkUtilitiesEditorStyle::Unregister()
{
	FSlateStyleRegistry::UnRegisterSlateStyle(*MediaFrameworkUtilitiesStyle::StyleInstance.Get());
	MediaFrameworkUtilitiesStyle::StyleInstance.Reset();
}

FName FMediaFrameworkUtilitiesEditorStyle::GetStyleSetName()
{
	return MediaFrameworkUtilitiesStyle::NAME_StyleName;
}

const ISlateStyle& FMediaFrameworkUtilitiesEditorStyle::Get()
{
	check(MediaFrameworkUtilitiesStyle::StyleInstance.IsValid());
	return *MediaFrameworkUtilitiesStyle::StyleInstance.Get();
}

#undef IMAGE_BRUSH
