// CIWorldActors.h — 世界层 Actor 层级（一次声明，便于 UHT 统一处理）
//
// ★ 重要：这些是 C++ 类。要让**真实客户端**进图，必须把它们和客户端 pak 里
//   的蓝图类对齐（见 world/README.md 的「类映射表」）。独立 C++ 工程只能
//   验证协议，无法让蓝图客户端 LoadMap —— 这是硬门槛，不是代码量问题。
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerState.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "CIWorldTypes.h"
#include "CIWorldRules.h"
#include "CIWorldActors.generated.h"

class FCIWorldSaveBridge;

// ============================================================================
// GameState：复制「世界本身」——时间、昼夜、天气、在线玩家摘要
// ============================================================================
UCLASS()
class CURSEDISLEWORLD_API ACIWorldGameState : public AGameStateBase
{
    GENERATED_BODY()
public:
    ACIWorldGameState();

    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    UPROPERTY(ReplicatedUsing=OnRep_WorldTime, BlueprintReadOnly, Category="World")
    float ServerTime = 0.f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="World")
    float DayProgress = 0.f;

    UPROPERTY(ReplicatedUsing=OnRep_Phase, BlueprintReadOnly, Category="World")
    ECIWorldPhase Phase = ECIWorldPhase::Day;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="World")
    float WeatherIntensity = 0.f;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="World")
    int32 OnlineCount = 0;

    UFUNCTION() void OnRep_WorldTime();
    UFUNCTION() void OnRep_Phase();

    FCIWorldRules& Rules() { return WorldRules; }

private:
    FCIWorldRules WorldRules;
    UPROPERTY() float AccumulatedForLog = 0.f;
};

// ============================================================================
// PlayerState：复制「账号与生存状态」——由 REST 存档恢复
// ============================================================================
UCLASS()
class CURSEDISLEWORLD_API ACIWorldPlayerState : public APlayerState
{
    GENERATED_BODY()
public:
    ACIWorldPlayerState();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Identity") FString Uuid;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Identity") FString Login;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Loadout")  FString DinoId;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Loadout")  FString AvatarId;

    UPROPERTY(ReplicatedUsing=OnRep_Vitals, BlueprintReadOnly, Category="Vitals") int32 Level = 1;
    UPROPERTY(ReplicatedUsing=OnRep_Vitals, BlueprintReadOnly, Category="Vitals") float Health = 100.f;
    UPROPERTY(ReplicatedUsing=OnRep_Vitals, BlueprintReadOnly, Category="Vitals") float Hunger = 100.f;
    UPROPERTY(ReplicatedUsing=OnRep_Vitals, BlueprintReadOnly, Category="Vitals") float Stamina = 100.f;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Vitals") bool bAlive = true;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Vitals") float Experience = 0.f;

    UFUNCTION() void OnRep_Vitals();

    void ApplyFromSnapshot(const FCIWorldPlayerSnapshot& S);
    FCIWorldPlayerSnapshot ToSnapshot() const;
};

// ============================================================================
// Pawn：复制「位置/朝向/动画」，服务端权威移动
// ============================================================================
UCLASS()
class CURSEDISLEWORLD_API ACIWorldPawn : public APawn
{
    GENERATED_BODY()
public:
    ACIWorldPawn();
    virtual void Tick(float DeltaSeconds) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const override;

    UPROPERTY(Replicated, BlueprintReadOnly, Category="Move") FVector_NetQuantize10 ReplicatedPosition;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Move") FRotator ReplicatedRotation;
    UPROPERTY(Replicated, BlueprintReadOnly, Category="Move") ECIWorldAnimState AnimState = ECIWorldAnimState::Idle;

    /** 客户端只发「期望速度」，位置由服务端算完再复制回来。 */
    UFUNCTION(Server, Reliable, WithValidation, Category="Move")
    void ServerMove(FVector RequestedVelocity, float ClientDelta);
    bool ServerMove_Validate(FVector RequestedVelocity, float ClientDelta);
    void ServerMove_Implementation(FVector RequestedVelocity, float ClientDelta);

    UFUNCTION(Server, Reliable, WithValidation, Category="Action")
    void ServerSetAnim(ECIWorldAnimState NewAnim);
    bool ServerSetAnim_Validate(ECIWorldAnimState NewAnim);
    void ServerSetAnim_Implementation(ECIWorldAnimState NewAnim);

    void SetOwnerSnapshot(FCIWorldPlayerSnapshot* InSnapshot) { OwnerSnapshot = InSnapshot; }

private:
    FCIWorldPlayerSnapshot* OwnerSnapshot = nullptr;
};

// ============================================================================
// PlayerController：RPC 入口（选龙、聊天、表情）+ 客户端通知
// ============================================================================
UCLASS()
class CURSEDISLEWORLD_API ACIWorldPlayerController : public APlayerController
{
    GENERATED_BODY()
public:
    ACIWorldPlayerController();

    UFUNCTION(Server, Reliable, WithValidation) void ServerSelectDino(const FString& DinoId, const FString& AvatarId);
    bool ServerSelectDino_Validate(const FString& DinoId, const FString& AvatarId);
    void ServerSelectDino_Implementation(const FString& DinoId, const FString& AvatarId);

    UFUNCTION(Server, Reliable, WithValidation) void ServerChat(const FString& Message);
    bool ServerChat_Validate(const FString& Message);
    void ServerChat_Implementation(const FString& Message);

    UFUNCTION(Client, Reliable) void ClientWorldNotice(const FString& Text);
    void ClientWorldNotice_Implementation(const FString& Text);

    /** 多播：某人说话/升级/死亡，所有客户端都能看到 */
    UFUNCTION(NetMulticast, Unreliable) void MulticastAnnounce(const FString& Text);
    void MulticastAnnounce_Implementation(const FString& Text);
};

// ============================================================================
// GameMode：登录 → 恢复存档 → Spawn → 世界 tick → 退出写回
// ============================================================================
UCLASS()
class CURSEDISLEWORLD_API ACIWorldGameMode : public AGameModeBase
{
    GENERATED_BODY()
public:
    ACIWorldGameMode();

    virtual void InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage) override;
    virtual void PreLogin(const FString& Options, const FString& Address,
                          const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual void PostLogin(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void Tick(float DeltaSeconds) override;

    /** ★ RPC 必须改权威快照，不能只改 PlayerState —— 否则下一帧 Tick 会把它覆盖回去。 */
    void SetPlayerLoadout(const FString& Uuid, const FString& DinoId, const FString& AvatarId);

private:
    /** 每个连接一份权威快照；GameState 的 tick 直接推进这些快照。 */
    TMap<FString, FCIWorldPlayerSnapshot> AuthoritativePlayers;
    TMap<FString, FString> ConnectionUuid;   // ControllerName -> Uuid
    TMap<FString, float>   RespawnTimers;
    FCIWorldSaveBridge* SaveBridge = nullptr;
    FCIWorldRules Rules;
    float SaveFlushAccum = 0.f;

    void RestorePlayer(APlayerController* PC, const FString& Uuid, const FString& Login,
                       const FString& DinoId, const FString& AvatarId);
    void FlushAllSaves();
    void HandleRespawns(float Dt);
};
