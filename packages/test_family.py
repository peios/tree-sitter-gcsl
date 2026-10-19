"""Check the actual emitted package family and parser payload contracts."""

import json
import tomllib
import unittest
from pathlib import Path


class FamilyTests(unittest.TestCase):
    def test_payloads(self) -> None:
        root = Path("packages.store")
        index = json.loads((root / "index.json").read_text())
        self.assertFalse(index["release_ready"])
        packages = {p["name"].removeprefix("dev.peios."): p for p in index["packages"]}
        self.assertEqual(
            set(packages),
            {
                "tree-sitter-gcsl" + suffix
                for suffix in ["", "-wasm", "-debuginfo", "-debugsource", "-source"]
            },
        )
        payloads = {}
        for name, package in packages.items():
            self.assertEqual(package["version"], "0.1.0-1")
            self.assertFalse(package["release_ready"])
            payloads[name] = tomllib.loads((root / package["manifest"]).read_text())["files"]
        self.assertTrue(
            payloads["tree-sitter-gcsl"]["usr/lib/x86_64-linux-peios/tree-sitter/gcsl.so"]["exec"]
        )
        self.assertIn("usr/share/tree-sitter/grammars/gcsl.wasm", payloads["tree-sitter-gcsl-wasm"])
        for name in ["tree-sitter-gcsl", "tree-sitter-gcsl-wasm"]:
            for query in ["highlights", "indents", "folds", "textobjects"]:
                self.assertIn(
                    "usr/share/tree-sitter/queries/gcsl/" + query + ".scm", payloads[name]
                )
        self.assertIn(
            "usr/lib/debug/usr/lib/x86_64-linux-peios/tree-sitter/gcsl.so.debug",
            payloads["tree-sitter-gcsl-debuginfo"],
        )
        self.assertIn(
            "usr/src/debug/dev.peios.tree-sitter-gcsl/src/scanner.c",
            payloads["tree-sitter-gcsl-debugsource"],
        )
        source = payloads["tree-sitter-gcsl-source"]
        for name in ["grammar.js", "src/parser.c", "src/scanner.c", "MODULE.bazel.lock", "LICENSE"]:
            self.assertIn("usr/src/tree-sitter-gcsl/" + name, source)
        self.assertFalse(
            any(
                ".git" in Path(name).parts
                or any(part.startswith("bazel-") for part in Path(name).parts[:-1])
                or name.endswith(".wasm")
                for name in source
            )
        )


if __name__ == "__main__":
    unittest.main()
