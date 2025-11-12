// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "QOIFactory.h"
#include "UnrealQOI.h"
#include <Engine/Texture2D.h>
#include <Misc/FileHelper.h>
#include <Logging/LogMacros.h>
#include "koi/koi_image.h"

DEFINE_LOG_CATEGORY_STATIC(LogQOIFactory, Log, All);

UQOIFactory::UQOIFactory()
{
    bEditorImport = true;
    bCreateNew = false;
    Formats.Add(TEXT("qoi;Quite OK Image Format"));
    SupportedClass = UTexture2D::StaticClass();
}

bool UQOIFactory::FactoryCanImport(const FString& Filename)
{
    return FPaths::GetExtension(Filename).Equals(TEXT("qoi"), ESearchCase::IgnoreCase);
}

UObject* UQOIFactory::FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled)
{
    bOutOperationCanceled = false;

    int32 Width = 0, Height = 0;
    TArray<uint8> RawRGBA;
    if (!DecodeQOIFormat(Filename, RawRGBA, Width, Height))
    {
        UE_LOG(LogQOIFactory, Error, TEXT("Failed to decode QOI texture: %s"), *Filename);
        return nullptr;
    }

    if (RawRGBA.Num() != Width * Height * 4)
    {
        UE_LOG(LogQOIFactory, Error, TEXT("Decoded data size doesn't match width * height * 4"));
        return nullptr;
    }

    UTexture2D* NewTexture = NewObject<UTexture2D>(InParent, InName, Flags);
    if (!NewTexture) {
        UE_LOG(LogQOIFactory, Error, TEXT("Failed to create texture object"));
        return nullptr;
    }

    // Conversion RGBA → BGRA (Unreal expects BGRA for TSF_BGRA8)
    for (int32 i = 0; i < RawRGBA.Num(); i += 4)
    {
        uint8 R = RawRGBA[i];
        RawRGBA[i] = RawRGBA[i + 2];     // B
        RawRGBA[i + 2] = R;              // R
    }

    NewTexture->Source.Init(Width, Height, 1, 1, TSF_BGRA8, RawRGBA.GetData());
    NewTexture->SRGB = true;
    NewTexture->CompressionSettings = TC_Default;
    NewTexture->MipGenSettings = TMGS_FromTextureGroup;
    NewTexture->UpdateResource();
    NewTexture->MarkPackageDirty();
    return NewTexture;
}

bool UQOIFactory::DecodeQOIFormat(const FString& Filename, TArray<uint8>& OutRawRGBA, int32& OutWidth, int32& OutHeight)
{
    TArray<uint8> FileData;
    if (!FFileHelper::LoadFileToArray(FileData, *Filename))
    {
        UE_LOG(LogQOIFactory, Error, TEXT("Cannot read file %s"), *Filename);
        return false;
    }

    // Check QOI validity
    if (koi_info_from_memory(FileData.GetData(), FileData.Num(), nullptr, nullptr, nullptr) == 0) {
        UE_LOG(LogQOIFactory, Error, TEXT("File %s is not a valid QOI image"), *Filename);
        return false;
    }

    // Decode QOI data
    int Comp;
    unsigned char* ImgData = koi_load_from_memory(FileData.GetData(), FileData.Num(), &OutWidth, &OutHeight, &Comp, 4);
    if (!ImgData) {
        UE_LOG(LogQOIFactory, Error, TEXT("Failed decode file %s : %s"), *Filename, *FString(koi_failure_reason()));
        return false;
    }

    // Copy decoded data to output array
    int32 DataSize = OutWidth * OutHeight * 4;
    OutRawRGBA.SetNum(DataSize);
    FMemory::Memcpy(OutRawRGBA.GetData(), ImgData, DataSize);

    // Free decoded image data
    koi_image_free(ImgData);
    return true;
}
