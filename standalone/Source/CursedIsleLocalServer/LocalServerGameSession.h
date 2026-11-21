// LocalServerGameSession.h
// 放行所有登录，避免 Null OnlineSubsystem 下 ApproveLogin 把连接拒掉。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameSession.h"
#include "LocalServerGameSession.generated.h"

UCLASS()
class CURSEDISLELOCALSERVER_API ALocalServerGameSession : public AGameSession
{
    GENERATED_BODY()

public:
    // 4.27: virtual FString ApproveLogin(const FString& Options);
    // 返回空串 = 允许登录。若签名与你的引擎修订版不同，以 GameSession.h 为准。
    virtual FString ApproveLogin(const FString& Options) override;
};
