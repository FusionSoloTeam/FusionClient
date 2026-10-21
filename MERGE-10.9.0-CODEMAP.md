# VClient / TClient merge to 10.9.0 — codemap & status

## Result: merge complete, compiles, runs

| Check | Result |
|---|---|
| Conflict files | 35 → **0 unresolved** |
| Build | `mingw32-make -j16 DDNet` → **exit 0, 0 errors** |
| Binary | `build/DDNet.exe`, 24.5 MB, 23:30 |
| Runtime | Vulkan 1.4 context on RTX 5060 Ti, sound OK, serverbrowser OK, **no crash/assert** in 18 s |
| HUDE | `tclient_hud_layout.cfg` loaded cleanly |
| Version | `TCLIENT_VERSION "10.9.0"`, DDNet `GAME_RELEASE_VERSION 20.1` |
| Committed? | **No — merge left in progress (MERGE_HEAD intact), as requested** |

## Ground truth (established this session)

| Item | Value |
|---|---|
| Fork's real upstream base | TClient **V10.8.7** (`4e4269396`, DDNet 19.8, Mar 2026) |
| Fork's history | Was **orphaned** — `5f6fba77b "Create README"` had an empty tree, then sources re-uploaded. No merge base with upstream. |
| Fix | Re-parented `b47795e4d` onto `4e4269396`, replayed 6 commits, verified tree byte-identical (`20ef28049…`) |
| Merge base now | V10.8.7 — a genuine 3-way merge |
| Upstream jump | V10.8.7 → V10.9.0: 718 files, +43,980 / −34,566 |

Safety refs: branch `backup/pre-merge-10.9.0`, tag `backup-pre-merge-2026-10-21`.
Backup: `Desktop\TClient-backup-2026-10-21` (4452 files, 1.6 GB, includes `.git`, excludes `build/`).

## API breaks upstream introduced, and how they were bridged

| Upstream change | Bridge |
|---|---|
| `MapScreen(float,float,float,float)` → `MapScreen(const CScreenRect&)` | **Compat overload** added in `IGraphics` (`graphics.h`) restoring exact old semantics — ~60 call sites untouched |
| `MapScreenToWorld(..., float aPoints[4])` → returns `CScreenRect` | All call sites converted |
| `GetScreen(float*,...)` → `CScreenRect GetScreen()` | All call sites converted |
| `minimum()` / `maximum()` **deleted** from `math.h` | **Re-added** (108 call sites preserved verbatim) |
| `base/system.h` **deleted**, split into dbg/log/mem/str/time/types | 10 of your files re-pointed |
| `engine/shared/http.h` → `engine/http.h`, `CHttpRequest` → `IHttpRequest` | `lyrics.cpp/h` ported |
| `IOSEEK_START` → `EIoSeekOrigin::START` | `menus_settings_assets.cpp` |
| `CSmoothValue::ChangeValue` → `ScaleValue` | `editor.cpp` |
| `MEMORY_BLOCK_USAGE_*` → `EMemoryBlockUsage::*` | `backend_vulkan.cpp` (+ added `NONE` for your spill accounting) |
| `CColorVertex` / `SetColorVertex` **removed** | Lyrics glow now uses `SetColor` (per-vertex vertical shading lost — cosmetic) |
| `SetAntiPingInterferenceCallback` → `SetAntiPingInterfereCallback` | Renamed |
| `MAX_LINE_LENGTH` → `MAX_CHAT_LENGTH` | Renamed |
| `CMapView::m_Zoom` → `Zoom()` accessor, `m_WorldZoom` dropped | Converted |
| `CLayerGroup::Mapping(float[4])` → `CScreenRect Mapping()` | Converted |
| `DoMapEditor` **deleted** (moved to `CMapView::Render`) | Dead duplicate removed |
| `RenderPlayerGhost` **deleted** | Your ghost logic was already absorbed into `RenderPlayer(RenderGhost)`; duplicate dropped |
| Multiview moved to `multiview.cpp` | Your `WorldAspect()` aspect-ratio fix ported there |

## Your features that needed manual reconstruction

| Feature | What happened |
|---|---|
| **Jelly deform** (`r_jelly`) | Upstream added `IsLocal` to `RenderTee`; yours added BodyScale/FeetScale/BodyAngle/FeetAngle. **Unified into one impl** carrying both |
| **Tiny tees** | `IsLocal` drives it — preserved through the unification |
| **HUDE / HUD layout** | `hud_layout.cpp` survived; `m_TopLeft`-based cursor clamp rewritten against `CScreenRect` |
| **Aspect ratio** (`WorldAspect()`) | Survived in `graphics.h`; `MapScreenToInterface` uses it instead of `ScreenAspect()`; ported into `multiview.cpp` |
| **Discord IPC backend** | You added a whole `#elif defined(DISCORD_IPC)` class; upstream's factory fragment landed inside it — your class restored intact |
| **Editor map tabs** | `editor.h` block kept; `LoadCurrentMap` → `LoadIngameMap`; `MapTabBar` re-declared after upstream's `UpdateBrushPicker()` |
| **Editor tele-lines / alignment guides** | ~981-line block restored verbatim |
| **Antiping frozen-player option** | Upstream independently made the same radio-menu change — no port needed |
| **Motion blur, mute-others, menu scale, chat options, prefixes, hook-coll overlay** | Ported into upstream's new `menus_settings_*.cpp` split |
| **Warlist previews** | **NOT ported** — needs reimplementation in `menus_settings_appearance.cpp` (see below) |

## Upstream's settings split (why menus_settings.cpp conflicts were big)

`menus_settings.cpp` went from one file to 13. Functions moved out; keeping your versions in place would have caused duplicate symbols. Resolution: took upstream's side, then ported your edits into the right new file.

## Post-merge bug fixes (user-reported)

| # | Bug | Root cause | Fix |
|---|---|---|---|
| 1 | "Disable jump particles" did nothing | `tc_air_jump_hide_particles` was defined and shown in the UI but **never read** — pre-existing | Wired into all 3 `m_Effects.AirJump` sites; own tees only (matches the ring) |
| 2 | Gradient nicknames only coloured the sender | Name-range collection was gated on `Line.m_ClientId < 0`, so it only ran for *system* messages — pre-existing | Removed the guard; tagged names now get their own skin-tone gradient |
| 3 | Translate left blank space when disabled | `HSplitTop` reserved the row *before* the `if` | Moved the split inside the branch (incoming + outgoing) |
| 4 | Camera popped to the player at the end of a flight | Exponential chase left a steady-state lag (≈ velocity/speed, ~130u) that snapped when the 2 s cutoff fired | Fixed-duration smoothstep from a captured start to the **live** position, so it lands exactly |
| 5 | Chat preview took 220px of the Appearance→Chat page | Always reserved | `CHAT_PREVIEW_ENABLED = false` flag; preview code kept intact, space reclaimed |
| 6 | "Connection Problems" drawn in world space, lower-middle | `RenderConnectionWarning` inherited the scaled `MapScreen` left behind by `RenderFinishPrediction` | Sets `MapScreen(0,0,m_Width,m_Height)` itself; centred with `m_Width/2` |
| 7 | Discord RPC build version `1.1.0` | — | `VCLIENT_VERSION` → `"1.1"` |

Bugs 1, 2, 6 were pre-existing (present before the merge); 3, 4, 5, 7 were too, but all are now fixed.

## Known gaps / follow-ups

1. **Warlist scoreboard previews** (coloured name/clan previews in Appearance) — not ported.
2. **Chat scroll regions** (`tc_chat_scroll`) — config exists, UI not ported.
3. **Lyrics glow gradient** — per-vertex shading lost with `SetColorVertex` removal; folded into a single averaged shade.

## Build commands

```sh
cd build
cmake .
mingw32-make -j16 DDNet
```

## To review / revert / finish

```sh
git status                  # 0 conflicts, merge in progress
git diff --cached --stat    # the merge result
git merge --abort           # revert everything
git commit                  # when you're happy
```
