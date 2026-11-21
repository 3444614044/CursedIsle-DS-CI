// LocalServerGameMode.h
// 握手验证服 GameMode：不实现任何游戏玩法，只在登录链路各节点打日志。
// 目的：确认客户端确实走到了 NMT_Login → PreLogin → PostLogin。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LocalServerGameMode.generated.h"

UCLASS()
class CURSEDISLELOCALSERVER_API ALocalServerGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALocalServerGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;

    // 4.27 签名：PreLogin(const FString& Options, const FString& Address,
    //                     const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
    // 若你的 4.27 修订版签名不同，以 Engine/Source/Runtime/Engine/Classes/GameFramework/GameModeBase.h 为准。
    virtual void PreLogin(const FString& Options, const FString& Address,
                          const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;

    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

private:
    void DumpConnectionInfo(APlayerController* PC, const TCHAR* Stage);
};
