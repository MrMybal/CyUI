using UnrealBuildTool;
using System.IO;

public class CyUI : ModuleRules
{
    public CyUI(ReadOnlyTargetRules Target) : base(Target)
    {
        if (!Target.bBuildEditor)
            throw new BuildException("CyUI is an optional editor-only module.");
        string Library = Path.Combine(ModuleDirectory, "Library");
        if (!Directory.Exists(Library))
            Library = Path.GetFullPath(Path.Combine(ModuleDirectory, "../../ThirdParty/CyUI"));
        if (!File.Exists(Path.Combine(Library, "include/cyui/Core.h")))
            throw new BuildException("Prepare this module with tools/package_unreal.py before building it.");
        PCHUsage = PCHUsageMode.NoPCHs;
        bUseUnity = false;
        bEnableExceptions = true;
        CppStandard = CppStandardVersion.Cpp20;
        PublicDependencyModuleNames.AddRange(new[] { "Core", "CoreUObject", "Engine", "Slate", "SlateCore", "InputCore" });
        PrivateDependencyModuleNames.AddRange(new[] { "RenderCore", "RHI", "ApplicationCore" });
        PublicIncludePaths.Add(Path.Combine(Library, "include"));
        PublicSystemIncludePaths.Add(Path.Combine(Library, "third_party/imgui"));
        if (Directory.Exists(Path.Combine(Library, "third_party/texteditor")))
            PublicSystemIncludePaths.Add(Path.Combine(Library, "third_party/texteditor"));
        // Name-independent, so a renamed copy (package_unreal.py --module-name) only renames
        // this class/file: UBT defines <NAME>_API for the module, CyUI code uses CYUI_API.
        PublicDefinitions.Add("CYUI_UNREAL_MODULE=" + Name);
        if (Name != "CyUI")
            PublicDefinitions.Add("CYUI_API=" + Name.ToUpperInvariant() + "_API");
        PublicDefinitions.Add("IMGUI_API=CYUI_API");
        foreach (string Feature in new string[] { /*CYUI_MODULES*/ })
            PublicDefinitions.Add("CYUI_HAS_" + Feature + "=1");
    }
}
