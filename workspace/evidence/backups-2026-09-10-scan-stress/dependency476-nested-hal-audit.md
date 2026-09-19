# Additional nested HAL dependency audit

Read-only audit on 2026-09-14; reference files were not restored or edited.

- HAL top-level HEAD remains 290edc31b50decca660c1a11ce3506fd9b2e1e27.
- Nested components/mbedtls/mbedtls HEAD remains its recorded gitlink,
  582ff482038db6e4010dbf6f943d97b05ad06ea5, but has 334 tracked modified files.
- All observed modified-file mtimes are at or before 2026-09-03 05:12:41
  local time, predating this resumed work. Timestamps alone do not establish
  who originally made the changes.
- 332 files compare byte-for-byte after normalizing the `esp_` namespace
  prefix in HEAD and worktree streams. The remaining ecp_curves.c and
  ecp_curves_new.c also compare exactly after additionally normalizing
  `esp_ ##` token-pasting prefixes. This comparison did not modify files.
- These are namespace conversions, including macro-generated names; the
  audit does not independently certify cryptographic behavior or ABI safety.
  The S31 HAL CMake adapter includes headers from this nested tree, so its
  contents are part of the actual build input, not an irrelevant clean-tree
  cosmetic difference.

Complete diff saved as hal-mbedtls-existing-476.patch (6351954 bytes), SHA256:

```
b71796ea181962d84ca3e4b97e8429cfb2ad86921ec754e3aeba1c0bbb821d5b
```

host476-f0-dependencies.log: F0_DEPENDENCY_LOCK=PASS for all four repositories.
Important limit: the reference verifier uses --ignore-submodules=all; this
pass does not assert pristine nested submodules. The supplemental patch and
prefix/macro audit logs preserve the actual state without changing reference
sources or weakening the existing lock. No clone/fetch/download was performed.
