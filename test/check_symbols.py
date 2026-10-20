#!/usr/bin/env python3
"""Symbol regression test for NickelTolinoShopRedirect.

Every libnickel symbol the hook relies on must exist in the 5.18 firmware:

- `.sym = "..."` (nh_hook): hooked through libnickel's GOT, so libnickel must
  either define it (D) or import it (U, e.g. QWebChannel::registerObject).
- `.name = "..."` (nh_dlsym): resolved with dlsym, so it must be defined (D).
- Caller allowlists (kStartUrlCallers / kSearchUrlCallers): names dladdr()
  returns, so they must be defined (D).

The reference (`libnickel-5.18.syms.gz`) lists the defined and imported `_Z`
dynamic symbols of `libnickel.so.1.0.0` from firmware 5.18.264769 (BuildID
bcfe1a2b7dd40f6c53bbb049d59e0e25119adc21), one "D name" / "U name" per line.
To update it for a new firmware, extract the new `libnickel.so.1.0.0` and run

    { llvm-nm -D --defined-only libnickel.so.1.0.0 | awk '{print "D " $NF}'; \\
      llvm-nm -D --undefined-only libnickel.so.1.0.0 | awk '{print "U " $NF}'; } \\
      | grep -E ' _Z' | sed 's/@.*//' | sort -u | gzip -9n > test/libnickel-5.18.syms.gz
"""

import gzip
import re
import sys
from pathlib import Path

REPO = Path(__file__).resolve().parent.parent
HOOK_SRC = REPO / "hook" / "src" / "shopredirect.cc"
REF = Path(__file__).resolve().parent / "libnickel-5.18.syms.gz"

# `\s*=` right after the field name excludes `.sym_new = "..."`.
HOOK_RE = re.compile(r'\.sym\s*=\s*"([^"]+)"')
DLSYM_RE = re.compile(r'\.name\s*=\s*"(_Z[^"]+)"')
CALLERS_RE = re.compile(r'static const char \*const k\w+Callers\[\] = \{(.*?)\};', re.S)


def main() -> int:
    src = HOOK_SRC.read_text(encoding="utf-8")
    defined, imported = set(), set()
    with gzip.open(REF, "rt", encoding="utf-8") as f:
        for line in f:
            kind, _, name = line.strip().partition(" ")
            (defined if kind == "D" else imported).add(name)

    hooks = HOOK_RE.findall(src)
    dlsyms = DLSYM_RE.findall(src)
    callers = [n for block in CALLERS_RE.findall(src) for n in re.findall(r'"(_Z[^"]+)"', block)]

    errors = []
    for s in hooks:
        if s not in defined and s not in imported:
            errors.append("hook symbol not in libnickel 5.18: %s" % s)
    for s in dlsyms:
        if s not in defined:
            errors.append("dlsym symbol not defined in libnickel 5.18: %s" % s)
    for s in callers:
        if s not in defined:
            errors.append("allowlisted caller not defined in libnickel 5.18: %s" % s)

    if not hooks or not dlsyms or not callers:
        errors.append("found no hooks/dlsyms/callers in %s - did the source layout change?" % HOOK_SRC.name)

    for e in errors:
        print("FAIL: " + e)
    if errors:
        return 1
    print("OK: %d hook(s), %d dlsym(s), %d caller(s) found in libnickel 5.18"
          % (len(hooks), len(dlsyms), len(callers)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
