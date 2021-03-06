// Copyright (c) Improbable Worlds Ltd, All Rights Reserved

#include "SpatialGDKModule.h"

#include "ModuleManager.h"
#include "Paths.h"
#include "PlatformProcess.h"
#include "UObjectBase.h"

#define LOCTEXT_NAMESPACE "FSpatialGDKModule"

DEFINE_LOG_CATEGORY(LogSpatialGDKModule);

IMPLEMENT_MODULE(FSpatialGDKModule, SpatialGDK)

void FSpatialGDKModule::StartupModule()
{
}

void FSpatialGDKModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE
