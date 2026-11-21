// CIWorldRules.h — 纯逻辑世界规则（不依赖 UObject / World，可独立单测）
//
// ★ 设计意图：这些规则与 world_sim.py 的 Python 参考实现逐字段一一对应。
//   Python 版今天就能跑自测；C++ 版是把它搬进真实 UE4 工程时的权威实现。
//   改规则请两边同步，避免模拟器与 DS 行为漂移。
#pragma once

#include "CoreMinimal.h"
#include "CIWorldTypes.h"

/** 可调参数。全部集中在此，便于与 Python 版 rules dict 对齐。 */
struct FCIWorldRuleParams
{
    float DayLengthSeconds = 1200.f;      // 一个完整昼夜 20 分钟
    float HungerDecayPerSec = 0.08f;      // 饥饿每秒下降
    float StaminaRegenPerSec = 6.f;       // 静止时体力恢复
    float StaminaDrainRunPerSec = 9.f;    // 奔跑时体力消耗
    float StarveDamagePerSec = 1.2f;      // 饿到 0 后掉血
    float HealthRegenPerSec = 0.6f;       // 饱食时回血
    float GrowthXpPerSec = 0.35f;         // 存活经验
    float XpPerLevel = 100.f;
    float MaxSpeed = 950.f;               // cm/s，服务端权威限速
    float WorldHalfExtent = 100000.f;     // 正方形世界半径（cm），超出钳制
    float RespawnDelaySeconds = 8.f;
    int32 MaxPlayers = 80;
};

class CURSEDISLEWORLD_API FCIWorldRules
{
public:
    explicit FCIWorldRules(const FCIWorldRuleParams& InParams = FCIWorldRuleParams())
        : P(InParams) {}

    /** 推进整个世界 dt 秒；返回是否有玩家状态发生值得复制的变化。 */
    bool Tick(FCIWorldSnapshot& World, float DeltaSeconds);

    /** 单个玩家 tick：饥饿/体力/血量/经验/死亡与复活。 */
    void TickPlayer(FCIWorldPlayerSnapshot& Player, float DeltaSeconds,
                    const FCIWorldSnapshot& World);

    /** 服务端权威移动：限速 + 边界钳制 + 朝向。客户端只发输入，位置由服务端算。 */
    void ApplyMove(FCIWorldPlayerSnapshot& Player, const FVector& RequestedVelocity,
                   float DeltaSeconds);

    static ECIWorldPhase PhaseFromProgress(float Progress);
    static FString PhaseName(ECIWorldPhase Phase);

    const FCIWorldRuleParams& Params() const { return P; }

private:
    FCIWorldRuleParams P;
};
