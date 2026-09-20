#!/usr/bin/env bash
# Linux x86_64 / aarch64. Dependencies stay inside the chosen workspace.
set -euo pipefail
team=$(cd "$(dirname "$0")/.." && pwd)
root=$(realpath "${1:?Usage: setup-dependencies.sh /path/to/openvela-workspace}")
deps="$root/.s31-deps"
mkdir -p "$deps"
hal="$deps/esp-hal-3rdparty"
revision=290edc31b50decca660c1a11ce3506fd9b2e1e27
if [[ ! -d "$hal" ]]; then
  git clone https://github.com/78/esp-hal-3rdparty.git "$hal"
  git -C "$hal" checkout --detach "$revision"
fi
[[ $(git -C "$hal" rev-parse HEAD) == "$revision" ]]
git -C "$hal" submodule update --init --recursive --jobs 8
apply_once() {
  local target=$1 patch=$2
  [[ -s "$patch" ]] || return 0
  if git -C "$target" apply --reverse --check "$patch" 2>/dev/null; then return 0; fi
  git -C "$target" apply --check "$patch"
  git -C "$target" apply --whitespace=nowarn "$patch"
}
apply_once "$hal" "$team/workspace/dependencies/hal.patch"
apply_once "$hal/components/mbedtls/mbedtls" "$team/workspace/dependencies/hal-mbedtls.patch"
python3 -m venv "$deps/venv"
"$deps/venv/bin/pip" install -r "$team/workspace/dependencies/requirements.txt"
python3 - "$team/workspace/dependencies/toolchain.json" "$deps" <<'PY'
import hashlib, json, pathlib, platform, subprocess, sys, urllib.request
lock=json.loads(pathlib.Path(sys.argv[1]).read_text())
deps=pathlib.Path(sys.argv[2])
arch={'x86_64':'linux-amd64','aarch64':'linux-arm64'}[platform.machine()]
entry=lock[arch]
if not (deps/'riscv32-esp-elf/bin/riscv32-esp-elf-gcc').exists():
    archive=deps/'toolchain.tar.xz'
    if not archive.exists(): urllib.request.urlretrieve(entry['url'],archive)
    if hashlib.sha256(archive.read_bytes()).hexdigest()!=entry['sha256']:
        sys.exit('Toolchain checksum mismatch')
    subprocess.run(['tar','-xJf',str(archive),'-C',str(deps)],check=True)
PY
printf 'Dependencies ready: %s\n' "$deps"
