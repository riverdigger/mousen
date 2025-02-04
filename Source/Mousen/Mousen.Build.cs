// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class Mousen : ModuleRules
{
	public Mousen(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "GameplayTasks", "GameplayAbilities", "HeadMountedDisplay", "PlayFabGSDK" });
        PrivateDependencyModuleNames.AddRange(new string[] { "CoreUObject", "Engine", "NetCore" });
    }
}
