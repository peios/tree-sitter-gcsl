"""Small declared-input checks using the documented Linux development tools."""

def _check_impl(ctx):
    launcher = ctx.actions.declare_file(ctx.label.name + ".sh")
    args = " ".join(['"$PWD/%s"' % f.short_path for f in ctx.files.arguments])
    ctx.actions.write(launcher, """#!/bin/sh
set -eu
cd "${RUNFILES_DIR:-$0.runfiles}/%s"
export PATH=/usr/bin:/bin
exec python3 %s %s %s
""" % (ctx.workspace_name, ctx.file.script.short_path, ctx.attr.mode, args), is_executable = True)
    return [DefaultInfo(executable = launcher, runfiles = ctx.runfiles(files = ctx.files.data + ctx.files.arguments + [ctx.file.script]))]

host_test = rule(
    implementation = _check_impl,
    test = True,
    attrs = {
        "script": attr.label(allow_single_file = True, mandatory = True),
        "mode": attr.string(default = ""),
        "arguments": attr.label_list(allow_files = True),
        "data": attr.label_list(allow_files = True),
    },
    doc = "Run a Python check with declared files and positional file arguments.",
)

host_binary = rule(
    implementation = _check_impl,
    executable = True,
    attrs = {
        "script": attr.label(allow_single_file = True, mandatory = True),
        "mode": attr.string(default = ""),
        "arguments": attr.label_list(allow_files = True),
        "data": attr.label_list(allow_files = True),
    },
    doc = "Run a repository maintenance command using declared tools.",
)
