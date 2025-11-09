// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include <CoreMinimal.h>
#include <Factories/Factory.h>
#include "QOIFactory.generated.h"

UCLASS()
class UQOIFactory : public UFactory
{
    GENERATED_BODY()

public:
    UQOIFactory();

    /* UFactory implementation */
    virtual bool FactoryCanImport(const FString& Filename) override;
    virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn, bool& bOutOperationCanceled) override;

private:
    bool DecodeQOIFormat(const FString& Filename, TArray<uint8>& OutRawRGBA, int32& OutWidth, int32& OutHeight);
};