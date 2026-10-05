#!/usr/bin/env python3
"""P81.2 OFFLINE REHEARSAL HARNESS - pipeline validator (stdlib only).

HARNESS_IS_NOT_LIVE_EVIDENCE = YES
GPU_STATE = UNKNOWN
LIVE_EXECUTION_AUTHORIZED = NO

What this harness proves: runbook order, bounds, SAVE, INDEX, SHA256,
CLOSE, FAIL/ABORT/BLOCKED semantics and recovery shape - all OFFLINE
against synthetic MOCK fixtures. It does NOT prove macOS live state,
hardware identity, panic-rate, or any live command output.

What this harness never does: it never executes host commands against
the machine. All host commands are modelled ONLY via fixture files.
The forbidden host-command catalog below is documentation-only inside
this docstring (the prohibition grep-test strips docstrings before
scanning). Executable code loads the catalog from
fixtures/forbidden-catalog.txt at runtime and never spells a catalog
entry as a literal in executable context.

Forbidden host-command catalog (documentation only, never executed):
    - ioreg
    - kextstat
    - kmutil
    - kextcache
    - nvram
    - diskutil
    - log show
    - system_profiler
    - sntp
    - mount
    - reboot
    - shutdown
    - sudo

Execution-sink ban (documentation only): this file must not import or
reference child-process launch facilities (the module whose name starts
with "sub" and ends with "process"), nor os.system / os.popen /
os.exec* / os.spawn* / pty / commands.get / popen, in any executable
line. Hashing uses hashlib from the standard library so no external
checksum tool is required.

Clock method: fixtures model method G-M0244-01-R1 synthetically. The
engine only parses the fixture (monotonic-jump and skew gates); host
clock facilities are never consulted as evidence.

Scenario semantics (fail-closed):
    S01 nominal -> PASS. Every other scenario -> FAIL, ABORT or
    BLOCKED. Partial PASS never exists. A FAIL never becomes PASS and
    an ABORT never becomes PASS inside one run.
"""

# Documentation-only catalog restatement for human readers (full-line
# comments are allowlisted by the prohibition grep-test):
# forbidden: ioreg, kextstat, kmutil, kextcache
# forbidden: nvram, diskutil, log show, system_profiler
# forbidden: sntp, mount, reboot, shutdown, sudo
# execution sinks banned: subprocess / os.system / os.popen /
# os.exec* / os.spawn* / pty / popen (documentation only, see docstring)

import argparse
import hashlib
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path

HARNESS_IS_NOT_LIVE_EVIDENCE = "YES"
GPU_STATE = "UNKNOWN"
LIVE_EXECUTION_AUTHORIZED = "NO"
HARNESS_VERSION = "P81.2-RELAY-V3"
TIMEOUT_BOUND_S = 60
RETRY_BOUND = 1
SPACE_FLOOR_GB = 5.0
CLOCK_SKEW_BOUND_S = 5
ALLOWLIST_CURRENT = "v3"

GROUND_TRUTH_DIGESTS = {
    "IDENT-1": "7ece704b55589d0c7831bcd52fdf637f5c7dc26ae0a76aa392cedb66d8a00591",
    "IDENT-2": "b038026c0ead7b46b00e03589f93aef95f5e023dbbb55ef8327d8ae484f12b86",
    "IDENT-3": "f942f32317b176d347a10d6b82ade42a0a360fa503fb74d9674285631fd38f6a",
    "IDENT-4": "6c8d64dd947cebd13736e9bb76cc082177f5d8be1d73bbfe87071ad7b719835f",
    "EVENT-1": "9cb666baecae39243676593fb0c67bee82aff43fa3ea1a7aff3f5001427fd43b",
    "EVENT-2": "c8cbb5515802718193957ba411ebe3c626dff7fac5ad2ff9cf56db51ed05582e",
    "FOTO-meta": "a86ef093211855d1330e8ea0e3e6a01fe11b1f9f7d9e769c79656c3872451ca5",
}

CLOSE_VERDICTS = ("PASS", "FAIL", "FAIL-3", "ABORT", "BLOCKED")

HARNESS_ROOT = Path(__file__).resolve().parent
FIXTURES = HARNESS_ROOT / "fixtures"
EXPECTED = HARNESS_ROOT / "expected"
RESULTS = HARNESS_ROOT / "results"

REQUIRED_ORDER = [
    "IDENT-1",
    "IDENT-2",
    "IDENT-3",
    "IDENT-4",
    "EVENT-1",
    "EVENT-2",
    "FOTO-meta",
    "INDEX",
    "CLOSE",
]

SECRET_PATTERNS = [
    re.compile(r"FAKE_SECRET\s*=\s*\S+"),
    re.compile(r"\bAKIA[0-9A-Z]{12,}\b"),
    re.compile(r"-----BEGIN [A-Z ]*PRIVATE KEY-----"),
    re.compile(r"\bxox[baprs]-[A-Za-z0-9-]{6,}\b"),
    re.compile(r"gh[pousr]_[A-Za-z0-9]{8,}"),
    re.compile(r"Bearer\s+[A-Za-z0-9\-._~+/=]{10,}", re.IGNORECASE),
    re.compile(r"password\s*[:=]\s*\S+", re.IGNORECASE),
    re.compile(r"aws_secret_access_key\s*[:=]\s*\S+", re.IGNORECASE),
    re.compile(r"secret\s*[:=]\s*\S+", re.IGNORECASE),
]

REDACTED = "[REDACTED]"


def stamp():
    return {
        "HARNESS_IS_NOT_LIVE_EVIDENCE": HARNESS_IS_NOT_LIVE_EVIDENCE,
        "GPU_STATE": GPU_STATE,
        "LIVE_EXECUTION_AUTHORIZED": LIVE_EXECUTION_AUTHORIZED,
        "harness": HARNESS_VERSION,
    }


def load_forbidden_catalog():
    """Read the forbidden-action catalog from its fixture file."""
    path = FIXTURES / "forbidden-catalog.txt"
    entries = []
    for raw in path.read_text(encoding="utf-8").splitlines():
        line = raw.strip()
        if not line or line.startswith("#"):
            continue
        entries.append(line)
    return entries


def sha256_text(text):
    return hashlib.sha256(text.encode("utf-8")).hexdigest()


def _normalize_action_token(token):
    t = (token or "").strip().lower()
    t = re.sub(r"\s+", " ", t)
    return t


def is_forbidden_action(token, forbidden):
    norm = _normalize_action_token(token)
    if not norm:
        return False
    for entry in forbidden:
        e = _normalize_action_token(entry)
        if not e:
            continue
        if norm == e:
            return True
        if e in norm:
            return True
        parts = re.split(r"[\s/\\:;|&()]+", norm)
        if e in parts:
            return True
        for part in parts:
            if part == e:
                return True
    return False


def build_close_text(scenario_id, verdict="PASS"):
    return (
        f"MOCK CLOSE // scenario={scenario_id} // verdict={verdict}\n"
        "signed:MOCK-SIGNATURE-FULL\n"
        "HARNESS_IS_NOT_LIVE_EVIDENCE=YES GPU_STATE=UNKNOWN "
        "LIVE_EXECUTION_AUTHORIZED=NO\n"
    )


def redact_secrets(text):
    out = text
    for pat in SECRET_PATTERNS:
        out = pat.sub(REDACTED, out)
    return out


def redaction_holds(text):
    for pat in SECRET_PATTERNS:
        if pat.search(text):
            return False
    return True


def gemini_partial_verdict_extractor(fragment):
    """Fragment input must yield BLOCKED, never APPROVE.

    Only an exact, fully-signed APPROVE line is accepted; anything
    else (fragment, truncation, duplication) is BLOCKED fail-closed.
    """
    clean = (fragment or "").strip()
    if clean == "VERDICT: APPROVE // signed:MOCK-SIGNATURE-FULL":
        return "APPROVE"
    return "BLOCKED"


def nominal_artifact_texts():
    return {
        "IDENT-1": "MOCK IDENT-1 static identity // MOCK-SERIAL-0000 // no real serial\n",
        "IDENT-2": "MOCK IDENT-2 pci identity // class MOCK-CLASS-00 // allowlisted\n",
        "IDENT-3": "MOCK IDENT-3 lifecycle // precondition-only // read-only\n",
        "IDENT-4": "MOCK IDENT-4 clock method=G-M0244-01-R1 skew_s=0 jumps=0\n",
        "EVENT-1": "MOCK event record 1 // nominal // MOCK\n",
        "EVENT-2": "MOCK event record 2 // nominal // MOCK\n",
        "FOTO-meta": json.dumps(
            {"mock": True, "exif_only": True, "camera": "MOCK-CAM-0",
             "note": "synthetic metadata, no real photo"}, indent=2) + "\n",
    }


def build_index_text(entries):
    lines = ["# MOCK INDEX — P81.2 rehearsal (synthetic, not live evidence)"]
    lines.append("")
    lines.append("| artifact | sha256 |")
    lines.append("|---|---|")
    for name in REQUIRED_ORDER:
        if name in ("INDEX", "CLOSE"):
            continue
        digest = entries.get(name, "MISSING")
        lines.append(f"| {name} | {digest} |")
    lines.append("")
    lines.append("MOCK-SIGNATURE: INDEX-MOCK-SIG")
    return "\n".join(lines) + "\n"


def make_nominal_bundle(scenario_id):
    texts = nominal_artifact_texts()
    digests = {k: sha256_text(v) for k, v in texts.items()}
    index_text = build_index_text(digests)
    close_text = build_close_text(scenario_id, "PASS")
    return {
        "scenario": scenario_id,
        "order": list(REQUIRED_ORDER),
        "texts": dict(texts),
        "digests": dict(digests),
        "index_text": index_text,
        "close_text": close_text,
        "timeout_s": 12,
        "retries_used": 0,
        "space_gb": 42.0,
        "save_ok": True,
        "operator_abort": False,
        "panic": False,
        "no_signal": False,
        "no_video": False,
        "kext_lines": ["com.mock.allowlisted-driver"],
        "ident2_diff": "allowlisted",
        "allowlist_version": ALLOWLIST_CURRENT,
        "action_token": "read-only-identity-observe",
        "gemini_fragment": "VERDICT: APPROVE // signed:MOCK-SIGNATURE-FULL",
        "secret_input": "no secrets here // MOCK\n",
        "clock_skew_s": 0,
        "clock_jumps": 0,
    }


def validate_bundle(bundle, forbidden):
    """Fail-closed pipeline. Returns (verdict, reasons list, checks dict)."""
    reasons = []
    checks = {}

    token = bundle.get("action_token", "")
    if is_forbidden_action(token, forbidden):
        reasons.append("abort:forbidden-action")
        checks["forbidden"] = "ABORT"
        return "ABORT", reasons, checks
    checks["forbidden"] = "ok"

    if bundle.get("operator_abort"):
        reasons.append("abort:operator")
        return "ABORT", reasons, checks

    if bundle.get("panic"):
        reasons.append("abort:panic-simulated")
        return "ABORT", reasons, checks
    if bundle.get("no_signal"):
        reasons.append("abort:no-signal-simulated")
        return "ABORT", reasons, checks
    if bundle.get("no_video"):
        reasons.append("abort:no-video-simulated")
        return "ABORT", reasons, checks

    if "timeout_s" not in bundle or "retries_used" not in bundle:
        reasons.append("fail:missing-field-timeout")
        return "FAIL", reasons, checks
    timeout_s = bundle.get("timeout_s")
    retries = bundle.get("retries_used")
    if timeout_s is None or retries is None:
        reasons.append("fail:missing-field-timeout")
        return "FAIL", reasons, checks
    if timeout_s > TIMEOUT_BOUND_S:
        if retries >= RETRY_BOUND:
            reasons.append("abort:timeout-persistent-retry-exhausted")
            return "ABORT", reasons, checks
        reasons.append("abort:timeout-persistent")
        return "ABORT", reasons, checks
    checks["bounds-time"] = "ok"

    if bundle.get("secret_input") and not redaction_holds(bundle["secret_input"]):
        redacted = redact_secrets(bundle["secret_input"])
        bundle["secret_redacted"] = redacted
        if not redaction_holds(redacted):
            reasons.append("fail:redaction-incomplete")
            return "FAIL", reasons, checks
        reasons.append("fail:secret-detected-redacted")
        checks["redaction"] = "redacted"
        return "FAIL", reasons, checks
    checks["redaction"] = "ok"

    frag = bundle.get("gemini_fragment", "")
    if frag != "VERDICT: APPROVE // signed:MOCK-SIGNATURE-FULL":
        got = gemini_partial_verdict_extractor(frag)
        if got != "BLOCKED":
            reasons.append("fail:verdict-extractor-not-fail-closed")
            return "FAIL", reasons, checks
        reasons.append("blocked:partial-verdict")
        checks["gemini"] = "BLOCKED"
        return "BLOCKED", reasons, checks
    checks["gemini"] = "ok"

    order = bundle.get("order", [])
    if order != REQUIRED_ORDER:
        reasons.append("fail:runbook-order")
        return "FAIL", reasons, checks
    checks["order"] = "ok"

    texts = bundle.get("texts", {})
    for name in REQUIRED_ORDER:
        if name in ("INDEX", "CLOSE"):
            continue
        if name not in texts:
            reasons.append(f"fail:missing-required:{name}")
            return "FAIL", reasons, checks
    checks["required"] = "ok"

    index_text = bundle.get("index_text", "")
    if not index_text.startswith("# MOCK INDEX"):
        reasons.append("fail:malformed-index-header")
        return "FAIL", reasons, checks
    rows = re.findall(r"^\| (\S+) \| ([0-9a-f]+|MISSING|TRUNC) \|$", index_text, re.M)
    names = [r[0] for r in rows]
    if len(names) != len(set(names)):
        reasons.append("fail:duplicate-index-entry")
        return "FAIL", reasons, checks
    if set(names) != {n for n in REQUIRED_ORDER if n not in ("INDEX", "CLOSE")}:
        reasons.append("fail:malformed-index-rows")
        return "FAIL", reasons, checks
    checks["index-shape"] = "ok"

    for name, listed in rows:
        if listed in ("MISSING", "TRUNC"):
            reasons.append(f"fail:hash-entry-invalid:{name}")
            return "FAIL", reasons, checks
        if len(listed) != 64:
            reasons.append(f"fail:hash-truncated:{name}")
            return "FAIL", reasons, checks
        actual = sha256_text(texts[name])
        if actual != listed:
            reasons.append(f"fail:hash-mismatch:{name}")
            return "FAIL", reasons, checks
        pinned = GROUND_TRUTH_DIGESTS.get(name)
        if pinned is None or actual != pinned:
            reasons.append(f"fail:ground-truth-mismatch:{name}")
            return "FAIL", reasons, checks
    checks["hashes"] = "ok"

    if bundle.get("ident2_diff") != "allowlisted":
        reasons.append("fail:ident2-diff-outside-allowlist")
        return "FAIL", reasons, checks
    checks["ident2"] = "ok"

    allowed_kext = {"com.mock.allowlisted-driver"}
    kext_lines = bundle.get("kext_lines", [])
    if kext_lines is None or len(kext_lines) < 1:
        reasons.append("fail:empty-inventory")
        return "FAIL", reasons, checks
    for line in kext_lines:
        if line not in allowed_kext:
            reasons.append("fail-3:kext-unexpected-line")
            return "FAIL-3", reasons, checks
    checks["kext"] = "ok"

    if bundle.get("allowlist_version") != ALLOWLIST_CURRENT:
        reasons.append("fail:stale-allowlist")
        return "FAIL", reasons, checks
    checks["allowlist"] = "ok"

    if "clock_skew_s" not in bundle or "clock_jumps" not in bundle:
        reasons.append("fail:missing-field-clock")
        return "FAIL", reasons, checks
    skew_val = bundle.get("clock_skew_s")
    jumps_val = bundle.get("clock_jumps")
    if skew_val is None or jumps_val is None:
        reasons.append("fail:missing-field-clock")
        return "FAIL", reasons, checks
    if skew_val > CLOCK_SKEW_BOUND_S or jumps_val > 0:
        reasons.append("fail:clock-gate-invalid")
        return "FAIL", reasons, checks
    checks["clock"] = "ok"

    if "space_gb" not in bundle:
        reasons.append("fail:missing-field-space")
        return "FAIL", reasons, checks
    space_val = bundle.get("space_gb")
    if space_val is None or space_val < SPACE_FLOOR_GB:
        reasons.append("fail:evidence-destination-space")
        return "FAIL", reasons, checks
    checks["space"] = "ok"

    if not bundle.get("save_ok"):
        reasons.append("fail:save-failure")
        return "FAIL", reasons, checks
    checks["save"] = "ok"

    close_text = bundle.get("close_text", "")
    scenario_id = bundle.get("scenario", "")
    matched_verdict = None
    for cand in CLOSE_VERDICTS:
        if close_text == build_close_text(scenario_id, cand):
            matched_verdict = cand
            break
    if matched_verdict is None:
        if "signed:MOCK-SIGNATURE-FULL" not in close_text:
            reasons.append("fail:close-missing-signature")
            return "FAIL", reasons, checks
        if close_text.count("verdict=") > 1:
            reasons.append("fail:duplicate-verdict")
            return "FAIL", reasons, checks
        if close_text.count("verdict=") < 1:
            reasons.append("fail:close-missing-verdict")
            return "FAIL", reasons, checks
        reasons.append("fail:close-shape-mismatch")
        return "FAIL", reasons, checks
    checks["close"] = "ok"
    if matched_verdict != "PASS":
        reasons.append(f"close-declared-nonpass:{matched_verdict}")
        return matched_verdict, reasons, checks

    reasons.append("pass:nominal")
    return "PASS", reasons, checks


def apply_scenario(code, forbidden):
    b = make_nominal_bundle(code)
    if code == "S01":
        pass
    elif code == "S02":
        del b["texts"]["EVENT-2"]
    elif code == "S03":
        b["index_text"] = "MALFORMED INDEX // no header // MOCK\n| broken |\n"
    elif code == "S04":
        b["texts"]["IDENT-1"] = b["texts"]["IDENT-1"].replace("0000", "0001")
    elif code == "S05":
        b["ident2_diff"] = "non-allowlisted-pci-class"
    elif code == "S06":
        b["timeout_s"] = 75
    elif code == "S07":
        b["timeout_s"] = 90
        b["retries_used"] = 1
    elif code == "S08":
        b["no_video"] = True
    elif code == "S09":
        b["panic"] = True
    elif code == "S10":
        b["no_signal"] = True
    elif code == "S11":
        b["kext_lines"] = ["com.mock.allowlisted-driver", "com.evil.extra-line"]
    elif code == "S12":
        b["clock_skew_s"] = 120
        b["clock_jumps"] = 2
    elif code == "S13":
        b["space_gb"] = 1.2
    elif code == "S14":
        b["save_ok"] = False
    elif code == "S15":
        b["close_text"] = b["close_text"].replace("signed:MOCK-SIGNATURE-FULL", "unsigned")
    elif code == "S16":
        b["operator_abort"] = True
    elif code == "S17":
        b["action_token"] = forbidden[0] if forbidden else "reserved-forbidden-slot"
    elif code == "S18":
        b["allowlist_version"] = "v0-stale"
    elif code == "S19":
        b["gemini_fragment"] = "VERDICT: APP"
    elif code == "S20":
        b["secret_input"] = "note // FAKE_SECRET=FAKE-FAKE-FAKE-0000 // must redact\n"
    else:
        raise ValueError(code)
    return b


EXPECTED_VERDICTS = {
    "S01": "PASS",
    "S02": "FAIL",
    "S03": "FAIL",
    "S04": "FAIL",
    "S05": "FAIL",
    "S06": "ABORT",
    "S07": "ABORT",
    "S08": "ABORT",
    "S09": "ABORT",
    "S10": "ABORT",
    "S11": "FAIL-3",
    "S12": "FAIL",
    "S13": "FAIL",
    "S14": "FAIL",
    "S15": "FAIL",
    "S16": "ABORT",
    "S17": "ABORT",
    "S18": "FAIL",
    "S19": "BLOCKED",
    "S20": "FAIL",
}

SCENARIO_DESCRIPTIONS = {
    "S01": "PASS nominal",
    "S02": "missing artifact (EVENT-2 removed)",
    "S03": "malformed INDEX (no header)",
    "S04": "bad shasum (IDENT-1 modified post-INDEX)",
    "S05": "IDENT-2 diff outside allowlist",
    "S06": "command timeout 75s > 60s bound",
    "S07": "one bounded retry then ABORT (timeout persists)",
    "S08": "no-video simulated",
    "S09": "panic simulated",
    "S10": "no-signal simulated",
    "S11": "unexpected extra line in driver inventory",
    "S12": "clock gate invalid (skew+jumps)",
    "S13": "evidence destination 1.2GB < 5GB floor",
    "S14": "SAVE failure",
    "S15": "CLOSE missing signature",
    "S16": "operator abort",
    "S17": "forbidden action injected (catalog entry)",
    "S18": "stale allowlist regression (v0 vs current)",
    "S19": "partial-verdict extractor regression (fragment -> BLOCKED)",
    "S20": "secret-redaction regression (fake secret -> redacted, FAIL-closed)",
}


MUTATIONS = [
    ("M1-flip-status", "flip CLOSE verdict token",
     lambda b: b.update(close_text=b["close_text"].replace("verdict=PASS", "verdict=FAIL"))),
    ("M2-remove-artifact", "remove EVENT-1",
     lambda b: b["texts"].pop("EVENT-1", None)),
    ("M3-truncate-hash", "truncate one INDEX hash row",
     lambda b: b.update(index_text=re.sub(r"([0-9a-f]{64})", lambda m: m.group(1)[:16],
                                          b["index_text"], count=1))),
    ("M4-inject-extra-driver-line", "inject extra driver inventory line",
     lambda b: b.update(kext_lines=["com.mock.allowlisted-driver", "com.mock.rogue-line"])),
    ("M5-change-timeout", "timeout 999s",
     lambda b: b.update(timeout_s=999)),
    ("M6-drop-close", "drop CLOSE signature",
     lambda b: b.update(close_text="MOCK CLOSE // unsigned // no verdict\n")),
    ("M7-inject-forbidden-token", "inject catalog entry as action",
     None),
    ("M8-duplicate-verdict", "duplicate verdict token in CLOSE",
     lambda b: b.update(close_text=b["close_text"] + "verdict=FAIL\n")),
    ("M9-partial-verdict", "fragment verdict input",
     lambda b: b.update(gemini_fragment="VERDICT: APP")),
    ("M10-tamper-reindex", "tamper text plus rebuild INDEX (ground-truth pin)",
     None),
    ("M11-empty-inventory", "empty driver inventory",
     lambda b: b.update(kext_lines=[])),
    ("M12-forbidden-upper", "forbidden token upper-case variant",
     None),
    ("M13-forbidden-args", "forbidden token with args variant",
     None),
    ("M14-forbidden-path", "forbidden token path-qualified variant",
     None),
    ("M15-forbidden-padded", "forbidden token whitespace-padded variant",
     None),
    ("M16-forbidden-multi", "forbidden token multi-action variant",
     None),
    ("M17-secret-ghp", "ghp token shape must redact and FAIL",
     lambda b: b.update(secret_input="deploy key ghp_AbCdEfGh1234567890XYZ // leak\n")),
    ("M18-secret-bearer", "bearer token shape must redact and FAIL",
     lambda b: b.update(secret_input="auth Bearer AbCdEfGh1234567890.xyz-_~+/== // leak\n")),
    ("M19-secret-password", "generic password shape must redact and FAIL",
     lambda b: b.update(secret_input="login password: hunter2-secret // leak\n")),
    ("M20-secret-aws", "aws secret shape must redact and FAIL",
     lambda b: b.update(secret_input="cfg aws_secret_access_key = wJalrXUtnFEMI/K7MDENG/bPxRfiCYEXAMPLEKEY // leak\n")),
    ("M21-close-prefix", "CLOSE prefix injection must FAIL shape",
     lambda b: b.update(close_text="INJECTED-PREFIX\n" + b["close_text"])),
    ("M22-close-suffix", "CLOSE suffix injection must FAIL shape",
     lambda b: b.update(close_text=b["close_text"] + "INJECTED-SUFFIX\n")),
    ("M23-missing-timeout", "missing timeout field must FAIL",
     lambda b: b.pop("timeout_s", None)),
    ("M24-missing-clock", "missing clock field must FAIL",
     lambda b: b.pop("clock_skew_s", None)),
    ("M25-missing-space", "missing space field must FAIL",
     lambda b: b.pop("space_gb", None)),
]


def run_mutations(forbidden):
    outcomes = []
    for mid, desc, fn in MUTATIONS:
        b = make_nominal_bundle("MUT")
        if mid == "M7-inject-forbidden-token":
            b["action_token"] = forbidden[0] if forbidden else "reserved-forbidden-slot"
        elif mid == "M10-tamper-reindex":
            b["texts"]["IDENT-1"] = b["texts"]["IDENT-1"].replace("0000", "0001")
            fresh = {k: sha256_text(v) for k, v in b["texts"].items()}
            b["index_text"] = build_index_text(fresh)
        elif mid == "M12-forbidden-upper":
            base = forbidden[-1] if forbidden else "reserved-forbidden-slot"
            b["action_token"] = base.upper()
        elif mid == "M13-forbidden-args":
            base = forbidden[0] if forbidden else "reserved-forbidden-slot"
            b["action_token"] = base + " --last 1m"
        elif mid == "M14-forbidden-path":
            base = forbidden[0] if forbidden else "reserved-forbidden-slot"
            b["action_token"] = "/usr/sbin/" + base
        elif mid == "M15-forbidden-padded":
            base = forbidden[0] if forbidden else "reserved-forbidden-slot"
            b["action_token"] = "  " + base + "  "
        elif mid == "M16-forbidden-multi":
            base = forbidden[0] if forbidden else "reserved-forbidden-slot"
            b["action_token"] = "echo ok; " + base + " --force"
        else:
            fn(b)
        verdict, reasons, _ = validate_bundle(b, forbidden)
        killed = verdict != "PASS"
        outcomes.append({"id": mid, "desc": desc, "verdict": verdict,
                         "reasons": reasons, "killed": killed,
                         "critical": True})
    killed = sum(1 for o in outcomes if o["killed"])
    rate = 100.0 * killed / len(outcomes) if outcomes else 0.0
    return outcomes, rate


def write_results_index(scenario_rows, mutation_rows, kill_rate):
    RESULTS.mkdir(parents=True, exist_ok=True)
    now = datetime.now(timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ")
    lines = ["# P81.2 rehearsal results INDEX (synthetic, not live evidence)",
             "",
             f"generated: {now} // {HARNESS_VERSION}",
             "HARNESS_IS_NOT_LIVE_EVIDENCE=YES GPU_STATE=UNKNOWN LIVE_EXECUTION_AUTHORIZED=NO",
             "",
             "| scenario | expected | got | match | reasons |",
             "|---|---|---|---|---|"]
    for r in scenario_rows:
        lines.append(f"| {r['scenario']} | {r['expected']} | {r['got']} | "
                     f"{'MATCH' if r['match'] else 'MISMATCH'} | {';'.join(r['reasons'])} |")
    lines += ["",
              f"MUTATION_KILL_RATE = {kill_rate:.1f}% ({sum(1 for m in mutation_rows if m['killed'])}/{len(mutation_rows)})",
              ""]
    for m in mutation_rows:
        lines.append(f"- {m['id']}: {m['verdict']} killed={m['killed']}")
    lines += ["",
              "HARNESS_IS_NOT_LIVE_EVIDENCE = YES",
              "live=0"]
    text = "\n".join(lines) + "\n"
    (RESULTS / "INDEX.md").write_text(text, encoding="utf-8")
    (RESULTS / "INDEX.sha256").write_text(sha256_text(text) + "  INDEX.md\n", encoding="utf-8")
    return text


def write_scenario_close(row):
    sdir = RESULTS / row["scenario"]
    sdir.mkdir(parents=True, exist_ok=True)
    body = (f"MOCK CLOSE // scenario={row['scenario']} // got={row['got']} "
            f"expected={row['expected']} // match={row['match']}\n"
            f"reasons: {';'.join(row['reasons'])}\n"
            "signed:MOCK-SIGNATURE-FULL\n"
            "HARNESS_IS_NOT_LIVE_EVIDENCE=YES GPU_STATE=UNKNOWN "
            "LIVE_EXECUTION_AUTHORIZED=NO\n")
    (sdir / "CLOSE.md").write_text(body, encoding="utf-8")
    idx = build_index_text({k: sha256_text(v) for k, v in
                            nominal_artifact_texts().items()})
    (sdir / "INDEX.md").write_text(idx, encoding="utf-8")
    (sdir / "shasum.txt").write_text(
        "".join(f"{sha256_text(v)}  {k}\n" for k, v in
                sorted(nominal_artifact_texts().items())), encoding="utf-8")


def run_all(write=True):
    forbidden = load_forbidden_catalog()
    scenario_rows = []
    for code in [f"S{i:02d}" for i in range(1, 21)]:
        bundle = apply_scenario(code, forbidden)
        got, reasons, _ = validate_bundle(bundle, forbidden)
        expected = EXPECTED_VERDICTS[code]
        match = (got == expected) or (expected == "FAIL" and got in ("FAIL", "FAIL-3"))
        scenario_rows.append({"scenario": code, "expected": expected, "got": got,
                              "match": match, "reasons": reasons,
                              "desc": SCENARIO_DESCRIPTIONS[code]})
    mutation_rows, kill_rate = run_mutations(forbidden)
    all_match = all(r["match"] for r in scenario_rows)
    verdict = "P81_2 = ACCEPT" if (all_match and kill_rate == 100.0) else "P81_2 = REJECT"
    if write:
        write_results_index(scenario_rows, mutation_rows, kill_rate)
        for r in scenario_rows:
            write_scenario_close(r)
        summary = {"stamp": stamp(), "scenarios": scenario_rows,
                   "mutations": mutation_rows, "kill_rate": kill_rate,
                   "verdict": verdict}
        (RESULTS / "SUMMARY.json").write_text(json.dumps(summary, indent=2) + "\n",
                                              encoding="utf-8")
    return scenario_rows, mutation_rows, kill_rate, verdict


def main(argv=None):
    ap = argparse.ArgumentParser(description="P81.2 offline rehearsal harness (stdlib only)")
    ap.add_argument("--no-write", action="store_true")
    ap.add_argument("--self-test", action="store_true")
    args = ap.parse_args(argv)
    scenarios, mutations, kill_rate, verdict = run_all(write=not args.no_write)
    print("P81.2 OFFLINE REHEARSAL HARNESS // " + HARNESS_VERSION)
    print("HARNESS_IS_NOT_LIVE_EVIDENCE = YES // GPU_STATE = UNKNOWN // "
          "LIVE_EXECUTION_AUTHORIZED = NO")
    for r in scenarios:
        flag = "OK " if r["match"] else "BAD"
        print(f"[{flag}] {r['scenario']} {r['desc']}: expected={r['expected']} "
              f"got={r['got']} reasons={';'.join(r['reasons'])}")
    for m in mutations:
        print(f"[{'KILL' if m['killed'] else 'LIVE'}] {m['id']} {m['desc']}: {m['verdict']}")
    print(f"MUTATION_KILL_RATE = {kill_rate:.1f}%")
    print(verdict)
    print("HARNESS_IS_NOT_LIVE_EVIDENCE = YES")
    print("live=0")
    if args.self_test:
        bad = [r for r in scenarios if not r["match"]]
        live = [m for m in mutations if not m["killed"] and m["critical"]]
        if bad or live or kill_rate != 100.0:
            return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
