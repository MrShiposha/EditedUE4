// Copyright 1998-2018 Epic Games, Inc. All Rights Reserved.

#include "OculusHMD_Layer.h"

#if OCULUS_HMD_SUPPORTED_PLATFORMS
//#include "MediaTexture.h"
//#include "ScreenRendering.h"
//#include "ScenePrivate.h"
//#include "PostProcess/SceneFilterRendering.h"
#include "PostProcess/SceneRenderTargets.h"
#include "HeadMountedDisplayTypes.h" // for LogHMD
#include "XRThreadUtils.h"
#include "Engine/Texture2D.h"
#include "UObject/ConstructorHelpers.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Engine/GameEngine.h"


namespace OculusHMD
{

//-------------------------------------------------------------------------------------------------
// FOvrpLayer
//-------------------------------------------------------------------------------------------------

FOvrpLayer::FOvrpLayer(uint32 InOvrpLayerId) : 
	OvrpLayerId(InOvrpLayerId)
{
}


FOvrpLayer::~FOvrpLayer()
{
	if (InRenderThread())
	{
		ExecuteOnRHIThread_DoNotWait([this]()
		{
			ovrp_DestroyLayer(OvrpLayerId);
		});
	}
	else
	{
		ovrp_DestroyLayer(OvrpLayerId);
	}
}


//-------------------------------------------------------------------------------------------------
// FLayer
//-------------------------------------------------------------------------------------------------

FLayer::FLayer(uint32 InId, const IStereoLayers::FLayerDesc& InDesc) :
	Id(InId),
	OvrpLayerId(0),
	bUpdateTexture(false),
	bInvertY(false),
	bHasDepth(false),
	PokeAHoleComponentPtr(nullptr), 
	PokeAHoleActor(nullptr)
{
	FMemory::Memzero(OvrpLayerDesc);
	FMemory::Memzero(OvrpLayerSubmit);
	SetDesc(InDesc);
}


FLayer::FLayer(const FLayer& Layer) :
	Id(Layer.Id),
	Desc(Layer.Desc),
	OvrpLayerId(Layer.OvrpLayerId),
	OvrpLayer(Layer.OvrpLayer),
	TextureSetProxy(Layer.TextureSetProxy),
	DepthTextureSetProxy(Layer.DepthTextureSetProxy),
	RightTextureSetProxy(Layer.RightTextureSetProxy),
	RightDepthTextureSetProxy(Layer.RightDepthTextureSetProxy),
	bUpdateTexture(Layer.bUpdateTexture),
	bInvertY(Layer.bInvertY),
	bHasDepth(Layer.bHasDepth),
	PokeAHoleComponentPtr(Layer.PokeAHoleComponentPtr),
	PokeAHoleActor(Layer.PokeAHoleActor)
{
	FMemory::Memcpy(&OvrpLayerDesc, &Layer.OvrpLayerDesc, sizeof(OvrpLayerDesc));
	FMemory::Memcpy(&OvrpLayerSubmit, &Layer.OvrpLayerSubmit, sizeof(OvrpLayerSubmit));
}


FLayer::~FLayer()
{
}


void FLayer::SetDesc(const IStereoLayers::FLayerDesc& InDesc)
{
	if (Desc.Texture != InDesc.Texture || Desc.LeftTexture != InDesc.LeftTexture)
	{
		bUpdateTexture = true;
	}

	Desc = InDesc;

#if PLATFORM_ANDROID
	// PokeAHole is un-necessary on PC due to depth buffer sharing and compositing
	HandlePokeAHoleComponent();
#else
	// Mark all layers as supporting depth for now, due to artifacts with ovrpLayerSubmitFlag_NoDepth
	Desc.Flags |= IStereoLayers::LAYER_FLAG_SUPPORT_DEPTH;
#endif
}

#if PLATFORM_ANDROID
void FLayer::HandlePokeAHoleComponent()
{
	if (NeedsPokeAHole())
	{
		const FString BaseComponentName = FString::Printf(TEXT("OculusPokeAHole_%d"), Id);
		const FName ComponentName(*BaseComponentName);

		if (!PokeAHoleComponentPtr) {
			UWorld* World = nullptr;
			for (const FWorldContext& Context : GEngine->GetWorldContexts())
			{
				if (Context.WorldType == EWorldType::Game || Context.WorldType == EWorldType::PIE)
				{
					World = Context.World();
				}
			}

			if (!World)
			{
				return;
			}

			PokeAHoleActor = World->SpawnActor<AActor>();

			PokeAHoleComponentPtr = NewObject<UProceduralMeshComponent>(PokeAHoleActor, ComponentName);
			PokeAHoleComponentPtr->RegisterComponent();

			TArray<FVector> Vertices;
			TArray<int32> Triangles;
			TArray<FVector> Normals;
			TArray<FVector2D> UV0;
			TArray<FLinearColor> VertexColors;
			TArray<FProcMeshTangent> Tangents;

			BuildPokeAHoleMesh(Vertices, Triangles, UV0);
			PokeAHoleComponentPtr->CreateMeshSection_LinearColor(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);

			UMaterial *PokeAHoleMaterial = Cast<UMaterial>(StaticLoadObject(UMaterial::StaticClass(), NULL, TEXT("/OculusVR/Materials/PokeAHoleMaterial")));
			UMaterialInstanceDynamic* DynamicMaterial = UMaterialInstanceDynamic::Create(PokeAHoleMaterial, NULL);

			PokeAHoleComponentPtr->SetMaterial(0, DynamicMaterial);

		}
		PokeAHoleComponentPtr->SetWorldTransform(Desc.Transform);

	}

	return;
}

static void AppendFaceIndices(const int v0, const int v1, const int v2, const int v3, TArray<int32>& Triangles, bool inverse)
{
	if (inverse)
	{
		Triangles.Add(v0);
		Triangles.Add(v2);
		Triangles.Add(v1);
		Triangles.Add(v0);
		Triangles.Add(v3);
		Triangles.Add(v2);
	}
	else
	{
		Triangles.Add(v0);
		Triangles.Add(v1);
		Triangles.Add(v2);
		Triangles.Add(v0);
		Triangles.Add(v2);
		Triangles.Add(v3);
	}
}

void FLayer::BuildPokeAHoleMesh(TArray<FVector>& Vertices, TArray<int32>& Triangles, TArray<FVector2D>& UV0)
{
	if (Desc.ShapeType == IStereoLayers::QuadLayer)
	{
		const float QuadScale = 0.99;

		FIntPoint TexSize = Desc.Texture.IsValid() ? Desc.Texture->GetTexture2D()->GetSizeXY() : Desc.LayerSize;
		float AspectRatio = TexSize.X ? (float)TexSize.Y / (float)TexSize.X : 3.0f / 4.0f;

		float QuadSizeX = Desc.QuadSize.X;
		float QuadSizeY = (Desc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO) ? Desc.QuadSize.X * AspectRatio : Desc.QuadSize.Y;

		Vertices.Init(FVector::ZeroVector, 4);
		Vertices[0] = FVector(0.0, -QuadSizeX / 2, -QuadSizeY / 2) * QuadScale;
		Vertices[1] = FVector(0.0, QuadSizeX / 2, -QuadSizeY / 2) * QuadScale;
		Vertices[2] = FVector(0.0, QuadSizeX / 2, QuadSizeY / 2) * QuadScale;
		Vertices[3] = FVector(0.0, -QuadSizeX / 2, QuadSizeY / 2) * QuadScale;

		UV0.Init(FVector2D::ZeroVector, 4);
		UV0[0] = FVector2D(1, 0);
		UV0[1] = FVector2D(1, 1);
		UV0[2] = FVector2D(0, 0);
		UV0[3] = FVector2D(0, 1);

		Triangles.Reserve(6);
		AppendFaceIndices(0, 1, 2, 3, Triangles, false);
	}
	else if (Desc.ShapeType == IStereoLayers::CylinderLayer)
	{
		const float CylinderScale = 0.99;

		FIntPoint TexSize = Desc.Texture.IsValid() ? Desc.Texture->GetTexture2D()->GetSizeXY() : Desc.LayerSize;
		float AspectRatio = TexSize.X ? (float)TexSize.Y / (float)TexSize.X : 3.0f / 4.0f;

		float CylinderHeight = (Desc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO) ? Desc.CylinderOverlayArc * AspectRatio : Desc.CylinderHeight;

		const FVector XAxis = FVector(1, 0, 0);
		const FVector YAxis = FVector(0, 1, 0);
		const FVector HalfHeight = FVector(0, 0, CylinderHeight / 2);

		const float ArcAngle = Desc.CylinderOverlayArc / Desc.CylinderRadius;
		const int Sides = (int)( (ArcAngle * 180) / (PI * 5) ); // one triangle every 10 degrees of cylinder for a good-cheap approximation
		Vertices.Init(FVector::ZeroVector, 2 * (Sides + 1));
		UV0.Init(FVector2D::ZeroVector, 2 * (Sides + 1));
		Triangles.Init(0, Sides * 6);

		float CurrentAngle = -ArcAngle / 2;
		const float AngleStep = ArcAngle / Sides;
		

		for (int Side = 0; Side < Sides + 1; Side++)
		{
			FVector MidVertex = Desc.CylinderRadius * (FMath::Cos(CurrentAngle) * XAxis + FMath::Sin(CurrentAngle) * YAxis);
			Vertices[2 * Side] = (MidVertex - HalfHeight) * CylinderScale;
			Vertices[(2 * Side) + 1] = (MidVertex + HalfHeight) * CylinderScale;

			UV0[2 * Side] = FVector2D(1 - (Side / (float)Sides ), 0);
			UV0[(2 * Side) + 1] = FVector2D(1 - (Side / (float)Sides ), 1);

			CurrentAngle += AngleStep;

			if (Side < Sides)
			{
				Triangles[6 * Side + 0] = 2 * Side;
				Triangles[6 * Side + 2] = 2 * Side + 1;
				Triangles[6 * Side + 1] = 2 * (Side + 1) + 1;
				Triangles[6 * Side + 3] = 2 * Side;
				Triangles[6 * Side + 5] = 2 * (Side + 1) + 1;
				Triangles[6 * Side + 4] = 2 * (Side + 1);
			}
		}
	}
	else if (Desc.ShapeType == IStereoLayers::CubemapLayer)
	{
		const float CubemapScale = 1000;
		Vertices.Init(FVector::ZeroVector, 8);
		Vertices[0] = FVector(-1.0, -1.0, -1.0) * CubemapScale;
		Vertices[1] = FVector(-1.0, -1.0, 1.0) * CubemapScale;
		Vertices[2] = FVector(-1.0, 1.0, -1.0) * CubemapScale;
		Vertices[3] = FVector(-1.0, 1.0, 1.0) * CubemapScale;
		Vertices[4] = FVector(1.0, -1.0, -1.0) * CubemapScale;
		Vertices[5] = FVector(1.0, -1.0, 1.0) * CubemapScale;
		Vertices[6] = FVector(1.0, 1.0, -1.0) * CubemapScale;
		Vertices[7] = FVector(1.0, 1.0, 1.0) * CubemapScale;

		Triangles.Reserve(24);
		AppendFaceIndices(0, 1, 3, 2, Triangles, false);
		AppendFaceIndices(4, 5, 7, 6, Triangles, true);
		AppendFaceIndices(0, 1, 5, 4, Triangles, true);
		AppendFaceIndices(2, 3, 7, 6, Triangles, false);
		AppendFaceIndices(0, 2, 6, 4, Triangles, false);
		AppendFaceIndices(1, 3, 7, 5, Triangles, true);
	}
}
#endif


void FLayer::SetEyeLayerDesc(const ovrpLayerDesc_EyeFov& InEyeLayerDesc, const ovrpRecti InViewportRect[ovrpEye_Count])
{
	OvrpLayerDesc.EyeFov = InEyeLayerDesc;

	for(int eye = 0; eye < ovrpEye_Count; eye++)
	{
		OvrpLayerSubmit.ViewportRect[eye] = InViewportRect[eye];
	}

	bHasDepth = InEyeLayerDesc.DepthFormat != ovrpTextureFormat_None;
}


TSharedPtr<FLayer, ESPMode::ThreadSafe> FLayer::Clone() const
{
	return MakeShareable(new FLayer(*this));
}


bool FLayer::CanReuseResources(const FLayer* InLayer) const
{
	if (!InLayer || !InLayer->OvrpLayer.IsValid())
	{
		return false;
	}

	if (OvrpLayerDesc.Shape != InLayer->OvrpLayerDesc.Shape ||
		OvrpLayerDesc.Layout != InLayer->OvrpLayerDesc.Layout ||
		OvrpLayerDesc.TextureSize.w != InLayer->OvrpLayerDesc.TextureSize.w ||
		OvrpLayerDesc.TextureSize.h != InLayer->OvrpLayerDesc.TextureSize.h ||
		OvrpLayerDesc.MipLevels != InLayer->OvrpLayerDesc.MipLevels ||
		OvrpLayerDesc.SampleCount != InLayer->OvrpLayerDesc.SampleCount ||
		OvrpLayerDesc.Format != InLayer->OvrpLayerDesc.Format ||
		((OvrpLayerDesc.LayerFlags ^ InLayer->OvrpLayerDesc.LayerFlags) & ovrpLayerFlag_Static))
	{
		return false;
	}

	if (OvrpLayerDesc.Shape == ovrpShape_EyeFov)
	{
		if (OvrpLayerDesc.EyeFov.DepthFormat != InLayer->OvrpLayerDesc.EyeFov.DepthFormat)
		{
			return false;
		}
	}

	return true;
}


void FLayer::Initialize_RenderThread(FCustomPresent* CustomPresent, FRHICommandListImmediate& RHICmdList, const FLayer* InLayer)
{
	CheckInRenderThread();

	if (Id == 0)
	{
		// OvrpLayerDesc and OvrpViewportRects already initialized, as this is the eyeFOV layer. The only necessary modification is to take into account MSAA level, that can only be accurately determined on the RT.
	}
	else
	{
		bInvertY = ( CustomPresent->GetLayerFlags() & ovrpLayerFlag_TextureOriginAtBottomLeft ) != 0;

		uint32 SizeX = 0, SizeY = 0;

		if (Desc.Texture.IsValid())
		{
			FRHITexture2D* Texture2D = Desc.Texture->GetTexture2D();
			FRHITextureCube* TextureCube = Desc.Texture->GetTextureCube();

			if (Texture2D)
			{
				SizeX = Texture2D->GetSizeX();
				SizeY = Texture2D->GetSizeY();
			}
			else if (TextureCube)
			{
				SizeX = SizeY = TextureCube->GetSize();
			}
		}
		else
		{
			SizeX = Desc.LayerSize.X;
			SizeY = Desc.LayerSize.Y;
		}

		if (SizeX == 0 || SizeY == 0)
			return;

		ovrpShape Shape;
		
		switch (Desc.ShapeType)
		{
		case IStereoLayers::QuadLayer:
			Shape = ovrpShape_Quad;
			break;

		case IStereoLayers::CylinderLayer:
			Shape = ovrpShape_Cylinder;
			break;

		case IStereoLayers::CubemapLayer:
			Shape = ovrpShape_Cubemap;
			break;

		default:
			return;
		}

		EPixelFormat Format = Desc.Texture.IsValid() ? CustomPresent->GetPixelFormat(Desc.Texture->GetFormat()) : CustomPresent->GetDefaultPixelFormat();
#if PLATFORM_ANDROID
		uint32 NumMips = 1;
#else
		uint32 NumMips = 0;
#endif
		uint32 NumSamples = 1;
		int LayerFlags = CustomPresent->GetLayerFlags();

		if(!(Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE))
			LayerFlags |= ovrpLayerFlag_Static;

		// Calculate layer desc
		ovrp_CalculateLayerDesc(
			Shape,
			!Desc.LeftTexture.IsValid() ? ovrpLayout_Mono : ovrpLayout_Stereo,
			ovrpSizei { (int) SizeX, (int) SizeY },
			NumMips,
			NumSamples,
			CustomPresent->GetOvrpTextureFormat(Format),
			LayerFlags,
			&OvrpLayerDesc);

		// Calculate viewport rect
		for (uint32 EyeIndex = 0; EyeIndex < ovrpEye_Count; EyeIndex++)
		{
			ovrpRecti& ViewportRect = OvrpLayerSubmit.ViewportRect[EyeIndex];
			ViewportRect.Pos.x = (int)(Desc.UVRect.Min.X * SizeX + 0.5f);
			ViewportRect.Pos.y = (int)(Desc.UVRect.Min.Y * SizeY + 0.5f);
			ViewportRect.Size.w = (int)(Desc.UVRect.Max.X * SizeX + 0.5f) - ViewportRect.Pos.x;
			ViewportRect.Size.h = (int)(Desc.UVRect.Max.Y * SizeY + 0.5f) - ViewportRect.Pos.y;
		}
	}
	
	// Reuse/Create texture set
	if (CanReuseResources(InLayer))
	{
		OvrpLayerId = InLayer->OvrpLayerId;
		OvrpLayer = InLayer->OvrpLayer;
		TextureSetProxy = InLayer->TextureSetProxy;
		DepthTextureSetProxy = InLayer->DepthTextureSetProxy;
		RightTextureSetProxy = InLayer->RightTextureSetProxy;
		RightDepthTextureSetProxy = InLayer->RightDepthTextureSetProxy;
		bUpdateTexture = InLayer->bUpdateTexture;
	}
	else
	{
		bool bLayerCreated = false;
		TArray<ovrpTextureHandle> ColorTextures;
		TArray<ovrpTextureHandle> DepthTextures;
		TArray<ovrpTextureHandle> RightColorTextures;
		TArray<ovrpTextureHandle> RightDepthTextures;

		ExecuteOnRHIThread([&]()
		{
			// UNDONE Do this in RenderThread once OVRPlugin allows ovrp_SetupLayer to be called asynchronously
			int32 TextureCount;
			if (OVRP_SUCCESS(ovrp_SetupLayer(CustomPresent->GetOvrpDevice(), OvrpLayerDesc.Base, (int*) &OvrpLayerId)) &&
				OVRP_SUCCESS(ovrp_GetLayerTextureStageCount(OvrpLayerId, &TextureCount)))
			{
				// Left
				{
					ColorTextures.SetNum(TextureCount);
					if (bHasDepth)
					{
						DepthTextures.SetNum(TextureCount);
					}
					

					for (int32 TextureIndex = 0; TextureIndex < TextureCount; TextureIndex++)
					{
						ovrpTextureHandle* DepthTexHdlPtr = bHasDepth ? &DepthTextures[TextureIndex] : nullptr;
						if (!OVRP_SUCCESS(ovrp_GetLayerTexture2(OvrpLayerId, TextureIndex, ovrpEye_Left, &ColorTextures[TextureIndex], DepthTexHdlPtr)))
						{
							UE_LOG(LogHMD, Error, TEXT("Failed to create Oculus layer texture. NOTE: This causes a leak of %d other texture(s), which will go unused."), TextureIndex);
							// skip setting bLayerCreated and allocating any other textures
							return;
						}
					}
				}

				// Right
				if(OvrpLayerDesc.Layout == ovrpLayout_Stereo)
				{
					RightColorTextures.SetNum(TextureCount);
					if (bHasDepth)
					{
						RightDepthTextures.SetNum(TextureCount);
					}

					for (int32 TextureIndex = 0; TextureIndex < TextureCount; TextureIndex++)
					{
						ovrpTextureHandle* DepthTexHdlPtr = bHasDepth ? &RightDepthTextures[TextureIndex] : nullptr;
						if (!OVRP_SUCCESS(ovrp_GetLayerTexture2(OvrpLayerId, TextureIndex, ovrpEye_Right, &RightColorTextures[TextureIndex], DepthTexHdlPtr)))
						{
							UE_LOG(LogHMD, Error, TEXT("Failed to create Oculus layer texture. NOTE: This causes a leak of %d other texture(s), which will go unused."), TextureCount + TextureIndex);
							// skip setting bLayerCreated and allocating any other textures
							return;
						}
					}
				}

				bLayerCreated = true;
			}
		});

		if(bLayerCreated)
		{
			OvrpLayer = MakeShareable<FOvrpLayer>(new FOvrpLayer(OvrpLayerId));

			uint32 SizeX = OvrpLayerDesc.TextureSize.w;
			uint32 SizeY = OvrpLayerDesc.TextureSize.h;
			EPixelFormat ColorFormat = CustomPresent->GetPixelFormat(OvrpLayerDesc.Format);
			EPixelFormat DepthFormat = PF_DepthStencil;
			uint32 NumMips = OvrpLayerDesc.MipLevels;
			uint32 NumSamples = OvrpLayerDesc.SampleCount;
			uint32 NumSamplesTileMem = 1;
			if (OvrpLayerDesc.Shape == ovrpShape_EyeFov)
			{
				NumSamplesTileMem = CustomPresent->GetSystemRecommendedMSAALevel();
			}

			ERHIResourceType ResourceType;			
			if (OvrpLayerDesc.Shape == ovrpShape_Cubemap || OvrpLayerDesc.Shape == ovrpShape_OffcenterCubemap)
			{
				ResourceType = RRT_TextureCube;
			}
			else if (OvrpLayerDesc.Layout == ovrpLayout_Array)
			{
				ResourceType = RRT_Texture2DArray;
			}
			else
			{
				ResourceType = RRT_Texture2D;
			}

			FSceneRenderTargets& SceneContext = FSceneRenderTargets::Get(RHICmdList);

			uint32 ColorTexCreateFlags = TexCreate_ShaderResource | TexCreate_RenderTargetable;
			uint32 DepthTexCreateFlags = TexCreate_ShaderResource | TexCreate_DepthStencilTargetable;

			FClearValueBinding ColorTextureBinding = FClearValueBinding();

			FClearValueBinding DepthTextureBinding = SceneContext.GetDefaultDepthClear();

			TextureSetProxy = CustomPresent->CreateTextureSetProxy_RenderThread(SizeX, SizeY, ColorFormat, ColorTextureBinding, NumMips, NumSamples, NumSamplesTileMem, ResourceType, ColorTextures, ColorTexCreateFlags);

			if (bHasDepth)
			{
				DepthTextureSetProxy = CustomPresent->CreateTextureSetProxy_RenderThread(SizeX, SizeY, DepthFormat, DepthTextureBinding, 1, NumSamples, NumSamplesTileMem, ResourceType, DepthTextures, DepthTexCreateFlags);
			}

			if (OvrpLayerDesc.Layout == ovrpLayout_Stereo)
			{
				RightTextureSetProxy = CustomPresent->CreateTextureSetProxy_RenderThread(SizeX, SizeY, ColorFormat, ColorTextureBinding, NumMips, NumSamples, NumSamplesTileMem, ResourceType, RightColorTextures, ColorTexCreateFlags);

				if (bHasDepth)
				{
					RightDepthTextureSetProxy = CustomPresent->CreateTextureSetProxy_RenderThread(SizeX, SizeY, DepthFormat, DepthTextureBinding, 1, NumSamples, NumSamplesTileMem, ResourceType, RightDepthTextures, DepthTexCreateFlags);
				}
			}
		}

		bUpdateTexture = true;
	}

	if (Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_CONTINUOUS_UPDATE && Desc.Texture.IsValid())
	{
		bUpdateTexture = true;
	}
}

void FLayer::UpdateTexture_RenderThread(FCustomPresent* CustomPresent, FRHICommandListImmediate& RHICmdList)
{
	CheckInRenderThread();

	if (bUpdateTexture && TextureSetProxy.IsValid())
	{
		// Copy textures
		if (Desc.Texture.IsValid())
		{
			bool bAlphaPremultiply = true;
			bool bNoAlphaWrite = (Desc.Flags & IStereoLayers::LAYER_FLAG_TEX_NO_ALPHA_CHANNEL) != 0;
			bool bIsCubemap = (Desc.ShapeType == IStereoLayers::ELayerShape::CubemapLayer);

			// Left
			{
				FRHITexture* SrcTexture = Desc.LeftTexture.IsValid() ? Desc.LeftTexture : Desc.Texture;
				FRHITexture* DstTexture = TextureSetProxy->GetTexture();

				const ovrpRecti& OvrpViewportRect = OvrpLayerSubmit.ViewportRect[ovrpEye_Left];
				FIntRect DstRect(OvrpViewportRect.Pos.x, OvrpViewportRect.Pos.y, OvrpViewportRect.Pos.x + OvrpViewportRect.Size.w, OvrpViewportRect.Pos.y + OvrpViewportRect.Size.h);

				CustomPresent->CopyTexture_RenderThread(RHICmdList, DstTexture, SrcTexture, DstRect, FIntRect(), bAlphaPremultiply, bNoAlphaWrite, bInvertY);
			}

			// Right
			if(OvrpLayerDesc.Layout != ovrpLayout_Mono)
			{
				FRHITexture* SrcTexture = Desc.Texture;
				FRHITexture* DstTexture = RightTextureSetProxy.IsValid() ? RightTextureSetProxy->GetTexture() : TextureSetProxy->GetTexture();

				const ovrpRecti& OvrpViewportRect = OvrpLayerSubmit.ViewportRect[ovrpEye_Right];
				FIntRect DstRect(OvrpViewportRect.Pos.x, OvrpViewportRect.Pos.y, OvrpViewportRect.Pos.x + OvrpViewportRect.Size.w, OvrpViewportRect.Pos.y + OvrpViewportRect.Size.h);

				CustomPresent->CopyTexture_RenderThread(RHICmdList, DstTexture, SrcTexture, DstRect, FIntRect(), bAlphaPremultiply, bNoAlphaWrite, bInvertY);
			}

			bUpdateTexture = false;
		}

		// Generate mips
		TextureSetProxy->GenerateMips_RenderThread(RHICmdList);

		if (RightTextureSetProxy.IsValid())
		{
			RightTextureSetProxy->GenerateMips_RenderThread(RHICmdList);
		}
	}
}


const ovrpLayerSubmit* FLayer::UpdateLayer_RHIThread(const FSettings* Settings, const FGameFrame* Frame, const int LayerIndex)
{
	OvrpLayerSubmit.LayerId = OvrpLayerId;
	OvrpLayerSubmit.TextureStage = TextureSetProxy.IsValid() ? TextureSetProxy->GetSwapChainIndex_RHIThread() : 0;

	if (Id != 0)
	{
		int SizeX = OvrpLayerDesc.TextureSize.w;
		int SizeY = OvrpLayerDesc.TextureSize.h;

		float AspectRatio = SizeX ? (float)SizeY / (float)SizeX : 3.0f / 4.0f;
		FVector LocationScaleInv(Frame->WorldToMetersScale);
		FVector LocationScale = LocationScaleInv.Reciprocal();
		ovrpVector3f Scale = ToOvrpVector3f(Desc.Transform.GetScale3D() * LocationScale);

		switch (OvrpLayerDesc.Shape)
		{
		case ovrpShape_Quad:
			{
				float QuadSizeY = (Desc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO) ? Desc.QuadSize.X * AspectRatio : Desc.QuadSize.Y;
				OvrpLayerSubmit.Quad.Size = ovrpSizef { Desc.QuadSize.X * Scale.x, QuadSizeY * Scale.y };
			}
			break;
		case ovrpShape_Cylinder:
			{
				float CylinderHeight = (Desc.Flags & IStereoLayers::LAYER_FLAG_QUAD_PRESERVE_TEX_RATIO) ? Desc.CylinderOverlayArc * AspectRatio : Desc.CylinderHeight;
				OvrpLayerSubmit.Cylinder.ArcWidth = Desc.CylinderOverlayArc * Scale.x;
				OvrpLayerSubmit.Cylinder.Height = CylinderHeight * Scale.x;
				OvrpLayerSubmit.Cylinder.Radius = Desc.CylinderRadius * Scale.x;
			}
			break;
		}

		FQuat BaseOrientation;
		FVector BaseLocation;

		switch (Desc.PositionType)
		{
		case IStereoLayers::WorldLocked:
			BaseOrientation = Frame->TrackingToWorld.GetRotation();
			BaseLocation = Frame->TrackingToWorld.GetTranslation();
			break;

		case IStereoLayers::TrackerLocked:
			BaseOrientation = FQuat::Identity;
			BaseLocation = FVector::ZeroVector;
			break;

		case IStereoLayers::FaceLocked:
			BaseOrientation = FQuat::Identity;
			BaseLocation = FVector::ZeroVector;
			break;
		}

		FTransform PlayerTransform(BaseOrientation, BaseLocation);

		FQuat Orientation = Desc.Transform.Rotator().Quaternion();
		FVector Location = Desc.Transform.GetLocation();

		OvrpLayerSubmit.Pose.Orientation = ToOvrpQuatf(BaseOrientation.Inverse() * Orientation);
		OvrpLayerSubmit.Pose.Position = ToOvrpVector3f((PlayerTransform.InverseTransformPosition(Location)) * LocationScale);
		OvrpLayerSubmit.LayerSubmitFlags = 0;

		if (Desc.PositionType == IStereoLayers::FaceLocked)
		{
			OvrpLayerSubmit.LayerSubmitFlags |= ovrpLayerSubmitFlag_HeadLocked;
		}

		if (!(Desc.Flags & IStereoLayers::LAYER_FLAG_SUPPORT_DEPTH))
		{
			OvrpLayerSubmit.LayerSubmitFlags |= ovrpLayerSubmitFlag_NoDepth;
		}
	}
	else
	{
		OvrpLayerSubmit.EyeFov.DepthFar = 0;
		OvrpLayerSubmit.EyeFov.DepthNear = Frame->NearClippingPlane / 100.f; //physical scale is 100UU/meter
		OvrpLayerSubmit.LayerSubmitFlags = ovrpLayerSubmitFlag_ReverseZ;

		if (Settings->Flags.bPixelDensityAdaptive)
		{
			for (int eye = 0; eye < ovrpEye_Count; eye++)
			{
				OvrpLayerSubmit.ViewportRect[eye] = ToOvrpRecti(Settings->EyeRenderViewport[eye]);
			}
		}

#if PLATFORM_ANDROID
		if (LayerIndex != 0)
		{
			OvrpLayerSubmit.LayerSubmitFlags |= ovrpLayerSubmitFlag_InverseAlpha;
		}
#endif
	}

	return &OvrpLayerSubmit.Base;
}


void FLayer::IncrementSwapChainIndex_RHIThread(FCustomPresent* CustomPresent)
{
	CheckInRHIThread();

	if (TextureSetProxy.IsValid())
	{
		TextureSetProxy->IncrementSwapChainIndex_RHIThread(CustomPresent);
	}

	if (DepthTextureSetProxy.IsValid())
	{
		DepthTextureSetProxy->IncrementSwapChainIndex_RHIThread(CustomPresent);
	}

	if (RightTextureSetProxy.IsValid())
	{
		RightTextureSetProxy->IncrementSwapChainIndex_RHIThread(CustomPresent);
	}

	if (RightDepthTextureSetProxy.IsValid())
	{
		RightDepthTextureSetProxy->IncrementSwapChainIndex_RHIThread(CustomPresent);
	}
}


void FLayer::ReleaseResources_RHIThread()
{
	CheckInRHIThread();

	OvrpLayerId = 0;
	OvrpLayer.Reset();
	TextureSetProxy.Reset();
	DepthTextureSetProxy.Reset();
	RightTextureSetProxy.Reset();
	RightDepthTextureSetProxy.Reset();
	bUpdateTexture = false;
}

} // namespace OculusHMD

#endif //OCULUS_HMD_SUPPORTED_PLATFORMS
