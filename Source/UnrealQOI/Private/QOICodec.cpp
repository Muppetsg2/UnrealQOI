// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "QOICodec.h"

#include "HAL/UnrealMemory.h"
#include "Misc/ScopeExit.h"

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
	namespace Private
	{
		FString FailureReason(const char* Reason)
		{
			return Reason ? FString(ANSI_TO_TCHAR(Reason)) : FString(TEXT("unknown error"));
		}

		void AppendToArray(void* Context, void* Data, int Size)
		{
			static_cast<TArray64<uint8>*>(Context)->Append(static_cast<const uint8*>(Data), Size);
		}
	}

	bool HasQOIMagic(const uint8* Data, int64 Size)
	{
		return Data != nullptr
			&& Size >= HeaderSize + EndMarkerSize
			&& Data[0] == 'q' && Data[1] == 'o' && Data[2] == 'i' && Data[3] == 'f';
	}

	bool Decode(const uint8* Data, int64 Size, FQOIImage& OutImage, FString& OutError)
	{
		if (!HasQOIMagic(Data, Size))
		{
			OutError = TEXT("Not a QOI file (missing 'qoif' header).");
			return false;
		}

		if (Size > MAX_int32)
		{
			OutError = FString::Printf(TEXT("File is too large (%lld bytes)."), Size);
			return false;
		}

		int Width = 0;
		int Height = 0;
		int ChannelsInFile = 0;
		uint8* Pixels = koi_load_from_memory(Data, static_cast<int>(Size), &Width, &Height, &ChannelsInFile, KOI_rgb_alpha);
		if (Pixels == nullptr)
		{
			OutError = FString::Printf(TEXT("Decoding failed: %s."), *Private::FailureReason(koi_failure_reason()));
			return false;
		}
		ON_SCOPE_EXIT
		{
			koi_image_free(Pixels);
		};

		if (Width <= 0 || Height <= 0)
		{
			OutError = FString::Printf(TEXT("Invalid image dimensions %dx%d."), Width, Height);
			return false;
		}

		const int64 NumPixels = static_cast<int64>(Width) * Height;

		OutImage.Width = Width;
		OutImage.Height = Height;
		OutImage.Channels = ChannelsInFile;
		OutImage.bLinear = Data[13] == 1; // Byte 13 of the header: 0 = sRGB with linear alpha, 1 = all channels linear.
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

	bool Encode(const FQOIImage& Image, TArray64<uint8>& OutData, FString& OutError)
	{
		if (Image.Width <= 0 || Image.Height <= 0)
		{
			OutError = FString::Printf(TEXT("Invalid image dimensions %dx%d."), Image.Width, Image.Height);
			return false;
		}

		const int64 NumPixels = static_cast<int64>(Image.Width) * Image.Height;
		if (NumPixels > static_cast<int64>(MAX_uint32))
		{
			OutError = FString::Printf(TEXT("Image is too large for QOI (%dx%d)."), Image.Width, Image.Height);
			return false;
		}

		if (Image.BGRA.Num() != NumPixels * 4)
		{
			OutError = FString::Printf(TEXT("Pixel buffer has %lld bytes, expected %lld."), Image.BGRA.Num(), NumPixels * 4);
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

		// Thread-local setting, so concurrent exports cannot interfere with each other.
		koi_set_qoi_color_space_on_write_thread(Image.bLinear ? 1 : 0);
		const int Result = koi_write_qoi_to_func(&Private::AppendToArray, &OutData, Image.Width, Image.Height, Channels, Packed.GetData());
		koi_set_qoi_color_space_on_write_thread(0);

		if (Result == 0)
		{
			OutError = FString::Printf(TEXT("Encoding failed: %s."), *Private::FailureReason(koiw_failure_reason()));
			OutData.Reset();
			return false;
		}

		return true;
	}
}
