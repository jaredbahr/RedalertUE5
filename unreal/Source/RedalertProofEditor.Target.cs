using UnrealBuildTool;
using System.Collections.Generic;

public class RedalertProofEditorTarget : TargetRules
{
    public RedalertProofEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V6;
        IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;
        ExtraModuleNames.Add("RedalertProof");
    }
}
