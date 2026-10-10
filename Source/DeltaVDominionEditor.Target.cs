using UnrealBuildTool;

public class DeltaVDominionEditorTarget : TargetRules
{
	public DeltaVDominionEditorTarget(TargetInfo Target) : base(Target)
	{
		Type = TargetType.Editor;
		DefaultBuildSettings = BuildSettingsVersion.Latest;
		IncludeOrderVersion = EngineIncludeOrderVersion.Latest;
		ExtraModuleNames.Add("DeltaVDominion");
	}
}
