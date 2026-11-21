#!/usr/bin/env bash
# 在任意目录执行：bash ~/ue4_ds_ci_ready/push_fix.sh
# 作用：重新提交修复版 workflow、取消误触发的旧 run、推送、重新触发构建
set -e
REPO=3444614044/CursedIsle-DS-CI

HERE="$(cd "$(dirname "$0")" && pwd)"
if [ -f "$HERE/standalone/CursedIsleLocalServer.uproject" ]; then DIR="$HERE"
else DIR="$HOME/ue4_ds_ci_ready"; fi
cd "$DIR"
echo "工程目录: $PWD"

# 1) 仓库自愈（.git 丢了就重建，identity 必设）
[ -d .git ] || git init
git config user.email "3444614044@users.noreply.github.com"
git config user.name  "3444614044"
git add -A
git diff --cached --quiet || git commit -m "fix: x86_64 runner + LinuxAArch64 cross compile"
git branch -M main
git log --oneline -1

TOKEN=$(gh auth token)

# 2) 取消上一轮误触发的旧 workflow run（arm runner 版会秒失败）
for ST in in_progress queued; do
  IDS=$(gh api "repos/$REPO/actions/workflows/build-ds.yml/runs?status=$ST&per_page=10" \
        -q '.workflow_runs[].id' 2>/dev/null || true)
  for RID in $IDS; do
    gh api -X POST "repos/$REPO/actions/runs/$RID/cancel" >/dev/null 2>&1 || true
    echo "  已取消旧 run $RID"
  done
done

# 3) 推送（token 写进 URL，不用 credential helper）
git remote remove origin 2>/dev/null || true
git remote add origin "https://3444614044:${TOKEN}@github.com/$REPO.git"
git push -u origin main || git push --force -u origin main
git remote set-url origin "https://github.com/$REPO.git"

# 4) 触发新 workflow（纯 API，不 fork git）
gh api "repos/$REPO/actions/workflows/build-ds.yml/dispatches" -f ref=main
sleep 8
gh api "repos/$REPO/actions/workflows/build-ds.yml/runs?per_page=1" \
  -q '.workflow_runs[0] | "RUN=\(.id)  STATUS=\(.status)  URL=\(.html_url)"'

echo
echo "轮询：gh api repos/$REPO/actions/runs/<RUN>/.."
