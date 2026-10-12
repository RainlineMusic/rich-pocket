# Rich Pocket 0.1.0 — development prototype

Built from the supplied Duck Pocket source, retaining the fonts, dial artwork,
three themes, menu styling, bypass blur, host renderer options,
resize behaviour and logo double-click sound. Four large colour controls have
labels above them; Input and Output are smaller controls on the right.
The editor is 800 × 320 at its default size, with no graphs or freeze control.
Graph history capture and its menu entries are disabled.
The obsolete Duck sidechain/filter/lookahead controls are hidden. Original
processor, build configuration and documentation are preserved in `Legacy/`.

## Controls and DSP

| Control | Range | Character |
|---|---|---|
| Warm | 0–100% | Asymmetric diode curve, primarily even harmonics, gentle low shelf |
| Body | 0–100% | Symmetric diode bridge, odd harmonics, slight Warm interaction |
| Silk | 0–100% | C2 polynomial soft knee, gentle high-frequency softening |
| Air | 0–100% | Soft knee applied to a high-pass branch, gentle presence shelf |
| Input / Output | −12…+12 dB | Gain before / after colour processing |

Each colour adds a modest nonlinear residual to the input. Body receives an
additional 0.18 times the Warm residual, allowing a small controlled interaction.
A shared 90% residual mix budget prevents transfer-curve reversal with every
colour fully enabled; above the budget, residual contributions scale together.
Air's branch uses a 900 Hz one-pole high-pass. A 3 Hz DC blocker acts on the
combined residual only. Shelf asymptotes share a maximum ±1.5 dB budget;
their poles are 350 Hz and 5 kHz. There is no hidden limiter or automatic loudness
normalization. Increased Input deliberately increases harmonic content.

The nonlinear curves use precomputed tables over ±16 with linear interpolation
and analytic continuation beyond this range. The audio callback does not allocate
tables or buffers. The processor supports mono/stereo, oversize host blocks,
parameter smoothing, click-free bypass and state recall. It processes at fixed
8× oversampling with JUCE's high-quality polyphase IIR filters, reporting integer
latency to the host. Only the residual is oversampled: with all colours at zero,
unity Input/Output reproduces the original samples within float rounding precision after the reported delay.

`Source/RichDSP.h` contains all curve equations and blend coefficients.
Wright omega solves `w + ln(w) = z`. Warm uses the normalized asymmetric curve
`(omega(4x+1)-1)/2`. Body uses the normalized bridge curve
`[x-(omega(6x-2)-omega(-6x-2))/12] / [1/(omega(-2)+1)]`.
Silk and Air use `q(u)=u−2u³/3+u⁵/5` within |u|≤1 and ±8/15 outside,
divided by their drives, respectively 1.8 and 1.2. Its derivative is
`(1−u²)²` inside the knee. These are independently implemented mathematical
models; no claim is made that they reproduce Aroma's proprietary algorithms.

Sources: [Pines, Diode VCAs, DAFx 2025](https://www.corianderpines.org/download/dafx25_diode_vca.pdf),
[Piecewise Polynomial Waveshapers, DAFx 2023](https://www.dafx.de/paper-archive/2023/DAFx23_paper_61.pdf).
The current prototype uses oversampling, not ADAA. Listening tests and high-frequency
alias measurements are still required before choosing a final release quality mode.

## Calibration

The −23 dB ceiling means the strongest harmonic relative to the fundamental
(dBc), not THD. The test condition is a 1 kHz sine at **0 dBFS peak**, unity
Input/Output, one colour at 100%, after filter settling. The test checks harmonics
2 through 20. This is a reference condition, not a guarantee for every input level,
frequency or combined setting. `RichIntegrationTest` measures the actual full
oversampled processor; `RichDSPTest` also validates the standalone curves,
table accuracy, monotonicity, DC rejection and high-level finite output.

## Authorization

`RICH_ENABLE_LICENSE=OFF` by default. The original `Source/License.h`, offline
signature verification, online API code and activation UI are retained. This mode
bypasses the activation gate and does not call `checkOnOpen()`.
Before enabling a public release, migrate the retained Duck product/device scope,
license storage, extension and public key to Rich Pocket; configure the Rich
backend using `DUCK_LICENSE_API_URL`, and test online/offline activation.
Simply turning the flag on currently restores the inherited Duck license contract.

## Building

Use JUCE 8.0.4 in a sibling `JUCE` directory, or provide `JUCE_DIR`.
The default format list includes AAX; AAX requires its SDK. A minimal source build:

```sh
cmake -S . -B build -DJUCE_DIR=/path/to/JUCE \
  '-DDUCK_FORMATS=VST3;Standalone' -DRICH_ENABLE_LICENSE=OFF \
  -DDUCK_BUILD_UI_TESTS=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target RichPocket_VST3 RichPocket_Standalone RichDSPTest RichIntegrationTest
ctest --test-dir build -C Release --output-on-failure
```

The software snapshot integration test runs headless on Linux. Native window
and OpenGL testing require a graphical session and are not covered by this test.
Its optional first argument is the screenshot output directory. It captures all
three themes at widths 400, 800 and 1200, plus bypass. `build_macos.sh` builds
universal VST3/AU/Standalone. Inherited Windows/macOS CI has been adapted for
Rich Pocket, including optional CLAP, AAX and validation. AAX developer bundles
still need the original signing process for normal Pro Tools use.

Internal `DuckPocket...` class names, some CMake option names and `DuckAssets`
remain to minimize changes to the supplied foundation. Product/bundle/plugin
identifiers and UI preference storage are separate from Duck Pocket. Original
Duck test files remain archived evidence of the base and are not built as Rich tests.
