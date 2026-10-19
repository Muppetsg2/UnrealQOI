// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "Modules/ModuleManager.h"
#include "UnrealQOILog.h"

DEFINE_LOG_CATEGORY(LogUnrealQOI);

// The factory and exporter are discovered automatically through reflection,
// so the module itself needs no startup or shutdown logic.
IMPLEMENT_MODULE(FDefaultModuleImpl, UnrealQOI)
