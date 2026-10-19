// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "QOIExporter.h"

#include "Engine/Texture2D.h"
#include "Misc/FeedbackContext.h"
#include "Serialization/Archive.h"
#include "UnrealQOI.h"

UQOIExporter::UQOIExporter()
{
	SupportedClass = UTexture2D::StaticClass();
	FormatExtension.Add(TEXT("QOI"));
	FormatDescription.Add(TEXT("Quite OK Image Format"));
	PreferredFormatIndex = 0;
	bText = false;
}

bool UQOIExporter::SupportsObject(UObject* Object) const
{
	UTexture2D* Texture = Cast<UTexture2D>(Object);
	return Texture != nullptr && Texture->Source.IsValid();
}

bool UQOIExporter::ExportBinary(UObject* Object, const TCHAR* Type, FArchive& Ar, FFeedbackContext* Warn, int32 FileIndex, uint32 PortFlags)
{
	UTexture2D* Texture = Cast<UTexture2D>(Object);
	if (Texture == nullptr)
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("QOI export: '%s' is not a Texture2D."), *GetNameSafe(Object));
		return false;
	}

	// Reads the source data (every source format; HDR is clamped to sRGB, linear textures stay linear).
	FQOIImage Image;
	TArray64<uint8> Encoded;
	FString Error;
	if (!UnrealQOI::FromTexture(Texture, Image, &Error) || !UnrealQOI::Encode(Image, Encoded, &Error))
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("QOI export of '%s' failed: %s"), *Texture->GetName(), *Error);
		return false;
	}

	Ar.Serialize(Encoded.GetData(), Encoded.Num());
	return !Ar.IsError();
}
