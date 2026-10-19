"""Validate parser artifacts and prepare debug information and provenance."""

import hashlib
import json
import re
import shutil
import subprocess
import sys
from pathlib import Path


def output(*command: str) -> str:
    return subprocess.check_output(command, text=True)


def main() -> None:
    manifest = json.loads(Path(sys.argv[1]).read_text())
    root = Path(sys.argv[2]).resolve()
    parser = Path(manifest["parser"])
    notes = output("readelf", "-nW", str(parser))
    dynamic = output("readelf", "-dW", str(parser))
    program = output("readelf", "-lW", str(parser))
    symbols = output("nm", "-D", "--defined-only", str(parser))
    if "tree_sitter_gcsl" not in symbols:
        raise ValueError("parser has no public language entry point")
    if "GNU_RELRO" not in program or "BIND_NOW" not in dynamic:
        raise ValueError("parser lacks immediate binding or RELRO")
    if re.search(r"GNU_STACK.*RWE", program):
        raise ValueError("parser requests an executable stack")
    if manifest["native"] and not ("IBT" in notes and "SHSTK" in notes):
        raise ValueError("native parser lacks IBT and SHSTK properties")
    library = root / "usr/lib/x86_64-linux-peios/tree-sitter/gcsl.so"
    debug = root / "usr/lib/debug/usr/lib/x86_64-linux-peios/tree-sitter/gcsl.so.debug"
    library.parent.mkdir(parents=True)
    debug.parent.mkdir(parents=True)
    subprocess.run(["objcopy", "--only-keep-debug", str(parser), str(debug)], check=True)
    subprocess.run(["objcopy", "--strip-debug", str(parser), str(library)], check=True)
    subprocess.run(["objcopy", "--add-gnu-debuglink=" + str(debug), str(library)], check=True)
    wasm = root / "usr/share/tree-sitter/grammars/gcsl.wasm"
    wasm.parent.mkdir(parents=True)
    shutil.copyfile(manifest["wasm"], wasm)
    if wasm.read_bytes()[:8] != b"\0asm\x01\0\0\0":
        raise ValueError("invalid WebAssembly module header")
    sources = root / "usr/src/debug/dev.peios.tree-sitter-gcsl"
    for entry in manifest["sources"]:
        relative = Path(entry["relative"])
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError("invalid source path")
        destination = sources / relative
        destination.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(entry["path"], destination)
    licences = root / "licenses/parser"
    licences.mkdir(parents=True)
    shutil.copyfile(manifest["runtime_license"], licences / "tree-sitter-MIT.txt")
    provenance = {
        "profile": "peios-native" if manifest["native"] else "linux-development",
        "signed": False,
        "release_ready": False,
        "licences": {"parser": "MIT"},
        "generator": "tree-sitter 0.27.1, ABI 15",
        "wasm_toolchain": "WASI SDK 34.0, Binaryen 132",
        "compiler": output("cc", "--version").splitlines()[0],
        "needed": re.findall(r"Shared library: \[(.*?)\]", dynamic),
        "artifact_sha256": {
            "parser": hashlib.sha256(parser.read_bytes()).hexdigest(),
            "wasm": hashlib.sha256(wasm.read_bytes()).hexdigest(),
        },
    }
    (root / "build.json").write_text(json.dumps(provenance, sort_keys=True, indent=4) + "\n")


if __name__ == "__main__":
    main()
