// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"

/**
 * Thin, engine-friendly wrapper around the koi QOI encoder/decoder.
 * This is the only place in the plugin that talks to the C library.
 */
namespace UnrealQOI
{
	/** Size of the QOI header (magic, width, height, channels, colorspace). */
	constexpr int64 HeaderSize = 14;

	/** Size of the QOI end marker. */
	constexpr int64 EndMarkerSize = 8;

	/** A decoded image, or an image to encode. */
	struct FQOIImage
	{
		int32 Width = 0;
		int32 Height = 0;

		/** Channel count stored in the QOI header (3 = RGB, 4 = RGBA). Informative only. */
		int32 Channels = 4;

		/** True if the QOI header marks all channels as linear (colorspace 1), false for sRGB with linear alpha (colorspace 0). */
		bool bLinear = false;

		/** Tightly packed BGRA8 pixels, top-left pixel first (the layout of TSF_BGRA8 / ERawImageFormat::BGRA8). */
		TArray64<uint8> BGRA;
	};

	/** Returns true if the buffer is large enough for a QOI file and starts with the "qoif" magic. */
	bool HasQOIMagic(const uint8* Data, int64 Size);

	/** Decodes a QOI file held in memory. On failure returns false and describes the problem in OutError. */
	bool Decode(const uint8* Data, int64 Size, FQOIImage& OutImage, FString& OutError);

	/**
	 * Encodes an image to QOI. The header advertises 3 channels when every pixel is fully opaque, 4 otherwise.
	 * On failure returns false and describes the problem in OutError.
	 */
	bool Encode(const FQOIImage& Image, TArray64<uint8>& OutData, FString& OutError);
}
