// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

// File I/O and conversions between QOI images and engine types (FImage, textures, render targets).

#include "UnrealQOI.h"

#include "Engine/Texture2D.h"
#include "Engine/TextureRenderTarget2D.h"
#include "HAL/FileManager.h"
#include "ImageCore.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Templates/UniquePtr.h"
#include "TextureResource.h"
#include "UObject/Package.h"

// FTextureSource::GetMipImage (which handles every source format and its gamma
// space) is used on newer engines; older ones read the raw mip and describe it manually.
#define UNREALQOI_USE_GETMIPIMAGE !UE_VERSION_OLDER_THAN(5, 2, 0)

namespace UnrealQOI
{
	namespace EnginePrivate
	{
		void SetError(FString* OutError, const FString& Message)
		{
			if (OutError)
			{
				*OutError = Message;
			}
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

		/** Converts any FImage to BGRA8, either kept linear or encoded as sRGB. */
		void ConvertImage(const FImage& Source, bool bStoreLinear, FQOIImage& OutImage)
		{
			FImage BGRAImage;
			Source.CopyTo(BGRAImage, ERawImageFormat::BGRA8, bStoreLinear ? EGammaSpace::Linear : EGammaSpace::sRGB);

			OutImage.Width = BGRAImage.SizeX;
			OutImage.Height = BGRAImage.SizeY;
			OutImage.Channels = 4;
			OutImage.bLinear = bStoreLinear;
			OutImage.BGRA = MoveTemp(BGRAImage.RawData);
		}

#if WITH_EDITORONLY_DATA
		/** Reads mip 0 of the first block/layer of the texture's source data. */
		bool ReadSourceImage(UTexture2D& Texture, FImage& OutImage, FString* OutError)
		{
			FTextureSource& Source = Texture.Source;

#if UNREALQOI_USE_GETMIPIMAGE
			if (!Source.GetMipImage(OutImage, /*BlockIndex*/ 0, /*LayerIndex*/ 0, /*MipIndex*/ 0))
			{
				SetError(OutError, TEXT("Could not read the texture's source data."));
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
				SetError(OutError, FString::Printf(TEXT("Source format %d is not supported on this engine version."), static_cast<int32>(Source.GetFormat())));
				return false;
			}

			TArray64<uint8> MipData;
			if (!Source.GetMipData(MipData, /*MipIndex*/ 0))
			{
				SetError(OutError, TEXT("Could not read the texture's source data."));
				return false;
			}

			OutImage.Init(static_cast<int32>(Source.GetSizeX()), static_cast<int32>(Source.GetSizeY()), RawFormat, GammaSpace);
			if (MipData.Num() < OutImage.RawData.Num())
			{
				SetError(OutError, FString::Printf(TEXT("Source data is %lld bytes, expected %lld."), MipData.Num(), OutImage.RawData.Num()));
				return false;
			}
			FMemory::Memcpy(OutImage.RawData.GetData(), MipData.GetData(), OutImage.RawData.Num());
			return true;
#endif
		}

		bool FromTextureSource(UTexture2D& Texture, FQOIImage& OutImage, FString* OutError)
		{
			FImage SourceImage;
			if (!ReadSourceImage(Texture, SourceImage, OutError))
			{
				return false;
			}

			if (Texture.Source.GetNumBlocks() > 1 || Texture.Source.GetNumSlices() > 1 || Texture.Source.GetNumLayers() > 1)
			{
				UE_LOG(LogUnrealQOI, Warning, TEXT("'%s' has several source blocks, slices or layers; only the first one is used."), *Texture.GetName());
			}

			// QOI stores 8 bits per channel. Low dynamic range data keeps the gamma space of the texture,
			// so linear data (masks, normal maps) round-trips unchanged and is flagged as linear in the header.
			// High dynamic range data is clamped to [0, 1] and stored as sRGB, like the engine's PNG export.
			const bool bIsHDR = IsHDRFormat(SourceImage.Format);
			if (bIsHDR)
			{
				UE_LOG(LogUnrealQOI, Warning, TEXT("'%s' has HDR source data; it is clamped to 8-bit sRGB."), *Texture.GetName());
			}

			ConvertImage(SourceImage, !Texture.SRGB && !bIsHDR, OutImage);
			return true;
		}
#endif // WITH_EDITORONLY_DATA

		/** Reads the CPU copy of mip 0 of an uncompressed 8-bit texture (packaged games). */
		bool FromTexturePlatformData(UTexture2D& Texture, FQOIImage& OutImage, FString* OutError)
		{
			FTexturePlatformData* PlatformData = Texture.GetPlatformData();
			if (PlatformData == nullptr || PlatformData->Mips.Num() == 0)
			{
				SetError(OutError, FString::Printf(TEXT("'%s' has no pixel data."), *Texture.GetName()));
				return false;
			}

			const EPixelFormat PixelFormat = PlatformData->PixelFormat;
			if (PixelFormat != PF_B8G8R8A8 && PixelFormat != PF_R8G8B8A8)
			{
				SetError(OutError, FString::Printf(TEXT("'%s' uses a compressed or unsupported pixel format (%d). Draw it into a render target and save the render target instead."), *Texture.GetName(), static_cast<int32>(PixelFormat)));
				return false;
			}

			FTexture2DMipMap& Mip = PlatformData->Mips[0];
			const int64 NumPixels = static_cast<int64>(Mip.SizeX) * Mip.SizeY;
			if (NumPixels <= 0 || !Mip.BulkData.IsBulkDataLoaded() || Mip.BulkData.GetBulkDataSize() < NumPixels * 4)
			{
				SetError(OutError, FString::Printf(TEXT("The pixels of '%s' are not available on the CPU. Draw it into a render target and save the render target instead."), *Texture.GetName()));
				return false;
			}

			const uint8* Src = static_cast<const uint8*>(Mip.BulkData.LockReadOnly());
			if (Src == nullptr)
			{
				Mip.BulkData.Unlock();
				SetError(OutError, FString::Printf(TEXT("Could not access the pixels of '%s'."), *Texture.GetName()));
				return false;
			}

			OutImage.Width = Mip.SizeX;
			OutImage.Height = Mip.SizeY;
			OutImage.Channels = 4;
			OutImage.bLinear = !Texture.SRGB;
			OutImage.BGRA.SetNumUninitialized(NumPixels * 4);

			if (PixelFormat == PF_B8G8R8A8)
			{
				FMemory::Memcpy(OutImage.BGRA.GetData(), Src, NumPixels * 4);
			}
			else
			{
				uint8* Dst = OutImage.BGRA.GetData();
				for (int64 Index = 0; Index < NumPixels; ++Index, Src += 4, Dst += 4)
				{
					Dst[0] = Src[2];
					Dst[1] = Src[1];
					Dst[2] = Src[0];
					Dst[3] = Src[3];
				}
			}

			Mip.BulkData.Unlock();
			return true;
		}
	}

	// -------------------------------------------------------------------------
	// Files
	// -------------------------------------------------------------------------

	FString ResolvePath(const FString& FilePath)
	{
		if (FilePath.IsEmpty() || !FPaths::IsRelative(FilePath))
		{
			return FilePath;
		}
		return FPaths::ConvertRelativePathToFull(FPaths::ProjectSavedDir(), FilePath);
	}

	bool ReadFileInfo(const FString& FilePath, FQOIImageInfo& OutInfo, FString* OutError)
	{
		using namespace EnginePrivate;

		const FString FullPath = ResolvePath(FilePath);
		TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*FullPath));
		if (!Reader)
		{
			SetError(OutError, FString::Printf(TEXT("Cannot open '%s'."), *FullPath));
			return false;
		}

		const int64 FileSize = Reader->TotalSize();
		if (FileSize < HeaderSize + EndMarkerSize)
		{
			SetError(OutError, FString::Printf(TEXT("'%s' is not a QOI file (too small)."), *FullPath));
			return false;
		}

		// ReadInfo only needs the header; the size check above covers the rest of the file.
		uint8 Buffer[HeaderSize + EndMarkerSize] = {};
		Reader->Serialize(Buffer, HeaderSize);
		if (Reader->IsError())
		{
			SetError(OutError, FString::Printf(TEXT("Cannot read '%s'."), *FullPath));
			return false;
		}

		return ReadInfo(Buffer, UE_ARRAY_COUNT(Buffer), OutInfo, OutError);
	}

	bool LoadFromFile(const FString& FilePath, FQOIImage& OutImage, FString* OutError)
	{
		using namespace EnginePrivate;

		const FString FullPath = ResolvePath(FilePath);
		TArray64<uint8> FileData;
		if (!FFileHelper::LoadFileToArray(FileData, *FullPath, FILEREAD_Silent))
		{
			SetError(OutError, FString::Printf(TEXT("Cannot read '%s'."), *FullPath));
			return false;
		}

		return Decode(FileData.GetData(), FileData.Num(), OutImage, OutError);
	}

	bool SaveToFile(const FString& FilePath, const FQOIImage& Image, FString* OutError)
	{
		using namespace EnginePrivate;

		if (FilePath.IsEmpty())
		{
			SetError(OutError, TEXT("No file path given."));
			return false;
		}

		TArray64<uint8> Encoded;
		if (!Encode(Image, Encoded, OutError))
		{
			return false;
		}

		const FString FullPath = ResolvePath(FilePath);
		IFileManager::Get().MakeDirectory(*FPaths::GetPath(FullPath), /*Tree*/ true);
		if (!FFileHelper::SaveArrayToFile(Encoded, *FullPath))
		{
			SetError(OutError, FString::Printf(TEXT("Cannot write '%s'."), *FullPath));
			return false;
		}
		return true;
	}

	// -------------------------------------------------------------------------
	// Pixels
	// -------------------------------------------------------------------------

	bool FromImage(const FImage& Source, FQOIImage& OutImage, FString* OutError)
	{
		using namespace EnginePrivate;

		if (Source.SizeX <= 0 || Source.SizeY <= 0 || Source.NumSlices < 1)
		{
			SetError(OutError, FString::Printf(TEXT("Invalid image dimensions %dx%d."), Source.SizeX, Source.SizeY));
			return false;
		}

		if (Source.NumSlices > 1)
		{
			UE_LOG(LogUnrealQOI, Warning, TEXT("The image has %d slices; only the first one is used."), Source.NumSlices);
		}

		const bool bStoreLinear = Source.GammaSpace == EGammaSpace::Linear && !IsHDRFormat(Source.Format);

		if (Source.NumSlices > 1)
		{
			FImage FirstSlice;
			FirstSlice.Init(Source.SizeX, Source.SizeY, Source.Format, Source.GammaSpace);
			FMemory::Memcpy(FirstSlice.RawData.GetData(), Source.RawData.GetData(), FirstSlice.RawData.Num());
			ConvertImage(FirstSlice, bStoreLinear, OutImage);
		}
		else
		{
			ConvertImage(Source, bStoreLinear, OutImage);
		}
		return true;
	}

	void ToImage(const FQOIImage& Image, FImage& OutImage)
	{
		if (!Image.IsValid())
		{
			OutImage = FImage();
			return;
		}

		OutImage.Init(Image.Width, Image.Height, ERawImageFormat::BGRA8, Image.bLinear ? EGammaSpace::Linear : EGammaSpace::sRGB);
		FMemory::Memcpy(OutImage.RawData.GetData(), Image.BGRA.GetData(), Image.BGRA.Num());
	}

	// -------------------------------------------------------------------------
	// Engine objects
	// -------------------------------------------------------------------------

	UTexture2D* CreateTexture(const FQOIImage& Image, FName Name, FString* OutError)
	{
		using namespace EnginePrivate;

		if (!IsInGameThread())
		{
			SetError(OutError, TEXT("Textures can only be created on the game thread."));
			return nullptr;
		}

		if (!Image.IsValid())
		{
			SetError(OutError, TEXT("The image is empty or its pixel buffer has the wrong size."));
			return nullptr;
		}

		const FName UniqueName = Name.IsNone() ? NAME_None : MakeUniqueObjectName(GetTransientPackage(), UTexture2D::StaticClass(), Name);
		UTexture2D* Texture = UTexture2D::CreateTransient(Image.Width, Image.Height, PF_B8G8R8A8, UniqueName);
		if (Texture == nullptr || Texture->GetPlatformData() == nullptr || Texture->GetPlatformData()->Mips.Num() == 0)
		{
			SetError(OutError, FString::Printf(TEXT("Could not create a %dx%d texture."), Image.Width, Image.Height));
			return nullptr;
		}

		FTexture2DMipMap& Mip = Texture->GetPlatformData()->Mips[0];
		void* Dest = Mip.BulkData.Lock(LOCK_READ_WRITE);
		const bool bSizeMatches = Dest != nullptr && Mip.BulkData.GetBulkDataSize() == Image.BGRA.Num();
		if (bSizeMatches)
		{
			FMemory::Memcpy(Dest, Image.BGRA.GetData(), Image.BGRA.Num());
		}
		Mip.BulkData.Unlock();

		if (!bSizeMatches)
		{
			SetError(OutError, TEXT("Unexpected texture memory layout."));
			return nullptr;
		}

#if WITH_EDITORONLY_DATA
		Texture->MipGenSettings = TMGS_NoMipmaps;
#endif
		Texture->SRGB = !Image.bLinear;
		Texture->UpdateResource();
		return Texture;
	}

	bool FromTexture(UTexture2D* Texture, FQOIImage& OutImage, FString* OutError)
	{
		using namespace EnginePrivate;

		if (Texture == nullptr)
		{
			SetError(OutError, TEXT("No texture given."));
			return false;
		}

		if (!IsInGameThread())
		{
			SetError(OutError, TEXT("Textures can only be read on the game thread."));
			return false;
		}

#if WITH_EDITORONLY_DATA
		if (Texture->Source.IsValid())
		{
			return FromTextureSource(*Texture, OutImage, OutError);
		}
#endif

		return FromTexturePlatformData(*Texture, OutImage, OutError);
	}

	bool FromRenderTarget(UTextureRenderTarget2D* RenderTarget, FQOIImage& OutImage, FString* OutError)
	{
		using namespace EnginePrivate;

		if (RenderTarget == nullptr)
		{
			SetError(OutError, TEXT("No render target given."));
			return false;
		}

		if (!IsInGameThread())
		{
			SetError(OutError, TEXT("Render targets can only be read on the game thread."));
			return false;
		}

		FTextureRenderTargetResource* Resource = RenderTarget->GameThread_GetRenderTargetResource();
		if (Resource == nullptr)
		{
			SetError(OutError, FString::Printf(TEXT("'%s' has no render resource."), *RenderTarget->GetName()));
			return false;
		}

		// RCM_UNorm with the default linear-to-gamma flag: float targets are converted to sRGB.
		TArray<FColor> Pixels;
		if (!Resource->ReadPixels(Pixels, FReadSurfaceDataFlags(RCM_UNorm, CubeFace_MAX)))
		{
			SetError(OutError, FString::Printf(TEXT("Could not read the pixels of '%s'."), *RenderTarget->GetName()));
			return false;
		}

		const FIntPoint Size = Resource->GetSizeXY();
		return FromColors(Pixels, Size.X, Size.Y, OutImage, OutError);
	}
}
