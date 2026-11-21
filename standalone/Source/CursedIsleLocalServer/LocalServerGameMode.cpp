// LocalServerGameMode.cpp
#include "LocalServerGameMode.h"
#include "LocalServerGameSession.h"
#include "GameFramework/GameSession.h"
#include "GameFramework/PlayerController.h"
#include "Engine/NetConnection.h"
#include "Net/UnrealNetwork.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogLocalDS, Log, All);

ALocalServerGameMode::ALocalServerGameMode()
{
    GameSessionClass = ALocalServerGameSession::StaticClass();
    bUseSeamlessTravel = false;
    bStartPlayersAsSpectators = false;
    UE_LOG(LogLocalDS, Log, TEXT("======================================================"));
    UE_LOG(LogLocalDS, Log, TEXT("[LocalDS] GameMode 已创建（握手验证服，无游戏逻辑）"));
    UE_LOG(LogLocalDS, Log, TEXT("======================================================"));
}

void ALocalServerGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    UE_LOG(LogLocalDS, Warning, TEXT("[LocalDS] InitGame  Map=%s  Options=%s  Error=%s"),
           *MapName, *Options, *ErrorMessage);
}

void ALocalServerGameMode::PreLogin(const FString& Options, const FString& Address,
                                   const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    // ★ 走到这里说明：无连接握手 + NMT_Hello 版本校验 + NMT_Challenge + NMT_Login 全通了
    UE_LOG(LogLocalDS, Warning, TEXT("======================================================"));
    UE_LOG(LogLocalDS, Warning, TEXT("[LocalDS] >>> PreLogin 成功，客户端已通过 UE4 握手 <<<"));
    UE_LOG(LogLocalDS, Warning, TEXT("[LocalDS]    Address = %s"), *Address);
    UE_LOG(LogLocalDS, Warning, TEXT("[LocalDS]    Options = %s"), *Options);
    UE_LOG(LogLocalDS, Warning, TEXT("[LocalDS]    UniqueId= %s"), *UniqueId.ToString());
    UE_LOG(LogLocalDS, Warning, TEXT("======================================================"));

    // 本地单机：不拒绝任何登录。返回空 ErrorMessage 即放行。
    ErrorMessage.Empty();
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
}

void ALocalServerGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);
    UE_LOG(LogLocalDS, Warning, TEXT("[LocalDS] >>> PostLogin：Login/SpawnPlayActor 完成 <<<"));
    DumpConnectionInfo(NewPlayer, TEXT("PostLogin"));
}

void ALocalServerGameMode::Logout(AController* Exiting)
{
    UE_LOG(LogLocalDS, Warning, TEXT("[LocalDS] Logout 玩家离开（若很快发生，多半是 NMT_Welcome 后 LoadMap 失败）"));
    Super::Logout(Exiting);
}

void ALocalServerGameMode::DumpConnectionInfo(APlayerController* PC, const TCHAR* Stage)
{
    if (!PC)
    {
        UE_LOG(LogLocalDS, Warning, TEXT("[LocalDS] %s: PlayerController=null"), Stage);
        return;
    }
    UNetConnection* Conn = PC->GetNetConnection();
    UE_LOG(LogLocalDS, Log, TEXT("[LocalDS] %s: PC=%s  Conn=%s"),
           Stage, *PC->GetName(), Conn ? *Conn->GetName() : TEXT("null"));
}
