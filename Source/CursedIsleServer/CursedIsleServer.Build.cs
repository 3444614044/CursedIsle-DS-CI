// CursedIsleServer.Build.cs
using UnrealBuildTool;

public class CursedIsleServer : ModuleRules
{
    public CursedIsleServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "OnlineSubsystem",
            "OnlineSubsystemUtils",
            "Sockets",
            // 方案 A（默认）：标准 IpNetDriver，抓包端口 7777–7863 与之一致。
            // 若 ws_probe 确认客户端走 WebSocket 封装，再取消下一行并切方案 B（见 Config/DefaultEngine.ini）：
            // "WebSocketNetworking",
            "Json",
            "JsonUtilities",
            "HTTP"
        });

        PrivateDependencyModuleNames.AddRange(new string[] {
            "GameplayTasks",
            "AIModule",
            "NavigationSystem"
        });
    }
}
