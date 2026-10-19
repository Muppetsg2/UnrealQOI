// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "QOIExporter.h"

#include "QOICodec.h"

#include "Engine/Texture2D.h"
#include "ImageCore.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FeedbackContext.h"
#include "Serialization/Archive.h"

// FTextureSource::GetMipImage (which handles every source format and its gamma
// space) is used on newer engines; older ones read the raw mip and describe it manually.
#define UNREALQOI_USE_GETMIPIMAGE !UE_VERSION_OLDER_THAN(5, 2, 0)

namespace UnrealQOI::Private
{
	/** Reads mip 0 of the first block/layer of the texture's source data. */
	bool ReadSourceImage(UTexture2D& Texture, FImage& OutImage, FString& OutError)
	{
		FTextureSource& Source = Texture.Source;
		if (!Source.IsValid())
		{
			OutError = TEXT("The texture has no source data.");
			return false;
		}

#if UNREALQOI_USE_GETMIPIMAGE
		if (!Source.GetMipImage(OutImage, /*BlockIndex*/ 0, /*LayerIndex*/ 0, /*MipIndex*/ 0))
		{
			OutError = TEXT("Could not read the source data.");
			return false;
		}
		return true;
#else
		ERawImageFormat::Type RawFormat;
		EGammaSpace GammaSpace = EGammaSpace::Linear;
		switch (Source.GetFormat())
		{
		case TSF_G8:
			RawFormat = ERawImageFormat::G8;
			GammaSpace = Texture.SRGB ? EGammaSpace::sRGB : EGammaSpace::Linear;
			break;
		case TSF_BGRA8:
			RawFormat = ERawImageFormat::BGRA8;
			GammaSpace = Texture.SRGB ? EGammaSpace::sRGB : EGammaSpace::Linear;
			break;
		case TSF_BGRE8:
			RawFormat = ERawImageFormat::BGRE8;
			break;
		case TSF_G16:
			RawFormat = ERawImageFormat::G16;
			break;
		case TSF_RGBA16:
			RawFormat = ERawImageFormat::RGBA16;
			break;
		case TSF_RGBA16F:
			RawFormat = ERawImageFormat::RGBA16F;
			break;
		default:
			OutError = FString::Printf(TEXT("Source format %d is not supported on this engine version."), static_cast<int32>(Source.GetFormat()));
			return false;
		}

		TArray64<uint8> MipData;
		if (!Source.GetMipData(MipData, /*MipIndex*/ 0))
		{
			OutError = TEXT("Could not read the source data.");
			return false;
		}

		OutImage.Init(static_cast<int32>(Source.GetSizeX()), static_cast<int32>(Source.GetSizeY()), RawFormat, GammaSpace);
		if (MipData.Num() < OutImage.RawData.Num())
		{
			OutError = FString::Printf(TEXT("Source data is %lld bytes, expected %lld."), MipData.Num(), OutImage.RawData.Num());
			return false;
		}
		FMemory::Memcpy(OutImage.RawData.GetData(), MipData.GetData(), OutImage.RawData.Num());
		return true;
#endif
	}

	bool IsHDRFormat(ERawImageFormat::Type Format)
	{
		switch (Format)
		{
		case ERawImageFormat::BGRE8:
		case ERawImageFormat::RGBA16F:
		case ERawImageFormat::RGBA32F:
#if UNREALQOI_USE_GETMIPIMAGE
		case ERawImageFormat::R16F:
		case ERawImageFormat::R32F:
#endif
			return true;
		default:
			return false;
		}
	}
}

UQOIExporter::UQOIExporter()
{
	SupportedClass = UTexture2D::StaticClass();
	FormatExtension.Add(TEXT("QOI"));
	FormatDescription.Add(TEXT("Quite OK Image Format"));
	PreferredFormatIndex = 0;
	bText = false;
}

bool UQOIExporter::SupportsObject(UObject* Object) const
{
	UTexture2D* Texture = Cast<UTexture2D>(Object);
	return Texture != nullptr && Texture->Source.IsValid();
}

bool UQOIExporter::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
	UTexture2D* Texture = Cast<UTexture2D>(Object);
	if (Texture == nullptr)
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("QOI export: '%s' is not a Texture2D."), *GetNameSafe(Object));
		return false;
	}

	FImage SourceImage;
	FString Error;
	if (!UnrealQOI::Private::ReadSourceImage(*Texture, SourceImage, Error))
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("QOI export of '%s' failed: %s"), *Texture->GetName(), *Error);
		return false;
	}

	if (Texture->Source.GetNumBlocks() > 1 || Texture->Source.GetNumSlices() > 1 || Texture->Source.GetNumLayers() > 1)
	{
		Warn->Logf(ELogVerbosity::Warning, TEXT("QOI export of '%s': the texture has several source blocks, slices or layers; only the first one is exported."), *Texture->GetName());
	}

	// QOI stores 8 bits per channel. Low dynamic range data keeps the gamma space of the texture,
	// so linear data (masks, normal maps) round-trips unchanged and is flagged as linear in the header.
	// High dynamic range data is clamped to [0, 1] and stored as sRGB, like the engine's PNG export.
	const bool bIsHDR = UnrealQOI::Private::IsHDRFormat(SourceImage.Format);
	const bool bStoreLinear = !Texture->SRGB && !bIsHDR;
	if (bIsHDR)
	{
		Warn->Logf(ELogVerbosity::Warning, TEXT("QOI export of '%s': HDR source data is clamped to 8-bit sRGB."), *Texture->GetName());
	}

	FImage BGRAImage;
	SourceImage.CopyTo(BGRAImage, ERawImageFormat::BGRA8, bStoreLinear ? EGammaSpace::Linear : EGammaSpace::sRGB);

	UnrealQOI::FQOIImage QOIImage;
	QOIImage.Width = BGRAImage.SizeX;
	QOIImage.Height = BGRAImage.SizeY;
	QOIImage.bLinear = bStoreLinear;
	QOIImage.BGRA = MoveTemp(BGRAImage.RawData);

	TArray64<uint8> Encoded;
	if (!UnrealQOI::Encode(QOIImage, Encoded, Error))
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("QOI export of '%s' failed: %s"), *Texture->GetName(), *Error);
		return false;
	}

	Ar.Serialize(Encoded.GetData(), Encoded.Num());
	return !Ar.IsError();
}
