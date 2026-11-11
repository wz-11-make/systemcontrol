#!/bin/bash
# 在 repo sync 之后重新接上 agent 的四处接线。
# 幂等:已经打过 patch 的会跳过。
set -u
SRC="$(cd "$(dirname "$0")/../../.." && pwd)"   # -> <source root>
HERE="$(cd "$(dirname "$0")" && pwd)"

apply_one() {
    local repo="$1" patch="$2"
    if [ ! -d "$SRC/$repo" ]; then
        echo "  ! 缺少 $repo,跳过"
        return
    fi
    if git -C "$SRC/$repo" apply --check "$HERE/$patch" 2>/dev/null; then
        git -C "$SRC/$repo" apply "$HERE/$patch"
        echo "  + 已应用 $patch"
    elif git -C "$SRC/$repo" apply --reverse --check "$HERE/$patch" 2>/dev/null; then
        echo "  = $patch 已在,跳过"
    else
        echo "  ! $patch 应用失败(可能有冲突),请手动检查"
    fi
}

echo "接线检查/应用:"
apply_one "frameworks/base"                  "0001-frameworks-base-agent-service.patch"
apply_one "device/xiaomi/venus"              "0002-device-venus-product.patch"
apply_one "device/xiaomi/sm8350-common"      "0003-device-sm8350-sepolicy.patch"
