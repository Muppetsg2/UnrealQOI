// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "QOITypes.h"
#include "QOIBlueprintLibrary.generated.h"

class UTexture2D;
class UTextureRenderTarget2D;

/**
 * Blueprint functions for loading and saving QOI (Quite OK Image) files at runtime.
 *
 * Relative file paths are resolved against the project's Saved directory,
 * e.g. "Screenshots/Shot.qoi" becomes "<Project>/Saved/Screenshots/Shot.qoi".
 */
UCLASS()
class UNREALQOI_API UQOIBlueprintLibrary : public UBlueprintFunctionLibrary
{
	GENERATED_BODY()

public:
	/** Reads the width, height, channel count and colorspace of a .qoi file without decoding it. */
	UFUNCTION(BlueprintCallable, Category = "QOI", meta = (DisplayName = "Get QOI File Info"))
	static bool GetQOIFileInfo(const FString& FilePath, FQOIImageInfo& OutInfo, FString& OutError);

	/**
	 * Loads a .qoi file as a new transient texture (uncompressed, no mips).
	 * Returns None on failure. Store the result in a variable to keep it alive.
	 */
	UFUNCTION(BlueprintCallable, Category = "QOI|Load", meta = (DisplayName = "Load Texture From QOI File"))
	static UTexture2D* LoadTextureFromQOIFile(const FString& FilePath, FString& OutError);

	/** Decodes QOI file data held in memory as a new transient texture. Returns None on failure. */
	UFUNCTION(BlueprintCallable, Category = "QOI|Load", meta = (DisplayName = "Load Texture From QOI Bytes"))
	static UTexture2D* LoadTextureFromQOIBytes(const TArray<uint8>& Bytes, FString& OutError);

	/** Loads a .qoi file as an array of pixels (Width * Height colors, row by row, top-left first). */
	UFUNCTION(BlueprintCallable, Category = "QOI|Load", meta = (DisplayName = "Load Pixels From QOI File"))
	static bool LoadPixelsFromQOIFile(const FString& FilePath, TArray<FColor>& OutPixels, FQOIImageInfo& OutInfo, FString& OutError);

	/**
	 * Saves a texture to a .qoi file.
	 * In the editor every texture works. In packaged games only uncompressed 8-bit textures whose pixels are
	 * still in memory work (e.g. textures loaded with Load Texture From QOI File); for other textures draw them
	 * into a render target and use Save Render Target To QOI File.
	 */
	UFUNCTION(BlueprintCallable, Category = "QOI|Save", meta = (DisplayName = "Save Texture To QOI File"))
	static bool SaveTextureToQOIFile(UTexture2D* Texture, const FString& FilePath, FString& OutError);

	/**
	 * Saves the contents of a render target to a .qoi file. Float render targets are converted to sRGB.
	 * @param bIgnoreAlpha	Write every pixel as fully opaque. Scene captures often have meaningless alpha.
	 */
	UFUNCTION(BlueprintCallable, Category = "QOI|Save", meta = (DisplayName = "Save Render Target To QOI File", AdvancedDisplay = "bIgnoreAlpha"))
	static bool SaveRenderTargetToQOIFile(UTextureRenderTarget2D* RenderTarget, const FString& FilePath, FString& OutError, bool bIgnoreAlpha = true);

	/**
	 * Saves an array of pixels (Width * Height colors, row by row, top-left first) to a .qoi file.
	 * @param bIgnoreAlpha	Write every pixel as fully opaque.
	 * @param bLinear		Mark the image as linear data instead of sRGB color in the file header.
	 */
	UFUNCTION(BlueprintCallable, Category = "QOI|Save", meta = (DisplayName = "Save Pixels To QOI File", AdvancedDisplay = "bIgnoreAlpha,bLinear"))
	static bool SavePixelsToQOIFile(const TArray<FColor>& Pixels, int32 Width, int32 Height, const FString& FilePath, FString& OutError, bool bIgnoreAlpha = false, bool bLinear = false);

	/**
	 * Encodes an array of pixels (Width * Height colors, row by row, top-left first) to QOI file data.
	 * @param bIgnoreAlpha	Write every pixel as fully opaque.
	 * @param bLinear		Mark the image as linear data instead of sRGB color in the file header.
	 */
	UFUNCTION(BlueprintCallable, Category = "QOI|Save", meta = (DisplayName = "Encode Pixels To QOI Bytes", AdvancedDisplay = "bIgnoreAlpha,bLinear"))
	static bool EncodePixelsToQOIBytes(const TArray<FColor>& Pixels, int32 Width, int32 Height, TArray<uint8>& OutBytes, FString& OutError, bool bIgnoreAlpha = false, bool bLinear = false);
};
