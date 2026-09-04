#!/usr/bin/env python3
"""tinygpu-match-check.py — análise estática de elegibilidade IOKit/DriverKit da dext TinyGPU.

PURAMENTE LÓGICO. Nenhum hardware, nenhum driver, nenhum MMIO, nenhum boot.
Lê SOMENTE um snapshot IORegistry da GPU em JSON + constantes da personality
TinyGPU embutidas abaixo. Stdlib apenas, sem dependências.

Uso:
  python3 scripts/tinygpu-match-check.py snapshot.json
  python3 scripts/tinygpu-match-check.py snapshot.json --competitors competitors.json
  cat snapshot.json | python3 scripts/tinygpu-match-check.py
  python3 scripts/tinygpu-match-check.py --self-test

Formato do snapshot (JSON objeto; chaves toleram hífen/underscore/camelCase):
  {
    "vendor-id": "0x10DE", "device-id": "0x2504",
    "class-code": "0x030000",
    "subsystem-vendor-id": "0x10DE", "subsystem-id": "0x2504",
    "revision-id": "0xA1",
    "built-in": "ABSENT",            // YES | NO | ABSENT | UNKNOWN (bool/null ok)
    "IOPCITunnelled": "ABSENT",      // YES | NO | ABSENT | UNKNOWN (bool/null ok)
    "competitors": [ ... ]           // OPCIONAL, ver --competitors
  }

Vereditos finais: LIKELY_MATCH | LIKELY_NO_MATCH | INDETERMINATE.
"""

from __future__ import annotations

import argparse
import json
import sys

# ---------------------------------------------------------------------------
# Personality TinyGPU embutida como constantes documentadas.
#
# FONTE (fatos CONFIRMED por decode em Linux, análise estática TG0/TGM0):
#   - docs/tinygpu/binary-architecture.md §3 (plist do dext) + §9 (Correção TGM0
#     sobre IOPCITunnelCompatible) + §10 (tabela personality completa TGM0)
#   - Info.plist do dext:
#     TinyGPU.app/Contents/Library/SystemExtensions/
#       org.tinygrad.tinygpu.driver2.dext/Info.plist  (bplist, personality única
#       "TinyGPUDriver"; app Info.plist XML NÃO tem nenhuma chave IOPCI*).
#   - Entitlements de PRODUÇÃO (duas fontes convergentes, §5 do mesmo doc +
#     docs/tinygpu/amd-igpu-match-risk.md §3): blob LC_CODE_SIGNATURE do Mach-O
#     + embedded.provisionprofile (driver_release_0431).
# ---------------------------------------------------------------------------

# Chave provedora: a personality ancora em IOPCIDevice (CONFIRMED, decode plistlib).
TINYGPU_IO_PROVIDER_CLASS = "IOPCIDevice"

# Único critério PCI no plist (CONFIRMED). Sem "&" de máscara no plist → match de
# classe inteira display (0x03____). Ver binary-architecture.md §3/§10.
TINYGPU_IOPCI_CLASS_MATCH = 0x03000000

# Ausência verificada por decode completo (0 ocorrências no plist). Filtro por
# vendor vive no ENTITLEMENT (§5), não no plist (binary-architecture.md §3/§10).
TINYGPU_IOPCI_PRIMARY_MATCH = None    # ABSENT (CONFIRMED)
TINYGPU_IOPCI_MATCH = None            # ABSENT (CONFIRMED)
TINYGPU_IOPCI_SECONDARY_MATCH = None  # ABSENT (CONFIRMED)

# Declara SUPORTE a Thunderbolt; NÃO prova exclusão de não-tunneled.
# Correção TGM0 (binary-architecture.md §9): "To indicate that your PCIe driver
# supports Thunderbolt, include the IOPCITunnelCompatible key [...]" (Apple).
# A recíproca (excluir internas) NÃO é documentada → filtro tunneled-only UNKNOWN.
TINYGPU_IOPCI_TUNNEL_COMPATIBLE = True  # CONFIRMED (decode)

# Ausência verificada (0 ocorrências no plist e no binário) → sem pedido de
# prioridade (binary-architecture.md §10; amd-igpu-match-risk.md §2/§5).
TINYGPU_IO_PROBE_SCORE = None  # ABSENT (CONFIRMED)
TINYGPU_IO_MATCH_CATEGORY = "TinyGPUDriver"  # CONFIRMED (arbitragem)
TINYGPU_IO_USER_CLASS = "TinyGPUDriver"  # CONFIRMED
TINYGPU_IO_USER_SERVER_NAME = "org.tinygrad.tinygpu.Driver"  # CONFIRMED

# Entitlements de PRODUÇÃO da dext assinada (CONFIRMED, duas fontes §5):
#   com.apple.developer.driverkit.transport.pci =
#     [{IOPCIPrimaryMatch: 0x000010DE & 0x0000FFFF},
#      {IOPCIPrimaryMatch: 0x00001002 & 0x0000FFFF}]
# Formato 0xDDDDVVVV & mask → vendor 10DE (NVIDIA) qualquer device +
# vendor 1002 (AMD) qualquer device. Allowlist por VENDOR, não por SKU
# (logo 10de:2504 passa no entitlement; support-matrix.md §5).
# Variantes de dev (wildcard 0xFFFFFFFF&0x00000000) NÃO são a shipped.
TINYGPU_ENTITLEMENT_VENDORS = frozenset({0x10DE, 0x1002})  # any-device cada
TINYGPU_ENTITLEMENT_DESC = (
    "[0x000010DE&0x0000FFFF (10DE any-device), "
    "0x00001002&0x0000FFFF (1002 any-device)]"
)

# A dext NÃO tem isenção `...builtin` (CONFIRMED ausência: zero `builtin` no dext;
# 2x `builtin` no app são `__swift5_builtin__TEXT`, falso positivo —
# amd-igpu-match-risk.md §3.2). Mecanismo Apple `IOPCIDevice::matchPropertyTable`
# checa `built-in` (CONFIRMED fora deste script); aplicabilidade ao nó depende de
# `ioreg` vivo → produção UNKNOWN sem IORegistry real.
TINYGPU_HAS_BUILTIN_EXEMPTION = False

FINAL_MATCH = "LIKELY_MATCH"
FINAL_NO_MATCH = "LIKELY_NO_MATCH"
FINAL_INDET = "INDETERMINATE"

DISCLAIMER = "análise estática; IOKit pode ter regras adicionais"


# ---------------------------------------------------------------------------
# Normalização de entrada
# ---------------------------------------------------------------------------

def parse_hex(value):
    """Converte int | '0x…' | '10de' | decimal-str em int. Retorna None se ausente/inválido."""
    if value is None:
        return None
    if isinstance(value, int):
        return value
    if isinstance(value, float):
        return int(value)
    if isinstance(value, bytes):
        try:
            value = value.decode("utf-8", "ignore")
        except Exception:
            return None
    if isinstance(value, str):
        s = value.strip()
        if s == "":
            return None
        try:
            return int(s, 16) if s.lower().startswith("0x") else int(s, 16) if all(
                c in "0123456789abcdefABCDEF" for c in s
            ) and any(c in "abcdefABCDEF" for c in s) else int(s, 0)
        except ValueError:
            return None
    return None


def norm_presence(value, *, missing="ABSENT"):
    """Normaliza built-in / IOPCITunnelled para YES | NO | ABSENT | UNKNOWN.

    Aceita bool, int 0/1, None (ausente), str ("YES"/"NO"/"ABSENT"/"UNKNOWN",
    "true"/"false", "1"/"0", ""), chave ausente → ABSENT.
    """
    if value is None:
        return missing
    if isinstance(value, bool):
        return "YES" if value else "NO"
    if isinstance(value, int):
        if value == 1:
            return "YES"
        if value == 0:
            return "NO"
        return "UNKNOWN"
    if isinstance(value, str):
        s = value.strip().upper()
        if s in ("YES", "Y", "TRUE", "1", "PRESENT"):
            return "YES"
        if s in ("NO", "N", "FALSE", "0"):
            return "NO"
        if s in ("ABSENT", "MISSING", "NOT PRESENT", ""):
            return "ABSENT"
        if s in ("UNKNOWN", "?", "N/A", "NA"):
            return "UNKNOWN"
        return "UNKNOWN"
    return "UNKNOWN"


def get_field(snapshot, *aliases, default=None):
    for key in aliases:
        if key in snapshot:
            return snapshot[key]
    # fallback case-insensitive / normalizado (hífen vs underscore)
    norm = {str(k).lower().replace("_", "-"): v for k, v in snapshot.items()}
    for key in aliases:
        nk = key.lower().replace("_", "-")
        if nk in norm:
            return norm[nk]
    return default


def fmt_hex(value):
    if value is None:
        return "AUSENTE"
    width = 8 if value > 0xFFFFFF else 6 if value > 0xFFFF else 4
    return "0x%0*X" % (width, value)


# ---------------------------------------------------------------------------
# Avaliação
# ---------------------------------------------------------------------------

def eval_class(class_code):
    """Compara class-code do dispositivo vs IOPCIClassMatch=0x03000000.

    Semântica documentada (binary-architecture.md §3): máscara exata sem `&` →
    match de classe inteira display (base 0x03). Robusto a 24 bits (0x030000,
    sysfs/Linux) e 32 bits (0x03000000, IORegistry deslocado): extrai a base.
    """
    if class_code is None:
        return ("UNKNOWN", "class-code ausente → classe não avaliável (INDETERMINATE parcial)")
    if class_code > 0xFFFFFF:
        base = (class_code >> 24) & 0xFF
        detail = "(class-code & 0xFF000000) vs 0x03000000; base=%s" % fmt_hex(base)
    else:
        base = (class_code >> 16) & 0xFF
        detail = "(class-code & 0xFF0000) vs 0x030000; base=%s" % fmt_hex(base)
    if base == 0x03:
        return ("PASS", "display controller (base 0x03) casa IOPCIClassMatch=0x03000000 %s" % detail)
    return ("FAIL", "base %s ≠ 0x03 → NÃO casa IOPCIClassMatch=0x03000000 %s" % (fmt_hex(base), detail))


def eval_vendor(vendor):
    if vendor is None:
        return ("UNKNOWN", "vendor-id ausente → entitlement não avaliável (INDETERMINATE parcial)")
    allowed = sorted(TINYGPU_ENTITLEMENT_VENDORS)
    if vendor in TINYGPU_ENTITLEMENT_VENDORS:
        return ("PASS", "%s dentro da allowlist de produção %s (any-device)"
                % (fmt_hex(vendor), TINYGPU_ENTITLEMENT_DESC))
    return ("FAIL", "%s FORA da allowlist de produção %s (vendors permitidos: %s)"
            % (fmt_hex(vendor), TINYGPU_ENTITLEMENT_DESC,
               ", ".join(fmt_hex(v) for v in allowed)))


def eval_device(device):
    # NENHUM IOPCIPrimaryMatch/IOPCIMatch/IOPCISecondaryMatch no plist (ABSENT
    # CONFIRMED) → sem filtro por ID → qualquer device ALLOWED por ausência,
    # inclusive 0x2504. Nunca reprova sozinho.
    if device is None:
        return ("ALLOWED_AUSENCIA", "device-id ausente, mas sem IOPCI*Match por ID no plist → sem filtro (neutro)")
    extra = ""
    if device == 0x2504:
        extra = " (0x2504 = RTX 3060/GA106: passa trivialmente — filtro familiar 0x2500 é camada Python PCIIface, não matching DriverKit)"
    return ("ALLOWED_AUSENCIA", "%s ALLOWED por ausência (sem IOPCIPrimaryMatch/IOPCIMatch/IOPCISecondaryMatch no plist)%s"
            % (fmt_hex(device), extra))


def eval_builtin(presence):
    if presence == "YES":
        return ("BLOCK_RISK",
                "built-in=YES → possível bloqueio via matchPropertyTable "
                "(mecanismo CONFIRMED fora deste script; dext shipped SEM isenção "
                "...builtin — CONFIRMED ausência); aplicabilidade em produção "
                "UNKNOWN sem ioreg vivo → INDETERMINATE parcial nesta perna")
    if presence == "NO":
        return ("OK", "built-in=NO → sem bloqueio por esta perna")
    if presence == "ABSENT":
        return ("OK", "built-in=ABSENT (chave ausente) → sem bloqueio por esta perna")
    return ("UNKNOWN", "built-in=UNKNOWN → dado ausente; produção UNKNOWN → INDETERMINATE parcial")


def eval_tunnelled(presence):
    # INFORMATIVO APENAS. IOPCITunnelCompatible=True declara suporte a
    # Thunderbolt, não exclusão (Correção TGM0, binary-architecture.md §9).
    # NUNCA usar como prova de NO_MATCH.
    base = ("IOPCITunnelled=%s (propriedade do caminho IORegistry; presença real "
            "de túnel se verifica nos parents — guia Apple). " % presence)
    if presence == "YES":
        return ("INFO", base + "Compatível com o suporte declarado (IOPCITunnelCompatible=True). Informativo.")
    if presence in ("NO", "ABSENT"):
        return ("INFO", base + "Nó não-tunneled: TunnelCompatible NÃO exclui "
                "(filtro tunneled-only = UNKNOWN) → NUNCA usar como prova de NO_MATCH. Informativo apenas.")
    return ("INFO", base + "Valor desconhecido → nada se conclui; TunnelCompatible NÃO exclui. Informativo apenas.")


def eval_competitors(competitors):
    """Avalia concorrência (input opcional). Sempre heurística viva UNKNOWN.

    Retorna (status, texto, deve_rebaixar). `deve_rebaixar`=True somente quando há
    competidor plausível e o veredito-base era LIKELY_MATCH (arbitragem viva
    decide o vencedor; sem IOProbeScore/probe vivo não há prova).
    """
    if competitors is None:
        return ("NAO_AVALIADO", "concorrência não avaliada (input ausente — snapshot sem 'competitors' e sem --competitors)", False)
    if not isinstance(competitors, list):
        return ("UNKNOWN", "concorrência em formato inválido (esperada lista) → UNKNOWN", False)
    if len(competitors) == 0:
        return ("VAZIO", "lista de concorrentes vazia → sem disputa conhecida", False)
    notes = []
    plausible = False
    for comp in competitors:
        if not isinstance(comp, dict):
            notes.append("entrada inválida (não-objeto) ignorada")
            continue
        name = comp.get("name", comp.get("IOClass", "?"))
        probe = comp.get("IOProbeScore", comp.get("probeScore"))
        cat = comp.get("IOMatchCategory", comp.get("category"))
        try:
            probe_n = int(str(probe), 0) if probe is not None else 0
        except (ValueError, TypeError):
            probe_n = 0
        # TinyGPU tem IOProbeScore ABSENT (= default 0) e categoria própria.
        # Qualquer probe explícito > 0 ou categoria distinta = disputa não-provada.
        if probe_n > 0:
            plausible = True
            notes.append("%s: IOProbeScore=%d > default TinyGPU (ABSENT=0) → pode vencer arbitragem viva (UNKNOWN)" % (name, probe_n))
        elif cat is not None and cat != TINYGPU_IO_MATCH_CATEGORY:
            plausible = True
            notes.append("%s: IOMatchCategory=%r ≠ %r → arbitragem isolada por categoria; coexistência NÃO provada (UNKNOWN)" % (name, cat, TINYGPU_IO_MATCH_CATEGORY))
        else:
            notes.append("%s: sem sinal de prioridade (probe ausente, mesma categoria) → disputa UNKNOWN" % name)
            plausible = True
    text = "%d concorrente(s): " % len(competitors) + "; ".join(notes)
    text += ". Sem probe/match vivo, vencedor = UNKNOWN."
    return ("DISPUTA_ABERTA" if plausible else "VAZIO", text, plausible)


def evaluate(snapshot, competitors_override=None):
    vendor_raw = get_field(snapshot, "vendor-id", "vendor_id", "vendor", "IOPCIVendorID")
    device_raw = get_field(snapshot, "device-id", "device_id", "device", "IOPCIDeviceID")
    class_raw = get_field(snapshot, "class-code", "class_code", "class", "IOPCIClassCode")
    subsys_vendor_raw = get_field(snapshot, "subsystem-vendor-id", "subsystem_vendor_id", "subsystemVendorID")
    subsys_id_raw = get_field(snapshot, "subsystem-id", "subsystem_id", "subsystemID")
    revision_raw = get_field(snapshot, "revision-id", "revision_id", "revision", "revisionID")
    builtin_raw = get_field(snapshot, "built-in", "built_in", "builtin", "builtIn", default=None)
    tunnelled_raw = get_field(snapshot, "IOPCITunnelled", "tunnelled", "tunnel", "IOPCITunneled", default=None)

    # Chave ausente vs valor UNKNOWN: distingue ABSENT (sem chave) de UNKNOWN.
    builtin_present = any(k in snapshot for k in ("built-in", "built_in", "builtin", "builtIn")) or \
        any(str(k).lower().replace("_", "-") == "built-in" for k in snapshot)
    tunnelled_present = any(k in snapshot for k in ("IOPCITunnelled", "tunnelled", "tunnel", "IOPCITunneled")) or \
        any(str(k).lower().replace("_", "-") in ("iopcitunnelled", "tunnelled", "tunnel", "iopcitunneled") for k in snapshot)

    vendor = parse_hex(vendor_raw)
    device = parse_hex(device_raw)
    class_code = parse_hex(class_raw)
    subsys_vendor = parse_hex(subsys_vendor_raw)
    subsys_id = parse_hex(subsys_id_raw)
    revision = parse_hex(revision_raw)
    builtin = norm_presence(builtin_raw, missing="ABSENT" if not builtin_present else "ABSENT")
    # Se a chave existe com null explícito, plistlib/ioreg diria ausente → ABSENT.
    tunnelled = norm_presence(tunnelled_raw, missing="ABSENT" if not tunnelled_present else "ABSENT")

    class_status, class_text = eval_class(class_code)
    vendor_status, vendor_text = eval_vendor(vendor)
    device_status, device_text = eval_device(device)
    builtin_status, builtin_text = eval_builtin(builtin)
    tunnelled_status, tunnelled_text = eval_tunnelled(tunnelled)

    competitors = competitors_override
    if competitors is None:
        competitors = snapshot.get("competitors", snapshot.get("competitors_list", None))
    comp_status, comp_text, comp_downgrade = eval_competitors(competitors)

    # ---- Veredito final (tunnel NUNCA decide; concorrência só rebaixa) ----
    reasons = []
    if class_status == "FAIL":
        final = FINAL_NO_MATCH
        reasons.append("classe não-display reprova IOPCIClassMatch (plist)")
    elif vendor_status == "FAIL":
        final = FINAL_NO_MATCH
        reasons.append("vendor fora da allowlist de entitlement de produção")
    elif class_status == "UNKNOWN" or vendor_status == "UNKNOWN":
        final = FINAL_INDET
        reasons.append("classe e/ou vendor não avaliáveis (dado ausente)")
    else:  # classe PASS + vendor PASS
        if builtin_status == "BLOCK_RISK":
            final = FINAL_NO_MATCH  # LIKELY (não definitivo): produção UNKNOWN
            reasons.append("built-in=YES → bloqueio LIKELY via matchPropertyTable "
                           "(produção UNKNOWN sem ioreg vivo → INDETERMINATE parcial nesta perna)")
        elif builtin_status == "UNKNOWN":
            final = FINAL_INDET
            reasons.append("built-in UNKNOWN → gate de produção não decidível")
        else:
            final = FINAL_MATCH
            reasons.append("classe + vendor ok, sem built-in, sem filtro por device (ausência)")

    caveats = []
    # Tunnel é sempre ressalva informativa, nunca muda o final.
    caveats.append("tunnel informativo apenas (TunnelCompatible declara suporte, não exclui)")
    if competitors is None:
        caveats.append("concorrência não avaliada (input ausente)")
    elif comp_status == "VAZIO":
        pass  # sem disputa conhecida
    else:
        caveats.append("concorrência: %s" % comp_status)
    if final == FINAL_MATCH and comp_downgrade:
        final = FINAL_INDET
        reasons.append("rebaixado por concorrência plausível (arbitragem viva UNKNOWN)")
    if final == FINAL_MATCH:
        caveats.append("ressalvas: IOKit vivo (probe/categoria/driver anexado) não observável estaticamente")

    subsys_text = ("subsys vendor=%s id=%s revision=%s → sem chave IOPCI* correspondente "
                   "no plist → NEUTRO (nenhum filtro; informativo)"
                   % (fmt_hex(subsys_vendor), fmt_hex(subsys_id), fmt_hex(revision)))

    return {
        "vendor": vendor, "device": device, "class_code": class_code,
        "subsys_vendor": subsys_vendor, "subsys_id": subsys_id, "revision": revision,
        "builtin": builtin, "tunnelled": tunnelled,
        "class_status": class_status, "class_text": class_text,
        "vendor_status": vendor_status, "vendor_text": vendor_text,
        "device_status": device_status, "device_text": device_text,
        "builtin_status": builtin_status, "builtin_text": builtin_text,
        "tunnelled_status": tunnelled_status, "tunnelled_text": tunnelled_text,
        "subsys_text": subsys_text,
        "comp_status": comp_status, "comp_text": comp_text,
        "final": final, "reasons": reasons, "caveats": caveats,
    }


def render_report(result, source_label):
    L = []
    L.append("TinyGPU match check — análise estática (snapshot IORegistry vs personality TinyGPU)")
    L.append("Fonte personality: docs/tinygpu/binary-architecture.md §3/§9/§10 + Info.plist do dext")
    L.append("Snapshot: %s" % source_label)
    L.append("Personality: IOProviderClass=%s, IOPCIClassMatch=0x%08X, "
             "Primary/Match/Secondary=ABSENT, TunnelCompatible=True, "
             "entitlements %s" % (TINYGPU_IO_PROVIDER_CLASS,
                                  TINYGPU_IOPCI_CLASS_MATCH,
                                  TINYGPU_ENTITLEMENT_DESC))
    L.append("")
    L.append("PCI class: %s — %s" % (result["class_status"], result["class_text"]))
    L.append("Vendor: %s — %s" % (result["vendor_status"], result["vendor_text"]))
    L.append("Device: %s — %s" % (result["device_status"], result["device_text"]))
    L.append("Subsystems/Revision: %s" % result["subsys_text"])
    L.append("Built In: %s — %s" % (result["builtin_status"], result["builtin_text"]))
    L.append("Tunnelled: %s — %s" % (result["tunnelled_status"], result["tunnelled_text"]))
    L.append("TunnelCompatible: True — declara suporte a Thunderbolt; NÃO exclui nós "
             "não-tunneled (Correção TGM0); filtro tunneled-only = UNKNOWN; "
             "NUNCA usar Tunnelled como prova de NO_MATCH")
    L.append("Concorrência: %s — %s" % (result["comp_status"], result["comp_text"]))
    L.append("")
    L.append("Final: %s" % result["final"])
    L.append("Motivos: %s" % ("; ".join(result["reasons"]) if result["reasons"] else "—"))
    if result["caveats"]:
        L.append("Ressalvas: %s" % ("; ".join(result["caveats"])))
    L.append("Disclaimer: %s." % DISCLAIMER)
    return "\n".join(L) + "\n"


# ---------------------------------------------------------------------------
# Self-test embutido (--self-test): 3 casos, exit 0 se todos passam
# ---------------------------------------------------------------------------

def self_test_cases():
    # (a) RTX interna HIPOTÉTICA: classe+vendor ok, sem built-in, sem túnel →
    #     honestamente LIKELY_MATCH com ressalvas (tunnel informativo,
    #     arbitragem viva não observada). Sem built-in não há perna built-in.
    case_a = {
        "vendor-id": "0x10DE",
        "device-id": "0x2504",
        "class-code": "0x030000",
        "subsystem-vendor-id": "0x10DE",
        "subsystem-id": "0x2504",
        "revision-id": "0xA1",
        "built-in": "ABSENT",
        "IOPCITunnelled": "ABSENT",
    }
    # (b) Mesmo nó mas com built-in=YES → LIKELY_NO_MATCH (mecanismo
    #     matchPropertyTable LIKELY bloqueia; produção UNKNOWN → registra
    #     INDETERMINATE parcial na perna, mas o veredito agregado é NO_MATCH
    #     provável, não definitivo). Aceita-se INDETERMINATE como alternativa
    #     honesta caso a política de agregação mude.
    case_b = dict(case_a, **{"built-in": "YES"})
    # (c) Classe não-display (ex.: network 0x020000) → LIKELY_NO_MATCH
    #     determinístico no plist, independente de vendor/túnel/built-in.
    case_c = dict(case_a, **{"class-code": "0x020000"})
    return [
        ("a-interna-hipotetica-sem-builtin-sem-tunel", case_a, {FINAL_MATCH}),
        ("b-builtin-yes", case_b, {FINAL_NO_MATCH, FINAL_INDET}),
        ("c-classe-nao-display", case_c, {FINAL_NO_MATCH}),
    ]


def run_self_test(verbose=True):
    failures = 0
    for name, snapshot, expected in self_test_cases():
        result = evaluate(snapshot)
        ok = result["final"] in expected
        if verbose:
            print("=" * 72)
            print("self-test %s: esperado %s, obtido %s → %s"
                  % (name, "/".join(sorted(expected)), result["final"],
                     "PASS" if ok else "FAIL"))
            print(render_report(result, source_label="--self-test:%s" % name))
        if not ok:
            failures += 1
    # Checagem estrutural: o relatório deve conter as 7 seções + disclaimer.
    probe = render_report(evaluate(self_test_cases()[0][1]), source_label="probe")
    required = ["PCI class:", "Vendor:", "Device:", "Built In:", "Tunnelled:",
                "TunnelCompatible:", "Final:", DISCLAIMER]
    for token in required:
        if token not in probe:
            print("self-test ESTRUTURA: token ausente %r → FAIL" % token)
            failures += 1
    # Checagem semântica: tunnel NUNCA decide — mesmo snapshot com
    # IOPCITunnelled=YES deve dar o mesmo Final do caso (a).
    alt = dict(self_test_cases()[0][1], IOPCITunnelled="YES")
    if evaluate(alt)["final"] != evaluate(self_test_cases()[0][1])["final"]:
        print("self-test TUNNEL: Tunnelled alterou o Final → FAIL (proibido)")
        failures += 1
    elif verbose:
        print("self-test tunnel-neutralidade: PASS (YES vs ABSENT não muda o Final)")
    if verbose:
        print("=" * 72)
        print("self-test: %s" % ("ALL PASS" if failures == 0 else "%d FALHA(S)" % failures))
    return 1 if failures else 0


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def load_json_file(path):
    with open(path, "r", encoding="utf-8") as fh:
        return json.load(fh)


def main(argv=None):
    parser = argparse.ArgumentParser(
        description="Análise estática: snapshot IORegistry (JSON) vs personality "
                    "TinyGPU embutida. Sem hardware/driver/MMIO/boot.")
    parser.add_argument("snapshot", nargs="?",
                        help="caminho do snapshot JSON (ausente → lê stdin)")
    parser.add_argument("--competitors", default=None,
                        help="JSON opcional: lista [{name, IOProbeScore, "
                             "IOMatchCategory, ...}] ou objeto {'competitors': [...]}")
    parser.add_argument("--self-test", action="store_true",
                        help="roda os 3 casos embutidos e sai 0 se todos passam")
    parser.add_argument("--json", action="store_true",
                        help="emite também o resultado estruturado em JSON (stderr fica o texto)")
    args = parser.parse_args(argv)

    if args.self_test:
        return run_self_test(verbose=True)

    competitors_override = None
    if args.competitors:
        try:
            raw = load_json_file(args.competitors)
        except (OSError, json.JSONDecodeError) as exc:
            print("erro: --competitors ilegível: %s" % exc, file=sys.stderr)
            return 2
        competitors_override = raw.get("competitors", raw) if isinstance(raw, dict) else raw

    try:
        if args.snapshot:
            snapshot = load_json_file(args.snapshot)
            label = args.snapshot
        else:
            if sys.stdin.isatty():
                parser.print_usage(sys.stderr)
                print("erro: forneça SNAPSHOT.json ou pipe via stdin (ou --self-test)",
                      file=sys.stderr)
                return 2
            snapshot = json.load(sys.stdin)
            label = "<stdin>"
    except (OSError, json.JSONDecodeError) as exc:
        print("erro: snapshot ilegível: %s" % exc, file=sys.stderr)
        return 2
    if not isinstance(snapshot, dict):
        print("erro: snapshot deve ser um objeto JSON", file=sys.stderr)
        return 2

    result = evaluate(snapshot, competitors_override=competitors_override)
    print(render_report(result, source_label=label), end="")
    if args.json:
        print(json.dumps({"source": label, "result": result,
                           "personality": {
                               "IOProviderClass": TINYGPU_IO_PROVIDER_CLASS,
                               "IOPCIClassMatch": "0x%08X" % TINYGPU_IOPCI_CLASS_MATCH,
                               "IOPCIPrimaryMatch": "ABSENT",
                               "IOPCIMatch": "ABSENT",
                               "IOPCISecondaryMatch": "ABSENT",
                               "IOPCITunnelCompatible": True,
                               "IOProbeScore": "ABSENT",
                               "IOMatchCategory": TINYGPU_IO_MATCH_CATEGORY,
                               "entitlements": TINYGPU_ENTITLEMENT_DESC}},
                          indent=2, ensure_ascii=False))
    return 0


if __name__ == "__main__":
    sys.exit(main())
