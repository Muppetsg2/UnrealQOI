// Copyright (c) 2025 Muppetsg2
// Licensed under the MIT License.

#pragma once

#include <CoreMinimal.h>
#include <Modules/ModuleManager.h>

class FUnrealQOIModule : public IModuleInterface
{
public:
	/* IModuleInterface implementation */
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;
};