# Tektronix-style Scope Cursors and Readouts – Design Note

Research date: 2026-08-24. Sources are Tektronix manuals for TBS2000/TBS2000B, MDO3000, MSO/DPO4000, 2 Series MSO
and 4/5/6 Series MSO. Goal: decide what an educational SDL2 simulator scope should copy.

## 1. Cursor types

| Type | Behaviour | Where |
|---|---|---|
| **Waveform cursors** | Two vertical cursors attached to the source trace; each reads time *and* the amplitude where it crosses the waveform, plus deltas. "Waveform cursors measure vertical amplitude and horizontal time parameters simultaneously at the point the cursor crosses a waveform." [4/5/6 Help §Cursor configuration menu] | Default on every series; TBS2000B calls them "Time or Frequency cursors" [TBS2000B UM p.128] |
| **Screen cursors** | "In screen mode, two horizontal bars and two vertical bars span the graticule." [MDO3000 UM p.133]. V Bars "simply show the cursor time position in the waveform record"; H Bars "measure amplitude … not associated with the waveform" [4/5/6 Help]. TBS2000B: in Screen mode the vertical cursors "do not display amplitude values at crossing points; amplitude readings come exclusively from the horizontal cursors." [TBS2000B UM p.128] | All series; 4/5/6 splits into V Bars, H Bars, V&H Bars |
| **Linked / coupled** | "If linking is on, turning Multipurpose a moves the two cursors together. Turning Multipurpose b adjusts the time between the cursors." [MDO3000 UM p.132]. Same in 4/5/6: "Linked mode sets multipurpose knob A to move both cursors at the same time. Knob B will still move cursor B independently" [4/5/6 Help]. SCPI `CURSor:MODe {TRACk|INDependent}` [MSO4000 PM]. | All |
| **Split source** (touch scopes) | "Split allows each cursor to be on a different waveform." [4/5/6 Help] – useful for delay/phase between channels. | 2/4/5/6 Series |

**Source selection.** Default = *Selected Waveform*: cursors follow the last-touched channel ("The default menu
selection of Selected Waveform will cause the cursors to take measurements on the selected (last used) waveform."
[MDO3000 UM p.134]). On touch scopes "To move the cursors to a different channel or waveform, just tap in that
waveform graticule." [4/5/6 Help §Display and configure cursors]. Cursors are drawn in the channel colour: "The color
of the cursors indicates the channel on which they are taking measurements." [TBS2000B UM p.125]. Not available in XY mode.

## 2. Readout contents, units, placement

- Per-cursor: **a** and **b** rows (time relative to trigger = 0 s; negative left of trigger [TBS2000B p.128]) plus
  amplitude at crossing for waveform cursors. Readout rows are tagged with the knob that controls them:
  "a Readout: Indicates that the value is controlled by the Multipurpose a knob" [MDO3000 UM p.135]; square/circle
  glyphs map rows to knobs when both bar pairs are visible.
- **Δ rows**: "The Δ readouts indicate the difference between the cursor positions." [MDO3000 UM p.135] → Δt and ΔV.
- **1/Δt frequency**: implemented as a *cursor units* choice, not a separate mode. Cursor menu has "Cursor Units"
  [MDO3000 p.133]; SCPI `CURSor:VBArs:UNIts {SEConds|HERtz|DEGrees|PERcent}` – "HERtz sets the units of the VBArs
  cursors to 1/seconds", DEGrees for phase, PERcent for ratio cursors [MSO4000 PM p.2-143]. H-bar units `{BASE|PERcent}`.
- **dV/dt slope**: 2 Series MSO help lists cursor-derived "frequency calculations (1/dt) and voltage rate-of-change
  (dV/dt)" [2 Series MSO Help, ManualsLib 3455432]. On 4/5/6 the same ΔV/Δt row appears in the waveform-cursor readout;
  standalone dv/dt is otherwise a power-analysis measurement (SUP4-PWR) [4/5/6 Help].
- **Placement**: MDO3000 – "Readouts appear in the upper right corner of the graticule. If Zoom is on, the readout
  appears in the upper right corner of the zoom window." [p.135]. TBS2000B draws readouts on the waveform ("on-waveform
  cursor readouts" [TBS2000B datasheet]). 2/4/5/6 Series put a readout box in the graticule and a **Cursors badge** in
  the Results bar; "Double-tap the cursor readouts, or on a cursor bar (line), to open the configuration menu"
  [4/5/6 Help p.112]. 2 Series has "user-selectable readout location" [tek.com 2 Series page].
- **Off-screen**: cursor menu offers **"Bring Cursors On Screen"** [MDO3000 UM p.133 menu listing] – re-centres both
  cursors inside the visible record after a time-base or position change; readouts keep showing the off-screen values.

## 3. Automatic measurements and gating

MDO3000 time-domain list [UM pp.122-124]: Frequency, Period, Rise Time (10→90 %), Fall Time, Delay (50 % point of two
waveforms), Phase (degrees, 360° = one cycle), Positive/Negative Pulse Width (50 % points), Positive/Negative Duty
Cycle (%), Burst Width; amplitude: Peak-to-peak, Amplitude (High−Low), Max, Min, High, Low, Positive/Negative
Overshoot (= (Max−High)/Amplitude×100 %), Mean, Cycle Mean, RMS, Cycle RMS, plus Area/Cycle Area. TBS2000B offers 32,
same families [TBS2000B UM]. Every definition says "first cycle in the waveform **or gated region**".
Up to 4 (MDO3000) / many badges (4/5/6) can be on screen at once; Tek recommends automatic measurements over cursors
for repeatability [tek.com FAQ "automated measurements vs cursors"].

**Gating.** MDO3000: Measure ▸ More ▸ Gating = *Off (Full Record) / Screen / Between Cursors* [UM p.126]; TBS2000B has
the same three [TBS2000B UM]. 4/5/6: Gating panel per measurement, *None / Screen / Cursors / Logic / Search / Time*,
Global or Local; "Cursors takes measurements on that portion of the waveform between the cursors. Selecting Cursors
opens cursors on the measurement source." Tapping a gated measurement badge draws its vertical gate bars [4/5/6 Help
p.160-161]. Older Windows scopes let you drag gate cursors directly with the mouse [tek.com FAQ on gated measurements].

## 4. Knob/button conventions → mouse/keyboard mapping

Front panel (MDO3000 UM p.41; 2 Series Quick Start): **Cursors** button – push once = two vertical cursors on, push
again = off, push-and-hold = cursor menu. **Multipurpose a** moves cursor a, **b** moves cursor b. **Select** – with two
V cursors, toggles Linked; with V+H visible, toggles which pair is active. **Fine** – toggles coarse/fine step for the
knobs. TBS2000B (single knob): turn = move active cursor, *click* = "cycle through selecting the cursors" [p.128].
Touch scopes: "Touch and drag, or use the Multipurpose knobs, to move the cursors" [4/5/6 Help].

Sensible mouse/keyboard mapping for a simulator:
- Click-drag a cursor line (hit-test ± a few px; hovering highlights it; drag along its axis only). Click on a trace
  = set cursor source. Drag on the readout box / Δ region with Linked on = move pair.
- Keyboard: `C` toggle cursors, `Shift+C` cycle type (Waveform → Screen → off), `Tab`/`Space` = Select (active cursor
  or pair), `←/→` move active V cursor by 1 px, `↑/↓` H cursor, `Shift` = coarse (×10), `Alt`/`Ctrl` = fine (sub-pixel /
  sample), `L` = link, `Home` = bring cursors on screen, `F` = toggle s/Hz units.
- Scroll wheel over a cursor = nudge it; wheel with `Shift` = move the linked pair; wheel elsewhere keeps its
  time-base meaning.

## 5. Prioritised adoption list (value ÷ effort)

1. **Waveform cursors a/b with readout box** (t, V at crossing, Δt, ΔV, 1/Δt) – covers 80 % of teaching use; snap V at
   crossing by interpolating between samples.
2. **Linked mode + Select toggle** – trivial state, makes period/pulse-width measurement fast.
3. **Screen (H-bar) cursors** for amplitude with Δ and optional % (ratio) units – overshoot/rise-time teaching.
4. **Gate automatic measurements to cursor span** ("Between Cursors" toggle) – reuses existing measurement code; huge
   pedagogical value (shows *which* cycle a number comes from), drawn as light gate bars.
5. **Bring Cursors On Screen** + colouring cursors in the source channel colour; readout keeps live values when a
   cursor leaves the graticule.
6. **Fine/coarse step and click-to-select source** – small input-handling additions.
7. **Cursor units Hz/degrees** (phase cursors: Δt/period × 360°) – cheap once period measurement exists.
8. Later / low priority: split-source cursors, dV/dt row, draggable readout badge.

## Sources

- MDO3000 Series User Manual (Tektronix 071-3216) – pp.41, 122-126, 132-135. https://wiki.eecs.yorku.ca/course_archive/2015-16/F/2200/_media/mdo3000-oscilloscope_manual.pdf
- 4/5/6 Series MSO Printable Help (077-1303-07) – "Display and configure cursors", "Cursor configuration menu", "Gating panel". https://download.tek.com/manual/4-5-6_Series-MSO-Printable-Help_EN-US_077130307.pdf
- MSO4000/DPO4000 Programmer Manual – CURSor:MODe, CURSor:VBArs:UNIts, CURSor:HBArs:UNIts. https://download.tek.com/manual/077024801web.pdf
- TBS2000B Series User Manual – pp.125-128 (ManualsLib). https://www.manualslib.com/manual/2026279/Tektronix-Tbs2000b-Series.html?page=128
- TBS2000 Demo Guide p.14 (cursor procedure, Fine, MPK click). https://www.manualslib.com/manual/1348696/Tektronix-Tbs2000.html?page=14
- TBS2000B datasheet (on-waveform readouts, 32 measurements). https://www.tek.com/en/datasheet/digital-storage-oscilloscope-tbs2000b-series-datasheet
- 2 Series MSO Help (ManualsLib 3455432) and Quick Start (knobs A/B, Fine, Select). https://www.manualslib.com/manual/3455432/Tektronix-Mso-2-Series.html ; https://docs.rs-online.com/70d2/A700000009243591.pdf
- Tek FAQ: automated measurements vs cursors. https://www.tek.com/en/support/faqs/how-can-i-increase-measurement-accuracy-using-automated-measurements-vs-cursors
- Tek FAQ: moving gate cursors. https://www.tek.com/en/support/faqs/when-making-gated-measurements-it-possible-move-gate-cursors-without-going-measurement-
- Test & Measurement Tips, "Measurements in Tektronix MDO 3000 oscilloscopes". https://www.testandmeasurementtips.com/measurements-tektronix-mdo-3000-oscilloscopes/
