![QOI Logo](https://qoiformat.org/qoi-logo.svg)

# UnrealQOI
Import and export [QOI](https://qoiformat.org/) (`.qoi`) images in [Unreal Engine 5](https://www.unrealengine.com/).

Supports **Unreal Engine 5.0 and newer** (the code has no version-specific requirements beyond that).
Tested with **Unreal Engine 5.5.4**.

## Features

- **Import** `.qoi` files as `Texture2D` assets (drag & drop or the Import dialog).
  - The QOI colorspace is respected: images marked as *linear* are imported with **sRGB** disabled.
  - RGB (3-channel) images are imported with **Compression No Alpha** enabled.
  - Importing over an existing texture keeps its settings.
- **Reimport** from the original `.qoi` file (right-click the asset, **Reimport**).
- **Export** any `Texture2D` with source data to `.qoi`.
  - All source formats are supported (8/16-bit, grayscale, HDR*).
  - Linear textures (sRGB off, e.g. masks or normal maps) are written unchanged and marked as linear in the file.
  - The alpha channel is written only when the texture actually uses it.

\* QOI stores 8 bits per channel, so HDR data is clamped to [0, 1] and stored as sRGB. On Unreal Engine 5.0–5.1, 32-bit float and single-channel float sources cannot be exported.

## Description
QOI Format Import/Export plugin for Unreal Engine 5

## Usage

### Import
Drag & drop a `.qoi` file into the Content Browser, or open the Import dialog and select **Quite OK Image Format (*.qoi)**.

### Export
Right-click a `Texture2D` asset, choose **Asset Actions → Export...**, select **Quite OK Image Format (*.qoi)** as the file type and save.

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

UnrealQOI is an **editor-only** plugin: it is not compiled into packaged games and adds nothing to them.

## Building

The plugin has a single editor module and bundles the [koi](https://github.com/Muppetsg2/koi) QOI library (header-only C, in `Source/ThirdParty/koi`).
No additional setup is needed: the module is compiled together with your project.

To build a redistributable package:
```sh
RunUAT BuildPlugin -Plugin="{Path}/UnrealQOI/UnrealQOI.uplugin" -Package="{OutputPath}" -Rocket
```

If you experience build issues after switching engine versions, delete the plugin's `Binaries` and `Intermediate` folders and rebuild. The [UnrealEnginePluginMigrationTool](https://github.com/mickexd/UnrealEnginePluginMigrationTool) can also help to rebuild the plugin and show detailed error messages.

## License

[MIT License](https://github.com/Muppetsg2/UnrealQOI/blob/main/LICENSE).
The bundled koi library is available under the MIT License or as public domain (see `Source/ThirdParty/koi/LICENSE`).
