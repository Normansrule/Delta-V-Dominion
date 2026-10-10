using UnrealBuildTool;

// Dedicated server target. Building it requires a source build of the engine.
public class DeltaVDominionServerTarget : TargetRules
{
	public DeltaVDominionServerTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Server;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DeltaVDominion");
	}
}
