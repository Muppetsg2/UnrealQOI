// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "QOIFactory.h"

#include "Editor.h"
#include "EditorFramework/AssetImportData.h"
#include "Engine/Texture2D.h"
#include "Misc/FeedbackContext.h"
#include "Misc/Paths.h"
#include "Subsystems/ImportSubsystem.h"
#include "UnrealQOI.h"

namespace UnrealQOIEditor::Private
{
	bool IsQOIFilename(const FString& Filename)
	{
		return FPaths::GetExtension(Filename).Equals(TEXT("qoi"), ESearchCase::IgnoreCase);
	}

	/** Replaces the texture's source pixels. Must be called between PreEditChange and PostEditChange. */
	void SetTextureSource(UTexture2D& Texture, const FQOIImage& Image)
	{
		Texture.Source.Init(Image.Width, Image.Height, /*NewNumSlices*/ 1, /*NewNumMips*/ 1, TSF_BGRA8, Image.BGRA.GetData());
	}
}

UQOIFactory::UQOIFactory()
{
	SupportedClass = UTexture2D::StaticClass();
	Formats.Add(TEXT("qoi;Quite OK Image Format"));

	bCreateNew = false;
	bEditorImport = true;
	bText = false;

	// The engine's texture reimport handler accepts any texture, regardless of the source file type.
	// A higher priority makes sure .qoi textures are reimported by this factory.
	ImportPriority = DefaultImportPriority + 1;
}

bool UQOIFactory::FactoryCanImport(const FString& Filename)
{
	return UnrealQOIEditor::Private::IsQOIFilename(Filename);
}

UObject* UQOIFactory::FactoryCreateBinary(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags, UObject* Context, const TCHAR* Type, const uint8*& Buffer, const uint8* BufferEnd, FFeedbackContext* Warn)
{
	UImportSubsystem* ImportSubsystem = GEditor ? GEditor->GetEditorSubsystem<UImportSubsystem>() : nullptr;
	if (ImportSubsystem)
	{
		ImportSubsystem->BroadcastAssetPreImport(this, InClass, InParent, InName, Type);
	}

	auto Fail = [&](const FString& Message) -> UObject*
	{
		Warn->Logf(ELogVerbosity::Error, TEXT("QOI import of '%s' failed: %s"), *InName.ToString(), *Message);
		if (ImportSubsystem)
		{
			ImportSubsystem->BroadcastAssetPostImport(this, nullptr);
		}
		return nullptr;
	};

	FQOIImage Image;
	FString Error;
	if (!UnrealQOI::Decode(Buffer, BufferEnd - Buffer, Image, &Error))
	{
		return Fail(Error);
	}

	// When importing over an existing texture, reuse it so that its settings
	// (compression, LOD group, sRGB, ...) and all references to it are kept.
	UTexture2D* Texture = FindObject<UTexture2D>(InParent, *InName.ToString());
	const bool bIsNewTexture = Texture == nullptr;
	if (bIsNewTexture)
	{
		Texture = Cast<UTexture2D>(CreateOrOverwriteAsset(UTexture2D::StaticClass(), InParent, InName, Flags));
		if (Texture == nullptr)
		{
			return Fail(TEXT("Could not create the texture asset."));
		}
	}
	else
	{
		Texture->PreEditChange(nullptr);
	}

	UnrealQOIEditor::Private::SetTextureSource(*Texture, Image);

	if (bIsNewTexture)
	{
		// Colorspace 1 in the QOI header means the pixels are linear data (masks, normal maps, ...).
		Texture->SRGB = !Image.bLinear;
		// The header says the image was saved without alpha.
		Texture->CompressionNoAlpha = Image.Channels == 3;
	}

	if (Texture->AssetImportData)
	{
		Texture->AssetImportData->Update(CurrentFilename);
	}

	// Rebuilds the platform data and the render resource.
	Texture->PostEditChange();

	if (ImportSubsystem)
	{
		ImportSubsystem->BroadcastAssetPostImport(this, Texture);
	}

	return Texture;
}

bool UQOIFactory::CanReimport(UObject* Obj, TArray<FString>& OutFilenames)
{
	const UTexture2D* Texture = Cast<UTexture2D>(Obj);
	if (Texture != nullptr && Texture->AssetImportData != nullptr
		&& UnrealQOIEditor::Private::IsQOIFilename(Texture->AssetImportData->GetFirstFilename()))
	{
		Texture->AssetImportData->ExtractFilenames(OutFilenames);
		return true;
	}
	return false;
}

void UQOIFactory::SetReimportPaths(UObject* Obj, const TArray<FString>& NewReimportPaths)
{
	UTexture2D* Texture = Cast<UTexture2D>(Obj);
	if (Texture != nullptr && Texture->AssetImportData != nullptr && ensure(NewReimportPaths.Num() == 1))
	{
		Texture->AssetImportData->UpdateFilenameOnly(NewReimportPaths[0]);
	}
}

EReimportResult::Type UQOIFactory::Reimport(UObject* Obj)
{
	UTexture2D* Texture = Cast<UTexture2D>(Obj);
	if (Texture == nullptr || Texture->AssetImportData == nullptr)
	{
		return EReimportResult::Failed;
	}

	const FString Filename = Texture->AssetImportData->GetFirstFilename();
	if (Filename.IsEmpty() || !FPaths::FileExists(Filename))
	{
		UE_LOG(LogUnrealQOI, Warning, TEXT("Cannot reimport '%s': source file '%s' does not exist."), *Texture->GetPathName(), *Filename);
		return EReimportResult::Failed;
	}

	FQOIImage Image;
	FString Error;
	if (!UnrealQOI::LoadFromFile(Filename, Image, &Error))
	{
		UE_LOG(LogUnrealQOI, Error, TEXT("Cannot reimport '%s' from '%s': %s"), *Texture->GetPathName(), *Filename, *Error);
		return EReimportResult::Failed;
	}

	// Only the pixels are replaced; the texture settings chosen by the user stay as they are.
	Texture->PreEditChange(nullptr);
	UnrealQOIEditor::Private::SetTextureSource(*Texture, Image);
	Texture->AssetImportData->Update(Filename);
	Texture->PostEditChange();
	Texture->MarkPackageDirty();

	UE_LOG(LogUnrealQOI, Log, TEXT("Reimported '%s' from '%s'."), *Texture->GetPathName(), *Filename);
	return EReimportResult::Succeeded;
}

int32 UQOIFactory::GetPriority() const
{
	return ImportPriority;
}
