// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

// QOI encoding/decoding in memory and FColor conversions.
// This is the only file in the plugin that talks to the koi C library.

#include "UnrealQOI.h"

#include "HAL/UnrealMemory.h"
#include "Misc/ScopeExit.h"

// ---------------------------------------------------------------------------
// koi implementation
//
// The library is compiled exactly once, here. All configuration macros are
// undefined again at the end of the block so they cannot leak into other
// files that share this translation unit in unity builds.
// ---------------------------------------------------------------------------

#define KOI_IMAGE_IMPLEMENTATION
#define KOI_ONLY_QOI
#define KOI_NO_STDIO
#define KOI_NO_LINEAR
#define KOI_ASSERT(Expr) check(Expr)
#define KOI_MALLOC(Size) FMemory::Malloc(Size)
#define KOI_FREE(Ptr) FMemory::Free(Ptr)

#define KOI_IMAGE_WRITE_IMPLEMENTATION
#define KOI_WRITE_ONLY_QOI
#define KOI_WRITE_NO_STDIO
#define KOI_WRITE_ASSERT(Expr) check(Expr)

THIRD_PARTY_INCLUDES_START
#include "koi_image.h"
#include "koi_image_write.h"
THIRD_PARTY_INCLUDES_END

#undef KOI_IMAGE_IMPLEMENTATION
#undef KOI_ONLY_QOI
#undef KOI_NO_STDIO
#undef KOI_NO_LINEAR
#undef KOI_ASSERT
#undef KOI_MALLOC
#undef KOI_FREE
#undef KOI_IMAGE_WRITE_IMPLEMENTATION
#undef KOI_WRITE_ONLY_QOI
#undef KOI_WRITE_NO_STDIO
#undef KOI_WRITE_ASSERT

namespace UnrealQOI
{
	namespace CodecPrivate
	{
		void SetError(FString* OutError, const FString& Message)
		{
			if (OutError)
			{
				*OutError = Message;
			}
		}

		FString FailureReason(const char* Reason)
		{
			return Reason ? FString(ANSI_TO_TCHAR(Reason)) : FString(TEXT("unknown error"));
		}

		void AppendToArray(void* Context, void* Data, int Size)
		{
			static_cast<TArray64<uint8>*>(Context)->Append(static_cast<const uint8*>(Data), Size);
		}

		uint32 ReadBigEndian32(const uint8* Data)
		{
			return (static_cast<uint32>(Data[0]) << 24) | (static_cast<uint32>(Data[1]) << 16) | (static_cast<uint32>(Data[2]) << 8) | static_cast<uint32>(Data[3]);
		}
	}

	bool IsQOIData(const uint8* Data, int64 Size)
	{
		return Data != nullptr
			&& Size >= HeaderSize + EndMarkerSize
			&& Data[0] == 'q' && Data[1] == 'o' && Data[2] == 'i' && Data[3] == 'f';
	}

	bool ReadInfo(const uint8* Data, int64 Size, FQOIImageInfo& OutInfo, FString* OutError)
	{
		using namespace CodecPrivate;

		if (!IsQOIData(Data, Size))
		{
			SetError(OutError, TEXT("Not a QOI file (missing 'qoif' header)."));
			return false;
		}

		const uint32 Width = ReadBigEndian32(Data + 4);
		const uint32 Height = ReadBigEndian32(Data + 8);
		const uint8 Channels = Data[12];
		const uint8 Colorspace = Data[13];

		if (Width == 0 || Height == 0 || Width > static_cast<uint32>(MAX_int32) || Height > static_cast<uint32>(MAX_int32))
		{
			SetError(OutError, FString::Printf(TEXT("Invalid image dimensions %ux%u."), Width, Height));
			return false;
		}
		if (Channels != 3 && Channels != 4)
		{
			SetError(OutError, FString::Printf(TEXT("Invalid channel count %u (expected 3 or 4)."), static_cast<uint32>(Channels)));
			return false;
		}
		if (Colorspace > 1)
		{
			SetError(OutError, FString::Printf(TEXT("Invalid colorspace %u (expected 0 or 1)."), static_cast<uint32>(Colorspace)));
			return false;
		}

		OutInfo.Width = static_cast<int32>(Width);
		OutInfo.Height = static_cast<int32>(Height);
		OutInfo.Channels = Channels;
		OutInfo.bLinear = Colorspace == 1;
		return true;
	}

	bool Decode(const uint8* Data, int64 Size, FQOIImage& OutImage, FString* OutError)
	{
		using namespace CodecPrivate;

		FQOIImageInfo Info;
		if (!ReadInfo(Data, Size, Info, OutError))
		{
			return false;
		}

		if (Size > MAX_int32)
		{
			SetError(OutError, FString::Printf(TEXT("File is too large (%lld bytes)."), Size));
			return false;
		}

		int Width = 0;
		int Height = 0;
		int ChannelsInFile = 0;
		uint8* Pixels = koi_load_from_memory(Data, static_cast<int>(Size), &Width, &Height, &ChannelsInFile, KOI_rgb_alpha);
		if (Pixels == nullptr)
		{
			SetError(OutError, FString::Printf(TEXT("Decoding failed: %s."), *FailureReason(koi_failure_reason())));
			return false;
		}
		ON_SCOPE_EXIT
		{
			koi_image_free(Pixels);
		};

		if (Width != Info.Width || Height != Info.Height)
		{
			SetError(OutError, FString::Printf(TEXT("Unexpected image dimensions %dx%d."), Width, Height));
			return false;
		}

		const int64 NumPixels = static_cast<int64>(Width) * Height;

		OutImage.Width = Width;
		OutImage.Height = Height;
		OutImage.Channels = Info.Channels;
		OutImage.bLinear = Info.bLinear;
		OutImage.BGRA.SetNumUninitialized(NumPixels * 4);

		// koi returns RGBA, the engine wants BGRA.
		const uint8* Src = Pixels;
		uint8* Dst = OutImage.BGRA.GetData();
		for (int64 Index = 0; Index < NumPixels; ++Index, Src += 4, Dst += 4)
		{
			Dst[0] = Src[2];
			Dst[1] = Src[1];
			Dst[2] = Src[0];
			Dst[3] = Src[3];
		}

		return true;
	}

	bool Encode(const FQOIImage& Image, TArray64<uint8>& OutData, FString* OutError)
	{
		using namespace CodecPrivate;

		if (Image.Width <= 0 || Image.Height <= 0)
		{
			SetError(OutError, FString::Printf(TEXT("Invalid image dimensions %dx%d."), Image.Width, Image.Height));
			return false;
		}

		const int64 NumPixels = static_cast<int64>(Image.Width) * Image.Height;
		if (NumPixels > static_cast<int64>(MAX_uint32))
		{
			SetError(OutError, FString::Printf(TEXT("Image is too large for QOI (%dx%d)."), Image.Width, Image.Height));
			return false;
		}

		if (Image.BGRA.Num() != NumPixels * 4)
		{
			SetError(OutError, FString::Printf(TEXT("Pixel buffer has %lld bytes, expected %lld."), Image.BGRA.Num(), NumPixels * 4));
			return false;
		}

		const uint8* Src = Image.BGRA.GetData();

		bool bHasTranslucency = false;
		for (int64 Index = 0; Index < NumPixels; ++Index)
		{
			if (Src[Index * 4 + 3] != 255)
			{
				bHasTranslucency = true;
				break;
			}
		}

		// koi expects tightly packed RGB or RGBA.
		const int32 Channels = bHasTranslucency ? 4 : 3;
		TArray64<uint8> Packed;
		Packed.SetNumUninitialized(NumPixels * Channels);
		uint8* Dst = Packed.GetData();
		for (int64 Index = 0; Index < NumPixels; ++Index, Src += 4, Dst += Channels)
		{
			Dst[0] = Src[2];
			Dst[1] = Src[1];
			Dst[2] = Src[0];
			if (Channels == 4)
			{
				Dst[3] = Src[3];
			}
		}

		OutData.Reset();
		OutData.Reserve(HeaderSize + EndMarkerSize + NumPixels);

		// Thread-local setting, so concurrent encodes cannot interfere with each other.
		koi_set_qoi_color_space_on_write_thread(Image.bLinear ? 1 : 0);
		const int Result = koi_write_qoi_to_func(&AppendToArray, &OutData, Image.Width, Image.Height, Channels, Packed.GetData());
		koi_set_qoi_color_space_on_write_thread(0);

		if (Result == 0)
		{
			SetError(OutError, FString::Printf(TEXT("Encoding failed: %s."), *FailureReason(koiw_failure_reason())));
			OutData.Reset();
			return false;
		}

		return true;
	}

	bool FromColors(TConstArrayView<FColor> Pixels, int32 Width, int32 Height, FQOIImage& OutImage, FString* OutError)
	{
		using namespace CodecPrivate;

		if (Width <= 0 || Height <= 0)
		{
			SetError(OutError, FString::Printf(TEXT("Invalid image dimensions %dx%d."), Width, Height));
			return false;
		}

		const int64 NumPixels = static_cast<int64>(Width) * Height;
		if (Pixels.Num() != NumPixels)
		{
			SetError(OutError, FString::Printf(TEXT("Got %lld pixels, expected %lld (%dx%d)."), static_cast<int64>(Pixels.Num()), NumPixels, Width, Height));
			return false;
		}

		OutImage.Width = Width;
		OutImage.Height = Height;
		OutImage.Channels = 4;
		OutImage.bLinear = false;
		OutImage.BGRA.SetNumUninitialized(NumPixels * 4);

		// Copied per channel, so the result does not depend on the platform's FColor memory layout.
		uint8* Dst = OutImage.BGRA.GetData();
		for (const FColor& Color : Pixels)
		{
			Dst[0] = Color.B;
			Dst[1] = Color.G;
			Dst[2] = Color.R;
			Dst[3] = Color.A;
			Dst += 4;
		}
		return true;
	}

	void ToColors(const FQOIImage& Image, TArray<FColor>& OutPixels)
	{
		OutPixels.Reset();
		if (!Image.IsValid() || Image.BGRA.Num() / 4 > MAX_int32)
		{
			return;
		}

		const int64 NumPixels = Image.BGRA.Num() / 4;
		OutPixels.Reserve(static_cast<int32>(NumPixels));
		const uint8* Src = Image.BGRA.GetData();
		for (int64 Index = 0; Index < NumPixels; ++Index, Src += 4)
		{
			OutPixels.Emplace(Src[2], Src[1], Src[0], Src[3]);
		}
	}

	void MakeOpaque(FQOIImage& Image)
	{
		uint8* Data = Image.BGRA.GetData();
		for (int64 Index = 3; Index < Image.BGRA.Num(); Index += 4)
		{
			Data[Index] = 255;
		}
	}
}
