![QOI Logo](https://qoiformat.org/qoi-logo.svg)

# UnrealQOI
Import, export, load and save [QOI](https://qoiformat.org/) (`.qoi`) images in [Unreal Engine 5](https://www.unrealengine.com/), in the editor and at runtime, from C++ and Blueprints.

Supports **Unreal Engine 5.0 and newer**.
Tested with **Unreal Engine 5.5.4**.

## Modules

| Module | Type | Purpose |
| --- | --- | --- |
| `UnrealQOI` | Runtime | QOI encoding/decoding API for C++ and Blueprints. Works in packaged games. |
| `UnrealQOIEditor` | Editor | Import, reimport and export of `.qoi` files in the Content Browser. |

Both use the bundled [koi](https://github.com/Muppetsg2/koi) library (header-only C, `Source/ThirdParty/koi`).

## Editor

### Import
Drag & drop a `.qoi` file into the Content Browser, or open the Import dialog and select **Quite OK Image Format (*.qoi)**.

- Images marked as *linear* in the QOI header are imported with **sRGB** disabled.
- RGB (3-channel) images are imported with **Compression No Alpha** enabled.
- Importing over an existing texture keeps its settings.
- **Reimport** (right-click the asset → **Reimport**) reloads the pixels from the original file.

### Export
Right-click a `Texture2D` asset, choose **Asset Actions → Export...**, select **Quite OK Image Format (*.qoi)** as the file type and save.

- All source formats are supported (8/16-bit, grayscale, HDR*).
- Linear textures (sRGB off, e.g. masks or normal maps) are written unchanged and marked as linear in the file.
- The alpha channel is written only when the texture actually uses it.

\* QOI stores 8 bits per channel, so HDR data is clamped to [0, 1] and stored as sRGB. On Unreal Engine 5.0–5.1, 32-bit float and single-channel float sources cannot be exported.

## Runtime (Blueprints)

All nodes are in the **QOI** category. Relative paths are resolved against the project's `Saved` directory
(`Screenshots/Shot.qoi` → `<Project>/Saved/Screenshots/Shot.qoi`). Every node returns an error message, which is also written to the `LogUnrealQOI` log.

| Node | Description |
| --- | --- |
| **Get QOI File Info** | Width, height, channels and colorspace of a file, without decoding it. |
| **Load Texture From QOI File** / **Bytes** | Creates a transient, uncompressed `Texture2D`. Store it in a variable to keep it alive. |
| **Load Pixels From QOI File** | Pixels as an array of `Color`. |
| **Save Texture To QOI File** | Every texture in the editor. In packaged games only uncompressed 8-bit textures whose pixels are in memory (e.g. textures loaded with this plugin); draw other textures into a render target. |
| **Save Render Target To QOI File** | Contents of a render target (e.g. a Scene Capture). Float targets are converted to sRGB. |
| **Save Pixels To QOI File** / **Encode Pixels To QOI Bytes** | An array of `Color` (Width × Height, row by row, top-left first). |

## Runtime (C++)

Add the module to your `.Build.cs`:
```csharp
PrivateDependencyModuleNames.Add("UnrealQOI");
```

Include `UnrealQOI.h`. All functions return `false`/`nullptr` on failure and can fill an optional `FString* OutError`.

```cpp
#include "UnrealQOI.h"

// Load a file and create a texture (game thread).
FQOIImage Image;
FString Error;
if (UnrealQOI::LoadFromFile(TEXT("Textures/Icon.qoi"), Image, &Error))
{
	UTexture2D* Texture = UnrealQOI::CreateTexture(Image); // keep it in a UPROPERTY
}

// Save a render target.
FQOIImage Capture;
if (UnrealQOI::FromRenderTarget(RenderTarget, Capture, &Error))
{
	UnrealQOI::MakeOpaque(Capture);
	UnrealQOI::SaveToFile(TEXT("Captures/Capture.qoi"), Capture, &Error);
}
```

| Group | Functions | Thread |
| --- | --- | --- |
| Memory | `IsQOIData`, `ReadInfo`, `Decode`, `Encode` | any |
| Files | `ResolvePath`, `ReadFileInfo`, `LoadFromFile`, `SaveToFile` | any |
| Pixels | `FromColors`, `ToColors`, `FromImage`, `ToImage`, `MakeOpaque` | any |
| Engine objects | `CreateTexture`, `FromTexture`, `FromRenderTarget` | game thread |

`FromImage`/`ToImage` work with `FImage`; using them requires the `ImageCore` module in your `.Build.cs`.

### Example: saving screenshots as QOI

The engine hands the pixels of every screenshot (e.g. the `Shot` console command) to `UGameViewportClient::OnScreenshotCaptured`.
Encoding and writing happen on a worker thread, so the game does not hitch:

```cpp
#include "Async/Async.h"
#include "Engine/GameViewportClient.h"
#include "Misc/DateTime.h"
#include "UnrealQOI.h"

void UMyScreenshotSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	ScreenshotHandle = UGameViewportClient::OnScreenshotCaptured().AddUObject(this, &UMyScreenshotSubsystem::OnScreenshot);
}

void UMyScreenshotSubsystem::Deinitialize()
{
	UGameViewportClient::OnScreenshotCaptured().Remove(ScreenshotHandle);
	Super::Deinitialize();
}

void UMyScreenshotSubsystem::OnScreenshot(int32 Width, int32 Height, const TArray<FColor>& Pixels)
{
	FQOIImage Image;
	if (!UnrealQOI::FromColors(Pixels, Width, Height, Image))
	{
		return;
	}
	UnrealQOI::MakeOpaque(Image);

	const FString FilePath = FString::Printf(TEXT("Screenshots/%s.qoi"), *FDateTime::Now().ToString());
	Async(EAsyncExecution::ThreadPool, [Image = MoveTemp(Image), FilePath]()
	{
		FString Error;
		if (!UnrealQOI::SaveToFile(FilePath, Image, &Error))
		{
			UE_LOG(LogUnrealQOI, Warning, TEXT("Screenshot not saved: %s"), *Error);
		}
	});
}
```

## Installation
Clone the plugin into your Unreal project:
```sh
cd {YourUnrealProject}
git clone https://github.com/Muppetsg2/UnrealQOI.git ./Plugins/UnrealQOI
```

Or add it as a submodule:

```sh
cd {YourUnrealProject}
git submodule add https://github.com/Muppetsg2/UnrealQOI.git ./Plugins/UnrealQOI
```
Alternatively, download the precompiled binaries from the [release page](https://github.com/Muppetsg2/UnrealQOI/releases/latest) and copy them into the same `Plugins/UnrealQOI` folder.

To make the plugin available to every project of an engine installation, copy a packaged build of the plugin to `Engine/Plugins/Marketplace/UnrealQOI`.

## Building

No additional setup is needed: the modules are compiled together with your project.

To build a redistributable package:
```sh
RunUAT BuildPlugin -Plugin="{Path}/UnrealQOI/UnrealQOI.uplugin" -Package="{OutputPath}" -Rocket
```

If you experience build issues after switching engine versions, delete the plugin's `Binaries` and `Intermediate` folders and rebuild. The [UnrealEnginePluginMigrationTool](https://github.com/mickexd/UnrealEnginePluginMigrationTool) can also help to rebuild the plugin and show detailed error messages.

## License

[MIT License](https://github.com/Muppetsg2/UnrealQOI/blob/main/LICENSE).
The bundled koi library is available under the MIT License or as public domain (see `Source/ThirdParty/koi/LICENSE`).
