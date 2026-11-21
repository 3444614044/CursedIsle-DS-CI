# UE4.27 Dedicated Server（两层结构）

> 本轮进展：把「骨架」拆成**可立即编译的握手验证服**与**最终进图服**两层，并补齐协议笔记、UDP 探针、网络版本覆盖、启动脚本与排错手册。

## 为什么分两层

客户端拿到 `all_servers` 下发的 `IP:PORT` 后，走的是 UE4 二进制 NetDriver 协议，中间有三道关卡：

1. **无连接握手 + `NMT_Hello` 网络版本校验**（最容易卡住）
2. **`NMT_Login` → `PreLogin` → `NMT_Welcome`**
3. **LoadMap → `NMT_Join` → Actor 复制**（需要真实地图与 GameMode）

之前只有一个笼统骨架，编译不过也不知道卡在哪。现在：

| 层 | 目录 | 能验证/做到什么 | 依赖 |
| --- | --- | --- | --- |
| **Tier 1 握手验证服** | `standalone/` | 走完关卡 1、2，直到 `PreLogin/PostLogin`；确认引擎版本、协议、端口、传输形态 | **只要装了 UE4.27**，零游戏模块、零游戏地图 |
| **Tier 2 进图服** | `world/`（+ 根下 `world_sim.py`） | 关卡 3 与**完整世界规则/复制/存档**；Python 侧今天可实跑 | 接入真实游戏工程才能最终进图 |

先把 Tier 1 编译跑通，看到 `[LocalDS] >>> PreLogin 成功`，再谈 Tier 2。别一上来就啃地图和 GameMode。

**世界层快速验证（不需要引擎）**：

```bash
python3 world_sim.py --selftest        # 41 项规则断言
python3 world_sim.py --demo            # 3 人跑完一个昼夜
python3 world_map_inventory.py         # 重扫 pak 资产
./start.sh → w                        # 交互控制台世界层菜单
```

REST 后端跑起来后，`http://127.0.0.1:8080/party` 看板底部有「🌍 世界层」只读镜像，
`http://127.0.0.1:8080/party/world` 是 JSON 快照。

## Tier 1：`standalone/`（推荐先做这个）

完整 UE4.27 工程，**不依赖游戏模块**，只用引擎自带模块和 `/Engine/Maps/Entry`：

```
standalone/
├── CursedIsleLocalServer.uproject
├── Config/DefaultEngine.ini          UDP IpNetDriver / Null OnlineSubsystem / 详细日志
├── Config/DefaultGame.ini             ProjectVersion 对齐 0.9.9.0.849
└── Source/
    ├── CursedIsleLocalServerServer.Target.cs      ← UBT 正式 target 名
    ├── CursedIsleLocalServerEditor.Target.cs      ← 生成 IDE 工程用
    └── CursedIsleLocalServer/
        ├── CursedIsleLocalServer.Build.cs         纯引擎模块
        ├── CursedIsleLocalServerModule.cpp        IMPLEMENT_PRIMARY_GAME_MODULE
        ├── LocalServerGameMode.h/.cpp             InitGame / PreLogin / PostLogin / Logout 打日志
        └── LocalServerGameSession.h/.cpp          ApproveLogin 放行
```

### 编译与启动

```bash
export UE4_ROOT=/path/to/UnrealEngine-4.27
./ue4_ds/run_ds.sh check      # 前置检查（含 127.0.0.1 陷阱）
./ue4_ds/run_ds.sh build      # RunUAT BuildTarget，只编译 Server target，不 cook
./ue4_ds/run_ds.sh run        # 启动，日志写到 logs/ds.log
```

`run_ds.sh` 支持 `PORT / MAP / PLATFORM / CONFIG / NETVER_OVERRIDE / BIN` 环境变量；完整打包用 `./ue4_ds/run_ds.sh package`。

### 三件必须先做的事

1. **`python3 set_lan_ip.py`** —— 客户端在手机上时，`all_servers` 下发 `127.0.0.1` 会指向手机自己，必然连不上。改成后端机器的局域网 IP。
2. **手机装 `certs/ca.crt` + ProxyPin 域名映射 + 443→8443** —— REST 层不通，客户端根本拿不到本地服务器地址。
3. **`./probe.sh stop`** —— UDP 探针和 DS 抢 7777，起 DS 前必须让出端口。

### 必然会遇到的一关：网络版本

`FNetworkVersion` 校验和含项目名/版本/NetCL/EngineNetVer/GameNetVer，`standalone` 工程名与客户端不同，日志会出现：[citation:12][citation:13]

```
Client connecting with invalid version. LocalNetworkVersion: X, RemoteNetworkVersion: Y
```

把 **Y** 填进 `config.json → ue4_ds.network_version_override`，或 `NETVER_OVERRIDE=Y ./ue4_ds/run_ds.sh run`（脚本会转成 `-networkversionoverride=Y`）。注意引擎必须真是 4.27，override 只骗过第一道校验和。[citation:12]

### Tier 1 的能力边界（重要）

`NMT_Welcome` 里发的地图是 `/Engine/Maps/Entry`，客户端 pak 里没有，**客户端会在 LoadMap 停下**，随后断开。这是刻意的：Tier 1 只回答「引擎版本/协议/端口对不对」，不回答「能不能进图」。看到 `PreLogin 成功` 就是这一层的成功终点，别在这里反复调地图。

## Tier 2：`world/`（本轮已完善，旧 `Source/` 保留为历史）

★ **本轮把 Tier 2 从骨架推到了完整可接入状态**，详见 `world/README.md` 与 `docs/世界层完善.md`。

| 子目录 | 状态 |
| --- | --- |
| `world/Source/CursedIsleWorld/` | ✅ 完整 C++ 模块：昼夜/饥饿/体力/成长/死亡复活/权威移动、GameState/PlayerState/Pawn/PC/GameMode 复制层级、RPC、REST 存档桥接 |
| `world/Config/*.snippet` | ✅ 合并用配置，地图改为客户端真实存在的 `/Game/Maps/Orchard` |
| `../world_sim.py` | ✅ 同一套规则的 Python 参考实现，**41 项自测全绿**，今天就能跑 |
| `../world_map_inventory.py` | ✅ 扫出 pak 里 6 个 `.umap`、1947 个 `SurvivalHunter` 蓝图资产 |
| `../docs/世界层完善.md` | ✅ 三道硬门槛 + 接入步骤 + 实测结果 |
| `Source/` `Config/`（本目录根下） | ⚠ 旧骨架，仅作历史保留，不要再往这里加代码 |

**关键判定（有 pak 证据）**：客户端 `CursedIsle-Android_ASTC.pak` 里 GameMode/Pawn/PlayerState 是蓝图类，
NetGUID 与 PackageMap 必须和已 cook 的类路径逐字节一致。**独立 C++ 工程永远进不了图** ——
唯一路径是拿到游戏 UE 工程源码，把 `world/Source/CursedIsleWorld` 合进 `CursedIsle` 主工程，
同工程名 / 同模块 / 同地图 / 同类路径，并自行 cook 一份 Linux Server 版 `/Game/Maps/Orchard`。

> **后续进展**：玩法主体已用**野路子**做完 —— `../gameplay.py` 把它做成服务端权威
> JSON 状态机（34 种真实恐龙 / 95 个 `GA_*` 技能 / 孵化 / 战斗 / AI / 群组 / 昼夜，
> 51 项自测全绿），经 `/api/v1/world/state|action` 侧信道下发，**不走 UE4 复制**，
> 因此绕开了上面的内容层门槛。见 `../docs/玩法主体_野路子.md`。
> 本模块记录的是「走 UE4 DS 正路」的方案，仍然有效；两条路线并行。

## 协议与排错

- `docs/UE4_27_网络协议笔记.md`：StatelessConnect → `NMT_Hello` → `NMT_Challenge` → `NMT_Login` → `NMT_Welcome` → `NMT_Join` 全时序、`FNetworkVersion` 组成、端口与加密说明 [citation:18][citation:19][citation:24][citation:10]
- `docs/UE4握手排错.md`：9 类现象 → 原因 → 动作，含日志关键字速查表

## 探针

| 脚本 | 用途 |
| --- | --- |
| `./probe.sh start` | `udp_probe.py`，抓 UDP 7777 的原始 datagram（**默认，抓包确认是 UDP**） |
| `./probe.sh start-ws` | `ws_probe.py`，仅当 UDP 完全没包、怀疑走 TCP WebSocket 时用 |
| `./probe.sh stop` | 停掉探针，让出端口给 DS |
| `./probe.sh status` | 查看状态 |

两个探针都**只记录不应答**；应答只能靠 UE4 DS。

## 端口最终结论

真实游戏服端口来自 `all_servers` 下发的 Base64(`IP:PORT`)，范围 **7777–7863**（UE4 默认 NetDriver 段）。pak 里 `WebSocketPort=8889` 只是实验插件 ini 默认值，运行时被覆盖。`config.json → local_ds.port`、`ue4_ds.port`、`-Port=` 三者必须一致，默认 7777。
