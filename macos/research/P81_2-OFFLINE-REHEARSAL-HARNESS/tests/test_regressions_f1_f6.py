"""Regression tests for smoke findings F1-F6 (P81.2 fix turn)."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import run_rehearsal as H


class F1GroundTruthTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forbidden = H.load_forbidden_catalog()

    def test_f1_nominal_matches_ground_truth(self):
        b = H.make_nominal_bundle("S01")
        for name, text in b["texts"].items():
            self.assertEqual(H.sha256_text(text), H.GROUND_TRUTH_DIGESTS[name], name)
        got, _, checks = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "PASS")
        self.assertEqual(checks.get("hashes"), "ok")

    def test_f1_tamper_plus_reindex_must_fail(self):
        b = H.make_nominal_bundle("F1")
        b["texts"]["IDENT-1"] = b["texts"]["IDENT-1"].replace("0000", "0001")
        fresh = {k: H.sha256_text(v) for k, v in b["texts"].items()}
        b["index_text"] = H.build_index_text(fresh)
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertIn(got, ("FAIL", "FAIL-3"))
        self.assertTrue(any("ground-truth" in r for r in reasons), reasons)

    def test_f1_tamper_other_artifact_reindex_fails(self):
        b = H.make_nominal_bundle("F1")
        b["texts"]["EVENT-2"] = b["texts"]["EVENT-2"] + "tamper"
        fresh = {k: H.sha256_text(v) for k, v in b["texts"].items()}
        b["index_text"] = H.build_index_text(fresh)
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertIn(got, ("FAIL", "FAIL-3"))
        self.assertTrue(any("ground-truth" in r for r in reasons), reasons)


class F2EmptyInventoryTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forbidden = H.load_forbidden_catalog()

    def test_f2_empty_inventory_fails(self):
        b = H.make_nominal_bundle("F2")
        b["kext_lines"] = []
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL")
        self.assertTrue(any("empty-inventory" in r for r in reasons), reasons)

    def test_f2_nominal_inventory_passes(self):
        b = H.make_nominal_bundle("S01")
        self.assertGreaterEqual(len(b["kext_lines"]), 1)
        got, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "PASS")


class F3ForbiddenNormalizeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forbidden = H.load_forbidden_catalog()

    def _assert_abort(self, token):
        b = H.make_nominal_bundle("F3")
        b["action_token"] = token
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "ABORT", f"token={token!r} reasons={reasons}")
        self.assertTrue(any("forbidden" in r for r in reasons), reasons)

    def test_f3_exact_still_aborts(self):
        self._assert_abort(self.forbidden[0])

    def test_f3_upper_case_aborts(self):
        self._assert_abort(self.forbidden[0].upper())
        self._assert_abort("SUDO")

    def test_f3_args_variant_aborts(self):
        self._assert_abort(self.forbidden[0] + " --last 1m")
        self._assert_abort("sudo nvram -p")

    def test_f3_path_qualified_aborts(self):
        self._assert_abort("/usr/sbin/" + self.forbidden[0])
        self._assert_abort("/usr/bin/sudo")

    def test_f3_whitespace_variant_aborts(self):
        self._assert_abort("  " + self.forbidden[0] + "  ")
        self._assert_abort("  SUDO  ")

    def test_f3_multi_action_aborts(self):
        self._assert_abort("echo ok; " + self.forbidden[0] + " --force")
        self._assert_abort("echo hi; sudo reboot")

    def test_f3_nominal_token_passes(self):
        b = H.make_nominal_bundle("S01")
        got, _, checks = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "PASS")
        self.assertEqual(checks.get("forbidden"), "ok")


class F4RedactionTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forbidden = H.load_forbidden_catalog()

    def _assert_redact_fail(self, secret):
        b = H.make_nominal_bundle("F4")
        b["secret_input"] = secret
        self.assertFalse(H.redaction_holds(secret), secret)
        clean = H.redact_secrets(secret)
        self.assertTrue(H.redaction_holds(clean), clean)
        got, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL", secret)

    def test_f4_ghp_shape(self):
        self._assert_redact_fail("leak ghp_AbCdEfGh1234567890XYZ end\n")

    def test_f4_gho_shape(self):
        self._assert_redact_fail("leak gho_ZxYvWuTsRqPoNmLk1234 end\n")

    def test_f4_bearer_shape(self):
        self._assert_redact_fail("auth Bearer AbCdEfGh1234567890.xyz-_~+/== end\n")

    def test_f4_password_generic(self):
        self._assert_redact_fail("login password: hunter2-secret end\n")
        self._assert_redact_fail("cfg Password=supersecret123 end\n")

    def test_f4_aws_secret_shape(self):
        self._assert_redact_fail(
            "cfg aws_secret_access_key = wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY end\n")

    def test_f4_deny_unknown_secret_shape(self):
        self._assert_redact_fail("svc secret: mysecrettoken123 end\n")


class F5CloseShapeTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forbidden = H.load_forbidden_catalog()

    def test_f5_nominal_close_passes(self):
        b = H.make_nominal_bundle("S01")
        self.assertEqual(b["close_text"], H.build_close_text("S01", "PASS"))
        got, _, checks = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "PASS")
        self.assertEqual(checks.get("close"), "ok")

    def test_f5_prefix_injection_fails(self):
        b = H.make_nominal_bundle("F5")
        b["close_text"] = "INJECTED-PREFIX\n" + b["close_text"]
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL")
        self.assertTrue(any("shape-mismatch" in r or "close" in r for r in reasons), reasons)

    def test_f5_suffix_injection_fails(self):
        b = H.make_nominal_bundle("F5")
        b["close_text"] = b["close_text"] + "INJECTED-SUFFIX\n"
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL")
        self.assertTrue(any("shape-mismatch" in r or "close" in r for r in reasons), reasons)

    def test_f5_flip_still_not_pass(self):
        b = H.make_nominal_bundle("F5")
        b["close_text"] = H.build_close_text("F5", "FAIL")
        got, _, _ = H.validate_bundle(b, self.forbidden)
        self.assertNotEqual(got, "PASS")


class F6ExplicitPresenceTest(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.forbidden = H.load_forbidden_catalog()

    def test_f6_missing_timeout_fails(self):
        b = H.make_nominal_bundle("F6")
        del b["timeout_s"]
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL")
        self.assertTrue(any("missing-field-timeout" in r for r in reasons), reasons)

    def test_f6_missing_retries_fails(self):
        b = H.make_nominal_bundle("F6")
        del b["retries_used"]
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL")
        self.assertTrue(any("missing-field-timeout" in r for r in reasons), reasons)

    def test_f6_missing_clock_fails(self):
        b = H.make_nominal_bundle("F6")
        del b["clock_skew_s"]
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL")
        self.assertTrue(any("missing-field-clock" in r for r in reasons), reasons)

    def test_f6_missing_space_fails(self):
        b = H.make_nominal_bundle("F6")
        del b["space_gb"]
        got, reasons, _ = H.validate_bundle(b, self.forbidden)
        self.assertEqual(got, "FAIL")
        self.assertTrue(any("missing-field-space" in r for r in reasons), reasons)


if __name__ == "__main__":
    unittest.main()
