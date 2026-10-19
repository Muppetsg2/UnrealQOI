// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include "CoreMinimal.h"
#include "Exporters/Exporter.h"
#include "QOIExporter.generated.h"

/**
 * Exports Texture2D assets (mip 0 of their source data) to .qoi files.
 */
UCLASS()
class UQOIExporter : public UExporter
{
	GENERATED_BODY()

public:
	UQOIExporter();

	//~ Begin UExporter Interface
	virtual bool SupportsObject(UObject* Object) const override;
	virtual bool ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex = 0, uint32 PortFlags = 0) override;
	//~ End UExporter Interface
};
