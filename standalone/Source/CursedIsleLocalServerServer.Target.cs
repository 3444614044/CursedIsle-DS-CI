// CursedIsleLocalServerServer.Target.cs
// UE4.27 Dedicated Server target —— 独立握手验证服
// 规范命名：<Project>Server.Target.cs，UBT target 名 = CursedIsleLocalServerServer
// 只依赖引擎自带模块，不需要游戏 C++ 模块 / 原游戏地图，装了 4.27 即可编译。
using UnrealBuildTool;
using System.Collections.Generic;

public class CursedIsleLocalServerServerTarget : TargetRules
{
    public CursedIsleLocalServerServerTarget(TargetInfo Target) : base(Target)
    {
        Type = TargetType.Server;
        DefaultBuildSettings = BuildSettingsVersion.V2;
        bUsesSteam = false;
        bUseLoggingInShipping = true;   // Shipping 也保留日志，便于看握手

        ExtraModuleNames.Add("CursedIsleLocalServer");
    }
}
