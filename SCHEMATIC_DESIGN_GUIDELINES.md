# Schematic Design Guidelines v3.0 (LLM-Oriented, Reviewable, Machine-Checkable)

These guidelines define **how to draw** schematics so they are **readable, unambiguous, reviewable, and CAD-checkable**. They also define **verification logic** an LLM can run against a generated schematic.

## 0. Standards and references (symbol + documentation)

Use one symbol standard consistently per project:

- **IEC 60617** (database standard for graphical symbols) for electrotechnical diagrams.
- **IEEE/ANSI 315** for electrical/electronics diagram symbols; designed to fit a modular grid.
- **Reference designations**: align to **ASME Y14.44** for formation/application of reference designators.

Notes:
- IPC-2612 is still encountered in legacy flows, but IPC’s revision table marks **IPC-2612 as “No Longer Maintained”**—treat it as historical guidance, not a living standard.

---

## 1. Style profiles (pick one, then enforce it)

Define a project-wide style profile and do not mix styles.

### Profile A — **Orthogonal-Strict (recommended for LLM generation)**
- Wires are only **horizontal/vertical** with **90° corners**.
- No diagonal segments.
- Junction dots required for all connections.

### Profile B — **Orthogonal-Relaxed**
- Same as A, but allows short 45° segments **only inside symbols** (never as net wiring).

If unspecified: assume **Profile A**.

---

## 2. Coordinate/grid system (geometry contract)

Define grid units once, then enforce everywhere.

- **Primary Grid Unit (GU):** default 1 GU = 100 mil (2.54 mm). (You may choose 50 mil for dense logic sheets, but keep it consistent per sheet set.)
- **Pin coordinates:** every pin connection point must land on **integer GU coordinates**.
- **Wire segments:** endpoints must land on integer GU coordinates; segments are axis-aligned.
- **Text:** may use 0.5 GU placement, but must not overlap any wire or body bounding box.

---

## 3. Data model an LLM must assume (for checking)

Every schematic object must have:

- **Component**: `{refdes, value, footprint(optional), body_bbox, pins[{name/number, xy, dir}] }`
- **Wire segment**: `{x1,y1,x2,y2, net}` (axis-aligned only in Profile A)
- **Junction dot**: `{xy, net}`
- **Net label / port**: `{text, xy, net, scope(local/global/hier)}`
- **Sheet / hierarchy metadata**: `{sheet_id, title, revision, page_number, parent/child ports}`

---

## 4. Component placement rules (readability first)

### 4.1 Flow conventions
- Primary signal flow: **Left → Right**.
- Power entry and regulators: **top/left**; grounds: **bottom**.
- Feedback loops are allowed to violate flow locally, but must be **visually separated** (see §8).

### 4.2 Functional grouping
- Group parts into **functional blocks** (power, ADC, MCU, analog front-end, driver stage, etc.).
- Within a block: place components so **net topology is obvious without tracing long wires**.
- Between blocks: leave **white-space routing channels**.

### 4.3 Alignment and spacing (hard constraints)
- Components aligned to grid; minimize rotations of text and symbols.
- Minimum spacing:
  - **≥ 2 GU** between component body bounding boxes (BB-to-BB).
  - **≥ 1 GU** clearance between any wire and any component BB (except at pins).
  - **≥ 1 GU** between parallel wires.

### 4.4 Orientation conventions (consistent symbols)
- Resistors: horizontal preferred.
- Capacitors: vertical preferred; polarized caps clearly marked.
- Op-amps/comparators: triangle pointing **right**; inputs on left; output on right.
- Connectors: pin 1 orientation visually clear; pin numbering readable.

---

## 5. Wiring rules (unambiguous connectivity)

### 5.1 Manhattan routing (Profile A)
- Every wire segment must be **horizontal or vertical**.
- Corners must be 90°.
- Do not route through component bodies or through “symbol interior gaps” (capacitor plates, source circles, etc.).

### 5.2 Junction semantics (no ambiguity)
- **Connected crossing:** only valid if a junction dot is present **and** it is a T-junction (3-way).
- **Non-connected crossing:** allowed only as a **90° cross with NO dot**.
- **4-way (+) junctions are forbidden** (degree=4 node). Replace with two offset T-junctions separated by ≥ 1 GU.

### 5.3 Terminal escape and approach rules (anti-short visual rule)

#### 5.3.1 Terminal ESCAPE rule (routing FROM a terminal)
A wire leaving a pin must "escape" before turning to avoid looking like it runs through the symbol.

- **Escape distance:** first segment must extend **≥ 1.0 GU** perpendicular to the symbol edge before any turn.
- Component-specific defaults:
  - Vertical sources/caps/resistors: escape **horizontally**.
  - Horizontal resistors: escape **vertically**.
  - Op-amp inputs: escape **left**; output: escape **right**.

#### 5.3.2 Terminal APPROACH rule (routing TO a terminal)
A wire approaching a pin must maintain the same perpendicular approach to avoid appearing to touch the component body.

- **Approach direction:** final segment must arrive **perpendicular to the component body axis**.
- Component-specific defaults:
  - Vertical sources/caps/resistors: approach **horizontally** (from left or right).
  - Horizontal resistors: approach **vertically** (from top or bottom).
  - Op-amp inputs: approach from **left**; output: approach from **left**.

**Visual clarity requirement:** The corner where the wire turns toward the terminal must be **≥ 1.0 GU away** from the component body to avoid ambiguity.

### 5.4 Prefer labels over long wires
- Do not draw long cross-sheet wires. Use **net labels and ports**.
- Best practice: **net labels within a sheet**, **ports between sheets**.
- Ensure label scope is correct (local vs global vs hierarchical).

### 5.5 Wire hygiene
- Avoid “wire ladders” over IC pins; instead, place the IC so pins face the nets.
- Avoid stubs that end without a pin/label/junction.
- Do not run wires under text; keep text off wires.

---

## 6. Power and ground (draw it so reviews catch mistakes)

### 6.1 Power domains are explicit
- Every sheet must make power connectivity reviewable:
  - Either show rails on-sheet **or** include a clear “Power Notes” block listing domains and where they originate.
- Use consistent net names: `+3V3`, `+5V`, `VBAT`, `VREF`, `AVDD`, `DVDD`, etc.

### 6.2 Decoupling is mandatory for active ICs
- Every IC power pin must have decoupling represented (typical 0.1 µF + optional bulk). Put caps physically near the IC symbol when possible.
- If you use a “decoupling sheet,” you must cross-reference each IC and rail.

### 6.3 Ground strategy is intentional
- If AGND/DGND exist, show the **tie point** (net-tie or 0Ω) and name it.
- Avoid ground loops drawn as rings; use a clear star/tie intent at schematic level.

---

## 7. Symbols, pins, and library integrity (prevent downstream errors)

### 7.1 Pin correctness checks
- Pin numbers must match the footprint mapping.
- Power pins must be explicit unless your CAD flow formally controls hidden pins.

### 7.2 No-connect and unused pins
- All intentionally unused pins must be marked **NC / No-Connect** (graphical marker).
- Inputs that must not float should be pulled to a defined level.

### 7.3 Polarities and directions
- Diodes: anode/cathode obvious.
- Electrolytics: `+` marked.
- IC pin direction metadata (in/out/passive/power) should exist for ERC.

---

## 8. Op-amps, comparators, and feedback-heavy circuits (make the loop obvious)

### 8.1 Visual separation of loops
For circuits with both positive and negative feedback:
- Route **negative feedback** over the **top** of the amplifier symbol.
- Route **positive feedback** under the **bottom** of the symbol.

### 8.2 Power rails on amplifiers
- Show `V+`/`V-` (or `VDD`/`VSS`) pins and connect them.
- If using single-supply, do not imply ± rails.

### 8.3 Oscillators (review requirement)
- The complete loop from output back to the controlling input must be traceable visually.
- Timing components (R/C) must be co-located and labeled with their function (e.g., `R_TIME`, `C_TIME`).

---

## 9. Labeling, naming, and documentation

### 9.1 Reference designators (RefDes)
- Unique and consistent: `R#`, `C#`, `U#`, `Q#`, `D#`, `L#`, `J#`, `TP#`.
- Numbering must be stable and review-friendly.

### 9.2 Net naming
- Critical nets must be named (clocks, resets, references, enables, analog nodes).
- Use polarity suffixes: `_N`, `_P` for differential; or `nRESET`.
- Avoid ambiguous names (`SIGNAL1`, `NET12`) except auto-generated nets that are truly unimportant.

### 9.3 Units and values
- Engineering notation: `4.7k`, `10M`, `100n`, `1u`, `22p`.
- Add ratings where they matter for correctness (e.g., capacitor voltage, resistor power, shunt tolerance).

### 9.4 Sheet hygiene (review checklist items)
- Each sheet must have: title, sheet number, revision/date, author/owner, project identifier.
- Consistent templates across sheets.

---

## 10. Verification logic (what the LLM must check)

### 10.1 Geometry DRC (Critical)
1. **Wire-angle check (Profile A)**: all segments axis-aligned.
2. **Component collision check**: any wire intersecting a component body BB is a fail unless it ends at that component pin.
3. **Text collision check**: no text BB overlaps wire segments, junction dots, or component bodies.
4. **Minimum clearance**: enforce §4.3 and §5.5 spacing constraints.

### 10.2 Connectivity DRC (Critical)
1. **Unconnected pin check**: any non-NC pin not connected to a net is a fail.
2. **Junction correctness**:
   - If wires cross at same coordinate: must be either (a) 90° cross with no dot (not connected) or (b) T-junction with dot.
   - Node degree=4 is a fail.
3. **Net label coherence**:
   - Same net name must represent same connectivity.
   - Scope correctness (local/global/hier) must match intent and sheet structure.

### 10.3 Electrical plausibility ERC (Major)
1. **Floating inputs**: CMOS/logic inputs must not float; add pull-ups/downs as needed.
2. **Power pin completeness**: every active IC has rails connected + decoupling shown.
3. **Single-node nets**: nets with only one pin/label are flagged (likely mistake).
4. **Driver/receiver sanity**:
   - Outputs shorted together is a fail.
   - Bidirectional nets require explicit reason (bus, open-drain with pull-up, etc.).

Cad tools commonly provide DRC/ERC to catch these issues; use them as part of final verification.

### 10.4 Readability scoring (Minor → Major if very low)
Compute a heuristic score (0–100):
- Penalties: wire crossings, long unlabeled wires, dense clusters, repeated doglegs, many net stubs, unclear loop routing.
- Target: ≥ 85 for production schematics; ≥ 70 acceptable for quick internal drafts.

---

## 11. Auto-fix patterns (deterministic remediations)

- **Through-body wire** → reroute via a safe channel offset by ≥ 1 GU from the body BB.
- **4-way junction** → replace with two T-junctions offset by ≥ 1 GU; keep original net name.
- **Long wire across block** → convert mid-span into net labels near pins; remove the long segment.
- **Ambiguous crossing** → enforce 90° cross/no dot for non-connect; otherwise redraw as T with dot.
- **Text on wire** → move text to nearest empty quadrant; maintain association to the net/component.

---

## 11a. Probes must say what they are on (checked by `--label-test`)

Every circuit places probes, and every probe carries a name. The name is drawn at the probe on
the schematic and is the oscilloscope's channel name, so it is the same word in both places.

- **Name what the node is, not which channel it is.** `IN`, `OUT`, `GATE`, `SW OUT`, `NEUT`,
  `138KV`. `CH1` says which trace it is and nothing about the circuit, which is no help to
  someone reading a schematic they did not draw.
- **Seven characters**, the width of the label field. Longer names are a failure, not a
  truncation.
- **Unique within the circuit.** The names are the scope's channel names: two traces both called
  `VCAP` cannot be told apart.
- **A new template names its probes in the tables it already fills in** - `template_output` and
  `template_extra_probes` each take a name beside the part and terminal. Left out, a name is
  derived from the part and terminal the probe sits on, which is correct but generic; prefer the
  specific one.
- **Probe both halves of a comparison.** A circuit that exists to show two things side by side -
  two regulator topologies, ideal against real - with a probe on only one half is missing the
  point of the drawing.

`--label-test` enforces the first four over all templates on every build.

---

## 12. Final checklist (must pass before output)

- [ ] Profile A enforced: orthogonal wires only (or Profile B rules enforced).
- [ ] No wires pass through any component body BB.
- [ ] No node degree=4 junctions.
- [ ] Junction dots present on all T-junction connections.
- [ ] Every IC has explicit power connections and decoupling shown.
- [ ] All important nets labeled; cross-sheet connectivity uses ports/labels appropriately.
- [ ] All unused pins explicitly NC; unused logic inputs not floating.
- [ ] Title block + sheet metadata present and consistent.
- [ ] Every probe named for the node it sits on, unique, within seven characters.
- [ ] Both halves probed on any circuit that exists to compare two things.

---

## Appendix A — Practical note on “IPC-2612” in your original doc
If your goal is “best current practice,” treat IPC-2612 as legacy because IPC’s own revision table lists it as **no longer maintained**.
For symbol libraries and diagram symbols, lean on **IEC 60617** and/or **IEEE 315**, and for RefDes formation lean on **ASME Y14.44**.
