// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "UnrealQOI.h"
#include <Modules/ModuleManager.h>
#include "QOIFactory.h"
#include "QOIExporter.h"

#define LOCTEXT_NAMESPACE "FUnrealQOIModule"

void FUnrealQOIModule::StartupModule() 
{
	// This code will execute after your module is loaded into memory; the exact timing is specified in the .uplugin file per-module
}

void FUnrealQOIModule::ShutdownModule()
{
	// This function may be called during shutdown to clean up your module. For modules that support dynamic reloading,
	// we call this function before unloading the module.
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FUnrealQOIModule, UnrealQOI)