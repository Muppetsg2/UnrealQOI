// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

using UnrealBuildTool;

// Runtime module: QOI encoding/decoding for C++ and Blueprints, in the editor and in packaged games.
public class UnrealQOI : ModuleRules
{
	public UnrealQOI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Types used in the public headers.
		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine"
			}
		);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"ImageCore",
				"RenderCore",
				"RHI",
				"koi"
			}
		);
	}
}
