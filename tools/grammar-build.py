"""Generate, check, or compile parsers through a temporary working tree."""

import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

GENERATED = [
    "parser.c",
    "grammar.json",
    "node-types.json",
    "tree_sitter/alloc.h",
    "tree_sitter/array.h",
    "tree_sitter/parser.h",
]


def main() -> None:
    operation = sys.argv[1]
    cli = str(Path(sys.argv[2]).resolve())
    source = (
        Path(os.environ["BUILD_WORKSPACE_DIRECTORY"]) if operation == "generate" else Path.cwd()
    )
    with tempfile.TemporaryDirectory(prefix="gcsl-grammar-") as temporary:
        work = Path(temporary)
        shutil.copy2(source / "grammar.js", work)
        shutil.copy2(source / "tree-sitter.json", work)
        env = dict(os.environ, HOME=str(work), XDG_CACHE_HOME=str(work / "cache"))
        if operation in {"check", "generate"}:
            subprocess.run(
                [cli, "generate", "--abi", "15", "--js-runtime", "native"],
                cwd=work,
                env=env,
                check=True,
            )
            changed = [
                name
                for name in GENERATED
                if (source / "src" / name).read_bytes() != (work / "src" / name).read_bytes()
            ]
            if operation == "generate":
                for name in changed:
                    shutil.copyfile(work / "src" / name, source / "src" / name)
                print("Updated " + str(len(changed)) + " generated files")
            elif changed:
                raise SystemExit("Regenerate committed parser files: " + ", ".join(changed))
        elif operation == "wasm":
            shutil.copytree(source / "src", work / "src")
            env["TREE_SITTER_WASI_SDK_PATH"] = str(Path(sys.argv[3]).resolve().parent.parent)
            env["TREE_SITTER_BINARYEN_PATH"] = str(Path(sys.argv[4]).resolve().parent.parent)
            output = str(Path(sys.argv[5]).resolve())
            subprocess.run(
                [cli, "build", "--wasm", "--output", output, str(work)], env=env, check=True
            )
        else:
            raise SystemExit("Unknown grammar build operation: " + operation)


if __name__ == "__main__":
    main()
