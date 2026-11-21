// LocalGameMode.h — 最小 Dedicated Server GameMode 占位
// 真实游戏逻辑（恐龙成长 / 饥饿 / 孵化 / 战斗 / 昼夜）需自行实现或复用游戏模块
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "LocalGameMode.generated.h"

UCLASS()
class CURSEDISLESERVER_API ALocalGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ALocalGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;

protected:
    // 单机：允许无账号直连，便于本地验证握手是否打通
    UPROPERTY(EditDefaultsOnly, Category="Local")
    bool bAllowAnonymousLocalLogin = true;
};
