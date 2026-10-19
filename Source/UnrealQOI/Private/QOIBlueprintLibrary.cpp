// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "QOIBlueprintLibrary.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "Misc/Paths.h"
#include "UnrealQOI.h"

namespace UnrealQOI::BlueprintPrivate
{
	/** Logs a failure, so Blueprint errors are visible even when OutError is not connected. */
	bool Fail(const TCHAR* Function, const FString& Error)
	{
		UE_LOG(LogUnrealQOI, Warning, TEXT("%s: %s"), Function, *Error);
		return false;
	}

	FName TextureNameFromPath(const FString& FilePath)
	{
		const FString BaseName = FPaths::GetBaseFilename(FilePath);
		return BaseName.IsEmpty() ? NAME_None : FName(*FString::Printf(TEXT("T_%s"), *BaseName));
	}

	bool PixelsToImage(const TArray<FColor>& Pixels, int32 Width, int32 Height, bool bIgnoreAlpha, bool bLinear, FQOIImage& OutImage, FString& OutError)
	{
		if (!FromColors(Pixels, Width, Height, OutImage, &OutError))
		{
			return false;
		}
		if (bIgnoreAlpha)
		{
			MakeOpaque(OutImage);
		}
		OutImage.bLinear = bLinear;
		return true;
	}
}

bool UQOIBlueprintLibrary::GetQOIFileInfo(const FString& FilePath, FQOIImageInfo& OutInfo, FString& OutError)
{
	OutInfo = FQOIImageInfo();
	OutError.Reset();
	if (!UnrealQOI::ReadFileInfo(FilePath, OutInfo, &OutError))
	{
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("GetQOIFileInfo"), OutError);
	}
	return true;
}

UTexture2D* UQOIBlueprintLibrary::LoadTextureFromQOIFile(const FString& FilePath, FString& OutError)
{
	using namespace UnrealQOI::BlueprintPrivate;

	OutError.Reset();
	FQOIImage Image;
	UTexture2D* Texture = nullptr;
	if (UnrealQOI::LoadFromFile(FilePath, Image, &OutError))
	{
		Texture = UnrealQOI::CreateTexture(Image, TextureNameFromPath(FilePath), &OutError);
	}

	if (Texture == nullptr)
	{
		Fail(TEXT("LoadTextureFromQOIFile"), OutError);
	}
	return Texture;
}

UTexture2D* UQOIBlueprintLibrary::LoadTextureFromQOIBytes(const TArray<uint8>& Bytes, FString& OutError)
{
	using namespace UnrealQOI::BlueprintPrivate;

	OutError.Reset();
	FQOIImage Image;
	UTexture2D* Texture = nullptr;
	if (UnrealQOI::Decode(Bytes.GetData(), Bytes.Num(), Image, &OutError))
	{
		Texture = UnrealQOI::CreateTexture(Image, NAME_None, &OutError);
	}

	if (Texture == nullptr)
	{
		Fail(TEXT("LoadTextureFromQOIBytes"), OutError);
	}
	return Texture;
}

bool UQOIBlueprintLibrary::LoadPixelsFromQOIFile(const FString& FilePath, TArray<FColor>& OutPixels, FQOIImageInfo& OutInfo, FString& OutError)
{
	OutPixels.Reset();
	OutInfo = FQOIImageInfo();
	OutError.Reset();

	FQOIImage Image;
	if (!UnrealQOI::LoadFromFile(FilePath, Image, &OutError))
	{
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("LoadPixelsFromQOIFile"), OutError);
	}

	if (Image.BGRA.Num() / 4 > MAX_int32)
	{
		OutError = FString::Printf(TEXT("The image is too large for a Blueprint array (%dx%d)."), Image.Width, Image.Height);
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("LoadPixelsFromQOIFile"), OutError);
	}

	UnrealQOI::ToColors(Image, OutPixels);
	OutInfo = Image.GetInfo();
	return true;
}

bool UQOIBlueprintLibrary::SaveTextureToQOIFile(UTexture2D* Texture, const FString& FilePath, FString& OutError)
{
	OutError.Reset();
	FQOIImage Image;
	if (!UnrealQOI::FromTexture(Texture, Image, &OutError) || !UnrealQOI::SaveToFile(FilePath, Image, &OutError))
	{
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("SaveTextureToQOIFile"), OutError);
	}
	return true;
}

bool UQOIBlueprintLibrary::SaveRenderTargetToQOIFile(UTextureRenderTarget2D* RenderTarget, const FString& FilePath, FString& OutError, bool bIgnoreAlpha)
{
	OutError.Reset();
	FQOIImage Image;
	if (!UnrealQOI::FromRenderTarget(RenderTarget, Image, &OutError))
	{
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("SaveRenderTargetToQOIFile"), OutError);
	}

	if (bIgnoreAlpha)
	{
		UnrealQOI::MakeOpaque(Image);
	}

	if (!UnrealQOI::SaveToFile(FilePath, Image, &OutError))
	{
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("SaveRenderTargetToQOIFile"), OutError);
	}
	return true;
}

bool UQOIBlueprintLibrary::SavePixelsToQOIFile(const TArray<FColor>& Pixels, int32 Width, int32 Height, const FString& FilePath, FString& OutError, bool bIgnoreAlpha, bool bLinear)
{
	OutError.Reset();
	FQOIImage Image;
	if (!UnrealQOI::BlueprintPrivate::PixelsToImage(Pixels, Width, Height, bIgnoreAlpha, bLinear, Image, OutError)
		|| !UnrealQOI::SaveToFile(FilePath, Image, &OutError))
	{
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("SavePixelsToQOIFile"), OutError);
	}
	return true;
}

bool UQOIBlueprintLibrary::EncodePixelsToQOIBytes(const TArray<FColor>& Pixels, int32 Width, int32 Height, TArray<uint8>& OutBytes, FString& OutError, bool bIgnoreAlpha, bool bLinear)
{
	OutBytes.Reset();
	OutError.Reset();

	FQOIImage Image;
	TArray64<uint8> Encoded;
	if (!UnrealQOI::BlueprintPrivate::PixelsToImage(Pixels, Width, Height, bIgnoreAlpha, bLinear, Image, OutError)
		|| !UnrealQOI::Encode(Image, Encoded, &OutError))
	{
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("EncodePixelsToQOIBytes"), OutError);
	}

	if (Encoded.Num() > MAX_int32)
	{
		OutError = TEXT("The encoded image is too large for a Blueprint array.");
		return UnrealQOI::BlueprintPrivate::Fail(TEXT("EncodePixelsToQOIBytes"), OutError);
	}

	OutBytes.Append(Encoded.GetData(), static_cast<int32>(Encoded.Num()));
	return true;
}
