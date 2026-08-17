# Reconciling the July macOS work with the August Windows work

Written 2026-08-16. Untracked scratch file — delete once the merge has landed.

## Where things stand right now

Nothing is lost. Your uncommitted macOS edits are committed on a local branch.

| Ref | Commit | What it is |
|---|---|---|
| `mac-wip` | `4ec463e` | **All the July macOS work**, including the untracked files. Currently checked out. |
| `main` | `db0ea6d` | Initial commit, 32 behind `origin/main`. No local commits — it never had any. |
| `origin/main` | 32 commits | The August Windows/packaging work. |

Working tree is clean. A test merge was run and **aborted**, so there is no
half-finished merge state to clean up.

> ✅ **`mac-wip` was pushed to `origin/mac-wip` on 2026-08-16.** It is safe
> off-machine; the local branch tracks the remote.

## What actually happened

Not concurrent editing. The timeline:

- **Jul 23–31** — macOS work. Never committed, never pushed.
- **Aug 7–11** — Windows work, started from the last *pushed* commit (`db0ea6d`),
  so it never saw the macOS work and re-plowed some of the same ground.

Your recollection that you didn't touch the Mac code after starting Windows is
**correct** — the dates don't overlap.

(The `2026-08-16` mtimes on the conflicted files are an artifact of
`git merge --abort` rewriting them, not real edit dates.)

## The key finding: two complete, self-consistent designs

Both cover all three ports. Neither is half-finished.

| | July (`mac-wip`) | August (`origin/main`) |
|---|---|---|
| Toolbar | SVG icon buttons, 18×18 | plain text buttons |
| Find | separate bar, "1 of 5" match count, prev/next/close | inline input, no match count |
| Windows | multi-window (link opens its own window, Swift-matching) | single-window |
| Python launch | reopens most recent document | empty state |

**`origin/main` is consistent across all three ports** — verified: 0 `QIcon`
refs in C++ and Python, 8 text buttons in Rust, inline find in all three,
single-window in all three.

So the Windows testing was valid. Nothing observed there was misleading.

`shared/spec/SPEC.md` (added during the August work) explicitly specifies
single-window and the empty-state-on-launch behaviour, so August's direction is
the documented one, decided later and with a rationale.

## Decision

**Take `origin/main` across the board.** Confirmed for Rust ("the toolbar on Rust
sucked — which is why we changed it"); the same reasoning was extended to C++ and
Python because cherry-picking July's icon toolbars into those two is precisely
what would *create* the cross-port inconsistency that isn't there today.

### What this discards (all recoverable from `mac-wip`)

- icon toolbars in all three ports
- find bar match count + prev/next/close buttons
- multi-window support in all three ports
- Python launch-reopens-most-recent

### What to carry over

- `cpp/scripts/fix_bundle_deps.sh` — **keep.** Fills a real gap:
  `cpp/packaging/build_mac` runs `macdeployqt` but does *not* do the cross-keg
  Homebrew rewrite, and Homebrew Qt is still supported. Without it,
  `QtWebEngineProcess` still loads QtCore/QtGui from `/opt/homebrew`.
- `shared/icons/export.svg` redraw + `shared/icons/document.svg` — optional.
  Strict improvements to the shared assets, but nothing references them under
  this plan.

### ⚠️ Trap to avoid

`cpp/tests/nav_smoke.cpp` and `python/tests/nav_smoke.py` are macOS-only files
that **never conflicted**, so a naive merge pulls them in silently. Both assert
multi-window behaviour (`len(appmod._open_windows) >= 2`). They will fail against
single-window code. **They must be dropped along with the feature.**

## Progress

- [x] **Step 1** — `main` fast-forwarded to `origin/main` (`7f44e09`).
- [x] **Step 2** — `cpp/scripts/fix_bundle_deps.sh` restored and committed
      (`da6dd18`), mode `100755`. `main` is now 1 ahead of `origin/main`, unpushed.
- [ ] **Step 3** — build and run all three ports on macOS.

## Steps to execute

```bash
cd /Users/dws/src/marklens-ports

# 1. Save the one file worth keeping, before switching branches
cp cpp/scripts/fix_bundle_deps.sh /tmp/fix_bundle_deps.sh   # untracked, so it
                                                            # actually survives
                                                            # the checkout anyway

# 2. Fast-forward main to the August work
git checkout main
git merge --ff-only origin/main

# 3. Restore the macOS bundling script
mkdir -p cpp/scripts
cp /tmp/fix_bundle_deps.sh cpp/scripts/fix_bundle_deps.sh
chmod +x cpp/scripts/fix_bundle_deps.sh
git add cpp/scripts/fix_bundle_deps.sh
git commit -m "Add fix_bundle_deps.sh for Homebrew Qt cross-keg deps on macOS"

# 4. Sanity-check: build and run all three ports on macOS
```

`mac-wip` stays untouched as the full record of the July work. Anything from the
July design can come back later as a deliberate change applied to all three
ports at once.

## If you'd rather not discard the July UI

The alternative — never fully explored — is to treat the July design as the
target and forward-port it onto the August base *for all three ports together*,
keeping August's packaging, spec, GTK/WebKitGTK fixes and single-window model
while restoring the icon toolbars and richer find bar. That is real work and a
separate decision; it should not be smuggled into this merge.
