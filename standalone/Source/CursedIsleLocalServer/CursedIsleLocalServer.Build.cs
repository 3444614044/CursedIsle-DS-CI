// CursedIsleLocalServer.Build.cs — 纯引擎模块，零游戏依赖
using UnrealBuildTool;

public class CursedIsleLocalServer : ModuleRules
{
    public CursedIsleLocalServer(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "Networking",
            "Sockets",
            "OnlineSubsystem",
            "OnlineSubsystemUtils"
        });

        // 不链接 Steam / WebSocketNetworking / 任何游戏模块。
        // 若 udp_probe 抓包确认客户端其实走 TCP WebSocket 封装，再在
        // Config/DefaultEngine.ini 切方案 B 并取消下一行注释：
        // PrivateDependencyModuleNames.Add("WebSocketNetworking");
    }
}
