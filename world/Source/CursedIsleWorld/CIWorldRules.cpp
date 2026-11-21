// CIWorldRules.cpp
#include "CIWorldRules.h"

DEFINE_LOG_CATEGORY_STATIC(LogCIWorldRules, Log, All);

ECIWorldPhase FCIWorldRules::PhaseFromProgress(float Progress)
{
    // 0.00-0.15 黎明 / 0.15-0.60 白天 / 0.60-0.75 黄昏 / 0.75-1.00 夜晚
    const float P = FMath::Frac(Progress);
    if (P < 0.15f) return ECIWorldPhase::Dawn;
    if (P < 0.60f) return ECIWorldPhase::Day;
    if (P < 0.75f) return ECIWorldPhase::Dusk;
    return ECIWorldPhase::Night;
}

FString FCIWorldRules::PhaseName(ECIWorldPhase Phase)
{
    switch (Phase)
    {
    case ECIWorldPhase::Dawn:  return TEXT("Dawn");
    case ECIWorldPhase::Day:   return TEXT("Day");
    case ECIWorldPhase::Dusk:  return TEXT("Dusk");
    case ECIWorldPhase::Night: return TEXT("Night");
    }
    return TEXT("?");
}

bool FCIWorldRules::Tick(FCIWorldSnapshot& World, float DeltaSeconds)
{
    if (DeltaSeconds <= 0.f) return false;

    World.ServerTime += DeltaSeconds;
    World.DayProgress = FMath::Fmod(
        static_cast<float>(World.ServerTime) / P.DayLengthSeconds, 1.f);
    const ECIWorldPhase NewPhase = PhaseFromProgress(World.DayProgress);
    const bool bPhaseChanged = (NewPhase != World.Phase);
    World.Phase = NewPhase;
    // 天气强度用两段正弦做确定性伪随机，保证所有客户端算出来一致
    World.WeatherIntensity = FMath::Clamp(
        0.5f + 0.35f * FMath::Sin(World.ServerTime * 0.017f)
             + 0.15f * FMath::Sin(World.ServerTime * 0.053f), 0.f, 1.f);

    for (FCIWorldPlayerSnapshot& Player : World.Players)
    {
        TickPlayer(Player, DeltaSeconds, World);
    }
    World.OnlineCount = World.Players.Num();
    return bPhaseChanged;
}

void FCIWorldRules::TickPlayer(FCIWorldPlayerSnapshot& Player, float Dt,
                               const FCIWorldSnapshot& World)
{
    if (!Player.bAlive)
    {
        // 复活：倒计时在 GameMode 里处理，这里只保证死亡态属性不再变化
        Player.Health = 0.f;
        Player.Anim = ECIWorldAnimState::Dead;
        return;
    }

    // --- 饥饿 ---
    const bool bRunning = (Player.Anim == ECIWorldAnimState::Run);
    const float HungerRate = P.HungerDecayPerSec * (bRunning ? 1.8f : 1.f)
                           * (World.Phase == ECIWorldPhase::Night ? 0.85f : 1.f);
    Player.Hunger = FMath::Clamp(Player.Hunger - HungerRate * Dt, 0.f, 100.f);

    // --- 体力 ---
    if (bRunning)
        Player.Stamina = FMath::Clamp(Player.Stamina - P.StaminaDrainRunPerSec * Dt, 0.f, 100.f);
    else
        Player.Stamina = FMath::Clamp(Player.Stamina + P.StaminaRegenPerSec * Dt, 0.f, 100.f);
    if (Player.Stamina <= 0.f && Player.Anim == ECIWorldAnimState::Run)
        Player.Anim = ECIWorldAnimState::Walk;

    // --- 血量：饿肚子掉血，吃饱回血 ---
    if (Player.Hunger <= 0.f)
        Player.Health -= P.StarveDamagePerSec * Dt;
    else if (Player.Hunger > 40.f)
        Player.Health += P.HealthRegenPerSec * Dt;
    Player.Health = FMath::Clamp(Player.Health, 0.f, 100.f);

    // --- 成长 ---
    Player.Experience += P.GrowthXpPerSec * Dt;
    while (Player.Experience >= P.XpPerLevel)
    {
        Player.Experience -= P.XpPerLevel;
        Player.Level += 1;
        UE_LOG(LogCIWorldRules, Log, TEXT("[World] %s 升级到 Lv.%d"),
               *Player.Login, Player.Level);
    }

    if (Player.Health <= 0.f)
    {
        Player.bAlive = false;
        Player.Anim = ECIWorldAnimState::Dead;
        UE_LOG(LogCIWorldRules, Warning, TEXT("[World] %s 死亡"), *Player.Login);
    }
}

void FCIWorldRules::ApplyMove(FCIWorldPlayerSnapshot& Player,
                              const FVector& RequestedVelocity, float Dt)
{
    if (!Player.bAlive || Dt <= 0.f) return;

    // 权威限速：不信任客户端自报位移
    FVector V = RequestedVelocity;
    const float Speed = V.Size();
    if (Speed > P.MaxSpeed)
        V = V.GetSafeNormal() * P.MaxSpeed;

    FVector NewPos = Player.Position + V * Dt;
    // 边界钳制
    NewPos.X = FMath::Clamp(NewPos.X, -P.WorldHalfExtent, P.WorldHalfExtent);
    NewPos.Y = FMath::Clamp(NewPos.Y, -P.WorldHalfExtent, P.WorldHalfExtent);
    NewPos.Z = FMath::Clamp(NewPos.Z, -5000.f, 20000.f);
    Player.Position = NewPos;

    // 朝向与动画状态由速度推导，客户端不能自己说了算
    const float ActualSpeed = V.Size();
    if (!V.IsNearlyZero())
    {
        Player.Rotation = V.Rotation();
        Player.Anim = (ActualSpeed > P.MaxSpeed * 0.55f)
                    ? ECIWorldAnimState::Run : ECIWorldAnimState::Walk;
    }
    else
    {
        Player.Anim = ECIWorldAnimState::Idle;
    }
}
