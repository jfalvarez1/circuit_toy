# UI Declutter Plan (src/ui.c, src/input.c, include/ui.h)

Goal: make the left panel, scope controls and properties panel scale with the growing feature set without breaking existing shortcuts, saved circuits, or the pop-out scope. Each stage is independently buildable and testable. (Survey done 2026-08-24; line numbers are from that snapshot.)

## 1. Current layout map

| Region | Layout / state | Draw | Hit-test |
|---|---|---|---|
| Toolbar | `ui_init` ui.c:169-201 (absolute x, y=10) | `ui_render_toolbar` ui.c:1008 | `ui_handle_click` ui.c:6264-6302 (chain of `point_in_rect`) |
| Parts palette | items built by `ADD_TOOL/ADD_COMP/NEW_SECTION` ui.c:216-397; category assigned by `ranges[]` ui.c:404-415; bounds re-laid every frame in `ui_render_palette` ui.c:1145-1173 | `ui_render_palette` ui.c:1087, `draw_palette_item` ui.c:878 | headers ui.c:6452-6465, items ui.c:6467-6497, scrollbar ui.c:6406-6449; wheel via `ui_point_in_palette`/`ui_palette_scroll` ui.c:7233-7261 |
| Circuit templates | hand-listed `CircuitPaletteItem` blocks ui.c:431-645 (`col++` pattern); bounds re-laid in ui.c:1209-1238 | `draw_circuit_item` ui.c:943 | ui.c:6501-6530, gated by `categories[PCAT_CIRCUITS].collapsed` |
| My Circuits | rebuilt from `g_subcircuit_library` every frame ui.c:1245-1260 | ui.c:1262-1372 | after circuit items; right-click `ui_handle_right_click` ui.c:6561 |
| Properties | `ui_render_properties` ui.c:1555; each field pushes `ui->properties[n].bounds` (~100 call sites) | `draw_property_field` ui.c:1412, `draw_sweep_config` ui.c:1459 | ui.c:6400-6404 (`UI_ACTION_PROP_EDIT + prop_type`); wheel `ui_point_in_properties` ui.c:7286 |
| Scope screen | `scope_rect` in `ui_update_layout` ui.c:6975-6990; resize edges ui.c:5973-5990, `ui_handle_motion` ui.c:6591-6621 | `ui_render_oscilloscope` ui.c:3118-3998 | trigger/cursor drags ui.c:6129-6209; pan in input.c:705,902 |
| Scope controls | 18 `Button`s laid out with `PLACE_BTN` in `ui_update_layout` ui.c:6994-7040 (3 forced rows); info rows at hard-coded `r->h + 100` ui.c:4042-4111 | ui.c:3999-4038 | ui.c:6340-6398 (one `if` per button) |
| Pop-out scope | `ui_setup_popup_scope_coords` ui.c:7320 copies 19 button rects into `ScopeCoordsBackup` (ui.h:547) | same render fn on popup renderer | same click fn with swapped coords |
| Spotlight (Ctrl+K) | `ui_spotlight_*` ui.c:5107-5445; searches component names only | `ui_render_spotlight` ui.c:5321 | input.c:97-104 |
| Tooltips | `Button.tooltip` (ui.h:47) is filled everywhere but never drawn | - | - |

State lives in `UIState` (ui.h:150-410). No persisted UI settings (file_io.c only saves circuits), so layout changes cannot break saved data.

### Fragile parts found
1. `ranges[]` ui.c:404: any insertion into the ADD_COMP list shifts every later index; items past the table silently fall into the last category.
2. The circuit palette lists only 45 of 65 templates. Unreachable from the UI: RL_LOWPASS, RL_HIGHPASS, CENTERTAP_RECT, AC_DC_SUPPLY, AC_DC_AMERICAN, DIFFERENCE_AMP, TRANSIMPEDANCE, INSTR_AMP, SALLEN_KEY_LP, BANDPASS_ACTIVE, NOTCH_FILTER, WIEN_OSCILLATOR, CURRENT_SOURCE, WINDOW_COMP, HYSTERESIS_COMP, ZENER_REF, PRECISION_RECT, 7805_REG, LM317_REG, TL431_REF. Labels in ui.c duplicate `template_info[].short_name`.
3. Scope info rows use magic offsets (`r->h + 100`, `+15`, `+18`, `38*channels`) duplicated between render and `ui_update_layout`.
4. `ScopeCoordsBackup` must be extended by hand for each new scope button (PopOut already missing).
5. `UI_ACTION_SCOPE_STACK` and `UI_ACTION_SPOTLIGHT` are both 33 (ui.h:492,494).
6. Palette item bounds are stored in content coordinates and mutated during drawing; hit-testing depends on having rendered a frame.
7. Colours are `#define` triples; spacing constants (35 px item, 70 px pitch, 14 px header, 22 px button) are literals repeated in init, render and click code.

## 2. Target structure

- Left panel with three tabs: Parts | Circuits | Search. Tab strip under the toolbar; each tab keeps its own scroll.
- Parts: same collapsible categories, but `PaletteItem.category` is set at ADD time; `ranges[]` deleted.
- Circuits: grouped list auto-built from a `CircuitTemplateInfo.group` field (Analog basics, Filters, Op-amps, Transistors, Oscillators, Power supplies, Digital/mixed, Power systems, High voltage), iterating every template so nothing can be left out.
- Search tab: spotlight logic extended to tools, components and templates.
- Scope control bar: one primary row plus tabs (Display | Trigger | Cursors | Analysis), both described by a static table.
- Properties: collapsible header; collapsed state gives the room to the scope.
- `UiTheme` struct (paddings, sizes, colours) replacing scattered literals; hover tooltips for every button.

## 3. Stages

### Stage 0 - Safety net: headless layout self-check
Extract a render-free `ui_layout_palette(UIState*)` from the render pass and add a `--layout-test` (tools target or main.c branch) that runs `ui_init` + `ui_update_layout` at 1024x600 / 1280x720 / 1920x1080 and asserts: every palette/circuit item has non-empty, unique bounds; no two toolbar/scope buttons overlap; every `CircuitTemplateType` has a palette item; no action-ID collisions.

### Stage 1 - UiTheme + tooltips
`UiTheme` in ui.h replacing literals in `ui_init`, `ui_render_palette`, `ui_update_layout`, `draw_button`. Tooltip state (`hover_tooltip`, `hover_since`) set in `ui_handle_motion`, drawn last in `app_render` after 500 ms.

### Stage 2 - Data-driven part categories
`ADD_COMP(cat, comp, lbl)` sets `.category` directly; delete `ranges[]` and `NEW_SECTION`. Fix the action-ID collision (`UI_ACTION_SPOTLIGHT`).

### Stage 3 - Circuit templates from circuits.c
Add `group` to `CircuitTemplateInfo`; build the circuit palette by looping over all templates using `short_name`; group headers collapsible. Assert count == CIRCUIT_TYPE_COUNT-1 in the layout test.

### Stage 4 - Tabbed left panel
`LeftTab left_tab; int palette_scroll_per_tab[3]`; only the active tab's items get bounds; Ctrl+1/2/3 switch tabs; Search tab reuses spotlight with template results.

### Stage 5 - Scope control bar: primary row + tabs
`ScopeCtrl` table (`Button *btn; int w; int action; int tab`) replaces 18 fields; loop layout and loop hit-test; export `scope_controls_bottom_y` for the info rows; `ScopeCoordsBackup` becomes an array copy.

### Stage 6 - Collapsible properties panel
`properties_collapsed` flag; header click toggles; `min_scope_y` shrinks when collapsed.

## 4. Risks
- 1024x600 leaves ~520 px for the palette; verify the layout test at that size.
- Bounds are only valid after `ui_layout_palette`; call it from `ui_update_layout` and on tab switch, never rely on the render pass.
- Every new scope control must live in the `ScopeCtrl` table or the pop-out will not remap it.
- Hard-coded 70/60/35 also appear in `ui_editor.c` and `ui_layout.json`; update or mark the tool informational.
