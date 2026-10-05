"""Prohibition grep-test: run_rehearsal.py must never name a forbidden
host command or execution sink in executable context.

Allowlist: module/class/function docstrings (triple-quoted blocks),
full-line comments (stripped line starts with '#'), and any line
carrying the marker HARNESS-DOC-ALLOWLIST. Anything else containing a
forbidden token fails the suite.
"""
import re
import unittest
from pathlib import Path

TARGET = Path(__file__).resolve().parent.parent / "run_rehearsal.py"
MARKER = "HARNESS-DOC-ALLOWLIST"

FORBIDDEN = [
    r"\bioreg\b",
    r"\bkextstat\b",
    r"\bkmutil\b",
    r"\bkextcache\b",
    r"\bnvram\b",
    r"\bdiskutil\b",
    r"\blog\s+show\b",
    r"\bsystem_profiler\b",
    r"\bsntp\b",
    r"\bmount\b",
    r"\breboot\b",
    r"\bshutdown\b",
    r"\bsudo\b",
]

SINKS = [
    r"\bsubprocess\b",
    r"\bos\.system\b",
    r"\bos\.popen\b",
    r"\bos\.exec",
    r"\bos\.spawn",
    r"\bpty\b",
    r"\bcommands\.get",
    r"(?<![A-Za-z_.])popen\s*\(",
]

PATTERNS = [(p, re.compile(p)) for p in FORBIDDEN + SINKS]
TRIQ = re.compile(r"(\"\"\"|''')")


def strip_docstrings(lines):
    out = []
    in_doc = False
    quote = None
    for ln in lines:
        if not in_doc:
            m = TRIQ.search(ln)
            if m:
                quote = m.group(1)
                rest = ln[m.end():]
                if quote in rest:
                    out.append("")
                else:
                    in_doc = True
                    out.append("")
            else:
                out.append(ln)
        else:
            if quote in ln:
                in_doc = False
            out.append("")
    return out


class ProhibitionTest(unittest.TestCase):
    def test_no_forbidden_in_executable_context(self):
        lines = TARGET.read_text(encoding="utf-8").splitlines()
        code = strip_docstrings(lines)
        hits = []
        for i, ln in enumerate(code, start=1):
            s = ln.strip()
            if not s or s.startswith("#"):
                continue
            if MARKER in ln:
                continue
            for pat, rx in PATTERNS:
                if rx.search(ln):
                    hits.append(f"L{i} pattern={pat} :: {s[:160]}")
        self.assertEqual(hits, [], f"forbidden tokens in executable context:\n" + "\n".join(hits))

    def test_no_child_process_import(self):
        text = TARGET.read_text(encoding="utf-8")
        self.assertNotRegex(text, r"(?m)^\s*import\s+subprocess\b")
        self.assertNotRegex(text, r"(?m)^\s*from\s+subprocess\b")

    def test_catalog_fixture_covers_all(self):
        catalog = (TARGET.parent / "fixtures" / "forbidden-catalog.txt").read_text()
        for name in ["ioreg", "kextstat", "kmutil", "kextcache", "nvram", "diskutil",
                     "log show", "system_profiler", "sntp", "mount", "reboot",
                     "shutdown", "sudo"]:
            self.assertIn(name, catalog, name)


if __name__ == "__main__":
    unittest.main()
