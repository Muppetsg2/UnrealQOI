// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

using System.IO;
using UnrealBuildTool;

// Header-only third-party library (https://github.com/Muppetsg2/koi).
// The implementation is compiled once, in UnrealQOI/Private/QOICodec.cpp.
public class koi : ModuleRules
{
	public koi(ReadOnlyTargetRules Target) : base(Target)
	{
		Type = ModuleType.External;

		PublicSystemIncludePaths.Add(Path.Combine(ModuleDirectory, "include"));
	}
}
