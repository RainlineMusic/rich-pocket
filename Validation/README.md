# Validation of Rich Pocket 0.1.0

Built successfully with JUCE 8.0.4 / GCC 13 on Linux, Release, VST3 and integration test.
Windows/macOS build workflows are provided, but those binaries were not built here.

Full processor: 1 kHz sine, 0 dBFS peak, Input/Output 0 dB, one colour at 100%:

| Colour | Strongest harmonic | Level relative to fundamental |
|---|---|---|
| Warm | H2 | -25.9715 dBc |
| Body | H3 | -33.6354 dBc |
| Silk | H3 | -28.6034 dBc |
| Air | H3 | -31.5500 dBc |
| All four at 100% | H2 | -28.9625 dBc |

A shared residual mix budget makes the all-maximum combination more conservative.
Different combinations retain different harmonic spectra; strongest-harmonic level
alone does not measure total distortion or perceived intensity.

Passed: Wright omega equation, monotonic individual curves, table accuracy,
zero-colour transparency, DC rejection, high-level finite output, actual 8x processor
harmonic ceiling, mono layout/processing, stereo identity, oversized blocks,
44.1/48/96/192 kHz preparation and processing, state recall, both bypass paths with
non-unity gains, disabled activation, six-control layout at 400/800/1200 widths.
Software screenshots of Solid Dark, Neon, Amber and bypass were visually inspected.
Native OpenGL/window lifecycles, Windows/macOS host validation, high-frequency alias
measurements and listening comparisons on mastering material remain to be done.

The supplied source ZIP remains unchanged. Original Duck processor and documentation
are retained in Legacy; the original license implementation stays in Source/License.h.
