"""Stage the declared native parser and portable WebAssembly parser."""

def _stage_impl(ctx):
    output = ctx.actions.declare_directory(ctx.label.name)
    manifest = ctx.actions.declare_file(ctx.label.name + ".inputs.json")
    ctx.actions.write(manifest, json.encode({
        "native": ctx.attr.native,
        "parser": ctx.file.parser.path,
        "wasm": ctx.file.wasm.path,
        "runtime_license": ctx.file.runtime_license.path,
        "sources": [{"path": f.path, "relative": f.short_path} for f in ctx.files.sources],
    }))
    ctx.actions.run(
        executable = "/usr/bin/python3",
        arguments = [ctx.file._driver.path, manifest.path, output.path],
        inputs = ctx.files.sources + [ctx.file.parser, ctx.file.wasm, ctx.file.runtime_license, ctx.file._driver, manifest],
        outputs = [output],
        env = {"PATH": "/usr/bin:/bin", "LANG": "C.UTF-8"},
        mnemonic = "GrammarPackageStage",
    )
    return [DefaultInfo(files = depset([output]))]

grammar_stage = rule(
    implementation = _stage_impl,
    doc = "Validate and split parser artifacts without including editor or build-tool binaries.",
    attrs = {
        "parser": attr.label(allow_single_file = True, mandatory = True),
        "wasm": attr.label(allow_single_file = True, mandatory = True),
        "runtime_license": attr.label(allow_single_file = True, mandatory = True),
        "sources": attr.label_list(allow_files = True),
        "native": attr.bool(),
        "_driver": attr.label(default = "//packages:stage.py", allow_single_file = True),
    },
)
