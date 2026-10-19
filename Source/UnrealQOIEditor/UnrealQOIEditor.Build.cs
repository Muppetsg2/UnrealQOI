// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

using UnrealBuildTool;

// Editor module: imports .qoi files as Texture2D assets, reimports them and exports textures to .qoi.
public class UnrealQOIEditor : ModuleRules
{
	public UnrealQOIEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"UnrealEd",
				"UnrealQOI"
			}
		);
	}
}
