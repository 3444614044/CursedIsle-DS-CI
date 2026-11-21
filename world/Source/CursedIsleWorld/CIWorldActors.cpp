// CIWorldActors.cpp
#include "CIWorldActors.h"
#include "CIWorldSaveBridge.h"
#include "Net/UnrealNetwork.h"
#include "Kismet/GameplayStatics.h"
#include "Engine/NetConnection.h"
#include "Engine/World.h"
#include "GameFramework/GameSession.h"

DEFINE_LOG_CATEGORY_STATIC(LogCIWorld, Log, All);

// ============================================================================
// GameState
// ============================================================================
ACIWorldGameState::ACIWorldGameState()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
}

void ACIWorldGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(ACIWorldGameState, ServerTime);
    DOREPLIFETIME(ACIWorldGameState, DayProgress);
    DOREPLIFETIME(ACIWorldGameState, Phase);
    DOREPLIFETIME(ACIWorldGameState, WeatherIntensity);
    DOREPLIFETIME(ACIWorldGameState, OnlineCount);
}

void ACIWorldGameState::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetLocalRole() != ROLE_Authority) return;

    // GameMode 持有权威玩家快照；这里只推进「世界时间/昼夜/天气」，
    // 玩家逐人 tick 在 ACIWorldGameMode::Tick 里做（避免两处重复推进）。
    ServerTime += DeltaSeconds;
    DayProgress = FMath::Fmod(ServerTime / WorldRules.Params().DayLengthSeconds, 1.f);
    Phase = FCIWorldRules::PhaseFromProgress(DayProgress);
    WeatherIntensity = FMath::Clamp(
        0.5f + 0.35f * FMath::Sin(ServerTime * 0.017f)
             + 0.15f * FMath::Sin(ServerTime * 0.053f), 0.f, 1.f);

    AccumulatedForLog += DeltaSeconds;
    if (AccumulatedForLog >= 60.f)
    {
        AccumulatedForLog = 0.f;
        UE_LOG(LogCIWorld, Log, TEXT("[World] t=%.0fs  昼夜=%s(%.2f)  天气=%.2f  在线=%d"),
               ServerTime, *FCIWorldRules::PhaseName(Phase), DayProgress,
               WeatherIntensity, OnlineCount);
    }
}

void ACIWorldGameState::OnRep_WorldTime() { /* 客户端：交给 HUD/天气蓝图读值 */ }
void ACIWorldGameState::OnRep_Phase()
{
    UE_LOG(LogCIWorld, Log, TEXT("[World] 昼夜切换到 %s"), *FCIWorldRules::PhaseName(Phase));
}

// ============================================================================
// PlayerState
// ============================================================================
ACIWorldPlayerState::ACIWorldPlayerState()
{
    bReplicates = true;
}

void ACIWorldPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(ACIWorldPlayerState, Uuid);
    DOREPLIFETIME(ACIWorldPlayerState, Login);
    DOREPLIFETIME(ACIWorldPlayerState, DinoId);
    DOREPLIFETIME(ACIWorldPlayerState, AvatarId);
    DOREPLIFETIME(ACIWorldPlayerState, Level);
    DOREPLIFETIME(ACIWorldPlayerState, Health);
    DOREPLIFETIME(ACIWorldPlayerState, Hunger);
    DOREPLIFETIME(ACIWorldPlayerState, Stamina);
    DOREPLIFETIME(ACIWorldPlayerState, bAlive);
    DOREPLIFETIME(ACIWorldPlayerState, Experience);
}

void ACIWorldPlayerState::OnRep_Vitals() { /* 客户端 HUD 更新 */ }

void ACIWorldPlayerState::ApplyFromSnapshot(const FCIWorldPlayerSnapshot& S)
{
    Uuid = S.Uuid; Login = S.Login;
    DinoId = S.DinoId; AvatarId = S.AvatarId;
    Level = S.Level; Health = S.Health; Hunger = S.Hunger;
    Stamina = S.Stamina; bAlive = S.bAlive; Experience = S.Experience;
    SetPlayerName(S.Login);
}

FCIWorldPlayerSnapshot ACIWorldPlayerState::ToSnapshot() const
{
    FCIWorldPlayerSnapshot S;
    S.Uuid = Uuid; S.Login = Login; S.DinoId = DinoId; S.AvatarId = AvatarId;
    S.Level = Level; S.Health = Health; S.Hunger = Hunger;
    S.Stamina = Stamina; S.bAlive = bAlive; S.Experience = Experience;
    return S;
}

// ============================================================================
// Pawn
// ============================================================================
ACIWorldPawn::ACIWorldPawn()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(false);   // 我们用自己的复制字段，避免和 MovementComponent 打架
}

void ACIWorldPawn::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& Out) const
{
    Super::GetLifetimeReplicatedProps(Out);
    DOREPLIFETIME(ACIWorldPawn, ReplicatedPosition);
    DOREPLIFETIME(ACIWorldPawn, ReplicatedRotation);
    DOREPLIFETIME(ACIWorldPawn, AnimState);
}

void ACIWorldPawn::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetLocalRole() != ROLE_Authority) return;
    if (OwnerSnapshot)
    {
        ReplicatedPosition = OwnerSnapshot->Position;
        ReplicatedRotation = OwnerSnapshot->Rotation;
        AnimState = OwnerSnapshot->Anim;
        SetActorLocationAndRotation(OwnerSnapshot->Position, OwnerSnapshot->Rotation);
    }
}

bool ACIWorldPawn::ServerMove_Validate(FVector RequestedVelocity, float ClientDelta)
{
    // 防作弊：速度上限 + dt 合理性。校验失败 UE4 会直接断连。
    const FCIWorldRuleParams P;
    return RequestedVelocity.Size() <= P.MaxSpeed * 1.05f
        && ClientDelta > 0.f && ClientDelta <= 0.25f
        && !RequestedVelocity.ContainsNaN();
}

void ACIWorldPawn::ServerMove_Implementation(FVector RequestedVelocity, float ClientDelta)
{
    if (!OwnerSnapshot) return;
    // 走 GameMode 里那套 FCIWorldRules，保证限速/边界/动画推导一致
    FCIWorldRules R;
    R.ApplyMove(*OwnerSnapshot, RequestedVelocity, FMath::Min(ClientDelta, 0.1f));
}

bool ACIWorldPawn::ServerSetAnim_Validate(ECIWorldAnimState NewAnim)
{
    return NewAnim != ECIWorldAnimState::Dead;   // 死亡只能由服务端判定
}

void ACIWorldPawn::ServerSetAnim_Implementation(ECIWorldAnimState NewAnim)
{
    if (OwnerSnapshot && OwnerSnapshot->bAlive) OwnerSnapshot->Anim = NewAnim;
}

// ============================================================================
// PlayerController
// ============================================================================
ACIWorldPlayerController::ACIWorldPlayerController()
{
    bReplicates = true;
}

bool ACIWorldPlayerController::ServerSelectDino_Validate(const FString& DinoId, const FString& AvatarId)
{
    // TODO: 对白名单做校验（客户端 pak 里 DA_DinoNames 的 34 个 id）
    return DinoId.Len() <= 32 && AvatarId.Len() <= 32;
}

void ACIWorldPlayerController::ServerSelectDino_Implementation(const FString& DinoId, const FString& AvatarId)
{
    ACIWorldPlayerState* PS = GetPlayerState<ACIWorldPlayerState>();
    if (!PS) return;
    // ★ 必须写进 GameMode 的权威快照；只改 PlayerState 会被下一帧 Tick 覆盖。
    if (ACIWorldGameMode* GM = GetWorld()->GetAuthGameMode<ACIWorldGameMode>())
        GM->SetPlayerLoadout(PS->Uuid, DinoId, AvatarId);
    UE_LOG(LogCIWorld, Warning, TEXT("[World] %s 选择恐龙=%s 皮肤=%s"),
           *PS->Login, *DinoId, *AvatarId);
    MulticastAnnounce(FString::Printf(TEXT("%s 换上了 %s"), *PS->Login, *DinoId));
}

bool ACIWorldPlayerController::ServerChat_Validate(const FString& Message)
{
    return Message.Len() <= 200 && !Message.Contains(TEXT("\n"));
}

void ACIWorldPlayerController::ServerChat_Implementation(const FString& Message)
{
    ACIWorldPlayerState* PS = GetPlayerState<ACIWorldPlayerState>();
    const FString Who = PS ? PS->Login : TEXT("?");
    UE_LOG(LogCIWorld, Log, TEXT("[Chat] %s: %s"), *Who, *Message);
    MulticastAnnounce(FString::Printf(TEXT("[%s] %s"), *Who, *Message));
}

void ACIWorldPlayerController::ClientWorldNotice_Implementation(const FString& Text)
{
    UE_LOG(LogCIWorld, Log, TEXT("[Notice] %s"), *Text);
}

void ACIWorldPlayerController::MulticastAnnounce_Implementation(const FString& Text)
{
    UE_LOG(LogCIWorld, Log, TEXT("[All] %s"), *Text);
}

// ============================================================================
// GameMode
// ============================================================================
ACIWorldGameMode::ACIWorldGameMode()
{
    PrimaryActorTick.bCanEverTick = true;
    GameStateClass = ACIWorldGameState::StaticClass();
    PlayerStateClass = ACIWorldPlayerState::StaticClass();
    PlayerControllerClass = ACIWorldPlayerController::StaticClass();
    DefaultPawnClass = ACIWorldPawn::StaticClass();
    bUseSeamlessTravel = false;
    bStartPlayersAsSpectators = false;

    SaveBridge = new FCIWorldSaveBridge();
    UE_LOG(LogCIWorld, Warning, TEXT("======================================================"));
    UE_LOG(LogCIWorld, Warning, TEXT("[World] 世界层 GameMode 已创建（多人权威服）"));
    UE_LOG(LogCIWorld, Warning, TEXT("======================================================"));
}

void ACIWorldGameMode::InitGame(const FString& MapName, const FString& Options, FString& ErrorMessage)
{
    Super::InitGame(MapName, Options, ErrorMessage);
    UE_LOG(LogCIWorld, Warning, TEXT("[World] InitGame  Map=%s  Options=%s"), *MapName, *Options);
    SaveBridge->Load();
}

void ACIWorldGameMode::PreLogin(const FString& Options, const FString& Address,
                                const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage)
{
    // ★ 真实服务端应在这里拿 Options 里的 token 去 REST 后端 /api/v1/auth/verify 校验。
    //   本层接受任何带 uuid 的连接；uuid 缺失就回落成游客（便于本地联调）。
    const FString Uuid = UGameplayStatics::ParseOption(Options, TEXT("uuid"));
    UE_LOG(LogCIWorld, Warning,
           TEXT("[World] >>> PreLogin  Address=%s  uuid=%s  Options=%s"),
           *Address, *Uuid, *Options);

    if (AuthoritativePlayers.Num() >= Rules.Params().MaxPlayers)
    {
        ErrorMessage = TEXT("ServerFull");
        return;
    }
    ErrorMessage.Empty();
    Super::PreLogin(Options, Address, UniqueId, ErrorMessage);
}

void ACIWorldGameMode::PostLogin(APlayerController* NewPlayer)
{
    Super::PostLogin(NewPlayer);   // 内部会 SpawnDefaultPawnAtTransform

    const FString Options = NewPlayer->GetNetConnection()
                          ? NewPlayer->GetNetConnection()->RequestURL : FString();
    const FString Uuid   = UGameplayStatics::ParseOption(Options, TEXT("uuid"));
    const FString Login  = UGameplayStatics::ParseOption(Options, TEXT("login"));
    const FString Dino   = UGameplayStatics::ParseOption(Options, TEXT("dino"));
    const FString Avatar = UGameplayStatics::ParseOption(Options, TEXT("avatar"));

    const FString Key = NewPlayer->GetName();
    ConnectionUuid.Add(Key, Uuid.IsEmpty() ? TEXT("guest") : Uuid);
    RestorePlayer(NewPlayer, Uuid, Login, Dino, Avatar);

    UE_LOG(LogCIWorld, Warning, TEXT("[World] >>> PostLogin  %s (uuid=%s)  当前 %d 人 <<<"),
           *Login, *Uuid, AuthoritativePlayers.Num());

    if (ACIWorldPlayerController* PC = Cast<ACIWorldPlayerController>(NewPlayer))
        PC->ClientWorldNotice(FString::Printf(TEXT("欢迎 %s，世界时间已同步"), *Login));
}

void ACIWorldGameMode::RestorePlayer(APlayerController* PC, const FString& Uuid,
                                     const FString& Login, const FString& Dino,
                                     const FString& Avatar)
{
    FCIWorldPlayerSnapshot S;
    // 优先用 REST 存档；没有就新建。存档按 uuid 索引，朋友各用不同登录名互不覆盖。
    if (!SaveBridge->FindByUuid(Uuid, S))
    {
        S.Uuid = Uuid.IsEmpty() ? FGuid::NewGuid().ToString() : Uuid;
        S.Login = Login.IsEmpty() ? TEXT("player") : Login;
        S.DinoId = Dino.IsEmpty() ? TEXT("troo") : Dino;
        S.AvatarId = Avatar.IsEmpty() ? TEXT("galli") : Avatar;
        S.Position = FVector(FMath::RandRange(-2000.f, 2000.f),
                             FMath::RandRange(-2000.f, 2000.f), 200.f);
        UE_LOG(LogCIWorld, Log, TEXT("[World] 新玩家 %s，出生点 %s"),
               *S.Login, *S.Position.ToString());
    }
    else
    {
        UE_LOG(LogCIWorld, Log, TEXT("[World] 恢复存档 %s  Lv.%d  HP=%.0f  饥饿=%.0f  pos=%s"),
               *S.Login, S.Level, S.Health, S.Hunger, *S.Position.ToString());
    }

    AuthoritativePlayers.Add(S.Uuid, S);

    if (ACIWorldPlayerState* PS = PC->GetPlayerState<ACIWorldPlayerState>())
        PS->ApplyFromSnapshot(S);
    if (ACIWorldPawn* Pawn = Cast<ACIWorldPawn>(PC->GetPawn()))
        Pawn->SetOwnerSnapshot(AuthoritativePlayers.Find(S.Uuid));

    if (ACIWorldGameState* GS = GetWorld()->GetGameState<ACIWorldGameState>())
        GS->OnlineCount = AuthoritativePlayers.Num();
}

void ACIWorldGameMode::SetPlayerLoadout(const FString& Uuid, const FString& DinoId,
                                        const FString& AvatarId)
{
    if (FCIWorldPlayerSnapshot* S = AuthoritativePlayers.Find(Uuid))
    {
        S->DinoId = DinoId;
        S->AvatarId = AvatarId;
        // 立即同步给 PlayerState，不等下一帧
        for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
        {
            if (ACIWorldPlayerState* PS = It->Get()->GetPlayerState<ACIWorldPlayerState>())
                if (PS->Uuid == Uuid) PS->ApplyFromSnapshot(*S);
        }
        SaveBridge->Upsert(*S);   // 朋友在 REST 好友列表里马上能看到新恐龙
        SaveBridge->Save();
    }
}

void ACIWorldGameMode::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (GetLocalRole() != ROLE_Authority) return;

    // 1) 构造世界快照喂给纯规则层
    FCIWorldSnapshot World;
    if (ACIWorldGameState* GS = GetWorld()->GetGameState<ACIWorldGameState>())
    {
        World.ServerTime = GS->ServerTime;
        World.DayProgress = GS->DayProgress;
        World.Phase = GS->Phase;
    }
    for (auto& Pair : AuthoritativePlayers) World.Players.Add(Pair.Value);

    // 2) 推进玩家规则。★ 不调 Rules.Tick() —— 世界时间/昼夜/天气已由
    //    ACIWorldGameState::Tick 推进过一次，再调会重复累加并让相位差一帧。
    //    这里只逐人跑 TickPlayer（Python 版 world_sim.World.tick 把两者合在一起，
    //    行为等价，只是引擎里按 Actor 职责拆开）。
    for (FCIWorldPlayerSnapshot& S : World.Players)
        Rules.TickPlayer(S, DeltaSeconds, World);

    // 3) 写回权威快照 + PlayerState 复制属性
    for (FCIWorldPlayerSnapshot& S : World.Players)
    {
        if (FCIWorldPlayerSnapshot* Dest = AuthoritativePlayers.Find(S.Uuid)) *Dest = S;
    }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* PC = It->Get();
        const FString Uuid = ConnectionUuid.FindRef(PC->GetName());
        if (const FCIWorldPlayerSnapshot* S = AuthoritativePlayers.Find(Uuid))
            if (ACIWorldPlayerState* PS = PC->GetPlayerState<ACIWorldPlayerState>())
                PS->ApplyFromSnapshot(*S);
    }

    HandleRespawns(DeltaSeconds);

    // 4) 定期把存档刷回 data/users.json（REST 后端读同一份文件）
    SaveFlushAccum += DeltaSeconds;
    if (SaveFlushAccum >= 15.f) { SaveFlushAccum = 0.f; FlushAllSaves(); }
}

void ACIWorldGameMode::HandleRespawns(float Dt)
{
    for (auto& Pair : AuthoritativePlayers)
    {
        FCIWorldPlayerSnapshot& S = Pair.Value;
        if (S.bAlive) continue;
        float& T = RespawnTimers.FindOrAdd(S.Uuid, 0.f);
        T += Dt;
        if (T >= Rules.Params().RespawnDelaySeconds)
        {
            S.bAlive = true; S.Health = 100.f; S.Hunger = 60.f;
            S.Stamina = 100.f; S.Anim = ECIWorldAnimState::Idle;
            S.Position = FVector(0.f, 0.f, 200.f);
            T = 0.f;
            UE_LOG(LogCIWorld, Warning, TEXT("[World] %s 复活"), *S.Login);
        }
    }
}

void ACIWorldGameMode::FlushAllSaves()
{
    for (auto& Pair : AuthoritativePlayers)
        SaveBridge->Upsert(Pair.Value);
    SaveBridge->Save();
}

void ACIWorldGameMode::Logout(AController* Exiting)
{
    const FString Key = Exiting ? Exiting->GetName() : FString();
    const FString Uuid = ConnectionUuid.FindRef(Key);
    if (const FCIWorldPlayerSnapshot* S = AuthoritativePlayers.Find(Uuid))
    {
        SaveBridge->Upsert(*S);
        UE_LOG(LogCIWorld, Warning, TEXT("[World] Logout %s，存档已写回"), *S->Login);
        AuthoritativePlayers.Remove(Uuid);
    }
    SaveBridge->Save();
    if (ACIWorldGameState* GS = GetWorld()->GetGameState<ACIWorldGameState>())
        GS->OnlineCount = AuthoritativePlayers.Num();
    Super::Logout(Exiting);
}
