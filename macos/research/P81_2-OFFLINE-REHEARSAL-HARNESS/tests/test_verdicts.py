"""Harness self-asserts: PASS only nominal; ABORT/BLOCKED/FAIL mapping."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import run_rehearsal as H


class VerdictTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forbidden = H.load_forbidden_catalog()

    def test_pass_only_nominal(self):
        for code in [f"S{i:02d}" for i in range(1, 21)]:
            b = H.apply_scenario(code, self.forbidden)
            got, _, _ = H.validate_bundle(b, self.forbidden)
            if code == "S01":
                self.assertEqual(got, "PASS", "nominal must PASS")
            else:
                self.assertNotEqual(got, "PASS", f"{code} must never PASS")

    def test_expected_mapping(self):
        for code, exp in H.EXPECTED_VERDICTS.items():
            b = H.apply_scenario(code, self.forbidden)
            got, _, _ = H.validate_bundle(b, self.forbidden)
            if exp == "FAIL":
                self.assertIn(got, ("FAIL", "FAIL-3"), code)
            else:
                self.assertEqual(got, exp, code)

    def test_no_fail_to_pass_no_abort_to_pass(self):
        b = H.apply_scenario("S04", self.forbidden)
        got, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertNotEqual(got, "PASS")
        got2, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, got2, "verdicts must be stable, never escalate to PASS")

    def test_kext_unexpected_is_fail3(self):
        b = H.apply_scenario("S11", self.forbidden)
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL-3")
        self.assertTrue(any("kext" in r for r in reasons))

    def test_partial_verdict_blocked_never_approve(self):
        self.assertEqual(H.gemini_partial_verdict_extractor("VERDICT: APP"), "BLOCKED")
        self.assertEqual(H.gemini_partial_verdict_extractor(""), "BLOCKED")
        self.assertEqual(H.gemini_partial_verdict_extractor("VERDICT: APPROVE"), "BLOCKED")
        self.assertEqual(
            H.gemini_partial_verdict_extractor("VERDICT: APPROVE // signed:MOCK-SIGNATURE-FULL"),
            "APPROVE")

    def test_secret_redaction(self):
        dirty = "x FAKE_SECRET=FAKE-FAKE-FAKE-0000 y"
        clean = H.redact_secrets(dirty)
        self.assertNotIn("FAKE_SECRET=FAKE-FAKE-FAKE-0000", clean)
        self.assertTrue(H.redaction_holds(clean))
        b = H.apply_scenario("S20", self.forbidden)
        got, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL")
        self.assertTrue(H.redaction_holds(b.get("secret_redacted", dirty)))

    def test_hash_variants_fail_closed(self):
        base = H.make_nominal_bundle("HASH")
        ok, _, _ = H.validate_bundle(base, self.forbidden)
        self.assertEqual(ok, "PASS")
        mut = H.make_nominal_bundle("HASH")
        mut["texts"]["IDENT-1"] += "x"
        got, _, _ = H.validate_bundle(mut, self.forbidden)
        self.assertIn(got, ("FAIL", "FAIL-3"))
        mut2 = H.make_nominal_bundle("HASH")
        mut2["index_text"] = mut2["index_text"].replace(
            next(r for r in mut2["index_text"].splitlines() if r.startswith("| IDENT-1 |")),
            "| IDENT-1 | TRUNC |")
        got2, _, _ = H.validate_bundle(mut2, self.forbidden)
        self.assertIn(got2, ("FAIL", "FAIL-3"))


if __name__ == "__main__":
    unittest.main()
