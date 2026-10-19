// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#include "Modules/ModuleManager.h"
#include "UnrealQOI.h"

DEFINE_LOG_CATEGORY(LogUnrealQOI);

// The runtime module only provides functions; it needs no startup or shutdown logic.
IMPLEMENT_MODULE(FDefaultModuleImpl, UnrealQOI)
