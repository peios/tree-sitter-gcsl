"""Generate, check, or compile parsers through a temporary working tree.

Each operation reads the grammar from the working directory:

- `generate CLI OUT` writes the generated parser files to the directory OUT,
  laid out as `src/` holds them;
- `check CLI` fails if a committed generated file differs from what the CLI
  generates;
- `wasm CLI CLANG WASM_OPT OUT` compiles the Wasm parser to the file OUT.
"""

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
    source = Path.cwd()
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
            if operation == "generate":
                output = Path(sys.argv[3])
                for name in GENERATED:
                    (output / name).parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(work / "src" / name, output / name)
                return
            changed = [
                name
                for name in GENERATED
                if (source / "src" / name).read_bytes() != (work / "src" / name).read_bytes()
            ]
            if changed:
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
