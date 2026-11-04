#!/usr/bin/env python3
"""Build an existing S31 profile in a fresh output; never flash/download."""
from pathlib import Path
import argparse
import hashlib
import json
import subprocess
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / "tools/build-support"))
from environment import prepare, sha as dependency_sha

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("--output", type=Path, required=True, help="New directory; all generated source/build files stay here")
parser.add_argument("--jobs", type=int, default=4)
parser.add_argument("--workspace-source", action="store_true",
                    help="Share maintained source without copying it; use NuttX source protection")
parser.add_argument("--profile", default="korvo-camera-touch-sd-keys",
                    help="Existing board config (default: key qualification)")
args = parser.parse_args()
output = args.output.resolve()
if output.exists() or args.jobs < 1:
    parser.error("Use a new output directory and positive --jobs")
nuttx = root / "openvela-dev/nuttx"
profiles = nuttx / "boards/risc-v/esp32s31/esp32s31-core-function-board/configs"
if Path(args.profile).name != args.profile or not (profiles / args.profile / "defconfig").is_file():
    parser.error("--profile must name an existing S31 board config")
sha = lambda p: hashlib.sha256(p.read_bytes()).hexdigest()
protected = [nuttx / ".config", nuttx / "include/nuttx/config.h", nuttx / "include/nuttx/version.h"]
for profile in ("esp32s31-korvo-camera-touch-sd", "esp32s31-xts-flat-adc"):
    protected += [root / "openvela-dev/out" / profile / name for name in
                  (".config", "include/nuttx/config.h", "nuttx.bin", "appfs.img", "compile_commands.json")]
before = {str(p): sha(p) for p in protected if p.is_file()}
output.mkdir(parents=True)
try:
    env, host_report = prepare(root, output / "host-environment",
                               copy_source=not args.workspace_source)
except (RuntimeError, OSError, subprocess.SubprocessError) as error:
    parser.exit(1, f"Host dependency preparation failed: {error}\n")
(output / "toolchain-export.log").write_bytes(
    (output / "host-environment/toolchain-export.log").read_bytes())
snapshot = output / "source"
vela = snapshot / "openvela-dev"
vela.mkdir(parents=True)
copies = []
for name in ("nuttx", "apps", "external", "frameworks", "packages", "tests", "vendor", "build"):
    src, dest = root / "openvela-dev" / name, vela / name
    if args.workspace_source:
        # CMake's preserve mode creates its own headers/Kconfig outputs.  Keep
        # this sharing explicit; it is neither an immutable source snapshot
        # nor a standalone source package.
        dest.symlink_to(src, target_is_directory=True)
    else:
        # Keep directories such as etc/init.d; exclude only generated files.
        # curl's docs/cmdline-opts/*.d are source inputs for CLI help.
        command = ["rsync", "-a", "--exclude=.git", "--exclude=__pycache__",
                   "--include=*/", "--exclude=*.o", "--exclude=*.o.d", "--exclude=*.obj.d"]
        if name == "nuttx":
            command += ["--exclude=/.config", "--exclude=/include/nuttx/config.h", "--exclude=/include/nuttx/version.h",
                        "--exclude=/nuttx", "--exclude=/nuttx.bin", "--exclude=/nuttx.hex", "--exclude=/nuttx.map"]
        command += [str(src) + "/", str(dest) + "/"]
        subprocess.run(command, check=True)
        copies.append(command)
(vela / "prebuilts").symlink_to(root / "openvela-dev/prebuilts", target_is_directory=True)
(snapshot / "s31-reference").symlink_to(root / "s31-reference", target_is_directory=True)
build = output / "build"
configured_vela = root / "openvela-dev" if args.workspace_source else vela
configure = ["cmake", "-S", str(configured_vela / "nuttx"), "-B", str(build), "-G", "Ninja",
             "-DBOARD_CONFIG=esp32s31-core-function-board:" + args.profile,
             "-DNUTTX_APPS_DIR=" + str(configured_vela / "apps"),
             "-DPython3_EXECUTABLE=" + str(root / "s31-reference/.venv-nuttx/bin/python3"),
             "-DFETCHCONTENT_FULLY_DISCONNECTED=ON", "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
             "-DMAKE_PARALLELISM=-j" + str(args.jobs)]
if args.workspace_source:
    configure.append("-DNUTTX_PRESERVE_SOURCE=ON")
compile_cmd = ["cmake", "--build", str(build), "--parallel", str(args.jobs)]
report = {"scope": ("Fresh-output whole image, shared maintained source and existing locked dependencies; no historical build required"
                    if args.workspace_source else "Fresh-output whole image, local source snapshot and existing locked dependencies; no board operations"),
          "copies": copies, "configure": configure, "build": compile_cmd,
          "ninja_jobs": args.jobs, "external_make_jobs": args.jobs,
          "profile": args.profile, "profile_defaults": True,
          "workspace_source": args.workspace_source,
          "source_preservation_requested": args.workspace_source,
          "host_library_directory": host_report["library_directory"],
          "host_environment_receipt": str(output / "host-environment/receipt.json"),
          "host_environment_sha256": dependency_sha(output / "host-environment/receipt.json"),
          "protected_before": before, "script_sha256": sha(Path(__file__))}
result = 0
try:
    for command, name in ((configure, "configure.log"), (compile_cmd, "build.log")):
        print(name, flush=True)
        with (output / name).open("x") as log:
            result = subprocess.run(command, env=env, stdout=log, stderr=subprocess.STDOUT).returncode
        if result:
            break
finally:
    report["returncode"] = result
    report["protected_after"] = {str(p): sha(p) for p in protected if p.is_file()}
    report["protected_unchanged"] = before == report["protected_after"]
    report["artifacts"] = {str(p): {"sha256": sha(p), "bytes": p.stat().st_size}
                           for p in (build / "nuttx", build / "nuttx.bin", build / ".config", build / "appfs.img") if p.is_file()}
    (output / "receipt.json").write_text(json.dumps(report, indent=2) + "\n")
print(json.dumps({"returncode": result, "protected_unchanged": report["protected_unchanged"],
                  "receipt": str(output / "receipt.json")}))
raise SystemExit(result if result else (0 if report["protected_unchanged"] else 1))
