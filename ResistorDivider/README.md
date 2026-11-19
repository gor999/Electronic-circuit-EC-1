# Electronics Lab (Qt 6 Widgets)

Three tools in one window, selected with the tabs at the top:

- **Circuit Simulator** - draw a circuit on a grid and watch it run: wires
  coloured by voltage, moving dots for current, live readings, oscilloscope.
- **Resistor Divider** - series resistor chain calculator with a drawn circuit.
- **RC Filter** - first-order low-pass / high-pass calculator with schematic
  and gain/phase plot.
- **MOSFET 3D** - interactive 3D model of an n-channel enhancement MOSFET
  driven by a long-channel physics model: every voltage, dimension and
  material value can be changed and the device, carriers, graphs and
  explanations follow.

## Using the MOSFET 3D tab

### Changing parameters

The left panel has six tabs. Every control is a slider plus a text box (the
box accepts `1e17`, `0.35` and so on); hover a control for its range and for
what it affects, which is also shown under the panel.

| Tab | Parameters |
|---|---|
| Bias | VGS, VDS, VBS, VS (VG, VD, VB are shown), VTH0, or VTH calculated from the physics |
| Geometry | L, W, tox, xj, source and drain length, gate thickness, substrate thickness |
| Material | NA, source ND, drain ND, µn, µp, εr of silicon and oxide, T, ni at 300 K |
| Model | body effect, channel-length modulation, mobility degradation (θ), temperature dependence |
| Sweep | pinch-off experiment: sweep VDS or VGS, from/to, ramp time, step size, speed, Play/Pause, Step, Reset, plus a guided experiment |
| View | full 3D, 2D cross-section, top and side cameras; carriers, arrows, labels, components, exploded view, section plane, drawing scale |

Every change is checked before it is applied (ranges, source/drain length
large enough for the lateral diffusion, ni(T) well below NA, no forward-biased
body). After each change the **What happened** box lists which results changed
and by how much, whether the operating region changed, and why. If a change
only affects the drawing, it says so.

### Reading the results

- **Results:** region, VTH (with body effect), VOV, VDS,sat, ID, channel
  resistance, pinch-off point and ΔL, Cox, charge, field and transit time.
- **Probe:** a slider along the channel (or the Source / Centre / Drain /
  Pinch-off buttons) reads V(x), |Qinv(x)|, electron density, lateral field and
  drift velocity at that point; it is also drawn as a line in the 3D view.
- **Graphs:** two panes, each with a selector: ID–VDS (with a VGS family and
  the pinch-off locus), ID–VGS (lin or log), V(x), |Qinv(x)|, |Ex(x)|, n(x),
  v(x), pinch-off position vs VDS, ID vs time during a sweep, channel
  resistance vs VGS, ID vs L, ID vs W. Beyond the pinch-off point the profiles
  are drawn dashed, because the model is only approximate there.
- **Experiments:** 17 ready-made experiments (cut-off, linear, saturation,
  onset of pinch-off, short/long, narrow/wide, thin/thick oxide, doping, body
  effect, hot device...), each with a note on what to watch. **Save...** stores
  your own configurations; **Defaults** restores everything.
- **Compare A / B** shows two devices side by side with synchronized cameras;
  choose which one the controls edit, or copy A into B. Graphs show B in orange
  and the results table shows the B/A ratio.
- **Physics check...** shows every equation with the present numbers, a set of
  self-consistency checks (current continuity along the channel, ID continuous
  at VDS,sat, W and L scaling, cut-off, body effect...) and the assumptions.

### The model

Gradual-channel (charge-sheet) model, square law, n+ poly gate:

    VTH  = VTH0 + γ(√(2φF − VBS) − √(2φF))       γ = √(2q εSi NA) / Cox
    VOV  = VGS − VTH
    ID   = µn Cox (W/L) [VOV·VDS − VDS²/2]         linear, VDS < VOV
    ID   = µn Cox (W/2L) VOV² · L/(L − ΔL)        saturation
    ΔL   = √(2 εSi (VDS − VOV) / (q NA))           channel-length modulation
    Qinv(x) = −Cox (VGS − VTH − V(x))
    µ    = µ0 (T/300)^−1.5 / (1 + θ VOV)          optional

V(x) is solved from current continuity; past the pinch-off point electrons
move at the saturation velocity, which sets their density there.

Each displayed quantity is labelled **calculated**, **approximate** or
**illustrative**. Not modelled: subthreshold conduction, short-channel
effects (DIBL, velocity saturation in the channel) and 2D potential/field
maps of the whole cross-section. Source/drain length, gate thickness and
substrate thickness change the drawing only; µp only enters the substrate
resistivity. The model warns when it is outside its range (L < 1 µm,
tox < 3 nm, punch-through).

### Camera and inspection

- Drag to rotate, right- or middle-drag to pan, wheel to zoom. Double-click a
  part to focus on it; double-click empty space to go back.
- Hover a part for a short explanation, click it for the full description
  (using the present operating point).
- Thin layers are exaggerated on screen; **Dimensions and scale...** lists the
  real sizes and how each is drawn.
- Channel electrons are drawn as dots whose number follows the calculated
  sheet density and which move at the calculated drift velocity (slowed down;
  the time scale is in the View tab).

The 3D view needs OpenGL 3.3, which any desktop GPU from the last decade
provides. The Qt OpenGL and OpenGLWidgets modules it uses are part of Qt's base
package, so nothing extra has to be installed in Qt Creator.

## Using the circuit simulator

- **Place a part:** click it in the palette on the left (or press its key,
  shown in brackets), then drag on the workspace. A plain click places it at a
  default size. You can also drag a part from the palette onto the workspace.
  Press Esc to go back to Select / Move.
- **Connect:** parts connect where their end points meet. Use Wire (W) between
  them. A larger dot marks a junction of three or more ends.
- **Edit:** double-click a part (or right-click > Edit Properties) to change
  its values. Values accept prefixes, for example `4.7k` or `100n`. Apply
  changes the running circuit without closing the dialog.
- **Select and move:** click, Ctrl/Shift-click, or drag a rectangle; drag to
  move; drag an end handle of a selected part to stretch it. Rotate, Flip,
  Duplicate, Cut/Copy/Paste, Delete and Undo/Redo are in the Edit menu.
- **Switches** toggle with a click.
- **View:** mouse wheel zooms, middle-drag or Alt-drag pans, Fit shows the
  whole circuit.
- **Measure:** hover a part to read its voltage, current and power. Voltmeter
  and Ammeter parts show their reading on the schematic. Right-click > View in
  Scope plots a part's voltage and current with peak, peak-to-peak and
  frequency read-outs.
- **Simulation:** Pause/Run, Step and Reset on the toolbar; Speed sets how
  much simulated time passes per frame, Current sets how fast the dots move.
  The time step is under Options.
- **Circuits menu:** ten examples, from a voltage divider to a CMOS inverter
  and a half adder. File > Save As / Open store circuits as text.

## Project structure

    ResistorDivider/
    ├── CMakeLists.txt
    ├── main.cpp                      Entry point and dark theme (style sheet)
    ├── MainWindow.h/.cpp             Window with one tab per tool
    ├── Ui.h/.cpp                     Shared widget factories and input validation
    ├── Format.h                      Shared number / SI-prefix formatting
    │
    ├── mosfet/                       MOSFET 3D
    │   ├── physics/                  No Qt: parameters, formulas, device model
    │   │   ├── MosfetParameters.h    Dimensions, doping, material, model options (SI units)
    │   │   ├── Electrostatics.h      Cox, φF, Vbi, ni(T), µ(T), depletion widths
    │   │   ├── DeviceState.h         Bias in, operating point and profiles out (+ accuracy flags)
    │   │   ├── DeviceModel.h         Model interface
    │   │   ├── AnalyticalModel.cpp   Long-channel model with body effect, CLM, temperature
    │   │   └── Validation.h/.cpp     Self-consistency checks
    │   ├── config/Parameters.h/.cpp  Parameter registry (name, symbol, unit, range,
    │   │                             description, effects), validation, save format
    │   ├── geometry/                 No Qt: regions from the operating point, meshes, picking
    │   ├── particles/                Carrier dots matched to the calculated density
    │   ├── render/                   OpenGL 3.3: camera, materials, renderer
    │   └── ui/                       3D view, page, controls, graphs, experiments,
    │                                 explanations, physics check
    │
    ├── simulator/                    Circuit simulator
    │   ├── Circuit.h/.cpp            Element list and the simulation engine (MNA)
    │   ├── Element.h/.cpp            Base class, factory, save format, palette list
    │   ├── Elements.h                Declarations of all components
    │   ├── ElementsPassive.cpp       Wire, R, C, L, switch, ground, meters
    │   ├── ElementsSources.cpp       Voltage/current sources, rail, logic input
    │   ├── ElementsActive.cpp        Diode, LED, BJT, MOSFET, op-amp, gates
    │   ├── ElementPainter.h/.cpp     Schematic symbols (QPainter)
    │   ├── CircuitView.h/.cpp        Workspace: drawing and all editing
    │   ├── ScopePanel.h/.cpp         Oscilloscope strip
    │   ├── ElementDialog.h/.cpp      Property editor
    │   ├── ExampleCircuits.h/.cpp    Circuits menu
    │   └── SimulatorPage.h/.cpp      Tab: menus, toolbar, palette, frame timer
    │
    ├── DividerPage.h/.cpp            Resistor divider tab (form, table)
    ├── CircuitWidget.h/.cpp          QPainter drawing of the divider
    ├── ResistorCalculator.h/.cpp     Divider maths (no Qt)
    │
    ├── RcFilterPage.h/.cpp           RC filter tab (form, result cards)
    ├── RcCircuitWidget.h/.cpp        QPainter schematic of the filter
    ├── FrequencyResponseWidget.h/.cpp  Gain and phase plot
    └── RcFilterCalculator.h/.cpp     RC filter maths (no Qt)

## Build and run in Qt Creator

1. Install Qt 6 (built and tested with 6.4) with a desktop kit (MinGW, MSVC, GCC or Clang).
2. Qt Creator: File > Open File or Project... and choose `CMakeLists.txt`.
3. Select a Qt 6 desktop kit on the Configure Project page, then Configure Project.
4. Press Ctrl+R (Run).

If you opened an earlier version of this project, run Build > Clear CMake
Configuration and then Build > Run CMake, so the new source files are picked up.

## Build from the command line

    cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
    cmake --build build
    ./build/ResistorDivider          # Windows: build\ResistorDivider.exe

If CMake cannot find Qt, pass its location, for example
`-DCMAKE_PREFIX_PATH=C:/Qt/6.7.2/mingw_64`.

## How the classes interact

- `MainWindow` only creates a `QTabWidget` holding a `SimulatorPage`, a
  `DividerPage` and an `RcFilterPage`. Each page is self-contained.
- In the simulator, `SimulatorPage` owns the `Circuit` and a frame timer. Each
  frame it calls `Circuit::step()` several times, feeds the `ScopePanel`, and
  repaints the `CircuitView`. `CircuitView` edits the circuit's elements;
  `ElementPainter` draws them. The element classes hold only the electrical
  model and have no drawing code.
- In the MOSFET tab, data flows one way: `MosfetParameters` and a bias go
  into a `DeviceModel`, which returns a `DeviceState`. `MosfetGeometry` turns
  the state into 3D parts, and `MosfetView` draws them through
  `MosfetRenderer`; `CarrierParticles` places the electron dots from the same
  state. Physics and geometry have no Qt or OpenGL code. `MosfetPage` holds one
  or two slots (A and B), each with its own configuration, state and view; every
  control writes through the parameter registry in `config/Parameters`, which
  validates the change before the model is re-run. A Poisson / drift-diffusion
  solver can later replace `AnalyticalModel` behind the same interface.
- `Circuit` uses modified nodal analysis: element ends joined by wires become
  nodes, each element stamps its equations into a matrix, and the matrix is
  solved every time step. Diodes, transistors and op-amps are solved by Newton
  iteration; capacitors and inductors use the trapezoidal rule.
- Each page reads and validates its fields with `Ui::readPositiveValue`, calls
  its calculator, and passes the result struct to its display widgets.
- `ResistorCalculator` and `RcFilterCalculator` are plain C++ with no Qt
  dependency. They throw `std::invalid_argument` on input they cannot use.
- `CircuitWidget`, `RcCircuitWidget` and `FrequencyResponseWidget` only draw
  what they are given.

## RC filter formulas

    tau = R * C          wc = 1 / (R * C)          fc = 1 / (2 * pi * R * C)

    Low-pass    H(jw) = 1 / (1 + jwRC)
                |H|   = 1 / sqrt(1 + (wRC)^2)       phase = -arctan(wRC)

    High-pass   H(jw) = jwRC / (1 + jwRC)
                |H|   = wRC / sqrt(1 + (wRC)^2)     phase = 90 deg - arctan(wRC)

    Gain (dB) = 20 * log10(|H|)      Vout = Vin * |H|      Xc = 1 / (w * C)

Unit selectors (Ω/kΩ/MΩ, pF/nF/µF/mF, Hz/kHz/MHz) are converted to SI units
before the calculation.

## Input rules

- Divider: 1 to 20 resistors.
- All values must be positive numbers, with `.` as the decimal separator.
  Letters, signs and exponents cannot be typed.
- Empty, zero or invalid fields are reported in a message box and focused.
