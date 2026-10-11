using UnrealBuildTool;

public class DeltaVDominion : ModuleRules
{
	public DeltaVDominion(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		// Lets code include headers as "Core/...", "Combat/..." etc.
		PublicIncludePaths.Add(ModuleDirectory);

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"NavigationSystem",
			"NetCore",
			"UMG",
			"Niagara",
			"ChaosVehicles",		// drivable LTV and tank (Chaos wheeled vehicles)
			"PhysicsCore"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"StateTreeModule",
			"GameplayStateTreeModule",
			"OnlineSubsystem",
			"OnlineSubsystemUtils"
		});
	}
}
