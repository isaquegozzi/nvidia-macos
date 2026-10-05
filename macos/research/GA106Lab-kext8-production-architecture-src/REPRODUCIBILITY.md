# REPRODUCIBILITY — 1.7.0 (P68 §19)

Metodo P64: segunda build em copia segura /tmp/GA106Lab-kext7-repro
(sem mudar ownership), SKIP_MUTATIONS=1 (mutacoes nao alteram artefatos).
Resultado: KEXT byte-identical, CLI byte-identical, plist identical
(SHA iguais nas duas builds; ver MANIFEST-P68.txt). Primeira build (completa,
com harnesses) gerou mesmos artefatos; segundo build confirmou. BUILD_OK nas
duas vias unitarias; harness sysmem 16/16 CAUGHT na primeira.
