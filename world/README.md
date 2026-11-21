# UE4.27 世界层（Tier 2）— 完善版

> 上一轮的 `standalone/` 是 **Tier 1 握手验证服**：能走到 `PreLogin`，但 `NMT_Welcome` 发的是引擎空地图 `/Engine/Maps/Entry`，客户端在 LoadMap 停下。
> 这一轮把 **世界层** 做成了结构完整、可插进真实游戏工程的 C++ 模块，并配了一份**今天就能实跑**的 Python 参考实现。

---

## 一、先把最关键的结论放前面

**「完善世界层」有代码层能做的，也有代码层做不到的。**

客户端 pak 是 `CursedIsle/Content/Paks/CursedIsle-Android_ASTC.pak`（185 MB）。这轮把索引和资产路径扫了一遍，得到三个决定性事实：

| 事实 | 证据 | 含义 |
| --- | --- | --- |
| 客户端有 6 个 cook 过的地图 | `InitLevel / MainMenu / Orchard / ServerManager / ServerManagerWatchDog / Test` (.umap) | DS 的 `ServerDefaultMap` 必须对齐其中之一，不再用 `/Engine/Maps/Entry` |
| 游戏逻辑几乎全是**蓝图** | `/Game/SurvivalHunter/` 下 **1952** 个资产：`BP_PlayerState`、`GA_*`（GAS 技能）、`DA_NestData`、`DA_EggData`、`C_DayNightCurve`、`C_NightPPAlpha` | Pawn / PlayerState / GameMode 是蓝图类，其**复制属性布局与 NetGUID 由蓝图决定** |
| 资产是 **Android_ASTC** cooked | pak 文件名本身 | Linux DS 不能加载跨平台 cooked 资产；需要自己 cook 一份 Linux Server 版同名地图 |

由此得出硬判定：

> **独立 C++ 工程（哪怕是这份）永远无法让这个客户端进图。**
> 因为 `NMT_Welcome` 下发的 GameMode 类、以及随后 Actor 复制时的 NetGUID / PackageMap，
> 都必须和客户端已 cook 的类路径**逐字节一致**。客户端要实例化的是它自己 pak 里的蓝图类，
> 而独立工程里只有 `/Script/CursedIsleWorld.*` 这些客户端不认识的类。
>
> 唯一路径：**拿到游戏 UE 工程源码（或至少 Linux Server cooked 资产），把这份世界层模块合进 `CursedIsle` 主工程**，
> 让 DS 与客户端同工程名、同模块、同地图、同类路径。

这不是代码量问题，也不是端口/证书/网络版本能绕过的。前面几轮的所有坑都是「协议层」，这一轮撞到的是「内容层」。

---

## 二、这轮真正完善了什么

`Source/CursedIsleWorld/`（8 个文件，只依赖引擎模块）实现了**世界层该有的全部结构**：

| 能力 | 文件 | 说明 |
| --- | --- | --- |
| 共享类型 | `CIWorldTypes.h` | `FCIWorldPlayerSnapshot` / `FCIWorldSnapshot`，把「所有 DOREPLIFETIME 属性」收进一个结构，便于与模拟器逐字段对照 |
| **纯逻辑规则** | `CIWorldRules.h/.cpp` | 昼夜相位（20 分钟一轮）、天气强度、饥饿衰减、体力消耗/恢复、饿肚子掉血、饱食回血、经验/升级、死亡、服务端限速与边界钳制、复活倒计时 |
| GameState | `CIWorldActors.h/.cpp` | 复制 `ServerTime / DayProgress / Phase / WeatherIntensity / OnlineCount`，`OnRep_Phase` 通知客户端切昼夜 |
| PlayerState | 同上 | 复制 `uuid / login / dino / avatar / level / hp / hunger / stamina / alive / xp` |
| Pawn | 同上 | 复制 Transform + 动画状态；`ServerMove` 只收**期望速度**，位置由服务端算完再复制回来（权威移动，防瞬移） |
| PlayerController | 同上 | `ServerSelectDino` / `ServerChat` RPC + `ClientWorldNotice` / `MulticastAnnounce` |
| GameMode | 同上 | `PreLogin` 解析 uuid/token → `PostLogin` **从 REST 存档恢复** → Spawn → 每 tick 推进规则 → 定期/退出**写回存档** |
| **存档桥接** | `CIWorldSaveBridge.h/.cpp` | DS 与 `rest_backend.py` 共享 `data/users.json`；按 uuid 恢复朋友在 REST 里选的恐龙/等级；只增改 `world_*` / `level` / `selected_*`，**绝不覆盖** token 与货币字段；原子写避免半截文件 |

多人闭环是通的：朋友 A 选了 rex → 服务器写回 `users.json` → 朋友 B 的 `friends/online_list` 里能看到 rex → A 进 DS 时 `FindByUuid` 直接恢复 rex 和等级。

**未经 UE4.27 实际编译**（本机没有引擎源码）。API 按 4.27 签名写，接入时若某处签名不符，以 `Engine/Source/Runtime/Engine/Classes/GameFramework/` 下的头文件为准——`run_ds.sh check` 会检查引擎环境。

---

## 三、类映射表（接入真实工程时必填）

| 世界层 C++ 类 | 客户端 pak 里的对应物 | 接入动作 |
| --- | --- | --- |
| `ACIWorldGameMode` | 未扫到 `BP_GameMode`（0 命中），可能在 `libUE4.so` 的 C++ 类里 | 用 `-LogCmds` 抓 `NMT_Welcome` 里的 GameMode 类名，把 `GlobalDefaultGameMode` 改成它 |
| `ACIWorldPlayerState` | `/Game/SurvivalHunter/Blueprints/Core/BP_PlayerState` | 蓝图里继承/替换，或让 C++ 类注册成同名路径 |
| `ACIWorldPawn` | `/Game/SurvivalHunter/Blueprints/Core/ObseerMode/BP_Pawn` + 各恐龙蓝图 (`/Game/SurvivalHunter/Blueprints/Dinosaur/*`) | **最难**：客户端 Pawn 的复制属性由这些蓝图决定，NetGUID 导出顺序必须一致 |
| 昼夜曲线 | `C_DayNightCurve` / `C_NightPPAlpha` | DS 只下发 `DayProgress` 数值，表现层曲线留给客户端 |
| 恐龙/孵化/技能 | `DA_DinoNames` / `DA_EggData` / `DA_NestData` / `GA_*` | 已由 `../../gameplay.py` 用野路子实现（服务端 JSON 状态机，不走 UE4 复制），见 `../../docs/玩法主体_野路子.md` |

`data/pak_asset_index.json` 里有全部 **2382 条 `/Game/` 路径**，可用 `python3 world_map_inventory.py` 重新生成。

---

## 四、接入步骤（拿到游戏工程后）

```bash
# 1) 拷模块进真实工程（工程名必须是 CursedIsle）
cp -r ue4_ds/world/Source/CursedIsleWorld  <GameProject>/Source/
#    在 <GameProject>.uproject 的 Modules 加：
#    {"Name":"CursedIsleWorld","Type":"Runtime","LoadingPhase":"Default"}
#    并在 CursedIsleServer.Target.cs 的 ExtraModuleNames 加 "CursedIsleWorld"

# 2) 合并 Config 片段（不要整文件覆盖）
cat ue4_ds/world/Config/DefaultEngine.ini.snippet >> <GameProject>/Config/DefaultEngine.ini
cat ue4_ds/world/Config/DefaultGame.ini.snippet  >> <GameProject>/Config/DefaultGame.ini
#    把 GlobalDefaultGameMode 的类路径改成客户端真实 GameMode

# 3) cook Linux Server 版地图
#    BuildCookRun -server -serverplatform=Linux -map=/Game/Maps/Orchard+...
#    客户端 Android_ASTC pak 不能直接给 Linux DS 用

# 4) 编译 Server target 并启动
export UE4_ROOT=/path/to/UnrealEngine-4.27
./ue4_ds/run_ds.sh check
./ue4_ds/run_ds.sh build
MAP=/Game/Maps/Orchard PORT=7777 ./ue4_ds/run_ds.sh run
```

启动前照旧：`./probe.sh stop` 让出 7777；REST 后端 `./start.sh --auto` 起；手机装 CA、装联机版 APK。

**验证成功的日志关键字**（按顺序）：

```
[LocalDS] >>> PreLogin 成功          ← 协议层（Tier 1 已能到）
[World] InitGame  Map=/Game/Maps/Orchard
[World] >>> PostLogin  xxx  当前 N 人
[World] 恢复存档 xxx  Lv.3  HP=...  pos=...
[World] t=60s  昼夜=Day(0.05)  天气=0.62  在线=3
[Chat] 阿杰: 有人吗
```

如果卡在 `Failed to load map /Game/Maps/Orchard` → 服务端没 cook 这张图；
如果 `Could not find class` → GameMode 类路径没对齐；
如果 `Client connecting with invalid version` → `ProjectName` 不是 `CursedIsle` 或没加 `-networkversionoverride`。

---

## 五、`world_sim.py`：今天就能跑的世界层参考实现

C++ 没有引擎编译不了，所以同一套规则做了一份 **零依赖 Python 版**，可实跑、可自测：

```bash
python3 world_sim.py --selftest      # 规则断言：昼夜/饥饿/死亡/复活/限速/边界/多人/存档
python3 world_sim.py --demo          # 3 个玩家模拟 20 分钟，打印世界推进
python3 world_sim.py --status        # 从 data/users.json 载入当前玩家
python3 world_sim.py --udp 17778     # 可选：UDP 调试通道（JSON，非 UE4 协议）
```

它与 `CIWorldRules.cpp` **逐字段对齐**（参数、公式、相位阈值都一样），作用是：

- 在没有 UE4.27 的机器上验证世界规则的行为是否正确
- 作为把 C++ 搬进真实工程时的**可执行规格**
- 让多人联机看板能看到「世界层在线状态」而不只是 REST 在线

⚠️ UDP 通道是**调试用的 JSON**，不是 UE4 NetDriver 协议，不能拿来冒充 DS 让客户端连。真正应答客户端的只能是同版本 UE4 DS。

---

## 六、还缺什么（如实说）

| 层 | 状态 |
| --- | --- |
| 协议握手 / 网络版本 / 端口 / 证书 | ✅ 前几轮已完成 |
| REST 多用户 / 独立存档 / 好友在线 / 看板 | ✅ 上一轮已完成 |
| **世界规则**（昼夜/饥饿/体力/成长/死亡/复活/权威移动） | ✅ 本轮完成（C++ + Python 双实现） |
| **复制层级**（GameState/PlayerState/Pawn/PC/RPC） | ✅ 本轮完成（C++ 结构完整） |
| **存档绑定**（REST ↔ DS） | ✅ 本轮完成 |
| 地图资产（Linux Server cooked） | ❌ 需游戏工程 |
| 蓝图类 / NetGUID / PackageMap 对齐 | ❌ 需游戏工程 |
| 玩法主体（34 种恐龙能力、GAS 技能、孵化、巢穴、战斗伤害、AI、掉落） | ✅ 野路子已做完（`../../gameplay.py`，51 项自测）；仅客户端真机注入待做 |

一句话：**走 UE4 DS 正路时，世界层「骨架、规则、复制、存档」已写完并可验证，进图卡在游戏内容资产；而野路子（`../../gameplay.py`）用 REST 侧信道绕开了这道门槛，玩法主体已能跑，只剩客户端真机注入。**
