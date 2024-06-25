// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.Collections.Generic;
using System;

[SupportedPlatforms(UnrealPlatformClass.Server)]
public class MousenServerTarget : TargetRules
{
    public MousenServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.Latest;
        ExtraModuleNames.Add("Mousen");
    }
}
