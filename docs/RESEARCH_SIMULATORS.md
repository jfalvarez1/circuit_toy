# Circuit Playground: Simulator Research & Improvement Report

Date: 2026-08-24. Scope: how comparable simulators solve the problems this codebase has (oscillator damping,
NR convergence, op-amp rails), concrete ranked improvements with MNA formulas, vendor SPICE model import,
an electrical reference for the 47 templates, and test-oracle strategy.

## 0. Where this codebase stands today (grounding)

Observed in `src/simulation.c`, `src/component.c`, `include/simulation.h`, `include/types.h`:

| Aspect | Current implementation |
|---|---|
| Formulation | MNA, dense `Matrix`, rebuilt and LU-solved every NR iteration (`simulation_solve_step`). |
| Integration | Backward Euler only. Capacitor `Geq = C/dt`, `Ieq = C*v_prev/dt`; inductor `Req = L/dt`, `Veq = L*i_prev/dt` (`component.c` ~2267-2320). |
| Time step | Fixed `time_step` (1 ns..10 ms). Adaptive path exists (`ADAPTIVE_ERROR_TOL 0.05`, halve/double heuristics) but is *not* an LTE estimate: it uses the relative change of the solution between steps, which penalises fast-but-correct edges and ignores integration error. Disabled by default. |
| NR loop | `MAX_ITERATIONS 50`, `CONVERGENCE_TOL 1e-9` **absolute** on every unknown, including branch currents and volt-source variables. No RELTOL/VNTOL/ABSTOL split, no junction limiting; diodes clamp `Vd` to `[-100, 40*nVt]`, BJTs clamp `Vbe/Vbc` to `[-5nVt, 40nVt]` (hard clamps, not SPICE `pnjlim`). |
| DC operating point | Same NR loop with `dt = 1e9` (caps open / inductors short). No GMIN stepping, no source stepping, no `.nodeset`-style initial guess except the diode's `Vd=0.6` first-iteration guess. |
| GMIN | `1e-12` S from every node to ground, always on. Ground node modelled as a `1e10` S conductance (not eliminated). |
| Op-amp | VCVS with piecewise-linear saturation and hysteresis in `opamp_stamp_output`; post-solve `simulation_clamp_opamps` overwrites node voltages (breaks KCL for that step). `COMP_OPAMP_REAL` silently replaces any gain > 1000 with 150 and ignores `gbw`, `slew_rate`, `i_bias`, `cmrr`. |
| Transformer | Ideal: VCCS coupling `V_s = N*V_p` via 1 ohm "source" conductance plus 10 kohm magnetising resistance (energy-inconsistent; primary current is not reflected from secondary load). |
| Subcircuits | `SubCircuitDef` (`types.h` ~484-517): serialised component/wire blobs + up to 16 `SubCircuitPin`s + `num_internal_nodes`, stamped via `g_subcircuit_internal_node_offset`. |

These are exactly the areas Section 2 targets.

---

## 1. How comparable simulators work

### 1.1 CircuitJS1 / Falstad (open source, Java -> GWT/JS)

Source: <https://github.com/pfalstad/circuitjs1> (`SimulationManager.java`, `Diode.java`, `TransistorElm.java`,
`OpAmpElm.java`, `CapacitorElm.java`). Verified from the repository on 2026-08-24.

- **Formulation**: MNA with the linear part of the matrix stamped once (`analyzeCircuit`) and LU-factored; only
  nonlinear elements re-stamp (`stampNonLinear`) each sub-iteration. Wires/ground are eliminated in analysis
  rather than stamped as huge conductances.
- **Integration**: *Trapezoidal by default* for capacitors and inductors (`isTrapezoidal()`; `compResistance =
  timeStep/(2*C)` and `curSourceValue = -voltdiff/compResistance - current`), with a per-element
  "Backward Euler" flag as an escape hatch for trap ringing.
- **NR sub-iterations**: `subiterCount = 100` when adaptive stepping is possible, else `5000`. Each nonlinear
  element votes `sim.converged = false` if its own change is too large (e.g. op-amp when `|lastvd - vd| > 0.1`
  or output overshoots a rail by >0.1 V). Convergence = every element satisfied *and* `subiter > 0`.
- **Adaptive timestep**: if a step fails after `subiterCount`, `timeStep /= 2` and retry; after 3 consecutive
  "good" steps (converged in < 3 sub-iterations) `timeStep = min(2*timeStep, maxTimeStep)`. Failure below
  `minTimeStep` -> "Convergence failed!" This is *iteration-count* driven, not LTE-driven.
- **Diode model** (`Diode.java`): Shockley with `vscale = n*Vt`, leakage `Is`; **junction limiting** identical
  to Berkeley SPICE `pnjlim`: `vcrit = vscale*ln(vscale/(sqrt(2)*Is))`; if `vnew > vcrit` and
  `|vnew - vold| > 2*vscale` then `vnew = vold + vscale*ln(1 + (vnew-vold)/vscale)` (or `vcrit`), and the step
  is flagged non-converged. Zener breakdown uses the same limiter on a translated voltage with a steeper
  exponential (`zoffset` chosen for 5 mA at Vz).
- **BJT** (`TransistorElm.java`): Ebers-Moll with `limitStep` on both Vbe and Vbc, `gmin = 1e-12` in parallel
  with each junction, and **per-transistor dynamic GMIN**: after 100 non-converged iterations,
  `gmin = 10^(-9*(1 - localSubIters/300))` capped at 0.1 S (a GMIN-stepping variant applied only to the
  troublesome device).
- **Op-amp** (`OpAmpElm.java`): a voltage source row with NR-linearised gain. In the linear region it stamps
  `dx = gain` (1e5, or 1e3 with a low-gain flag "because 1e5 broke e-amp-dfdx.txt"); when `vd >= maxAdj/gain` it
  switches to a near-flat line (`dx = 1e-4`) through the rail, with a random 1-in-4 chance of switching to break
  limit cycles. Rails are true solution constraints, not post-solve clamps. `OpAmpRealElm` adds a
  single-pole GBW and slew limit.
- **Robustness with oscillators**: trapezoidal (no numerical damping) plus tiny timestep (default 5 us) and the
  op-amp's self-consistent rails. Falstad's own about page: <https://www.falstad.com/circuit/about.html>.

### 1.2 EveryCircuit / iCircuit (closed source)

- EveryCircuit: "custom-built simulation engine ... Kirchhoff laws, nonlinear semiconductor device equations",
  real-time interactive (<https://everycircuit.com/>). No published numerics; behaviour (smooth pot sweeps
  without convergence failures) is consistent with damped NR, trapezoidal/BE hybrid, and generous GMIN.
- iCircuit (Krueger Systems): "advanced simulation engine ... realtime always-on analysis"
  (<https://icircuitapp.com/>). No technical disclosure found.
- Lesson: both prioritise "never stop" over accuracy — take the CircuitJS1 approach (voting convergence, step
  halving, dynamic GMIN) rather than SPICE's hard `ITL4` abort.

### 1.3 LTspice (Analog Devices)

- Integration: **modified trapezoidal** (default), trap, or Gear-2. Trap "produces trap ringing ... oscillation
  step to step about the true solution"; modified trap post-processes with two interpolating lines to cancel
  it; **Gear "introduces artificial numerical dampening ... can make unstable circuits look stable"** and must
  not be used for oscillators. <https://ltwiki.org/files/LTspiceHelp.chm/html/integration_method_issues.htm>,
  <https://www.analog.com/en/resources/technical-articles/spice-differentiation.html>.
- Timestep: LTE-controlled, proprietary sparse solver; `.options` for `gmin`, `abstol`, `reltol`, `vntol`,
  `chgtol`, `trtol`, `srcstepmethod`, `gminsteps` (Infineon summary:
  <https://community.infineon.com/t5/Knowledge-Base-Articles/Resolving-convergence-challenges-in-LTspice/ta-p/1053215>).
- Op-amps: vendor macromodels (Boyle-style: differential input stage, gm/pole stages, output stage with
  clamps) or the built-in `UniversalOpamp2` levels 1-3 (behavioural: gain, GBW, slew, rails, Rout).
- Undocumented `.options method=be` exists for hard cases (<https://www.audio-perfection.com/spice-ltspice/ltspice-tips-tricks/undocumented-ltspice-features-solving-some-of-convergence-problems-using-backward-euler-integration-method/>).

### 1.4 ngspice (Berkeley SPICE3f5 lineage; used by KiCad, EasyEDA std, PartSim, Qucs-S, PySpice)

Manual: <https://ngspice.sourceforge.io/docs/ngspice-html-manual/manual.xhtml>; option pages:
<https://nmg.gitlab.io/ngspice-manual/analysesandoutputcontrol_batchmode/simulatorvariables__options/transientanalysisoptions.html>,
<https://nmg.gitlab.io/ngspice-manual/analysesandoutputcontrol_batchmode/simulatorvariables__options/dcsolutionoptions.html>.

- **Integration**: `METHOD=trap` (default) or `gear` with `MAXORD` 2..6. `XMU` (default 0.5) blends
  trap<->BE: values below 0.5 suppress trap ringing (this is the cheap "damped trap" you can copy).
- **Timestep**: LTE-based. Local truncation error of the charge/flux is estimated from divided differences of
  the last 3 solution points; the accepted error is `TRTOL * max(RELTOL*max(|q|,|q_pred|), CHGTOL)` with
  `TRTOL=7`, `RELTOL=1e-3`, `CHGTOL=1e-14`. Step is rejected and shrunk if LTE too large; grown by up to 2x
  when small. `ITL4=10` NR iterations per timepoint before shrinking the step by 8x.
- **Convergence**: per-node `|dv| <= RELTOL*max(|v_new|,|v_old|) + VNTOL(1 uV)`; per-branch
  `|di| <= RELTOL*max(|i|) + ABSTOL(1 pA)`. `ITL1=100` for DC op. If plain NR fails: **dynamic GMIN
  stepping** (start gmin=1e-3, divide down to 1e-12; `GMINSTEPS`), then **source stepping** (`SRCSTEPS`/`ITL6`;
  scale all independent sources 0->1). `pnjlim` (`devsup.c`) limits every junction voltage change;
  `fetlim`/`limvds` do the same for MOSFETs.
- **Op-amps**: vendor `.subckt` macromodels; no built-in ideal op-amp (users use `E` VCVS with `TABLE` or
  `limit()`).
- **Oscillators**: startup relies on trapezoidal (no damping) + tiny `.ic`/noise kick or the inherent asymmetry
  of the DC op point; Gear/`XMU<0.5` used only when trap ringing is worse than damping.

### 1.5 Qucs-S

Front-end over ngspice / Xyce / SPICE OPUS / QucsatorRF; ngspice recommended by the docs
(<https://qucs-s-help.readthedocs.io/en/latest/overview/choosing-a-sim-backend.html>). Its value for this
project: it demonstrates a schematic->SPICE netlist exporter with a clean subckt/library import path
(<https://qucs-s-help.readthedocs.io/en/latest/subckts-and-ext-models/spice-models/index.html>).

### 1.6 PartSim / EasyEDA

EasyEDA Std documented ngspice as its engine (<https://docs.easyeda.com/en/Simulation/Chapter1-Introduction/>),
EasyEDA Pro also exposes NGSpice for transient/AC (<https://prodocs.easyeda.com/en/simulation/introduction/>),
and the forum records a move to LTspice for the newer engine. PartSim used ngspice server-side. All of them
inherit ngspice's numerics wholesale, so "ngspice" is the de-facto golden reference for browser tools.

### 1.7 PySpice

Python front-end that builds netlists and drives ngspice either via the `ngspice` shared library (CFFI) or
server mode, returning NumPy arrays (<https://github.com/PySpice-org/PySpice>,
<https://ngspice.sourceforge.io/shared.html>). Ideal glue for the test oracle in Section 5.

### 1.8 Summary matrix

| Simulator | Integration | Step control | DC aids | Junction limiting | Op-amp |
|---|---|---|---|---|---|
| CircuitJS1 | Trap (BE opt-in) | Halve on NR failure, double after 3 good | Per-device dynamic GMIN | pnjlim-equivalent | PWL rails inside NR, single-pole "real" variant |
| LTspice | Modified trap / trap / Gear2 | LTE | GMIN + source stepping | pnjlim | Macromodels / UniversalOpamp |
| ngspice | Trap (XMU) / Gear | LTE (TRTOL,CHGTOL) | Dynamic GMIN, source stepping | pnjlim, fetlim | Macromodels |
| **This repo** | **BE only** | Fixed (heuristic adaptive off) | Static GMIN 1e-12 | Hard clamps | PWL VCVS + post-solve clamp |

---

## 2. Concrete improvements for this codebase (ranked by value / effort)

Effort: S (< 1 day), M (1-3 days), L (> 3 days). All formulas use `h = dt`, `n[0]`/`n[1]` matrix indices,
`STAMP_CONDUCTANCE` for the 4-entry conductance pattern, and the existing branch-current rows for L.

### 2.1 Trapezoidal integration with per-element BE fallback — value: very high, effort: S

Why: BE's amplification factor `1/(1+jwh)` damps every mode; for the Wien bridge and phase-shift oscillators
this is why gain had to be inflated and noise sources left connected (comment in `COMP_OPAMP_REAL`).
Trapezoidal is A-stable with |g|=1 on the imaginary axis, so a marginal oscillator neither grows nor decays
numerically.

Capacitor (Pileggi/Rohrer companion model; see
<http://circsimproj.blogspot.com/2009/07/companion-models.html>):

```
i_n+1 = (2C/h) v_n+1 - [ (2C/h) v_n + i_n ]
Geq   = 2C/h                      (BE: C/h)
Ieq   = (2C/h) v_n + i_n          (BE: (C/h) v_n)
```
Stamp `Geq` with `STAMP_CONDUCTANCE(n0,n1,Geq)`, `b[n0] += Ieq`, `b[n1] -= Ieq`. You must now store the
**capacitor current** `i_n = Geq*(v_n) - Ieq_prev` after each accepted step (add `i_prev` to
`props.capacitor`). CircuitJS1 does exactly this (`current = voltdiff/compResistance + curSourceValue`).

Inductor (keep the branch-current row):

```
v_n+1 = (2L/h) i_n+1 - [ (2L/h) i_n + v_n ]
Req   = 2L/h,   Veq = (2L/h) i_n + v_n
```
Row `curr_idx`: `+1/-1` on the node columns, `-Req` on the diagonal, `b[curr_idx] = Veq` (same pattern as now,
with `v_n` = previous inductor voltage stored per component).

Damped trap (ngspice `XMU`): `Geq = C/(h*mu)`, `Ieq = C v_n/(h*mu) + (1-mu)/mu * i_n` with `mu in [0.5,1]`
(`mu=0.5` trap, `mu=1` BE). Expose as a per-component "integration" flag and a global default; use BE for the
first step after a switch/relay/fuse event and after DC op to kill trap ringing.

### 2.2 pnjlim junction limiting for diodes, LEDs, zeners, BJT, TL431, MOSFET body diodes — value: very high, effort: S

Replace the hard `CLAMP(Vd, -100, 40*nVt)` with Berkeley `pnjlim` (Nagel 1975; ngspice `devsup.c`;
CircuitJS1 `Diode.limitStep`):

```
vcrit = nVt * ln( nVt / (sqrt(2) * Is) )
if (vnew > vcrit && |vnew - vold| > 2*nVt) {
    if (vold > 0) { arg = 1 + (vnew - vold)/nVt;
                    vnew = (arg > 0) ? vold + nVt*ln(arg) : vcrit; }
    else          { vnew = nVt * ln(vnew/nVt); }
    converged = false;      // force another NR pass
}
```
Apply to `Vd` in `COMP_DIODE/LED/SCHOTTKY/ZENER`, to `Vbe`, `Vbc` in the BJT (CircuitJS1 limits both), and to
the TL431's `V_ref - V_A` exponential. For reverse breakdown mirror the same limiter around `-Vz` (CircuitJS1
`vzcrit/zoffset`). Also add ngspice's `limvds`/`fetlim` for MOSFET `Vds/Vgs`. This removes most "solution may
not have converged" warnings and lets you drop the artificial `Gd >= 1e-12` floors.

### 2.3 SPICE-style convergence test and voting — value: high, effort: S

Current absolute `1e-9` on every unknown is too tight for branch currents in amps (a 1 A current needs 1 nA
agreement) and too loose for uV signals. Use:

```
|v_k+1 - v_k| <= RELTOL*max(|v_k+1|,|v_k|) + VNTOL      (RELTOL 1e-3, VNTOL 1e-6)
|i_k+1 - i_k| <= RELTOL*max(|i_k+1|,|i_k|) + ABSTOL     (ABSTOL 1e-12)
```
plus a `converged=false` vote from any element that applied limiting or switched a PWL region (op-amp rail,
zener, switch). Keep `MAX_ITERATIONS` but on failure **halve dt and retry** (CircuitJS1) instead of accepting
the unconverged vector.

### 2.4 LTE-based adaptive timestep — value: high, effort: M

Replace `simulation_estimate_error` with a real LTE estimate per reactive element (Kundert,
*The Designer's Guide to SPICE and Spectre*; ngspice `trunc` routines):

For trap, LTE of the charge is `LTE ~ (h^3/12) * q'''`, estimated from a 3-point divided difference of
`q = C*v` (or flux `L*i`):
```
DD3 = ( (q_n+1 - q_n)/h_n - (q_n - q_n-1)/h_n-1 ) / (h_n + h_n-1)      // ~ q''/2
LTE = (h_n^2) * |DD3| * h_n / 6                                           // trap
tol = TRTOL * max( RELTOL * max(|q_n+1|,|q_n|), CHGTOL )                  // TRTOL 7, CHGTOL 1e-14
h_new = h_n * sqrt( tol / LTE )   (clamp growth to 2x, shrink to >= 1/8)
```
For BE use `LTE = (h^2/2)|q''|` and `h_new = h*(tol/LTE)`. Reject and redo the step when `h_new < 0.9 h`.
Keep `sim->dt_target` as `maxTimeStep`; store 2 previous solutions (you already keep `prev_solution`).
Alternative with fewer stored points: **TR-BDF2** (Bank/Coughran 1985, `gamma = 2 - sqrt(2)`): trap
half-step to `t + gamma h`, then BDF2 to `t + h`; L-stable, 2nd order, one-step, with a built-in error
estimate `err = (h/3)*(...)`. Formulas: <https://math.la.asu.edu/~gardner/TRBDF.pdf>,
<https://www.sciencedirect.com/science/article/abs/pii/0168927495001158>. Good if you want trap accuracy
without ringing, but costs two NR solves per step.

### 2.5 GMIN stepping and source stepping for the DC operating point — value: high, effort: S-M

Wrap `simulation_dc_analysis`:
1. Plain NR (`ITL1 = 100`).
2. If it fails: **GMIN stepping** — set `gmin = 1e-2`; solve; if converged, `gmin /= 10` (ngspice dynamic
   gmin halves the exponent on failure) until `gmin <= GMIN`. Each solve starts from the previous solution.
3. If still failing: **source stepping** — multiply every independent source (V, I, wave generators at t=0)
   by `alpha` from 0 to 1 in `SRCSTEPS` (ngspice default 0 = off; use 10 steps, halve on failure).
4. Optional: `RSHUNT`-style user knob (ngspice) for floating nodes.
Cite: <https://sourceforge.net/p/ngspice/support-requests/46/> (gmin failure semantics),
<http://www.intusoft.com/articles/converg.pdf>. Also add `.nodeset`-like initial guesses: diodes at 0.6 V is
already done; add "op-amp output = midpoint of rails" and "BJT Vbe = 0.65".

### 2.6 Self-consistent op-amp rails and a real macro-model — value: high, effort: M

Delete `simulation_clamp_opamps` (it breaks KCL) once 2.3's voting is in place; the PWL stamp in
`opamp_stamp_output` already converges when the vote forces re-iteration (CircuitJS1's `dx=1e-4` trick: stamp
the rail region as `V_out = rail + 1e-4*(V+ - V-)` rather than a hard constant so the Jacobian is never singular
and the sign of `vd` still steers you out of saturation).

Behavioural macro-model for `COMP_OPAMP_REAL` (Boyle et al. 1974; TI/ADI "UniversalOpamp" style;
<https://www.electronicdesign.com/technologies/analog/article/21806271/spice-it-up-understanding-and-using-op-amp-macromodels>,
<https://analog-electronics.tudelft.nl/webbook/SED/_build/html/modeling_opamps/modeling_opamps_Modeling_of_the_operational_amplifier.html>):

```
Stage 1 (input):   Rin between +/-, Ibias current sources, Voffset in series with +
Stage 2 (gain/pole): internal node x:  I = gm*(V+ - V-), shunt R1 || C1 with
                    A0 = gm*R1,  f_p = 1/(2*pi*R1*C1) = GBW/A0     -> pick R1 = 1 Mohm, C1 = A0/(2*pi*GBW*R1)
                    slew: limit |I| to Imax = SR * C1  (soft: I = Imax*tanh(gm*vd/Imax))
Stage 3 (output):  E = limit(V_x, Vmin+Vsat_margin, Vmax-Vsat_margin)  then Rout to the output pin
```
MNA: stage 2 is a VCCS (4 entries `+-gm` on row `x`) plus the capacitor companion (2.1) and `1/R1`; the
`tanh` limiter is linearised each NR pass (`g = dI/dvd`, `Ieq = I - g*vd`). Rail-to-rail = `Vsat_margin` 0
vs ~1.5 V. With this model the Wien and phase-shift oscillators start from the DC op point without a noise
source because the pole introduces the phase needed and gain is correct (`A0 = 1e5`), and the 150x gain hack
becomes unnecessary.

### 2.7 Non-ideal transformer (coupled inductors) — value: high (all power-supply templates), effort: M

Replace the VCCS "transformer" with **two inductors plus mutual inductance** — the SPICE `K` element:

```
L1 = Lm (primary magnetising, e.g. 1-10 H for mains), L2 = N^2 * L1, M = k*sqrt(L1*L2), k ~ 0.98-0.999
v1 = L1 di1/dt + M di2/dt ;  v2 = M di1/dt + L2 di2/dt
Trap companion (branch rows i1, i2):
   v1 = (2L1/h) i1 + (2M/h) i2 - [ (2L1/h) i1_n + (2M/h) i2_n + v1_n ]
   v2 = (2M/h)  i1 + (2L2/h) i2 - [ (2M/h)  i1_n + (2L2/h) i2_n + v2_n ]
Stamp: row i1: +1/-1 on P1/P2 cols, -2L1/h on (i1,i1), -2M/h on (i1,i2); row i2 symmetric; RHS = bracket.
```
Leakage: `L_leak = (1-k)*L1` appears automatically from `k<1`; winding resistance `R_p`, `R_s` in series
with each branch (add `-R` to the diagonal of the branch row). Core loss: shunt `R_core` across primary.
Saturation (optional): make `L1` a function of flux `phi = L1*i1` via `L(i) = L0/(1 + (i/Isat)^2)` or the
LTspice `tanh` flux model (<https://ltwiki.org/index.php?title=The_Arbitrary_Inductor_model>). Refs:
<https://ltwiki.org/LTspiceHelp/LTspiceHelp/K_Mutual_Inductance.htm>,
<https://www.coilcraft.com/en-us/models/howto/simulation-model-considerations-part-ii-(1)/>.
This makes primary current reflect the secondary load (`i1 ~ -N*i2`), which the current model cannot do.

### 2.8 Capacitor ESR/ESL/leakage and inductor DCR/saturation (props already exist) — value: medium, effort: S

`props.capacitor.esr/esl/leakage` and `props.inductor.dcr/r_parallel/i_sat` are stored but the stamp uses only
`C` and `L`. Stamp them as a series RLC ladder without extra nodes by using the trap companion in
"resistance form": `Z_eq = ESR + Req_L + Req_C` all in one branch-current row:
```
row b: v_a - v_b = (ESR + 2ESL/h + h/(2C)) * i  + Veq
Veq   = -(2ESL/h) i_n - v_ESL,n  + v_C,n + (h/2C) i_n
```
(Capacitor in current-source form is `v_C,n+1 = v_C,n + (h/2C)(i_n + i_n+1)`.) Leakage = `1/R_leak`
conductance across the terminals. Inductor DCR: add `-DCR` to the diagonal of its branch row. Saturation:
`L(i) = L0 / (1 + (|i|/I_sat)^2)`, or `L(i) = L0 * (2/pi) * atan(...)`, linearised per NR pass on the branch
current (the branch row becomes nonlinear; vote non-converged when `L` changes > 1%).
DC-bias capacitance for MLCC (Section 3): `C(V) = C0 / (1 + (V/V_half)^2)` or a table; stamp via charge
`q(V)` with `Geq = 2*C(V_k)/h` and NR on `q`.

### 2.9 Thermal model coupling — value: medium, effort: M

`thermal_update_components` already accumulates damage; make temperature feed back electrically:
`R(T) = R25*(1 + tc*(T-25))` (resistor `temp_coeff` exists), diode `Is(T) = Is*(T/T0)^(XTI/n) *
exp(-Eg/(n*k) * (1/T - 1/T0))` with `Vt = kT/q` (SPICE diode temp equations, ngspice manual ch. 7),
BJT `Is(T)` likewise, and a first-order thermal RC per device (`P = V*I`, `dT/dt = (P - (T-Tamb)/Rth)/Cth`)
integrated with the same trap rule. This makes the LED/lamp/fuse templates "cook" realistically and gives
the Wien-bridge lamp stabiliser a physical basis.

### 2.10 Solver housekeeping — value: medium, effort: S-M

- Eliminate the ground node instead of stamping `1e10` S (ill-conditions the LU; SPICE removes row 0).
- Stamp the *linear* part once per step (or once per topology change) and add only nonlinear stamps per NR
  pass (CircuitJS1 splits `stamp()` / `doStep()`); a dense 200x200 LU per iteration is the current cost driver.
- Add a `.op` "initial guess" pass with sources ramped (source stepping) *before* the first transient step
  so oscillators start from a physically consistent state.
- Deterministic tiny start-up kick for oscillators: instead of a permanent noise source, apply a one-shot
  `1 mV` perturbation on the first step (SPICE users do `.ic` or `startup` on the supply).

### 2.11 Ranking

| # | Item | Value | Effort | Unlocks |
|---|---|---|---|---|
| 1 | 2.1 Trapezoidal + XMU | very high | S | Oscillators, RLC ringing accuracy, filter phase |
| 2 | 2.2 pnjlim | very high | S | Rectifier/LED/BJT convergence, remove clamps |
| 3 | 2.3 RELTOL/VNTOL + voting + halve-on-fail | high | S | Drops "may not have converged", enables 2.6 |
| 4 | 2.6 Op-amp macro (GBW/slew/Rout) + drop post-clamp | high | M | Correct Wien/phase-shift, Sallen-Key HF, slew-limited comparators |
| 5 | 2.7 Coupled-inductor transformer | high | M | All 5 power-supply templates, center-tap |
| 6 | 2.5 GMIN/source stepping | high | S-M | Robust DC op for regulators, mirrors, CMOS |
| 7 | 2.4 LTE timestep | high | M | Speed + accuracy on edges (square wave, 555) |
| 8 | 2.8 Parasitics/saturation | medium | S | Vendor model fidelity (Section 3) |
| 9 | 2.10 Solver housekeeping | medium | S-M | Performance, conditioning |
| 10 | 2.9 Thermal coupling | medium | M | Educational realism |

---

## 3. Vendor / SPICE model import

### 3.1 What vendors export

| Vendor / tool | Formats | Content | Licence notes |
|---|---|---|---|
| **Murata SimSurfing** (<https://www.murata.com/en-us/tool/simsurfing>, <https://ds.murata.co.jp/>) | Touchstone `.s2p`; SPICE netlist (`.mod`/`.lib`/`.cir` text, generic + LTspice/PSpice/ADS flavours); LTspice/PSpice *dynamic* libraries | **Static model**: `.SUBCKT` of pure R/L/C (parallel/series ladder, typically 5-20 elements) valid at 25 C / 0 V; **Dynamic model**: static ladder + a behavioural current source that rescales C and ESR with DC bias and temperature (<https://article.murata.com/en-us/article/mlcc-dynamic-model-supports-circuit-simulations>). SimSurfing can emit a static netlist *at a chosen bias/temperature* that equals the dynamic model at that point. | "You shall not use the DATA for any purpose other than the confirmation of characteristics ... and the electrical simulation"; "AS IS"; IP retained by Murata; **"You shall not redistribute or reproduce the DATA without prior consent of Murata."** (<https://www.murata.com/en-us/tool/data/librarydata/library-ltspice>, <https://www.murata.com/tool/data/spicedata/netlist-mlcc>) |
| **TDK** (TVCL, SEAT) (<https://www.tdk-electronics.tdk.com/en/3467182/design-support/design-tools/spice-libraries>, <https://product.tdk.com/en/technicalsupport/tvcl/index.html>) | S-parameters, equivalent-circuit netlists, PSpice/LTspice libraries; "Dynamic DC Bias Model" (<https://www.tdk-electronics.tdk.com/en/373812/tech-library/articles/tools-services/tools-services/dynamic-dc-bias-model-for-accurate-circuit-simulation/1035570>) | RLC ladders; DC-bias via behavioural sources / nonlinear C | Site terms; models downloadable per part; assume no redistribution |
| **Würth Elektronik** (REDEXPERT, GitHub libs) (<https://github.com/WurthElektronik/LTspice-Library>, <https://github.com/WurthElektronik/Pspice-Library>, user guide <https://www.we-online.com/files/pdf1/ug002c_user_manual_we_ltspice_library.pdf>) | `.lib`/`.sub` netlists + `.asy`; also ADS/Ansys/Qspice/IBIS | `.SUBCKT` RLC ladders for caps/inductors; transformers as coupled inductors with `K`; some behavioural saturation | Public GitHub, but **no OSS licence file** — contact `libraries@we-online.com` before bundling |
| **KEMET K-SIM** (<https://ksim3.kemet.com/capacitor-simulation>, <https://support.kemet.com/knowledge/how-do-i-download-spice-models>) | Chart-type "SPICE model" -> download in generic/LTspice/PSpice | RLC equivalent per part at chosen bias/temperature | Per-download terms |
| **Samsung Electro-Mechanics** (<https://weblib.samsungsem.com/mlcc/mlcc-ec.do>, guide <https://product.samsungsem.com/resources/file/SPICE_Library_Guide.pdf>) | PSpice `.lib`, HSPICE `.lib`, LTspice `.mod`+`.asy`, SIMetrix `.lib`, `.ckt` per bias/temperature, S-parameters | "Precise" (multi-branch ladder matching measured Z(f)) and "Simple" (single R-L-C) models | Web-library terms; no redistribution stated |

Practical consequence: **ship an importer, not the models.** Let users drop vendor files into a
`models/` folder; never commit them to the repo or embed them in templates.

### 3.2 SPICE subset required for passive models

Grammar observed across Murata/TDK/Würth/Samsung/KEMET files (all are ngspice-compatible):

```
* comment            (also '$' or ';' trailing comments in some vendors)
.SUBCKT name n1 n2 [n3 ...] [PARAMS: p=v ...]
+ continuation line (leading '+')
Rxxx n1 n2 value          | value may be {expr} or a .PARAM name
Lxxx n1 n2 value [Rser=..]
Cxxx n1 n2 value
Kxxx Lyyy Lzzz k          (coupled inductors, |k|<=1, 2 or more L names)
Xyyy n1 n2 subname [params]   (nested subckt instance)
.PARAM name=value ...
.ENDS [name]
.MODEL ... (rare in passives; skip or warn)
Bxxx / Exxx / Gxxx with expressions   (dynamic models — out of scope v1; detect and refuse cleanly)
```
Numbers: case-insensitive suffixes `T=1e12 G=1e9 MEG=1e6 K=1e3 M=1e-3 U=1e-6 N=1e-9 P=1e-12 F=1e-15
MIL=25.4e-6`; **`M` is milli, `MEG` is mega**; trailing letters after the suffix are ignored (`10uF`, `4.7KOhm`).
Node `0` is ground; node names are arbitrary tokens. Refs: <https://ltwiki.org/LTspiceHelp/LTspiceHelp/A_General_Structure_and_Conventions.htm>,
<https://www.embedded.com/guide-to-spice-simulation-for-circuit-analysis-and-design-part-19-defining-a-subcircuit-with-the-subckt-directive/>,
<https://nmg.gitlab.io/ngspice-manual/circuitdescription/paramparametricnetlists/subcircuitparameters.html>.

### 3.3 Proposed `.subckt` importer design

1. **Lexer/parser** (`src/spice_import.c`): join `+` continuations, strip comments, tokenise; parse the
   grammar above into an AST: `{name, pins[], params{}, elements[], instances[], children[]}`. Evaluate
   `{expr}` with a tiny arithmetic evaluator (+ - * / ^ and functions `sqrt exp log`) over the param scope
   (instance params override subckt defaults override `.PARAM`).
2. **Flattening**: recursively expand `X` instances; prefix internal node names (`X1.n3`) and element names.
   Produce a flat element list of R, L, C, K.
3. **Mapping to existing components** (all exist in `types.h`): `R -> COMP_RESISTOR`, `C -> COMP_CAPACITOR`
   (ideal=true), `L -> COMP_INDUCTOR` (ideal=true; `Rser=` -> `dcr`), `K -> COMP_TRANSFORMER` replaced by the
   coupled-inductor element from 2.7 (needs a new `COMP_MUTUAL` or a `coupling` array on the inductor; until then
   refuse `K`).
4. **Build a `SubCircuitDef`**: allocate components, auto-place them on a grid inside `internal_width/height`,
   create `Wire`s by connecting all elements sharing a node name, mark subckt pins as `SubCircuitPin`
   (`name` = pin token, `side` = left for odd index / right for even), compute `num_internal_nodes`. Serialise
   into `component_data/wire_data` exactly as `subcircuit_create_from_selection` does, then append to
   `g_subcircuit_library` (`MAX_SUBCIRCUIT_DEFS 32` — raise to 128 or make dynamic).
5. **Metadata**: keep the original file path, vendor comment header, and a "static @ 25 C / 0 V" tag in
   `description` (128 chars) so the UI can warn that DC-bias/temperature behaviour is not modelled.
6. **Validation**: after import, run an internal AC sweep (`simulation_freq_sweep` already exists) across
   1 kHz-1 GHz and compare `|Z(f)|` to the vendor `.s2p` (convert `S11` on 50 ohm: `Z = 50*(1+S11)/(1-S11)` for
   series-through or shunt fixtures per the vendor's note). Log max dB error.
7. **Dynamic models** (later): recognise the Murata/TDK behavioural pattern (`B` source with `V(...)`
   polynomial or table) and map it to the nonlinear `C(V)` of 2.8; until then, tell the user to export a
   static netlist at their bias point from SimSurfing.
8. **File I/O**: hook into `file_io.c` (`Load SPICE model...`), accept `.lib .mod .cir .sub .txt .sp`.

---

## 4. Electrical reference for the template circuits

For each family: governing equations, design rules, and what a correct simulation must show. Use these
numbers as acceptance checks (Section 5). Textbook refs: Sedra/Smith *Microelectronic Circuits*;
Horowitz/Hill *The Art of Electronics*; TI *Analog Engineer's Circuit Cookbook*; Wikipedia for formulas.

### 4.1 Voltage divider, Wheatstone bridge, LED with resistor
- Divider: `Vout = Vin*R2/(R1+R2)`, `Rout = R1||R2`. Loaded error `= R_out/(R_out+R_L)`.
- Wheatstone: `V_out = V_s*( R2/(R1+R2) - R4/(R3+R4) )`; balance when `R1/R2 = R3/R4`; small-delta
  sensitivity `~ V_s*dR/(4R)` (<https://en.wikipedia.org/wiki/Wheatstone_bridge>).
- LED: `I = (V_s - V_f)/R`; red `V_f ~ 1.8-2.0 V`, green 2.0-2.2, blue/white 2.8-3.4; 5 V/330 ohm/red ->
  ~9.7 mA. Sim must show the exponential model settling to `V_f` within 0.1 V of these.

### 4.2 RC / RL first-order filters
- `f_c = 1/(2 pi R C)` (RC) or `R/(2 pi L)` (RL); -3.01 dB and 45 deg at `f_c`; -20 dB/decade; step
  response `1 - e^{-t/tau}`, 63.2 % at `tau`, 99.3 % at `5 tau`. Bode sweep must reproduce -3 dB within 0.1 dB
  and phase within 1 deg; transient must match `e^{-t/tau}` (BE overshoots the time constant by `~h/(2 tau)`
  per step; trap is exact to `O(h^2)`).

### 4.3 Series / parallel RLC
- `w0 = 1/sqrt(LC)`, series `Q = (1/R) sqrt(L/C)`, parallel `Q = R sqrt(C/L)`, `BW = f0/Q`, damping
  `zeta = 1/(2Q)`. Underdamped step: `f_d = f0 sqrt(1 - zeta^2)`, envelope `e^{-zeta w0 t}`, overshoot
  `exp(-pi zeta/sqrt(1-zeta^2))`. **Key BE test**: with `R -> 0` the ringing amplitude must not decay
  faster than the analytic envelope (BE adds `~ (w0 h)^2/2` per-step damping).

### 4.4 Rectifiers and power supplies (half-wave, bridge, center-tap, AC-DC, 120 V/60 Hz -> 12 V)
- Peak `V_p = V_sec,rms*sqrt(2) - n*V_f` (n = 1 for half-wave/center-tap, 2 for bridge).
- Ripple (light-load, sawtooth approx): `V_r,pp ~ I_L/(f_r C)`, `f_r = f` (half-wave) or `2f` (full-wave)
  (<https://www.electronics-tutorials.ws/diode/diode_6.html>). `V_dc ~ V_p - V_r/2`.
- PIV: half-wave `V_p`, bridge `V_p`, center-tap `2 V_p`. Diode conduction angle shrinks as `C` grows; peak
  diode current `>> I_L` (typ 5-10x). Correct sim: ripple within 10 % of formula at `R_L C >> 1/f`, diode
  current spikes visible, transformer primary current reflecting load (requires 2.7).
- 12 V/1 A supply rule-of-thumb: `C = I/(2 f V_r) = 1/(120*1) ~ 8.3 mF` for 1 V ripple at 60 Hz.

### 4.5 Regulators: 7805, LM317, TL431, Zener reference
- Zener: `I_z = (V_in - V_z)/R_s - I_L`; keep `I_z >= I_z,min` (~5 mA); load regulation `dV = r_z dI`.
- 7805: `V_out = 5 V +-4 %`, dropout ~2 V, `I_q ~ 4-5 mA`, ripple rejection ~78 dB at 120 Hz
  (<https://hades.mech.northwestern.edu/images/6/6c/LM7805.pdf>). Sim must hold 5.00 V for `V_in >= 7 V`
  and follow `V_in - 2 V` below dropout.
- LM317: `V_out = 1.25 (1 + R2/R1) + I_adj R2`, `I_adj ~ 50 uA`, `R1 = 240 ohm` typical; min load 3.5-10 mA
  (<https://www.ti.com/lit/ds/symlink/lm317.pdf>). 240/720 ohm -> 5.0 V.
- TL431: `V_KA = V_ref (1 + R1/R2)`, `V_ref = 2.495 V`, needs `I_KA >= 1 mA`, cathode dynamic impedance
  ~0.2 ohm (<https://www.ti.com/lit/ds/symlink/tl431.pdf>). The exponential model in `component.c` (~4349)
  should regulate within 10 mV for 1-100 mA.

### 4.6 Op-amp DC circuits (follower, inverting, non-inverting, difference, summing, TIA, in-amp)
- Inverting `A = -R_f/R_in`, `Z_in = R_in`; non-inverting `A = 1 + R_f/R_g`; follower `A = 1`.
- Finite-gain error: `A_cl = A_ideal / (1 + 1/(A0 beta))`, i.e. 0.1 % low for `A0 beta = 1000`. A correct
  sim with `A0 = 1e5` shows gain errors `< 0.01 %` — a good regression number.
- Difference amp `V_o = (R2/R1)(V2 - V1)` with matched ratios; CMRR limited by resistor mismatch:
  `CMRR ~ (1 + R2/R1)/(4 tol)`.
- Summing `V_o = -R_f (V1/R1 + V2/R2 + ...)`. TIA `V_o = -I_in R_f`, bandwidth `f = sqrt(GBW/(2 pi R_f C_in))`;
  needs `C_f ~ sqrt(C_in/(2 pi R_f GBW))` for stability — with 2.6 the sim should ring without `C_f`.
- 3-op-amp in-amp: `G = (1 + 2R/R_g) * R3/R2`.
- Slew: max sine frequency without distortion `f_max = SR/(2 pi V_peak)` (0.5 V/us, 10 Vp -> 8 kHz).
  Once 2.6 lands, a 20 kHz 10 Vp follower must triangulate.

### 4.7 Integrator / differentiator
- Integrator `V_o = -(1/RC) integral V_in dt`; square in -> triangle out with slope `V_in/(RC)`; needs a
  bleed `R_f` (`f_low = 1/(2 pi R_f C)`) or the output drifts to a rail from `V_os`/`I_bias` — the sim should
  show that drift when `voffset != 0`.
- Differentiator `V_o = -RC dV_in/dt`; triangle -> square; add `R_s` in series with `C` to limit HF gain
  (`f = 1/(2 pi R_s C)`), otherwise noisy/oscillatory with a real op-amp.

### 4.8 Sallen-Key LP, active band-pass, twin-T notch
- Sallen-Key: `f_c = 1/(2 pi sqrt(R1 R2 C1 C2))`,
  `Q = sqrt(R1 R2 C1 C2) / (C2 (R1 + R2) + R1 C1 (1 - K))`. Unity-gain equal-R design: `C1 = 2Q^2 C2`
  ... Butterworth `Q = 0.7071` -> `C1/C2 = 2`; `-3 dB` at `f_c`, -40 dB/decade, group delay peak
  (<https://www.ti.com/lit/pdf/sboa226>). Sim must show exactly -3.01 dB at `f_c` for Butterworth and
  peaking `20 log Q` dB for `Q > 0.707`.
- MFB band-pass: `f_0 = (1/2 pi C) sqrt((R1 + R3)/(R1 R2 R3))` (two-C equal), `Q = pi f_0 R2 C`,
  mid-band gain `-R2/(2 R1)`.
- Twin-T: `f_n = 1/(2 pi R C)` with arms `R, R, R/2` and `C, C, 2C`; passive `Q = 0.25`; bootstrapped
  `Q = (1 + R_g/R_f)/4` (<https://www.analog.com/media/en/training-seminars/tutorials/MT-225.pdf>). Sim must
  show a null > 40 dB with ideal components, sensitive to 1 % mismatch (good Monte-Carlo demo).

### 4.9 Wien bridge and RC phase-shift oscillators
- Wien: `f = 1/(2 pi R C)` (equal R,C); feedback network gain `1/3` at `f`, zero phase; amplifier gain must
  be exactly 3 (`R_f/R_g = 2`) with startup `> 3` and a nonlinear limiter (lamp, diodes across `R_f`,
  JFET) to settle; diode limiters give 1-5 % THD (<https://en.wikipedia.org/wiki/Wien_bridge_oscillator>).
  Correct sim: grows from the op-point kick with `e^{sigma t}`, `sigma = (A-3) w0/3` approx, then limits at
  an amplitude set by the diode knee; frequency within 1 % of formula; FFT THD in the few-% range.
- RC phase-shift (3 x RC lead, inverting amp): `f = 1/(2 pi R C sqrt(6))`, required `|A| >= 29`
  (`R_f/R = 29`) for the unloaded ideal network (<https://en.wikipedia.org/wiki/Phase-shift_oscillator>).
  With BE at `h = 1 us` and `f = 1 kHz`, numerical damping is small but at `h = 100 us` the oscillation dies;
  trap keeps it alive — this is the flagship regression for 2.1.

### 4.10 BJT amplifiers: CE, multistage, differential pair, push-pull, current mirror / source
- Bias (voltage divider): `V_B = V_CC R2/(R1+R2)`, `I_E = (V_B - 0.7)/R_E`, `V_CE = V_CC - I_C(R_C + R_E)`;
  design `V_CE ~ V_CC/2`, divider current `~10 I_B`. Small signal `r_e = V_T/I_E ~ 25 mV/I_E`,
  `A_v = -R_C||R_L / (r_e + R_E,unbypassed)`, `R_in = R1||R2||beta(r_e + R_E)`. Sim must show these within
  ~10 % (Early effect `VAF = 100`) and clip symmetrically when overdriven.
- Differential pair: `A_d = g_m R_C/2` single-ended (`g_m = I_C/V_T`), `I_E,tail` split equally, tanh transfer
  `I_C1 - I_C2 = I_tail tanh(V_id/2V_T)` — full switching at `|V_id| > ~100 mV`.
- Push-pull class B: crossover dead-band of `+-0.6 V`; with bias diodes (class AB) the notch disappears —
  a visual test of the diode model.
- Current mirror: `I_out = I_ref / (1 + 2/beta)`; `I_ref = (V_CC - 0.7)/R`; output resistance `= VAF/I_C`
  (<https://en.wikipedia.org/wiki/Current_mirror>); Widlar: `V_T ln(I_ref/I_out) = I_out R_E`
  (<https://en.wikipedia.org/wiki/Widlar_current_source>). Sim: `I_out` constant within `1/VAF` per volt.

### 4.11 MOSFET: common source, common drain, CMOS inverter
- Level-1: `I_D = (k/2)(V_GS - V_th)^2 (1 + lambda V_DS)` sat; `g_m = sqrt(2 k I_D)`; CS gain
  `-g_m (R_D || r_o)`; source follower gain `g_m R_S/(1 + g_m R_S)`.
- CMOS inverter switching threshold `V_M = (V_thn + sqrt(k_p/k_n)(V_DD - |V_thp|)) / (1 + sqrt(k_p/k_n))`;
  symmetric at `V_DD/2` when `k_n = k_p` (<https://www.egr.msu.edu/classes/ece410/mason/files/Ch7.pdf>).
  VTC should show `V_OH = V_DD`, `V_OL = 0`, a short-circuit current spike at `V_M`, and gain `>> 1` in the
  transition.

### 4.12 Comparator, Schmitt trigger, window comparator
- Comparator: output slews to the rail; propagation limited only by slew (2.6) — with an ideal VCVS it is
  instantaneous, so verify there is no glitch/2-cycle at the crossing (this is what `opamp_stamp_output`'s
  hysteresis prevents).
- Inverting Schmitt: `V_TH,TL = +-V_sat R1/(R1+R2)`; hysteresis `2 V_sat R1/(R1+R2)`
  (<https://aec-iitkgp.vlabs.ac.in/exp/schmitt-trigger/theory.html>). Sim: X-Y plot must be a clean rectangle.
- Window comparator: two comparators with `V_L < V_in < V_H` -> high (TI SBOA221,
  <https://www.ti.com/lit/pdf/sboa221>). Open-collector wired-AND needs a pull-up; the sim should show the
  output high only in the window.

### 4.13 Precision rectifier, peak detector, clamper
- Half-wave precision rectifier: diode inside the loop, `V_o = V_in` for `V_in > 0`, else 0; the op-amp output
  jumps by `~2 V_f` at zero crossing — slew-limited in reality (2.6 makes this visible). Full-wave two-op-amp:
  `V_o = |V_in|` with `R` ratios 1:1:2 (<https://www.ti.com/lit/pdf/tidu030>).
- Peak detector: `V_C` tracks positive peaks, droop `dV/dt = I_leak/C` (bias current + diode leakage +
  `props.capacitor.leakage`); reset via switch.
- Positive clamper (DC restorer): output `= V_in + V_p - V_f`, requires `R C >> T` (10x); the first cycle
  charges C through the diode — check the output minimum equals `-V_f` and the swing is unchanged
  (<https://en.wikipedia.org/wiki/Clamper_(electronics)>).

---

## 5. Test-oracle ideas

### 5.1 ngspice as the golden reference

1. **Netlist exporter** (`tools/export_spice.py` or C `file_io_export_spice`): walk `circuit->components`,
   map each `COMP_*` to a SPICE card (R, C `ic=`, L, D with `.model D(IS= N= BV= CJO=)`, Q with
   `.model NPN(BF= IS= VAF=)`, M level 1, `E`/`G`/`H`/`F`, `V` with `SIN/PULSE/PWL`, op-amp as a
   behavioural `E1 out 0 VALUE={limit(A*(V(p)-V(m)), Vmin, Vmax)}` or the 2.6 macromodel), node ids as
   numbers with ground = 0. Emit `.tran <dt> <tstop> 0 <dt>` with `.options method=trap reltol=1e-4` and
   `wrdata out.txt v(n1) v(n2) ...`.
2. **Runner**: ngspice batch `ngspice -b circuit.cir` (Windows build from
   <https://sourceforge.net/projects/ngspice/>), or in-process via the shared library
   (`ngSpice_Init`, `ngSpice_Circ`, `ngSpice_Command("bg_run")`, `ngGet_Vec_Info` —
   <https://github.com/imr/ngspice/blob/master/src/include/ngspice/sharedspice.h>) or PySpice
   (<https://github.com/PySpice-org/PySpice>).
3. **Comparison**: resample both traces on the ngspice time grid (linear interpolation), then compute per-probe
   `max |dv|`, RMS error, and for oscillators compare *frequency and steady-state amplitude* (FFT peak) rather
   than samples (phase drifts). Tolerances: linear circuits 0.5 % RMS; diode/BJT 2 %; oscillators 1 % f, 5 % A.
4. **Automation**: a `ctest`/pytest job that loads each of the 47 templates headlessly (the app already has
   `--headless`-style debug runs per `run_with_debug.bat`), simulates N periods, dumps probes, and diffs against
   `tests/golden/<template>.csv` produced by ngspice. Regenerate goldens only on deliberate model changes.
5. **Property checks independent of ngspice**: energy balance (`sum P_sources = sum P_dissipated + dE_stored/dt`),
   KCL residual per node `< 1e-9 A` after each step (catches the post-solve clamp), and monotone convergence
   (`max_diff` decreasing) in NR.

### 5.2 Analytic unit tests (no external tool)

| Test | Expected | Checks |
|---|---|---|
| RC step, `tau = 1 ms`, `h = 10 us` | `v(t) = V(1 - e^{-t/tau})`, error `< 1e-4` (trap) | integrator order |
| Lossless LC, `f0 = 1 kHz`, 100 cycles | amplitude decay `< 0.1 %` (trap), BE decays `~ (w0 h)^2/2` per step | numerical damping |
| Series RLC `Q = 10` step | overshoot `exp(-pi/sqrt(4Q^2 - 1))`, `f_d` | 2nd-order accuracy |
| Diode + R from 5 V | `I` s.t. `V_f = nVt ln(I/Is + 1)`, `V_R = 5 - V_f` (solve with Lambert W) | Shockley stamp, pnjlim |
| Bridge + C, `R_L C = 100/f` | ripple `= I/(2fC)` within 10 % | rectifier |
| Inverting amp `A0 = 1e5`, `R_f/R_in = 10` | gain `-10*(1/(1 + 11/1e5))` | op-amp linear stamp |
| Comparator swing | output exactly at `vmax`/`vmin`, KCL residual 0 | rail handling without post-clamp |
| Wien `R = 10k, C = 16 nF` | `f = 994.7 Hz +-1 %`, THD `< 5 %` | oscillator start & limit |
| Phase-shift `R = 10k, C = 10 nF` | `f = 650 Hz +-2 %` | trap vs BE |
| LM317 `240/720` | `5.00 V +-0.5 %` | regulator model |
| Current mirror `beta = 100` | `I_out/I_ref = 1/1.02` | BJT model |
| CMOS inverter `k_n = k_p` | `V_M = V_DD/2 +-1 %` | MOSFET model |
| Ideal transformer 10:1 with load | `I_p = I_s/10`, `P_in = P_out` | 2.7 energy consistency |
| Vendor MLCC `.subckt` import | `|Z(f)|` vs `.s2p` within 1 dB, SRF within 5 % | importer + AC sweep |

### 5.3 Where the existing analysis code helps

`simulation_freq_sweep` (Bode) already gives an AC oracle for the filter family; `analysis.c` Monte Carlo can
be reused to test tolerance sensitivity of the twin-T null. Expose both from a CLI flag so the test job can
run without the SDL window.

---

## Sources (primary)

- CircuitJS1 source: <https://github.com/pfalstad/circuitjs1> — `SimulationManager.java` (sub-iteration/timestep
  logic), `Diode.java` (`limitStep`), `TransistorElm.java` (dynamic gmin), `OpAmpElm.java`, `CapacitorElm.java`.
- Falstad about page: <https://www.falstad.com/circuit/about.html>; CircuitJS1 hosted: <https://lushprojects.com/circuitjs/>.
- ngspice manual: <https://ngspice.sourceforge.io/docs/ngspice-html-manual/manual.xhtml>; option pages at
  <https://nmg.gitlab.io/ngspice-manual/>; shared lib: <https://ngspice.sourceforge.io/shared.html>.
- LTspice integration methods: <https://ltwiki.org/files/LTspiceHelp.chm/html/integration_method_issues.htm>;
  ADI "SPICE Differentiation": <https://www.analog.com/en/resources/technical-articles/spice-differentiation.html>.
- SPICE convergence: Intusoft <http://www.intusoft.com/articles/converg.pdf>; Kundert, *Designer's Guide to SPICE
  and Spectre* (Kluwer 1995); <https://kenkundert.com/docs/eda+t93-preso.pdf>.
- TR-BDF2: Bank et al. 1985; Hosea & Shampine 1996 <https://www.sciencedirect.com/science/article/abs/pii/0168927495001158>;
  <https://math.la.asu.edu/~gardner/TRBDF.pdf>.
- Companion models: <http://circsimproj.blogspot.com/2009/07/companion-models.html>.
- Op-amp macromodels: Boyle et al., IEEE JSSC 1974; <https://www.electronicdesign.com/technologies/analog/article/21806271/spice-it-up-understanding-and-using-op-amp-macromodels>;
  <https://qucs.sourceforge.net/docs/tutorial/opamp.pdf>.
- Transformers/K: <https://ltwiki.org/LTspiceHelp/LTspiceHelp/K_Mutual_Inductance.htm>;
  <https://www.analog.com/en/resources/technical-articles/ltspice-basic-steps-for-simulating-transformers.html>;
  <https://www.coilcraft.com/en-us/models/howto/simulation-model-considerations-part-ii-(1)/>.
- Nonlinear L/C: <https://ltwiki.org/index.php?title=The_Arbitrary_Inductor_model>;
  <https://www.analog.com/en/resources/analog-dialogue/raqs/raq-issue-192.html>.
- Murata: <https://www.murata.com/en-us/tool/simsurfing>, <https://www.murata.com/en-us/tool/data/spicedata>,
  <https://www.murata.com/en-us/tool/data/librarydata/library-ltspice>, <https://article.murata.com/en-us/article/mlcc-dynamic-model-supports-circuit-simulations>.
- TDK: <https://www.tdk-electronics.tdk.com/en/3467182/design-support/design-tools/spice-libraries>,
  <https://product.tdk.com/en/technicalsupport/tvcl/index.html>.
- Würth: <https://github.com/WurthElektronik/LTspice-Library>, <https://www.we-online.com/files/pdf1/ug002c_user_manual_we_ltspice_library.pdf>.
- KEMET: <https://ksim3.kemet.com/capacitor-simulation>; Samsung: <https://weblib.samsungsem.com/mlcc/mlcc-ec.do>,
  <https://product.samsungsem.com/resources/file/SPICE_Library_Guide.pdf>.
- SPICE syntax: <https://ltwiki.org/LTspiceHelp/LTspiceHelp/A_General_Structure_and_Conventions.htm>,
  <https://www.embedded.com/guide-to-spice-simulation-for-circuit-analysis-and-design-part-19-defining-a-subcircuit-with-the-subckt-directive/>.
- Qucs-S: <https://qucs-s-help.readthedocs.io/en/latest/overview/choosing-a-sim-backend.html>; EasyEDA:
  <https://docs.easyeda.com/en/Simulation/Chapter1-Introduction/>; PySpice: <https://github.com/PySpice-org/PySpice>.
- Circuit references: TI SBOA226 (Sallen-Key), SBOA221 (window comparator), TIDU030 (precision rectifier);
  LM317 <https://www.ti.com/lit/ds/symlink/lm317.pdf>; TL431 <https://www.ti.com/lit/ds/symlink/tl431.pdf>;
  LM7805 <https://hades.mech.northwestern.edu/images/6/6c/LM7805.pdf>; ADI MT-225 (twin-T);
  Wikipedia: Wien bridge, phase-shift oscillator, current mirror, Widlar, clamper, Wheatstone.
