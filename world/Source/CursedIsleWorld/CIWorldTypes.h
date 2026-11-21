// CIWorldTypes.h — 世界层共享类型（纯数据，无 UObject 依赖）
#pragma once

#include "CoreMinimal.h"
#include "CIWorldTypes.generated.h"

/** 昼夜相位。与客户端 /Game/SurvivalHunter/Blueprints/Core/Weather/C_DayNightCurve 对齐用。 */
UENUM(BlueprintType)
enum class ECIWorldPhase : uint8
{
    Dawn, Day, Dusk, Night
};

/** 玩家动作状态，供 AnimBP 读取（表现层在客户端，服务端只负责权威下发）。 */
UENUM(BlueprintType)
enum class ECIWorldAnimState : uint8
{
    Idle, Walk, Run, Eat, Drink, Attack, Sleep, Dead
};

/**
 * 玩家的「网络快照」——等价于 UE4 里各 Actor 上 DOREPLIFETIME 属性的合集。
 * 独立成结构体是为了让 CIWorldRules 可以在无引擎环境里被 Python 参考实现逐字段对照。
 */
USTRUCT(BlueprintType)
struct FCIWorldPlayerSnapshot
{
    GENERATED_BODY()

    UPROPERTY() FString Uuid;
    UPROPERTY() FString Login;
    UPROPERTY() FString DinoId;        // troo / rex / giga / spino / para ...
    UPROPERTY() FString AvatarId;      // galli / para / troo ...
    UPROPERTY() int32  Level = 1;
    UPROPERTY() float  Experience = 0.f;
    UPROPERTY() float  Health = 100.f;
    UPROPERTY() float  Hunger = 100.f;   // 100 = 饱
    UPROPERTY() float  Stamina = 100.f;
    UPROPERTY() FVector Position = FVector::ZeroVector;
    UPROPERTY() FRotator Rotation = FRotator::ZeroRotator;
    UPROPERTY() ECIWorldAnimState Anim = ECIWorldAnimState::Idle;
    UPROPERTY() bool   bAlive = true;
};

/** 服务端世界状态快照（GameState 复制内容的等价物）。 */
USTRUCT(BlueprintType)
struct FCIWorldSnapshot
{
    GENERATED_BODY()

    UPROPERTY() double ServerTime = 0.0;
    UPROPERTY() float  DayProgress = 0.f;   // 0..1，一个完整昼夜
    UPROPERTY() ECIWorldPhase Phase = ECIWorldPhase::Day;
    UPROPERTY() float  WeatherIntensity = 0.f;
    UPROPERTY() int32  OnlineCount = 0;
    UPROPERTY() TArray<FCIWorldPlayerSnapshot> Players;
};
