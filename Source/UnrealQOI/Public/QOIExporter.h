// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include <CoreMinimal.h>
#include <Exporters/Exporter.h>
#include "QOIExporter.generated.h"

UCLASS()
class UQOIExporter : public UExporter
{
    GENERATED_BODY()

public:
    UQOIExporter();

    /* UExporter implementation */
    virtual bool SupportsObject(UObject* Object) const override { return Object->IsA<UTexture2D>(); }
    virtual bool ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags) override;

private:
    bool ConvertTextureDataToRGBA8(const ETextureSourceFormat& Format, TArray<uint8, FDefaultAllocator64>& RawData, TArray<uint8>& OutRGBA);
    bool EncodeQOIFormat(UTexture2D* Texture, TArray<uint8>& OutData);
};