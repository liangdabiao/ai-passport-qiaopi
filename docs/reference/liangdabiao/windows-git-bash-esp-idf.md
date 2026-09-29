<p align="right">
  <a href="windows-git-bash-esp-idf.zh_CN.md">简体中文</a> · <strong>English</strong>
</p>

# Building ESP-IDF Firmware from Git Bash on Windows

Recorded while building the [Qiaopi Quiz app](qiaopi-quiz/README.md)
from a Git Bash (MSYS2) shell on Windows with ESP-IDF 5.5.3. Two things block you
outright, one of them is not fixable from inside the shell, and one pre-existing
part of the test suite genuinely cannot run there.

This is not a recommendation to work this way. The supported setup is the
official ESP-IDF installer with `cmd` or PowerShell, as described in
`docs/development/engineering/environment-setup.md`. What follows is what to do
when you are already in a POSIX shell — for instance because your tooling or your
AI coding agent lives there — and want the firmware gate to run.

## Blocker 1: ESP-IDF refuses to start when `MSYSTEM` is set

ESP-IDF 5.5.3 detects an MSYS shell and stops. In `tools/idf.py`:

```python
if 'MSYSTEM' in os.environ:
    print_warning('MSys/Mingw is no longer supported. ...')
    # ...and main() is never called
```

The warning is misleading: it is not a warning. `main()` is skipped, so `idf.py`
prints one line and exits successfully having done nothing. Separately,
`tools/idf_tools.py` has the same test in its `__main__` block and calls `fatal()`
there, which exits with status 1. The practical result is that `idf.py build`
cannot run at all.

The obvious fix — unset the variable — does not work, and it is worth
understanding why. The MSYS runtime re-injects `MSYSTEM` into every **native
Windows** child process it launches. Unsetting it in the shell does not stop
injection at the boundary:

```console
$ unset MSYSTEM
$ echo "'${MSYSTEM:-<unset>}'"
'<unset>'
$ python.exe -c "import os; print(os.environ.get('MSYSTEM'))"
MINGW64
```

The only two ways out are to run from `cmd`/PowerShell, where the variable is
absent, or to patch the local ESP-IDF copy.

**If you patch, patch the local toolchain, never the repository.** The
repository's checks cannot see `D:\esp\...`, so a patched IDF is an untracked
divergence that the next person will not know about. Leave a comment at the patch
site saying what upstream does, why it is unusable here, and that this is a local
modification — then write it down somewhere your team will read, because a
toolchain patch that nobody remembers is a bug report waiting to happen. In this
case both patches keep the upstream warning and continue into `main()`.

One more path detail: `idf.py` can resolve to an `idf-exe` wrapper instead of the
real script. Put ESP-IDF's own `tools/` directory ahead of it on `PATH` so the
real `idf.py` wins.

## Blocker 2: the IDF virtualenv and the interpreter on `PATH`

The tools installer builds a Python virtualenv named after the interpreter it
used — here `idf5.5_py3.12_env`, since ESP-IDF 5.5.3 uses Python 3.12. If
`python3` on your `PATH` is a different minor version, activation looks for a
virtualenv that does not exist:

```text
idf5.5_py3.13_env ... not found
```

The cleanest fix is not to fight it: put a one-line forwarder at the front of
`PATH` that `exec`s the virtualenv's own interpreter, so that `python` and
`python3` both mean "the interpreter IDF was installed with".

```sh
#!/bin/sh
exec "D:/esp/.espressif/python_env/idf5.5_py3.12_env/Scripts/python.exe" "$@"
```

`idf.py` will still warn that the interpreter "is not from installed venv" when
you launch it through a wrapper. When the wrapper points at that same
interpreter it is cosmetic; verify with `idf.py --version` before trusting it.

With both blockers handled, `idf.py --version` reports 5.5.3 and the firmware
gate runs normally. The gate's cold build is roughly 2,000 Ninja steps, so give
it several minutes and run it in the background rather than watching it.

## Getting a host C compiler without MSVC or MinGW

The static gate compiles host tests with `${CC:-cc}`. On a machine with neither
MSVC nor MinGW that fails before it starts. `zig cc` is a workable substitute and
travels as a Python wheel (`ziglang`), which is convenient when package
downloads from GitHub are slow but a PyPI mirror is fast.

Wrap it in a one-line script and point the gate at it:

```sh
#!/bin/sh
exec "/path/to/zig.exe" cc "$@"
```

```bash
CC=/path/to/cc ./tools/validate.sh --static
```

Note that `zig cc` compiles by launching its own sub-compilers, so a sandboxed
environment that restricts child processes may need this step to run
unsandboxed. That is an environment permission, not a project problem.

## What still does not work: the demo runtime tests

Four pre-existing tests — the audio, low-power, BLE, and Wi-Fi demo runtime
tests — cannot be linked with this toolchain, and it is worth being precise about
why rather than papering over it.

They include stub headers that **declare** LVGL and `ui_pixel` functions without
defining them, and then call only a small part of the demo module under test. The
remaining functions in that module reference the stubs. The tests rely on the
linker discarding those unreferenced functions, which requires GNU
`--gc-sections` semantics. `zig cc` always uses `lld`, and its PE/COFF mode does
not implement that flag:

```console
$ cc ... -Wl,--gc-sections -o t          # flag accepted, silently ignored
lld-link: error: undefined symbol: ui_pixel_screen_create
$ cc ... -Wl,/OPT:REF -o t               # the MSVC spelling
error: unsupported linker arg: /OPT:REF
```

Switching the target with `-target x86_64-windows-gnu` does not change the
linker, so it does not help. There is no flag combination that fixes this.

The right response is to verify these tests where a GNU toolchain exists — Linux
CI, which is what the repository's workflow uses — and **not** to weaken the
stubs, add `--allow-undefined`, or skip the tests to get a green local run. The
same applies to the three Python tests that depend on creating symlinks
(`test_check_repo`, `test_archive_firmware`, `test_install_passport_skills`):
without Developer Mode or administrator rights those fail on Windows for reasons
that have nothing to do with the code.

Confirm the diagnosis before believing it. The check that makes this credible is
exporting a pristine copy of the commit and reproducing the identical failure
there:

```bash
mkdir -p /tmp/baseline && git archive HEAD | tar -x -C /tmp/baseline
cd /tmp/baseline
CC=/path/to/cc ACTIONLINT_BIN=/path/to/actionlint ./tools/validate.sh --static
```

If the failures and their counts match the working tree, they are environmental.
That single command is the difference between "I could not run the tests" and "I
know why these tests cannot run here".

Because the gate stops at the first failure, on this host `--static` ends early.
Run the remaining checks individually so the rest of the suite still gets
exercised:

```bash
python3 tools/check_repo.py
actionlint -color .github/workflows/*.yml
for t in test_deep_sleep_contract test_verify_firmware; do python3 tests/$t.py; done
```

## Check list

- Confirm whether `MSYSTEM` is set before blaming anything else; it changes
  `idf.py`'s behavior from "runs" to "exits silently".
- Prefer `cmd`/PowerShell over patching ESP-IDF. If you patch, patch only the
  local copy and write the patch down.
- Make `python`/`python3` resolve to the interpreter the IDF virtualenv was built
  with.
- Put IDF's `tools/` directory ahead of any `idf-exe` wrapper on `PATH`.
- Use `zig cc` behind a `CC` wrapper when no other host compiler exists.
- Accept that the stub-based demo runtime tests need a GNU linker, verify them in
  CI, and never weaken them to get a local pass.
- Reproduce any suspected environmental failure on a pristine export before
  reporting it as one.

## Related documents

- [Qiaopi Quiz app](qiaopi-quiz/README.md) — the build these notes
  come from, including its verification results.
- [Keeping application logic on the host](host-testable-app-logic.md) — the tests
  that do run here, and why they are portable.
- `docs/development/engineering/environment-setup.md` — the supported setup path.
- `docs/development/engineering/build-and-test.md` — what the gate checks and in
  what order.
