using UnrealBuildTool;

public class RedalertProof : ModuleRules
{
    public RedalertProof(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
        PublicIncludePaths.Add(ModuleDirectory);
        PrivateDependencyModuleNames.AddRange(new[] { "NetCore", "ApplicationCore" });
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "InputCore" });
    }
}
