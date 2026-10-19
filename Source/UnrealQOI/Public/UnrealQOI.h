// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "QOITypes.h"

struct FImage;
class UTexture2D;
class UTextureRenderTarget2D;

UNREALQOI_API DECLARE_LOG_CATEGORY_EXTERN(LogUnrealQOI, Log, All);

/**
 * C++ API for reading and writing QOI (Quite OK Image) data, usable in the editor and in packaged games.
 *
 * Every function reports problems through its return value and, if OutError is not null, a readable message.
 *
 * Threading: functions in the "Memory", "Files" and "Pixels" sections are thread-safe and can be called from
 * worker threads (e.g. to save screenshots without a hitch). Functions in the "Engine objects" section
 * create or read UObjects and must be called on the game thread.
 */
namespace UnrealQOI
{
	/** Size of the QOI header (magic, width, height, channels, colorspace). */
	constexpr int64 HeaderSize = 14;

	/** Size of the QOI end marker. */
	constexpr int64 EndMarkerSize = 8;

	// ---------------------------------------------------------------------
	// Memory
	// ---------------------------------------------------------------------

	/** Returns true if the buffer is large enough for a QOI file and starts with the "qoif" magic. */
	UNREALQOI_API bool IsQOIData(const uint8* Data, int64 Size);

	/** Reads the header of a QOI file held in memory without decoding the pixels. */
	UNREALQOI_API bool ReadInfo(const uint8* Data, int64 Size, FQOIImageInfo& OutInfo, FString* OutError = nullptr);

	/** Decodes a QOI file held in memory. */
	UNREALQOI_API bool Decode(const uint8* Data, int64 Size, FQOIImage& OutImage, FString* OutError = nullptr);

	/** Encodes an image to QOI. The header advertises 3 channels when every pixel is fully opaque, 4 otherwise. */
	UNREALQOI_API bool Encode(const FQOIImage& Image, TArray64<uint8>& OutData, FString* OutError = nullptr);

	// ---------------------------------------------------------------------
	// Files
	// ---------------------------------------------------------------------

	/** Turns a relative path into an absolute one, relative to the project's Saved directory. Absolute paths are returned unchanged. */
	UNREALQOI_API FString ResolvePath(const FString& FilePath);

	/** Reads the header of a .qoi file without decoding the pixels. Relative paths are resolved with ResolvePath. */
	UNREALQOI_API bool ReadFileInfo(const FString& FilePath, FQOIImageInfo& OutInfo, FString* OutError = nullptr);

	/** Loads and decodes a .qoi file. Relative paths are resolved with ResolvePath. */
	UNREALQOI_API bool LoadFromFile(const FString& FilePath, FQOIImage& OutImage, FString* OutError = nullptr);

	/** Encodes an image and writes it to a .qoi file, creating missing directories. Relative paths are resolved with ResolvePath. */
	UNREALQOI_API bool SaveToFile(const FString& FilePath, const FQOIImage& Image, FString* OutError = nullptr);

	// ---------------------------------------------------------------------
	// Pixels
	// ---------------------------------------------------------------------

	/** Builds an image from FColor pixels (e.g. from UGameViewportClient::OnScreenshotCaptured). bLinear is left false. */
	UNREALQOI_API bool FromColors(TConstArrayView<FColor> Pixels, int32 Width, int32 Height, FQOIImage& OutImage, FString* OutError = nullptr);

	/** Copies the pixels of an image to an FColor array (Width * Height elements, top-left pixel first). */
	UNREALQOI_API void ToColors(const FQOIImage& Image, TArray<FColor>& OutPixels);

	/**
	 * Converts an FImage of any format to 8-bit BGRA. 8-bit and 16-bit linear images stay linear;
	 * sRGB images stay sRGB; HDR (float) images are clamped to [0, 1] and stored as sRGB.
	 */
	UNREALQOI_API bool FromImage(const FImage& Source, FQOIImage& OutImage, FString* OutError = nullptr);

	/** Copies an image to an FImage (ERawImageFormat::BGRA8, gamma space from bLinear). */
	UNREALQOI_API void ToImage(const FQOIImage& Image, FImage& OutImage);

	/** Sets the alpha of every pixel to 255. */
	UNREALQOI_API void MakeOpaque(FQOIImage& Image);

	// ---------------------------------------------------------------------
	// Engine objects (game thread only)
	// ---------------------------------------------------------------------

	/**
	 * Creates a transient, uncompressed Texture2D (PF_B8G8R8A8, no mips) from an image.
	 * sRGB is disabled for linear images. The texture is not referenced by anything: keep it in a UPROPERTY.
	 */
	UNREALQOI_API UTexture2D* CreateTexture(const FQOIImage& Image, FName Name = NAME_None, FString* OutError = nullptr);

	/**
	 * Reads the pixels of a Texture2D (mip 0).
	 * - In the editor the texture's source data is used, so every texture and source format works.
	 * - In packaged games the CPU copy of the texture data is used. This works for uncompressed
	 *   8-bit BGRA/RGBA textures whose data is still in memory (e.g. textures made with CreateTexture).
	 *   For other textures draw them into a render target and use FromRenderTarget.
	 */
	UNREALQOI_API bool FromTexture(UTexture2D* Texture, FQOIImage& OutImage, FString* OutError = nullptr);

	/**
	 * Reads the pixels of a render target. Float render targets are converted to sRGB.
	 * Flushes rendering commands, so avoid calling it every frame.
	 */
	UNREALQOI_API bool FromRenderTarget(UTextureRenderTarget2D* RenderTarget, FQOIImage& OutImage, FString* OutError = nullptr);
}
