// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "QOIExporter.h"
#include <Engine/Texture.h>
#include <Engine/Texture2D.h>
#include <Engine/TextureDefines.h>
#include <Misc/FileHelper.h>
#include <Serialization/BufferArchive.h>
#include <Logging/LogMacros.h>
#include "koi/koi_image_write.h"

DEFINE_LOG_CATEGORY_STATIC(LogQOIExporter, Log, All);

UQOIExporter::UQOIExporter()
{
    bText = false;
    FormatExtension.Add(FString(TEXT("qoi")));
    FormatDescription.Add(FString(TEXT("Quiet Ok Image Format")));
    SupportedClass = UTexture2D::StaticClass();
}

bool UQOIExporter::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
    UTexture2D* Texture = Cast<UTexture2D>(Object);
    if (!Texture) {
        UE_LOG(LogQOIExporter, Error, TEXT("ExportBinary called with non-texture object"));
        return false;
    }

    TArray<uint8> OutData;
    if (!EncodeQOIFormat(Texture, OutData))
    {
        UE_LOG(LogQOIExporter, Error, TEXT("Failed to encode texture %s"), *Texture->GetName());
        return false;
    }

    Ar.Serialize(OutData.GetData(), OutData.Num());
    return true;
}

bool UQOIExporter::ConvertTextureDataToRGBA8(const ETextureSourceFormat& Format, TArray<uint8, FDefaultAllocator64>& RawData, TArray<uint8>& OutRGBA) 
{
    auto FloatToByteColor = [](float v) -> uint8
    {
        v = FMath::Clamp(v, 0.0f, 1.0f);
        return static_cast<uint8>(v * 255.0f + 0.5f);
    };

    int32 PixelCount = RawData.Num();
    switch (Format)
    {
        case TSF_G8:
        {
            OutRGBA.SetNumUninitialized(PixelCount * 4);
            for (int32 i = 0; i < PixelCount; ++i)
            {
                const int32 s = i * 4;
                uint8 Gray = RawData[i];
                OutRGBA[s + 0] = Gray; // R
                OutRGBA[s + 1] = Gray; // G
                OutRGBA[s + 2] = Gray; // B
                OutRGBA[s + 3] = 255;  // A
            }
            break;
        }
        case TSF_BGRA8:
        {
            OutRGBA.SetNumUninitialized(PixelCount);
            for (int32 i = 0; i < PixelCount; i += 4)
            {
                OutRGBA[i + 0] = RawData[i + 2]; // R
                OutRGBA[i + 1] = RawData[i + 1]; // G
                OutRGBA[i + 2] = RawData[i + 0]; // B
                OutRGBA[i + 3] = RawData[i + 3]; // A
            }
            break;
        }
        case TSF_BGRE8: 
        {
            OutRGBA.SetNumUninitialized(PixelCount);
            // RGBE → RGBA (HDR to LDR conversion)
            for (int32 i = 0; i < PixelCount; i += 4)
            {
                const int32 s = i * 4;
                const float e = RawData[s + 3] - 128.0f; // exponent bias 128
                const float scale = powf(2.0f, e);       // scale factor

                float r = (RawData[s + 2] / 255.0f) * scale;
                float g = (RawData[s + 1] / 255.0f) * scale;
                float b = (RawData[s + 0] / 255.0f) * scale;

                // simple tone mapping (Reinhard)
                r = r / (1.0f + r);
                g = g / (1.0f + g);
                b = b / (1.0f + b);

                OutRGBA[s + 0] = FloatToByteColor(r);
                OutRGBA[s + 1] = FloatToByteColor(g);
                OutRGBA[s + 2] = FloatToByteColor(b);
                OutRGBA[s + 3] = 255;
            }
            break;
        }
        default:
        {
            UE_LOG(LogQOIExporter, Error, TEXT("Unsupported texture format %d"), static_cast<int32>(Format));
            return false;
        }
    }

    return true;
}

bool UQOIExporter::EncodeQOIFormat(UTexture2D* Texture, TArray<uint8>& OutData)
{
    // Get Texture Information
    FTextureSource& Src = Texture->Source;
    const int32 Width = Src.GetSizeX();
    const int32 Height = Src.GetSizeY();

    if (Src.GetSizeX() == 0 || Src.GetSizeY() == 0)
    {
        UE_LOG(LogQOIExporter, Error, TEXT("Texture %s has invalid dimensions"), *Texture->GetName());
        return false;
    }

    // Get Raw Texture Data
    TArray<uint8, FDefaultAllocator64> RawData;
    Src.GetMipData(RawData, 0);

    // Convert to RGBA8 if necessary
    const ETextureSourceFormat Format = Src.GetFormat();
    TArray<uint8> ConvertedData;
    if (!ConvertTextureDataToRGBA8(Format, RawData, ConvertedData))
    {
        UE_LOG(LogQOIExporter, Error, TEXT("Unsupported texture format for texture %s"), *Texture->GetName());
        return false;
    }

    // Encode to QOI format using koi library and write directly to archive
    int res = koi_write_qoi_to_func(
        [](void* context, void* data, int size)
        {
            TArray<uint8>* OutDataPtr = static_cast<TArray<uint8>*>(context);
            OutDataPtr->Append(static_cast<uint8*>(data), size);
        }, &OutData, Width, Height, 4, ConvertedData.GetData()
    );

    if (res == 0)
    {
        UE_LOG(LogQOIExporter, Error, TEXT("koi_write_qoi_to_func failed for texture %s"), *Texture->GetName());
        return false;
    }

    return true;
}