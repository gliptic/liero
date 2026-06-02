# Clang flag-day plan

Handover document for the tree-wide clang-format / clang-tidy
normalization. Written 2026-06-01 by Claude (via /agent-skills:using-agent-skills),
in dialogue with the maintainer.

## Why

PR #84 wired `clang-tidy` and `clang-format` into CI, but the historical
code is a mess of competing conventions:

- `src/game/`, `src/tc_tool/`, `src/video_tool/` use **hard tabs (width
  4), Allman braces, 80-col, `PointerAlignment: Left`** — a bespoke
  config inherited from the original Liero source.
- `src/tests/` uses a Google/Chromium-style override
  (`src/tests/.clang-format`): 2-space, attached braces.

CI today is diff-only: only changed lines are gated, so pre-existing
drift is invisible. `clang-format` is also advisory
(`continue-on-error: true`). That setup keeps new code clean but never
forces the old code to converge — the codebase will stay split forever
without a deliberate normalization pass.

Goal: one flag-day window where the entire tree is normalized to a
single, modern, well-supported standard, after which both tools run
tree-wide and block merge.

## Decisions

| Axis | Choice | Why |
|------|--------|-----|
| Base style | **Google C++ Style, with naming enforced** | Most widely-recognized C++ style; published, documented, contributor-familiar. Enforcing naming (not just whitespace) means the codebase actually conforms to the standard rather than being "Google-shaped with Liero identifiers". |
| Column limit | **100** | Modern Google-derived projects (Chromium, Abseil) relax 80 → 100. Reduces line-wrapping in template- and rollback-heavy code without hurting side-by-side diff readability. |
| Indent / braces | 2-space, attached (K&R) | Google defaults. Same as `src/tests/` already uses, so the test override goes away. |
| Pointer alignment | Left (`int* p`) | Matches current bespoke config; consistent with Google. `DerivePointerAlignment: false` to stop clang-format auto-flipping per file. |
| Test override | **Drop `src/tests/.clang-format`** | Redundant once the root config is Google. |
| Tidy strategy | **Fix all warnings** | Auto-`--fix` where safe, hand-fix the rest. After the flag day, tidy runs tree-wide and blocks merge. |
| PR strategy | **Three sequential PRs**: format → naming → other tidy | Each PR mechanically reviewable; bisect stays sane; if one phase reveals problems, earlier work isn't lost. |
| Git history | Add each flag-day commit to `.git-blame-ignore-revs` | Preserves blame across mechanical reformat. |

### Alternatives rejected

- **LLVM style** — naming-agnostic, smaller diff, simpler config. Was a
  reasonable choice; rejected because Google's broader recognition won
  out once the maintainer confirmed they wanted the marginal cost of
  doing naming too (we're already touching every line anyway).
- **Keep bespoke** — would lock the project into maintaining a custom
  `.clang-format` forever with no off-the-shelf docs for new
  contributors.
- **Google whitespace only (no naming)** — honestly just LLVM with
  extra config. If we're going Google, commit to it.
- **One mega-PR** — diff would be unreviewable. Any single problem
  blocks the entire flag day.
- **80-col strict Google** — wraps too aggressively given the existing
  signature length in rollback/serialization code.

## The standard, concretely

### `.clang-format` (replacement)

```yaml
---
BasedOnStyle: Google
ColumnLimit: 100
DerivePointerAlignment: false
PointerAlignment: Left
IncludeBlocks: Preserve
SortIncludes: CaseSensitive
# ForEachMacros / StatementMacros / WhitespaceSensitiveMacros: copy
# forward from the current .clang-format so cereal/SDL macros aren't
# rewrapped.
...
```

### `.clang-tidy` (delta)

Keep the existing `*` − subtractions list. Two changes:

1. **Remove** `-readability-identifier-naming` from the disabled list.
2. **Add** the naming convention with ADL-hook carve-outs:

```yaml
CheckOptions:
  # ... existing options retained ...
  - { key: readability-identifier-naming.ClassCase,           value: CamelCase }
  - { key: readability-identifier-naming.StructCase,          value: CamelCase }
  - { key: readability-identifier-naming.EnumCase,            value: CamelCase }
  - { key: readability-identifier-naming.FunctionCase,        value: CamelCase }
  - { key: readability-identifier-naming.MethodCase,          value: CamelCase }
  - { key: readability-identifier-naming.VariableCase,        value: lower_case }
  - { key: readability-identifier-naming.ParameterCase,       value: lower_case }
  - { key: readability-identifier-naming.MemberCase,          value: lower_case }
  - { key: readability-identifier-naming.PrivateMemberSuffix, value: '_' }
  - { key: readability-identifier-naming.ProtectedMemberSuffix, value: '_' }
  - { key: readability-identifier-naming.ConstantCase,        value: CamelCase }
  - { key: readability-identifier-naming.ConstantPrefix,      value: 'k' }
  - { key: readability-identifier-naming.EnumConstantCase,    value: CamelCase }
  - { key: readability-identifier-naming.EnumConstantPrefix,  value: 'k' }
  - { key: readability-identifier-naming.MacroDefinitionCase, value: UPPER_CASE }
  - { key: readability-identifier-naming.NamespaceCase,       value: lower_case }
  # Cereal ADL hooks must keep their lowercase names — they are looked
  # up by name, so renaming silently breaks serialization. The set
  # below was extended during PR2: cereal also calls back into user
  # archives via setNextName/startNode/finishNode/saveValue/loadValue/
  # saveBinaryValue/loadBinaryValue/sizeTag, and the free-function
  # hooks prologue/epilogue via ADL.
  - { key: readability-identifier-naming.MethodIgnoredRegexp,
      value: '^(serialize|save|load|save_minimal|load_minimal|setNextName|startNode|finishNode|saveValue|loadValue|saveBinaryValue|loadBinaryValue|sizeTag)$' }
  - { key: readability-identifier-naming.FunctionIgnoredRegexp,
      value: '^(serialize|save|load|save_minimal|load_minimal|prologue|epilogue)$' }
```

### What's safe

- `cereal::make_nvp(...)` is invoked with **explicit string literals**
  throughout (`"wormExplode"`, `"drawOnMap"`, etc.), not
  `CEREAL_NVP(x)` stringification. So renaming the C++ fields does
  **not** change the on-disk save/snapshot format. Verified by grep
  before the plan was finalized.

## Execution: three PRs

Each PR is self-contained. Land them back-to-back within a few days —
the longer the window between PR1 and PR3, the more painful every
in-flight feature branch becomes to rebase.

Each PR modifies only the config for the tool it runs: PR1 touches
`.clang-format`, PR2 touches `.clang-tidy` (to enable naming), PR3
touches the `.clang-tidy` workflow (to re-enable CI tree-wide).

Note: PR1 also has to **temporarily disable** the existing diff-only
clang-tidy CI. Because the reformat touches every line in `src/`,
diff-only gating becomes equivalent to a tree-wide run, exposing the
backlog of pre-existing tidy violations that diff-only had been
hiding. PR3 fixes those violations and re-enables the workflow as
tree-wide blocking.

### Pre-flag-day

1. Merge or close all open feature PRs that you can.
2. Announce a soft freeze on non-trivial new PRs for the duration.
3. Tag a `pre-clang-flag-day` git tag so reverting the whole window is
   one command.

### PR 1 — Format flag day

**Scope:** whitespace, braces, wrapping, include ordering. No
semantic changes, no identifier renames.

1. Replace `.clang-format` with the Google-based config above.
2. Delete `src/tests/.clang-format`.
3. Run formatter tree-wide, excluding generated files:
   ```bash
   find src -type f \( -name '*.cpp' -o -name '*.hpp' \
                     -o -name '*.h' -o -name '*.cc' -o -name '*.cxx' \) \
     ! -path 'src/game/metadata.cpp' \
     -print0 | xargs -0 clang-format -i
   ```
4. Flip `.github/workflows/clang-format.yml`:
   - Remove `continue-on-error: true`.
   - Change the script invocation from diff-only
     (`scripts/clang-format-diff.sh`) to tree-wide
     (`scripts/clang-format-all.sh` — create it, mirroring the
     existing diff script but iterating all `src/**` files and exiting
     non-zero on any diff).
5. Add a `clang-format-all` CMake target if not present (CLAUDE.md
   references it; verify it exists and is up to date).
6. Create `.git-blame-ignore-revs` at the repo root and add the PR1
   merge commit SHA. Document the local setup hint in `CLAUDE.md`:
   ```bash
   git config blame.ignoreRevsFile .git-blame-ignore-revs
   ```
7. **Verify:**
   - Full build all four configured presets (`linux-x64`,
     `linux-x64-debug`, `linux-x64-ci`, and at least one cross
     target you actively use).
   - `ctest --test-dir build/linux-x64-ci --output-on-failure` clean.
     Includes `test_determinism` / `test_rollback_*` — format-only
     changes shouldn't affect determinism, but these are cheap
     insurance.
   - `scripts/clang-format-diff.sh origin/master` reports clean on
     a freshly-formatted tree.

**Expected diff:** very large but mechanical. Review is "did
clang-format produce sane output," not line-by-line.

**Failure modes to watch:**
- Lines `// clang-format off` / `// clang-format on` that may be
  missing where macros need protection. Search the bespoke config's
  `StatementMacros` / `WhitespaceSensitiveMacros` / `ForEachMacros`
  lists and carry them forward into the new `.clang-format`.
- Mass reordering of `#include`s where order matters (e.g., a
  Windows header that must precede another). `IncludeBlocks: Preserve`
  is set to avoid this; do not change it to `Regroup` in PR1.

### PR 2 — Naming flag day

**Scope:** identifier renames driven by
`readability-identifier-naming`. No other tidy fixes in this PR.

1. Update `.clang-tidy` with the naming options above. **Do not yet**
   re-enable other auto-fixes — keep the diff laser-focused on naming.
2. Configure a build with `compile_commands.json`:
   ```bash
   cmake --preset linux-x64-ci -DOPENLIERO_BUILD_TESTS=ON
   cmake --build build/linux-x64-ci --config Release --target game
   ```
3. Run clang-tidy with `--fix --fix-errors`:
   ```bash
   find src -type f \( -name '*.cpp' -o -name '*.cc' -o -name '*.cxx' \) \
     ! -path 'src/game/metadata.cpp' \
     | xargs -P "$(nproc)" -n1 clang-tidy -p build/linux-x64-ci \
       --fix --fix-errors \
       --checks='-*,readability-identifier-naming'
   ```
   Run iteratively — tidy may need multiple passes to settle template
   instantiations and cross-TU references.
4. Hand-fix any cross-TU renames tidy missed. Heavily templated code
   and code referenced through macros are the usual suspects.
5. **Critical: rebuild from clean and run the full verification
   protocol below.** Determinism is load-bearing in this codebase
   (CLAUDE.md), so the rollback and determinism tests are
   non-negotiable.
6. After verification, grep to confirm cereal hooks survived:
   ```bash
   grep -rn -E "void (Serialize|Save|Load)\s*\(" src/game/
   # Expected: no matches. If any appear, the carve-out regex failed
   # and you need to fix them by hand before merge.
   ```
7. Add PR2 merge commit SHA to `.git-blame-ignore-revs`.

**Expected diff:** large, but every change is a rename. Use a
file-by-file review approach; spot-check for missed cross-references
in templates.

**Failure modes to watch:**
- Cereal hooks accidentally renamed. The carve-out regex catches
  exact names — if the project uses `serializeSomething` it would not
  be carved out and would be renamed (probably correct behavior, but
  verify).
- Macros that paste identifiers (`#define FOO(x) x_helper`). Tidy
  doesn't see through token-paste; if any exist, they need manual
  attention and possibly `NOLINT`.
- Const expressions used as template arguments — renaming a `constexpr
  int frameRate` → `kFrameRate` is fine unless something stringifies
  it. None expected here, but verify.
- `process()` / `processFrame()` are renamed to `Process` /
  `ProcessFrame`. Confirm `Controller::process()` overrides in
  `LocalController`, `ReplayController`, `RollbackController` all get
  renamed consistently — virtual dispatch is name-based, so a
  half-rename silently breaks polymorphism.

### Lessons from PR2 — apply to PR3

These are things the plan didn't anticipate; PR3 will hit the same
patterns at greater volume because it runs the full check set, not
just naming.

1. **Use `run-clang-tidy`, not raw `xargs`.** Parallel `xargs -P N -n1
   clang-tidy --fix` races on shared header rewrites: multiple TUs
   apply conflicting edits to the same header and the last writer
   wins. `run-clang-tidy` exports per-file fix YAML and applies them
   in one serialized pass via `clang-apply-replacements`. Standard
   invocation:
   ```bash
   run-clang-tidy -p build/linux-x64-ci \
       -header-filter='.*/src/.*' \
       -fix -j "$(nproc)" -quiet \
       '/src/.*\.(cpp|cc|cxx)$'
   ```
   Iterate until two consecutive runs produce no diff.

2. **Per-preset blind spots.** `compile_commands.json` only contains
   TUs that the configured preset compiles. linux-x64-ci with default
   options misses, at minimum:
   - `#if OPENLIERO_EMSCRIPTEN` blocks (gfx.cpp's main loop, gameEntry)
   - `#if _WIN32` / `#ifdef _WIN32` blocks (filesystem.cpp's
     Windows directory iteration, iceBridge.hpp's `kBridgeInvalid`)
   - `src/video_tool/` (gated on `OPENLIERO_BUILD_VIDEOTOOL=ON`)
   - `src/tc_tool/` (gated on `OPENLIERO_BUILD_TCTOOL=ON`)

   Mitigation for PR3:
   - Configure with both tool flags: `cmake --preset linux-x64-ci
     -DOPENLIERO_BUILD_TESTS=ON -DOPENLIERO_BUILD_VIDEOTOOL=ON
     -DOPENLIERO_BUILD_TCTOOL=ON`.
   - Accept that Windows and Emscripten branches will only surface in
     CI. Plan for a fix-loop after the initial PR push: each platform
     CI failure is followed by a hand-fix commit. PR2 needed four
     such commits (emscripten, then Windows three times).

3. **Windows API macro collisions.** `windows.h` `#define`s many
   identifiers (`FindFirstFile` → `FindFirstFileA`, `DrawText` →
   `DrawTextA`, `MessageBox`, `CreateWindow`, `LoadImage`,
   `CopyFile`/`MoveFile`/`DeleteFile`, `GetMessage`, `SendMessage`,
   `Polygon`, `Rectangle`, etc.). If user code declares an identifier
   that matches one of these macros, every callsite gets
   preprocessor-rewritten on Windows. PR2 hit this with the
   `FindFirstFile`/`FindClose`/`FindNextFile` local wrappers in
   filesystem.cpp and with `Font::DrawText` (4 overloads, 76
   callsites). PR3's `modernize-*` checks are unlikely to introduce
   new collisions, but `readability-*` renames could; grep before
   merging:
   ```bash
   grep -rn -E '\b(FindFirstFile|FindNextFile|DrawText|MessageBox|CreateWindow|LoadImage|CopyFile|MoveFile|DeleteFile|GetMessage|SendMessage|Polygon|Rectangle|Ellipse|GetObject|Yield|GetCurrentTime|GetTickCount)\b' src/
   ```

4. **`bugprone-reserved-identifier` strips leading underscores.** This
   check is enabled (it's in the `*` set). It rewrites
   `_USE_MATH_DEFINES` to `USE_MATH_DEFINES` — but `_USE_MATH_DEFINES`
   is the exact spelling MSVC's `<cmath>` looks for to expose `M_PI`.
   Similar hazards: `_ENetHost`/`_ENetPeer` forward declarations
   (struct tag identity with enet.h's typedef), `_WIN32` (compiler
   builtin, not affected). After PR3's tree-wide run, grep for
   stripped underscores in macro `#define`s and struct forward decls;
   add `// NOLINT(bugprone-reserved-identifier,
   readability-identifier-naming)` for the load-bearing ones.

5. **Method/type name collisions surface as `[-Wchanges-meaning]`.**
   If a rename produces `Type Method()` where `Type` is also a class,
   enum, or typedef in the same scope, the compiler reports
   "declaration of `Type Foo::Method() const` changes meaning of
   `Type`". PR2 hand-fixed roughly a dozen of these (SessionState,
   IceAgent::State, Level::Rect, FileSelector::Menu,
   RollbackController::GameState, NetTransport::State,
   SoundPlayer::Common, …). PR3's modernize/bugprone passes shouldn't
   create new collisions, but if any auto-rename does, the fix is
   manual: pick a different method name and update callers.

6. **X-macro NOLINT region is already in place.** `constants.hpp`
   wraps the `LIERO_CDEFS` / `LIERO_SDEFS` / `LIERO_HDEFS` /
   `LIERO_SOUNDDEFS` block in `NOLINTBEGIN(readability-identifier-naming)`
   /`NOLINTEND` because tidy's check rewrites the token-paste
   arguments and silently breaks every callsite. PR3 should leave
   this region alone — but be aware that other tidy checks may still
   fire inside it.

7. **Range-for / cereal carve-outs.** `DirectoryListing::begin/end`
   must stay lower_case for range-for to find them (already NOLINT'd).
   The cereal carve-out regex in `.clang-tidy` covers all known cases
   PR2 surfaced — see the config snippet above.

### PR 3 — Remaining tidy fixes

**Scope:** everything else the existing `.clang-tidy` enables —
modernize-*, bugprone-*, performance-*, readability-* (minus the
naming check already landed in PR2).

1. Configure with all targets enabled so tidy sees `src/video_tool/`
   and `src/tc_tool/`:
   ```bash
   CI=true cmake --preset linux-x64-ci \
       -DOPENLIERO_BUILD_TESTS=ON \
       -DOPENLIERO_BUILD_VIDEOTOOL=ON \
       -DOPENLIERO_BUILD_TCTOOL=ON
   cmake --build build/linux-x64-ci --config Release --target game
   ```
   (The `CI=true` env is required by the `linux-x64-ci` preset's
   condition guard.)

2. Run `run-clang-tidy` with the full check list (no `-checks=` ⇒
   uses `.clang-tidy`'s configured set):
   ```bash
   run-clang-tidy -p build/linux-x64-ci \
       -header-filter='.*/src/.*' \
       -fix -j "$(nproc)" -quiet \
       '/src/.*\.(cpp|cc|cxx)$'
   ```
   Run iteratively — modernize fixes can unlock further fixes on the
   next pass. Stop when two consecutive runs produce no diff.

3. Many checks have no auto-fix. For those, decide per-warning:
   either fix by hand or apply `// NOLINT(check-name): rationale` at
   the call site. Avoid file-wide `NOLINTBEGIN/END` unless an entire
   file is genuinely incompatible.

4. Apply NOLINTs for the `bugprone-reserved-identifier` hazards
   (lesson 4 above) — at least:
   - `_USE_MATH_DEFINES` in `src/game/ai/predictive_ai.cpp` (already
     done in PR2)
   - `_ENetHost`/`_ENetPeer` forward declarations if tidy rewrites
     them

5. Format pass after the renames settle (modernize-loop-convert and
   similar checks shift line lengths):
   ```bash
   scripts/clang-format-all.sh
   ```
   then reformat any files it flags.

6. **Critical verification** — same protocol as PR2, but `modernize-*`
   checks can change iteration patterns in subtle ways that may
   affect determinism:
   - All three local presets (linux-x64, linux-x64-debug,
     linux-x64-ci) build clean
   - `ctest --test-dir build/linux-x64-ci --build-config Release`
     all-green
   - `test_determinism`, `test_rollback_correctness`,
     `test_rollback_desync` pass standalone
   - Replay corpus from before the flag day still plays back (run
     the `videotool` binary against a known-good `.lrp` file)

7. Re-enable clang-tidy CI and flip it to tree-wide blocking:
   - Restore the `pull_request` trigger in
     `.github/workflows/clang-tidy.yml` (it's currently
     `workflow_dispatch` only).
   - Add `scripts/clang-tidy-all.sh` that runs tidy across all `src/`
     files via the existing `compile_commands.json`.
   - Update `.github/workflows/clang-tidy.yml` to call the new
     script instead of `clang-tidy-diff.sh`.
   - Make sure CI configures with `-DOPENLIERO_BUILD_VIDEOTOOL=ON
     -DOPENLIERO_BUILD_TCTOOL=ON` so the gate covers the same TUs
     tidy was run against locally.

8. Expect platform CI to fail on the first push for code paths
   linux-x64-ci doesn't compile (emscripten + Windows). Fix-loop
   them as PR2 did — each platform CI failure produces one
   hand-fix commit. macOS-arm64 was clean in PR2 but should still
   be watched in PR3.

9. Add PR3 merge commit SHA to `.git-blame-ignore-revs`.

**Expected diff:** smaller than PR1/PR2 but more semantic. This is
the PR that needs the most careful human review per-change.

## Verification protocol

Run after PR1 (sanity), and **always** after PR2 and PR3:

```bash
# 1. Clean build, all relevant configurations.
#    linux-x64-ci's condition guard requires CI=true in the env.
cmake --workflow --preset linux-x64
cmake --workflow --preset linux-x64-debug
CI=true cmake --preset linux-x64-ci \
    -DOPENLIERO_BUILD_TESTS=ON \
    -DOPENLIERO_BUILD_VIDEOTOOL=ON \
    -DOPENLIERO_BUILD_TCTOOL=ON
cmake --build build/linux-x64-ci --config Release

# 2. Full test suite
ctest --test-dir build/linux-x64-ci --build-config Release --output-on-failure

# 3. Determinism-focused suites (subset of step 2, but explicitly
#    named because rollback/desync coverage is the load-bearing
#    safety net for these PRs):
./build/linux-x64-ci/test_determinism
./build/linux-x64-ci/test_rollback_correctness
./build/linux-x64-ci/test_rollback_desync

# 4. Replay/snapshot compatibility — replays recorded BEFORE the flag
#    day must still play back correctly. Keep a small corpus of .lrp
#    files from before PR1 in scripts/test-replays/ and play them
#    through `videotool` or a dedicated replay-check binary.

# 5. Manual smoke test: launch the game, play a single-player round,
#    confirm settings.toml writes and reads back identically.
```

Passing all five is the merge gate. If any fails, the PR does not
land — diagnose, fix, and re-verify rather than NOLINT-ing your way
out of a real regression.

## Risks and mitigations

| Risk | Likelihood | Mitigation |
|------|-----------|------------|
| Cereal `serialize`/`save`/`load` accidentally renamed | Medium | Carve-out regex in `.clang-tidy` (broadened during PR2 to also cover archive interface methods + prologue/epilogue); post-PR grep to confirm. |
| Modernize loops change iteration order → determinism break | Medium | `test_determinism` + `test_rollback_*` in verification. Revert offending check or NOLINT the call site. |
| Tidy `--fix` corrupts a TU via cross-TU rename race | **Was Low, now mitigated** | Use `run-clang-tidy` not raw xargs. PR3 inherits this. |
| Macros with stringified identifiers break | Low | None surfaced in PR2; remain vigilant for `CEREAL_NVP(x)` sneaking in. |
| Windows API macro collision (`DrawText` → `DrawTextA`) | **Materialized in PR2** | Grep for known Windows macro names before merging (see PR3 lesson 3). |
| `bugprone-reserved-identifier` strips load-bearing leading underscores (`_USE_MATH_DEFINES`, `_ENetHost`) | **Materialized in PR2** | NOLINT the affected lines; PR3 should grep `#define` / `struct` forward decls for stripped underscores. |
| Per-preset blind spots (emscripten + `_WIN32` + tool gates not in linux-x64-ci) | **Materialized 4× in PR2** | Configure with VIDEOTOOL/TCTOOL ON; expect a fix-loop on first CI run for emscripten + Windows. |
| Long format-day window blocks contributors | High | Land all three PRs within ~3 days. Pre-merge sweep of open PRs. |
| Reviewer fatigue on huge diffs | High | Three PRs not one. Reviewer's job is "did the tool do the right thing" + verification, not line-by-line. |

## Post-flag-day cleanup

PR1 already updated `CLAUDE.md`'s style description and removed
`src/tests/.clang-format`. PR2 did not need to touch `CLAUDE.md`
further. After PR3 lands:

1. Update `CLAUDE.md`'s note that "`clang-tidy` CI is temporarily
   disabled during the clang flag-day window" — remove that line and
   replace with the new tree-wide gating story.
2. Confirm `clang-tidy` / `clang-format` CMake targets still work
   tree-wide.
3. Drop the legacy "clang-format is advisory" comment in the workflow
   (if any remains).
4. Tag `post-clang-flag-day` so the cutover is easy to reference
   later.

## Open questions / things this plan does NOT cover

- **Server (`server/`)** — Go code, governed by `gofmt`, out of scope
  for this plan.
- **Emscripten preset** — wasm-specific code paths inside
  `#if OPENLIERO_EMSCRIPTEN` are invisible to tidy on the linux-x64-ci
  preset. PR2 fixed them post-hoc via the Emscripten CI failure; PR3
  should plan for the same fix-loop.
- **External contributor PRs** — anyone with an in-flight branch will
  have to rebase across all three commits. Communicate the flag day
  via README and any active discussion channels before starting.
- **Editor / IDE configs** — if `.vscode/settings.json` or similar
  exist with hard-coded tab settings, update them in PR1.

## Quick recap for whoever picks this up

You're going to:
1. Replace `.clang-format` with a Google-derived config (col 100),
   delete `src/tests/.clang-format`, run formatter tree-wide, ship.
2. Enable `readability-identifier-naming` with Google conventions and
   cereal ADL carve-outs, run tidy `--fix`, hand-fix the rest, ship.
3. Run the rest of `clang-tidy --fix` for modernize/bugprone/perf
   fixes, flip CI to tree-wide gating, ship.

After each: full build, full test suite (including
`test_determinism` and `test_rollback_*`), replay check. Add each
merge commit to `.git-blame-ignore-revs`.

When done: one style across the entire C++ tree, CI blocks merge on
any drift, contributor onboarding can just say "we use Google style."
