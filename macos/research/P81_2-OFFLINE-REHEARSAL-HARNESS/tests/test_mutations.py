"""Mutation kill-rate asserts: 100% critical else REJECT."""
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import run_rehearsal as H


class MutationTest(unittest.TestCase):
    def test_kill_rate_100(self):
        forbidden = H.load_forbidden_catalog()
        rows, rate = H.run_mutations(forbidden)
        self.assertEqual(rate, 100.0, [r for r in rows if not r["killed"]])
        for r in rows:
            self.assertTrue(r["killed"], r["id"])


if __name__ == "__main__":
    unittest.main()
