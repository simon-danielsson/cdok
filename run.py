#!/usr/bin/env python3

import subprocess, sys, os
from pathlib import Path
from datetime import datetime, timedelta
from dataclasses import dataclass
from enum import Enum
import platform

ROOT = Path(__file__).parent.resolve()
SRC_DIR = Path(f"{ROOT}/src")
PROJ_NAME = ROOT.name
PROJ_REPO = f"https://github.com/simon-danielsson/{PROJ_NAME}"
AUTH = "Simon Danielsson"
AUTH_CONT = "contact@simondanielsson.se"
C_STD = "gnu23"

AUTO_RUN = False
PRINT_COMPILE_DETAILS = True

C_FLAGS_DEBUG = [
        "-O0",
        "-DDEBUG",
        "-fsanitize=address",
        "-fsanitize=undefined",
        "-fno-omit-frame-pointer",
        "-Wall",
        "-Wextra",
        "-Wpedantic",
        "-Wshadow",
        "-Werror=format-security",
        ]

C_FLAGS_RELEASE = ["-flto", "-O2", "-DNDEBUG", "-Wextra"]

BLD = "\033[1m"
RST = "\x1b[0m"

# classes & types -------------------------------------------------------------

class BuildType(Enum):
    Release = "release"
    Debug = "debug"
    Test = "test"

@dataclass
class Args:
    build: BuildType = BuildType.Debug
    help: bool = False
    prog: str = ""

@dataclass
class CmdExec:
    process: subprocess.CompletedProcess[str]
    exec_time: timedelta

# cmd exec --------------------------------------------------------------------

def run_cmd(cmd) -> CmdExec:
    start = datetime.now()
    process = subprocess.run(cmd, capture_output=True, text=True)
    end = datetime.now()
    exec_time = end - start
    return CmdExec(exec_time=exec_time, process=process)

# git & env -------------------------------------------------------------------

def get_git_vers() -> str:
    cmd = ["git", "describe", "--tags", "--abbrev=0"]
    result = run_cmd(cmd)
    if result.process.returncode != 0:
        return "v0.0.0"
    return result.process.stdout.strip()

def get_git_hash(short: bool) -> str:
    cmd = ["git", "rev-parse", "HEAD"]
    if short:
        cmd.insert(2, "--short")
    result = run_cmd(cmd)
    if result.process.returncode != 0:
        return "0".zfill(7)
    return result.process.stdout.strip()

GIT_V = get_git_vers()
GIT_HASH_SH = get_git_hash(True)
GIT_HASH = get_git_hash(False)

ENV_FLAGS = [
        f'-DENV_GITHASH="{GIT_HASH}"',
        f'-DENV_GITTAG="{GIT_V}"',
        f'-DENV_NAME="{PROJ_NAME}"',
        f'-DENV_AUTHOR="{AUTH}"',
        f'-DENV_CONTACT="{AUTH_CONT}"',
        f'-DENV_REPO="{PROJ_REPO}"',
        f"-std={C_STD}",
        ]

# build -----------------------------------------------------------------------

def collect_src_files(src: Path) -> list[str]:
    return [f"{path}" for path in src.rglob("*.c")]

def build(a: Args) -> None:
    build_dir = Path(f"{ROOT}/build/{a.build.value}")
    bin_name = f"{PROJ_NAME}_{a.build.value}_{GIT_V}_{GIT_HASH_SH}"
    c_flags: list[str] = ENV_FLAGS

    match a.build:
        case BuildType.Debug:
            c_flags = c_flags + C_FLAGS_DEBUG
        case BuildType.Release:
            c_flags = c_flags + C_FLAGS_RELEASE
        case BuildType.Test:
            c_flags.append("-DTEST")
            c_flags = c_flags + C_FLAGS_DEBUG

    os.makedirs(build_dir, exist_ok=True)

    build_cmd = c_flags + collect_src_files(SRC_DIR) + ["-o", f"{build_dir}/{bin_name}"]

    try:
        compiler = "clang"
        output = run_cmd([compiler] + build_cmd)
    except FileNotFoundError:
        compiler = "gcc"
        output = run_cmd([compiler] + build_cmd)

    if PRINT_COMPILE_DETAILS:
        print(f"{a.build.value} via " f"{compiler} ({C_STD}) {output.exec_time}")

    if AUTO_RUN:
        if output.process.returncode != 0:
            print(output.process.stderr)
            sys.exit(output.process.returncode)

        env = os.environ.copy()
        if platform.system() == "Darwin":
            env["MallocNanoZone"] = "0"
        exe_path = (build_dir / bin_name).resolve()
        os.execvpe(str(exe_path), [str(exe_path)], env)

# main ------------------------------------------------------------------------

def help() -> None:
    print(
            f"{BLD}run release{RST}\n"
            f"-> ./build/release\n"
            f"{BLD}run debug{RST}\n"
            f"-> ./build/debug\n"
            f"{BLD}run test{RST}\n"
            f"-> ./build/test"
            )

def get_args() -> Args:
    a: Args = Args()
    a.prog = sys.argv[0].rsplit("/")[-1]
    for arg in sys.argv:
        match arg:
            case r if r.startswith("r"):
                a.build = BuildType.Release
            case d if d.startswith("d"):
                a.build = BuildType.Debug
            case t if t.startswith("t"):
                a.build = BuildType.Test
            case h if h.startswith("h"):
                a.help = True
    return a

def main():
    a: Args = get_args()
    if a.help:
        help()
        return
    build(a)

if __name__ == "__main__":
    main()
