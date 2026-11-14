#pragma once
#include "cyui/modules/Plots.h"
#include <limits>

namespace cyui {
struct Peak {
    float minimum = 0, maximum = 0;
};
// Prepared envelope covers [start,end]; one view per channel. No decoder or FFT runs in a widget.
struct WaveformData {
    Range coverage;
    std::vector<ArrayView<Peak>> channels;
};
struct WaveformState {
    Range view{0, 1}, selection{0, 0}; // selection is kept ordered (min <= max)
    bool selecting = false, panning = false;
    double anchor = 0; // where the selection drag started
};
struct WaveformResult {
    bool seek = false, selectionChanged = false;
    double time = 0;
};
CYUI_API WaveformResult waveform(const char *id, WaveformState &, const WaveformData &, double playhead, ImVec2 size);
CYUI_API std::vector<Peak> makePeaks(ArrayView<float> samples, std::size_t columns);
// Logarithmic X; an invalid or non-positive X range resets to 20..20000.
CYUI_API PlotResult spectrum(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
CYUI_API PlotResult spectrum(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size, const PlotOptions &);
// Linear axes.
CYUI_API PlotResult oscilloscope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
CYUI_API PlotResult oscilloscope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size, const PlotOptions &);
// Square X/Y scope: both axes share one range symmetric around 0 (X sets the extent; zoom scales it).
CYUI_API PlotResult stereoScope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size);
CYUI_API PlotResult stereoScope(const char *id, PlotState &, ArrayView<PlotSeries>, ImVec2 size, const PlotOptions &);

// Frame-major matrix: values[frame * bins + bin], increasing time/frequency.
// Missing cells are NaN. Bin spacing may be linear or logarithmic independently of the view.
struct SpectrogramData {
    ArrayView<float> values;
    std::size_t frames = 0, bins = 0;
    Range time{0, 1}, frequency{20, 20000};
    bool logarithmicBins = false;
};
enum class SpectrogramOrientation { timeHorizontal, timeVertical };
struct SpectrogramOptions {
    Range intensity{-100, 0}; // dB, or another host-defined unit
    const char *unit = "dB";
    bool navigation = true, axes = true;
    std::size_t maxCells = 65536; // screen-space sampling budget, capped at 262144
    SpectrogramOrientation orientation = SpectrogramOrientation::timeHorizontal;
    bool reverseTime = false, legend = false;
    ColorRamp ramp;
    bool colorByFrequency = false; // hue from frequency, opacity from intensity; legend then shows Hz
};
struct SpectrogramResult {
    bool valid = false, hovered = false, clicked = false, viewChanged = false;
    std::size_t frame = 0, bin = 0;
    Point mouse; // time, Hz
    float intensity = std::numeric_limits<float>::quiet_NaN();
    Rect bounds; // data area, excluding the optional color legend
};
CYUI_API SpectrogramResult spectrogram(const char *id, PlotState &, const SpectrogramData &,
                                       ImVec2 size, const SpectrogramOptions & = {});

struct SpectrogramState {
    PlotState view{{{0, 8}, false}, {{20, 20000}, true}}; // semantic X=time, Y=frequency in both orientations
    bool followLatest = true;
    double window = 8; // seconds; navigation pauses following, double-click resumes
};
CYUI_API SpectrogramResult spectrogram(const char *id, SpectrogramState &, const SpectrogramData &,
                                       ImVec2 size, const SpectrogramOptions & = {});
// Bounded uniform-hop history. Append once per new analysis frame, outside drawing. view() returns
// a borrowed chronological snapshot, valid until the next mutation/view; synchronize in the host.
class CYUI_API SpectrogramHistory {
  public:
    bool reset(std::size_t capacity, std::size_t bins, double hopSeconds, Range frequency,
               bool logarithmicBins = false, double start = 0);
    bool append(ArrayView<float>);
    void clear(double start = 0);
    SpectrogramData view();
  private:
    std::vector<float> ring_, ordered_;
    std::size_t capacity_ = 0, bins_ = 0, count_ = 0, head_ = 0;
    std::uint64_t total_ = 0;
    double hop_ = 0, start_ = 0;
    Range frequency_;
    bool logarithmic_ = false, dirty_ = true;
};

struct SpectrumBand {
    Range frequency;
    double level = std::numeric_limits<double>::quiet_NaN();
};
struct BandSpectrumOptions {
    double floor = -90;
    ItemId seriesId = 1; // distinct from overlay IDs; result.index identifies the band
    const char *label = "";
    bool gradient = true, colorByFrequency = true;
    ColorRamp ramp{RampStyle::frequency, {}};
    PlotOptions plot;
};
CYUI_API PlotResult bandSpectrum(const char *id, PlotState &, ArrayView<SpectrumBand>,
                                  ArrayView<PlotSeries> overlays, ImVec2 size, const BandSpectrumOptions & = {});

// Per-panel density storage. Revisions prevent re-adding the same input on every UI frame.
// Buffers are bounded by resolution (16..256); view/projection/resolution changes clear history.
struct DensityState {
    std::vector<float> energy, color, scratch;
    int columns = 0, rows = 0, projection = -1;
    Range x, y;
    bool logX = false, logY = false, received = false;
    std::uint64_t revision = 0;
    double time = std::numeric_limits<double>::quiet_NaN();
    CYUI_API void clear();
};
struct PersistenceOptions {
    int resolution = 96;
    double halfLife = .5; // seconds; <=0 clears the previous density each frame
    float exposure = .2f, pointRadius = 4, diffusion = 2; // radius in design pixels; diffusion / second
    std::size_t maxPoints = 65536;
    ColorRamp ramp{RampStyle::intensity, {}};
    bool axes = true, navigation = true;
};
struct StereoDensityState {
    PlotState view{{{-1, 1}, false}, {{-1, 1}, false}};
    DensityState density;
};
struct StereoDensityOptions {
    PersistenceOptions persistence;
    bool midSide = true; // prepared input is L/R; display S=(L-R)/2, M=(L+R)/2 when enabled
};
CYUI_API PlotResult stereoDensity(const char *id, StereoDensityState &, ArrayView<Point> leftRight,
                                   std::uint64_t revision, ImVec2 size, const StereoDensityOptions & = {});
struct SpatialSample {
    double frequency = 1000, position = 0, level = -30; // Hz, normalized L/R [-1,+1], dB
};
struct SpatialScopeState {
    PlotState view{{{-1, 1}, false}, {{20, 20000}, true}};
    DensityState density;
};
struct SpatialScopeOptions {
    PersistenceOptions persistence;
    Range levels{-90, 0}, colorFrequency{20, 20000};
    bool colorByFrequency = true;
    ColorRamp ramp{RampStyle::frequency, {}};
};
CYUI_API PlotResult spatialScope(const char *id, SpatialScopeState &, ArrayView<SpatialSample>,
                                  std::uint64_t revision, ImVec2 size, const SpatialScopeOptions & = {});

inline constexpr double missingAudioValue = std::numeric_limits<double>::quiet_NaN();
struct AudioMeterResult {
    bool resetRequested = false; // host clears its own integrated/held measurements
    std::size_t clippedChannels = 0;
};
struct LoudnessData {
    double momentary = missingAudioValue, shortTerm = missingAudioValue, integrated = missingAudioValue;
    double rangeLow = missingAudioValue, rangeHigh = missingAudioValue; // absolute LUFS bounds
    ArrayView<PlotSeries> history; // seconds / LUFS, caller-owned labels
};
struct LoudnessOptions {
    Range levels{-60, 0};
    double target = -23;
    bool resetButton = true;
};
CYUI_API AudioMeterResult loudness(const char *id, PlotState &historyView, const LoudnessData &,
                                   ImVec2 size, const LoudnessOptions & = {});
struct TruePeakChannel {
    const char *label = "";
    double level = missingAudioValue, held = missingAudioValue; // dBTP; -infinity is silence
    std::uint64_t overs = 0;
};
struct TruePeakOptions {
    Range levels{-60, 6};
    double warning = -6, ceiling = 0;
    bool resetButton = true;
};
CYUI_API AudioMeterResult truePeak(const char *id, ArrayView<TruePeakChannel>, ImVec2 size,
                                   const TruePeakOptions & = {});
// Gauge -1..+1; optional time/correlation history. Pair with stereoScope for L/R or M/S traces.
CYUI_API PlotResult correlation(const char *id, PlotState &historyView, double value,
                                 ArrayView<PlotSeries> history, ImVec2 size);

struct ResponsePoint {
    ItemId id = 0;
    double frequency = 1000, gain = 0; // Hz / dB
    bool enabled = true;
};
struct ResponseState {
    PlotState view{{{20, 20000}, true}, {{-24, 24}, false}};
    std::vector<ResponsePoint> before, preview;
    ItemId active = 0;
    std::uint64_t revision = 0;
    bool dragging = false;
};
struct ResponseOptions {
    Range frequency{20, 20000}, gain{-24, 24}, phase{-180, 180};
    bool readOnly = false;
    std::uint64_t revision = 0;
    PlotOptions plot;
};
struct ResponseResult {
    PlotResult plot;
    Edit<std::vector<ResponsePoint>> points;
};
// Gain curves use the left dB axis; optional phase curves use the right degree axis.
// Stable unique nonzero handle IDs. Drag proposes Hz/dB edits, Escape cancels; revision/model
// changes or readOnly cancel active edits. Curves remain supplied by the host, including previews.
CYUI_API ResponseResult filterResponse(const char *id, ResponseState &, ArrayView<PlotSeries> gain,
                                        ArrayView<PlotSeries> phase, const std::vector<ResponsePoint> &,
                                        ImVec2 size, const ResponseOptions & = {});
struct DynamicsData {
    ArrayView<PlotSeries> transfer; // input dB / output dB
    double input = missingAudioValue, output = missingAudioValue, gainReduction = missingAudioValue;
};
struct DynamicsOptions {
    Range reduction{0, 36}; // positive dB of attenuation
    PlotOptions plot;
};
// Transfer curves, unity reference, operating point and gain-reduction bar.
CYUI_API PlotResult dynamics(const char *id, PlotState &, const DynamicsData &, ImVec2 size,
                              const DynamicsOptions & = {});
} // namespace cyui
