"""Check maintained code; generated parser files retain their generator's format."""

import os
import shutil
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent


def tool(name: str, version: str) -> str:
    directory = os.environ.get("PEIOS_STYLE_BIN")
    path = str(Path(directory) / name) if directory else shutil.which(name)
    if path is None:
        raise SystemExit("Missing style tool: " + name)
    found = subprocess.check_output([path, "--version"], text=True)
    if version not in [line.strip() for line in found.splitlines()]:
        raise SystemExit("Unexpected " + name + " version: " + found.strip())
    return path


def run(*args: str) -> None:
    subprocess.run(args, cwd=ROOT, check=True)


def main() -> None:
    python = [
        str(p.relative_to(ROOT))
        for folder in ["tools", "packages"]
        for p in (ROOT / folder).glob("*.py")
    ]
    ruff = tool("ruff", "ruff 0.17.0")
    run(ruff, "check", "--no-cache", "--config", "tools/ruff.toml", *python)
    run(ruff, "format", "--check", "--no-cache", "--config", "tools/ruff.toml", *python)
    c = ["src/scanner.c", *[str(p.relative_to(ROOT)) for p in (ROOT / "test").glob("*.c")]]
    run(
        tool("clang-format", "clang-format version 18.1.8"),
        "--dry-run",
        "--Werror",
        "--style=file:tools/clang-format.yaml",
        *c,
    )
    starlark = ["BUILD.bazel", "MODULE.bazel"] + [
        str(p.relative_to(ROOT))
        for folder in ["build", "packages", "vendor/ppk"]
        for p in (ROOT / folder).iterdir()
        if p.suffix == ".bzl" or p.name.endswith("BUILD.bazel")
    ]
    run(
        tool("buildifier", "buildifier version: 10.1.0"),
        "-mode=check",
        "-lint=warn",
        "-warnings=load,unused-variable,function-docstring",
        *starlark,
    )
    shell = ["bazel", "tools/check", "tools/check-docs", "tools/offline-build"]
    run(tool("shellcheck", "version: 0.11.0"), *shell)
    run(tool("shfmt", "v3.14.1"), "-d", "-i", "4", *shell)
    data = ["grammar.js", "tree-sitter.json"] + [
        str(p.relative_to(ROOT))
        for folder in ["packages", ".github", "tools"]
        for p in (ROOT / folder).rglob("*")
        if p.suffix in {".json", ".yml"}
    ]
    run(tool("prettier", "3.9.10"), "--config", "tools/prettier.json", "--check", *data)
    run(
        "rustup",
        "run",
        "1.98.1",
        "rustfmt",
        "--check",
        "--config-path",
        "tools/rustfmt.toml",
        "packages/check_blobs.rs",
    )


if __name__ == "__main__":
    main()
