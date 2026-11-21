// CursedIsleServer.Target.cs
// Unreal Engine 4.27 — Dedicated Server target
// 放到 <ProjectRoot>/Source/ 下，与 CursedIsle.Target.cs / CursedIsleEditor.Target.cs 同级
using UnrealBuildTool;
using System.Collections.Generic;

public class CursedIsleServerTarget : TargetRules
{
    public CursedIsleServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        bUsesSteam = false;
        bUseLoggingInShipping = true;   // 便于观察客户端握手

        // TODO: 换成游戏工程实际的主模块名（蓝图转 C++ 后通常就是项目名）
        ExtraModuleNames.Add("CursedIsle");
        // 本地服务端附加模块（LocalGameMode 等）
        ExtraModuleNames.Add("CursedIsleServer");
    }
}
