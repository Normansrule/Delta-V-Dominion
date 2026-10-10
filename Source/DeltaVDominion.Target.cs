using UnrealBuildTool;

public class DeltaVDominionTarget : TargetRules
{
	public DeltaVDominionTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Game;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DeltaVDominion");
	}
}
