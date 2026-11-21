#!/usr/bin/env bash
# =====================================================================
# CursedIsle 本地 UE4.27 Dedicated Server — 编译 / 启动 / 诊断
#
# 用法：
#   UE4_ROOT=/path/to/UnrealEngine-4.27 ./run_ds.sh build
#   ./run_ds.sh run                       # 自动找 Binaries 下的二进制
#   BIN=/path/to/CursedIsleLocalServerServer ./run_ds.sh run
#   NETVER_OVERRIDE=123456789 ./run_ds.sh run
#   MAP=/Game/Maps/ServerManager PORT=7777 ./run_ds.sh run
#   ./run_ds.sh help
#
# 环境变量：
#   UE4_ROOT        4.27 引擎根目录（源码版或安装版）
#   PLATFORM        Linux / Win64（默认 Linux）
#   CONFIG          Development / Shipping（默认 Development）
#   PORT            DS 监听端口（默认 7777，必须与 config.json → local_ds.port 一致）
#   MAP             启动地图（默认 /Engine/Maps/Entry，握手验证用）
#   NETVER_OVERRIDE 覆盖网络版本校验和，见 docs/UE4握手排错.md 第 3 节
#   BIN             已编译 DS 二进制路径
# =====================================================================
set -e

HERE="$(cd "$(dirname "$0")" && pwd)"
PROJ="$HERE/standalone/CursedIsleLocalServer.uproject"
CFG="$HERE/../config.json"
MODE=${1:-help}

PORT=${PORT:-7777}
PLATFORM=${PLATFORM:-Linux}
CONFIG=${CONFIG:-Development}
MAP=${MAP:-/Engine/Maps/Entry}
UE4_ROOT=${UE4_ROOT:-}
BIN=${BIN:-}
NETVER=${NETVER_OVERRIDE:-$(python3 -c "import json;print(json.load(open('$CFG'))['ue4_ds'].get('network_version_override') or '')" 2>/dev/null || echo "")}

LOGCMDS='LogNet VeryVerbose,LogNetVersion Verbose,LogLocalDS Verbose,LogHandshake VeryVerbose,LogWorld Log'

EXTRA=()
if [ -n "$NETVER" ]; then EXTRA+=("-networkversionoverride=$NETVER"); fi

help() {
  echo "CursedIsle 本地 UE4.27 DS"
  echo
  echo "  build   用 RunUAT BuildTarget 只编译 Server target（最快，不 cook）"
  echo "  package 用 BuildCookRun 完整打包（要进图时用，需提供真实地图）"
  echo "  run     启动已编译的 DS"
  echo "  check   检查前置条件（引擎、端口占用、下发的 IP 是否为 127.0.0.1）"
  echo
  echo "当前：PORT=$PORT PLATFORM=$PLATFORM CONFIG=$CONFIG MAP=$MAP NETVER=${NETVER:-<未设>}"
  echo "引擎：${UE4_ROOT:-<未设置 UE4_ROOT>}"
}

check() {
  echo "== 前置检查 =="
  [ -f "$PROJ" ] && echo "  ✔ uproject 存在" || echo "  ✘ 缺 uproject"
  if [ -n "$UE4_ROOT" ] && [ -d "$UE4_ROOT" ]; then echo "  ✔ UE4_ROOT 存在"; else echo "  ✘ UE4_ROOT 未设置或不存在"; fi
  python3 - <<PY
import json,base64
c=json.load(open("$CFG"))
d=c['local_ds']; print('  下发的游戏服:', base64.b64encode(f"{d['host']}:{d['port']}".encode()).decode(), '->', f"{d['host']}:{d['port']}")
if d['host'] in ('127.0.0.1','localhost'):
    print('  ⚠ 警告：host=127.0.0.1。客户端在手机上时这指向手机自己！')
    print('           先跑 python3 set_lan_ip.py 改成后端机器的局域网 IP')
else:
    print('  ✔ host 已是具体 IP')
print('  DS 端口:', c['ue4_ds']['port'], ' REST 下发端口:', d['port'], '一致' if c['ue4_ds']['port']==d['port'] else '✘ 不一致')
PY
  echo "  UDP $PORT 占用："
  (ss -lun 2>/dev/null || netstat -lun 2>/dev/null || true) | grep -E ":$PORT\b" || echo "    (未占用，可以起 DS)"
}

build() {
  if [ -z "$UE4_ROOT" ] || [ ! -d "$UE4_ROOT" ]; then
    echo "✘ 请设置 UE4_ROOT 指向 Unreal Engine 4.27 根目录"; exit 2
  fi
  UAT="$UE4_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
  [ -f "$UAT" ] || { echo "✘ 找不到 $UAT（Windows 请用 RunUAT.bat）"; exit 2; }
  echo "== BuildTarget: CursedIsleLocalServerServer / $PLATFORM / $CONFIG =="
  "$UAT" BuildTarget \
    -project="$PROJ" \
    -Target=CursedIsleLocalServerServer \
    -TargetPlatform="$PLATFORM" \
    -BuildConfiguration="$CONFIG" \
    -NoP4 -WaitMutex
  echo "完成。产物通常在 standalone/Binaries/$PLATFORM/ 下，用 ./run_ds.sh run 启动。"
}

package() {
  if [ -z "$UE4_ROOT" ] || [ ! -d "$UE4_ROOT" ]; then
    echo "✘ 请设置 UE4_ROOT"; exit 2
  fi
  UAT="$UE4_ROOT/Engine/Build/BatchFiles/RunUAT.sh"
  echo "== BuildCookRun 完整打包 =="
  "$UAT" BuildCookRun \
    -project="$PROJ" -NoP4 -platform="$PLATFORM" \
    -server -noclient -serverconfig="$CONFIG" \
    -build -cook -stage -pak -archive \
    -archivedirectory="$HERE/../../ds_build"
  echo "完成，产物在 ds_build/。若 cook 报错（无 Content），改用 build 模式只编译握手验证服。"
}

run() {
  if [ -z "$BIN" ]; then
    BIN=$(find "$HERE/standalone/Binaries" -type f -name 'CursedIsleLocalServerServer*' -perm -u+x 2>/dev/null | head -n 1 || true)
  fi
  if [ -z "$BIN" ] || [ ! -x "$BIN" ]; then
    echo "✘ 找不到 DS 二进制。先 ./run_ds.sh build，或用 BIN=/path/to/binary 指定"; exit 2
  fi
  echo "== 启动 DS =="
  echo "   BIN     = $BIN"
  echo "   MAP     = $MAP"
  echo "   PORT    = $PORT"
  echo "   NETVER  = ${NETVER:-<默认>}"
  echo "   LOGCMDS = $LOGCMDS"
  echo
  echo "   ★ 若日志出现 'Client connecting with invalid version. RemoteNetworkVersion: N'"
  echo "     把 N 填进 config.json → ue4_ds.network_version_override 后重启"
  echo
  exec "$BIN" "$MAP" -server -log -Port="$PORT" -UNATTENDED \
       "${EXTRA[@]}" -LogCmds="$LOGCMDS" -Abslog="$HERE/../logs/ds.log"
}

case "$MODE" in
  help|-h|--help) help ;;
  check)          check ;;
  build)          build ;;
  package)        package ;;
  run)            run ;;
  *)              echo "未知模式: $MODE"; help; exit 1 ;;
esac
