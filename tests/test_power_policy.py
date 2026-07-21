"""Compile, execute, and measure the production power policy model."""

from __future__ import annotations

import os
import re
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
APP_DIR = ROOT / "openvela_app" / "smart_band"
INCLUDE_DIR = APP_DIR / "include"
TEST_SOURCE = Path(__file__).with_name("power_policy_test.c")
PRODUCTION_SOURCE = APP_DIR / "logic" / "power_policy.c"
MINIMUM_LINE_COVERAGE = 90.0


def find_visual_studio_environment() -> Path | None:
    if os.name != "nt":
        return None
    program_files = os.environ.get("ProgramFiles(x86)", r"C:\Program Files (x86)")
    vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
    if not vswhere.is_file():
        return None
    result = subprocess.run(
        [str(vswhere), "-latest", "-products", "*", "-requires",
         "Microsoft.VisualStudio.Component.VC.Tools.x86.x64", "-property",
         "installationPath"],
        check=True, capture_output=True, text=True,
    )
    installation = result.stdout.strip()
    if not installation:
        return None
    environment = Path(installation) / "VC" / "Auxiliary" / "Build" / "vcvars64.bat"
    return environment if environment.is_file() else None


def compiler_family(compiler: str) -> str:
    name = Path(compiler).name.lower()
    if name in {"cl", "cl.exe"}:
        return "msvc"
    if "clang" in name:
        return "clang"
    return "gcc"


def expose_compiler_tools(compiler: str, family: str) -> None:
    if family != "msvc":
        compiler_dir = str(Path(compiler).resolve().parent)
        os.environ["PATH"] = compiler_dir + os.pathsep + os.environ.get("PATH", "")


def find_compiler() -> tuple[str, str, Path | None]:
    requested = os.environ.get("CC")
    if requested:
        resolved = shutil.which(requested)
        if resolved:
            family = compiler_family(resolved)
            expose_compiler_tools(resolved, family)
            environment = find_visual_studio_environment() if family == "msvc" else None
            return resolved, family, environment
        if Path(requested).name.lower() in {"cl", "cl.exe"}:
            environment = find_visual_studio_environment()
            if environment is not None:
                return "cl", "msvc", environment
        raise RuntimeError(f"requested C compiler is unavailable: {requested}")

    for candidate in ["gcc", "clang", "cc", "cl"]:
        resolved = shutil.which(candidate)
        if resolved:
            family = compiler_family(resolved)
            expose_compiler_tools(resolved, family)
            environment = find_visual_studio_environment() if family == "msvc" else None
            return resolved, family, environment
    environment = find_visual_studio_environment()
    if environment is not None:
        return "cl", "msvc", environment
    raise RuntimeError("no C11 compiler found; install GCC/Clang/MSVC or set CC")


def run_command(command: list[str], compiler_environment: Path | None,
                temp: Path) -> None:
    print("running:", " ".join(command), flush=True)
    if compiler_environment is None:
        subprocess.run(command, cwd=ROOT, check=True)
        return
    batch = temp / "compile_power_policy_test.bat"
    batch.write_text(
        "@echo off\r\n"
        f'call "{compiler_environment}" >nul || exit /b 1\r\n'
        f"{subprocess.list2cmdline(command)}\r\n",
        encoding="utf-8",
    )
    subprocess.run(["cmd.exe", "/d", "/c", str(batch)], cwd=ROOT, check=True)


def strict_build_and_run(compiler: str, family: str,
                         compiler_environment: Path | None, temp: Path) -> None:
    output = temp / ("power_policy_test.exe" if os.name == "nt" else "power_policy_test")
    sources = [str(TEST_SOURCE), str(PRODUCTION_SOURCE)]
    if family == "msvc":
        command = [compiler, "/nologo", "/std:c11", "/W4", "/WX",
                   f"/I{INCLUDE_DIR}", *sources, f"/Fo{temp}{os.sep}",
                   f"/Fe:{output}"]
    else:
        command = [compiler, "-std=c11", "-Wall", "-Wextra", "-Werror",
                   "-pedantic", f"-I{INCLUDE_DIR}", *sources, "-o", str(output)]
    run_command(command, compiler_environment, temp)
    subprocess.run([str(output)], cwd=ROOT, check=True)


def parse_gcov_coverage(output: str) -> float:
    matches = re.findall(r"Lines executed:([0-9]+(?:\.[0-9]+)?)%", output)
    if not matches:
        raise RuntimeError("gcov did not report production line coverage")
    return float(matches[-1])


def gcc_coverage(compiler: str, temp: Path) -> float:
    gcov = shutil.which("gcov")
    if gcov is None:
        sibling = Path(compiler).with_name("gcov.exe" if os.name == "nt" else "gcov")
        gcov = str(sibling) if sibling.is_file() else None
    if gcov is None:
        raise RuntimeError("GCC selected but matching gcov is unavailable")

    output = temp / ("power_policy_coverage.exe" if os.name == "nt" else "power_policy_coverage")
    object_file = temp / "power_policy.o"
    test_object = temp / "power_policy_test.o"
    common = ["-std=c11", "-O0", "--coverage", "-Wall", "-Wextra", "-Werror",
              "-pedantic", f"-I{INCLUDE_DIR}"]
    subprocess.run([compiler, *common, "-c", str(PRODUCTION_SOURCE), "-o",
                    str(object_file)], cwd=ROOT, check=True)
    subprocess.run([compiler, *common, "-c", str(TEST_SOURCE), "-o",
                    str(test_object)], cwd=ROOT, check=True)
    subprocess.run([compiler, "--coverage", str(object_file), str(test_object),
                    "-o", str(output)], cwd=ROOT, check=True)
    subprocess.run([str(output)], cwd=ROOT, check=True)
    report = subprocess.run(
        [gcov, "-b", "-c", str(object_file)], cwd=temp, check=True,
        capture_output=True, text=True,
    )
    print(report.stdout, end="")
    return parse_gcov_coverage(report.stdout)


def clang_coverage(compiler: str, temp: Path) -> float:
    llvm_profdata = shutil.which("llvm-profdata")
    llvm_cov = shutil.which("llvm-cov")
    if llvm_profdata is None or llvm_cov is None:
        raise RuntimeError("Clang selected but llvm-profdata/llvm-cov are unavailable")

    output = temp / ("power_policy_coverage.exe" if os.name == "nt" else "power_policy_coverage")
    profile = temp / "power_policy.profraw"
    merged = temp / "power_policy.profdata"
    command = [compiler, "-std=c11", "-O0", "-fprofile-instr-generate",
               "-fcoverage-mapping", f"-I{INCLUDE_DIR}", str(TEST_SOURCE),
               str(PRODUCTION_SOURCE), "-o", str(output)]
    subprocess.run(command, cwd=ROOT, check=True)
    environment = os.environ.copy()
    environment["LLVM_PROFILE_FILE"] = str(profile)
    subprocess.run([str(output)], cwd=ROOT, env=environment, check=True)
    subprocess.run([llvm_profdata, "merge", "-sparse", str(profile), "-o",
                    str(merged)], cwd=ROOT, check=True)
    report = subprocess.run(
        [llvm_cov, "report", str(output), f"-instr-profile={merged}",
         str(PRODUCTION_SOURCE)], cwd=ROOT, check=True,
        capture_output=True, text=True,
    )
    print(report.stdout, end="")
    line = report.stdout.strip().splitlines()[-1].split()
    if len(line) < 10 or not line[-1].endswith("%"):
        raise RuntimeError("llvm-cov did not report production line coverage")
    return float(line[-1].rstrip("%"))


def compile_run_and_cover() -> None:
    compiler, family, compiler_environment = find_compiler()
    with tempfile.TemporaryDirectory(prefix="smart-band-power-policy-") as directory:
        temp = Path(directory)
        strict_build_and_run(compiler, family, compiler_environment, temp)
        if family == "gcc":
            coverage = gcc_coverage(compiler, temp)
        elif family == "clang":
            coverage = clang_coverage(compiler, temp)
        else:
            print(
                "power_policy.c coverage deferred to the unified GCC coverage gate",
                flush=True,
            )
            return
        print(f"power_policy.c line coverage: {coverage:.2f}%")
        if coverage < MINIMUM_LINE_COVERAGE:
            raise RuntimeError(
                f"line coverage {coverage:.2f}% is below {MINIMUM_LINE_COVERAGE:.2f}%"
            )


if __name__ == "__main__":
    try:
        compile_run_and_cover()
    except (RuntimeError, subprocess.CalledProcessError) as error:
        print(f"power policy host test failed: {error}", file=sys.stderr)
        raise SystemExit(1) from error
