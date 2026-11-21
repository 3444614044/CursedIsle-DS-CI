// CIWorldSaveBridge.h — DS 与 rest_backend.py 共享同一份玩家存档
//
// ★ 关键：DS 和 REST 后端跑在同一台机器，直接读写 data/users.json。
//   这样「朋友在 REST 里选的恐龙 / 等级 / 货币」进 DS 时自动带上，
//   DS 里的成长/死亡/位置又写回同一份文件，下次登录还在。
//   只增改字段，不删 REST 侧字段（token / device_number / currency_* 全部保留）。
#pragma once

#include "CoreMinimal.h"
#include "CIWorldTypes.h"

class FJsonObject;

class CURSEDISLEWORLD_API FCIWorldSaveBridge
{
public:
    FCIWorldSaveBridge();

    /** 从磁盘载入；文件不存在时建立空结构。 */
    bool Load();
    bool Save() const;

    /** 按 uuid 查存档（REST 后端签发）。 */
    bool FindByUuid(const FString& Uuid, FCIWorldPlayerSnapshot& Out) const;

    /** 把世界层状态写回（保留 token / 货币等 REST 字段）。 */
    void Upsert(const FCIWorldPlayerSnapshot& S);

    FString Path() const { return FilePath; }

private:
    FString FilePath;
    TSharedPtr<FJsonObject> Root;
    mutable FCriticalSection Mutex;   // const Save()/FindByUuid() 也要加锁
};
