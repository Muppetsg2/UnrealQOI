![QOI Logo](https://qoiformat.org/qoi-logo.svg)

# UnrealQOI
Import and Export [QOI](https://qoiformat.org/) `.qoi` format file in [Unreal Engine 5](https://www.unrealengine.com/).

Tested with **Unreal Engine 5.5.4**

## Description
QOI Format Import/Export plugin for Unreal Engine 5

## Usage

### Import
Drag & Drop `.qoi` file to content panel or open import dialog and select **Quite OK Image Format(*.qoi)** files.

### Export
Right-click a `Texture2D` asset, choose **Asset Actions->Export...**, select **Quite OK Image Format(*.qoi)** as the export format, and save your image.

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
Alternatively, you can download the precompiled binary from the plugin [release page](https://github.com/Muppetsg2/UnrealQOI/releases/latest) and copy it directly into the same `Plugins/UnrealQOI` folder.

If you are creating a package without C++ access, copy the plugin to the `Engine/Plugins/Runtime` directory.

## Debugging

If you experience build issues with the plugin, use the [UnrealEnginePluginMigrationTool](https://github.com/mickexd/UnrealEnginePluginMigrationTool) to rebuild it and review the console output for detailed error messages.

## License

[MIT License](https://github.com/Muppetsg2/UnrealQOI/blob/master/LICENSE)
