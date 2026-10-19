// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "QOITypes.generated.h"

/** Information stored in the header of a QOI image. */
USTRUCT(BlueprintType)
struct UNREALQOI_API FQOIImageInfo
{
	GENERATED_BODY()

	/** Width of the image in pixels. */
	UPROPERTY(BlueprintReadOnly, Category = "QOI")
	int32 Width = 0;

	/** Height of the image in pixels. */
	UPROPERTY(BlueprintReadOnly, Category = "QOI")
	int32 Height = 0;

	/** Number of channels stored in the file: 3 (RGB) or 4 (RGBA). Informative only. */
	UPROPERTY(BlueprintReadOnly, Category = "QOI")
	int32 Channels = 4;

	/** True if all channels are linear (QOI colorspace 1), false for sRGB color with linear alpha (QOI colorspace 0). */
	UPROPERTY(BlueprintReadOnly, Category = "QOI")
	bool bLinear = false;
};

/**
 * An image decoded from QOI, or an image to encode to QOI (C++ only).
 *
 * Pixels are 8-bit BGRA, tightly packed, top-left pixel first. This is the memory
 * layout of FColor arrays (on little-endian platforms), ERawImageFormat::BGRA8 and PF_B8G8R8A8.
 */
struct FQOIImage
{
	int32 Width = 0;
	int32 Height = 0;

	/**
	 * Channel count from the QOI header (3 = RGB, 4 = RGBA). Filled in by decoding.
	 * Ignored when encoding: 3 channels are written when every pixel is fully opaque, 4 otherwise.
	 */
	int32 Channels = 4;

	/** True if the pixels are linear data (masks, normal maps, ...), false for sRGB color. Stored in the QOI header. */
	bool bLinear = false;

	/** Width * Height * 4 bytes of BGRA8 pixel data. */
	TArray64<uint8> BGRA;

	/** True if the image has positive dimensions and a pixel buffer of the matching size. */
	bool IsValid() const
	{
		return Width > 0 && Height > 0 && BGRA.Num() == static_cast<int64>(Width) * Height * 4;
	}

	FQOIImageInfo GetInfo() const
	{
		FQOIImageInfo Info;
		Info.Width = Width;
		Info.Height = Height;
		Info.Channels = Channels;
		Info.bLinear = bLinear;
		return Info;
	}
};
