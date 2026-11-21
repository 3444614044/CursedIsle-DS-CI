// CursedIsleLocalServerEditor.Target.cs
// 仅用于 GenerateProjectFiles / 生成 IDE 工程；真正出包用 Server target。
using UnrealBuildTool;
using System.Collections.Generic;

public class CursedIsleLocalServerEditorTarget : TargetRules
{
    public CursedIsleLocalServerEditorTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Editor;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        ExtraModuleNames.Add("CursedIsleLocalServer");
    }
}
