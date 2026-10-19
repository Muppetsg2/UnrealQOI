// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

using UnrealBuildTool;

public class UnrealQOI : ModuleRules
{
	public UnrealQOI(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Everything is private: no other module needs to include the plugin's headers.
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
				"CoreUObject",
				"Engine",
				"ImageCore",
				"UnrealEd",
				"koi"
			}
		);
	}
}
