"""Run upstream corpus and highlight tests against the declared parser artifact."""

import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> None:
    mode, cli, parser = sys.argv[1:]
    cli, parser = str(Path(cli).resolve()), str(Path(parser).resolve())
    source = Path.cwd()
    with tempfile.TemporaryDirectory(prefix="gcsl-tests-") as temporary:
        work = Path(temporary)
        for name in ["test", "queries", "src"]:
            shutil.copytree(source / name, work / name)
        shutil.copy2(source / "tree-sitter.json", work)
        config = work / "config.json"
        config.write_text(json.dumps({"parser-directories": []}))
        env = dict(os.environ, HOME=str(work), XDG_CACHE_HOME=str(work / "cache"))
        # Seed a private loader directory with the declared artifact. Tree-sitter's
        # corpus runner discovers languages independently of --lib-path.
        library_dir = work / "libraries"
        library_dir.mkdir()
        library = library_dir / ("gcsl.wasm" if mode == "wasm" else "gcsl.so")
        for path in (work / "src").rglob("*"):
            if path.is_file():
                os.utime(path, (0, 0))
        shutil.copyfile(parser, library)
        env.update(
            TREE_SITTER_LIBDIR=str(library_dir),
            CC="/unavailable-compiler",
            TREE_SITTER_WASI_SDK_PATH="/unavailable-toolchain",
            TREE_SITTER_BINARYEN_PATH="/unavailable-toolchain",
        )
        args = [cli, "test", "--config-path", str(config)]
        if mode == "wasm":
            args.append("--wasm")
        subprocess.run(args, cwd=work, env=env, check=True)


if __name__ == "__main__":
    main()
