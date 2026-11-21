// CursedIsleWorld.Build.cs
// ★ 这是「插进真实游戏工程」的 drop-in 模块，不是独立工程。
//   把它连同 .h/.cpp 一起拷到 <GameProject>/Source/CursedIsleWorld/，
//   并在 <GameProject>.uproject 的 Modules 里加一项（见 world/README.md）。
//
// 只依赖引擎模块：不链接任何游戏私有模块，方便单独验证编译。
using UnrealBuildTool;

public class CursedIsleWorld : ModuleRules
{
    public CursedIsleWorld(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(new string[] {
            "Core",
            "CoreUObject",
            "Engine",
            "NetCore",
            "Networking",
            "Sockets",
            "Json",
            "JsonUtilities"
        });

        // 不启用 OnlineSubsystem：本地/局域网服不需要 Steam/EOS 鉴权，
        // 身份由 REST 后端在 PreLogin 的 Options 里传进来。
    }
}
