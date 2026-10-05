"""INDEX/SHASUM asserts: correct ok; truncated/modified/missing/duplicated fail."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import run_rehearsal as H


class IndexHashTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forbidden = H.load_forbidden_catalog()

    def test_nominal_index_ok(self):
        b = H.make_nominal_bundle("S01")
        got, _, checks = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "PASS")
        self.assertEqual(checks.get("hashes"), "ok")

    def test_truncated_hash_fails(self):
        b = H.make_nominal_bundle("T")
        lines = b["index_text"].splitlines()
        for j, ln in enumerate(lines):
            if ln.startswith("| IDENT-2 |"):
                lines[j] = "| IDENT-2 | abc123 |"
        b["index_text"] = "\n".join(lines) + "\n"
        got, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertIn(got, ("FAIL", "FAIL-3"))

    def test_modified_post_index_fails(self):
        b = H.make_nominal_bundle("T")
        b["texts"]["EVENT-1"] += "tamper"
        got, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertIn(got, ("FAIL", "FAIL-3"))

    def test_missing_entry_fails(self):
        b = H.make_nominal_bundle("T")
        lines = [ln for ln in b["index_text"].splitlines() if not ln.startswith("| EVENT-1 |")]
        b["index_text"] = "\n".join(lines) + "\n"
        got, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertIn(got, ("FAIL", "FAIL-3"))

    def test_duplicate_entry_fails(self):
        b = H.make_nominal_bundle("T")
        dup = next(ln for ln in b["index_text"].splitlines() if ln.startswith("| IDENT-1 |"))
        b["index_text"] = b["index_text"] + dup + "\n"
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertIn(got, ("FAIL", "FAIL-3"))
        self.assertTrue(any("duplicate" in r for r in reasons))


if __name__ == "__main__":
    unittest.main()
