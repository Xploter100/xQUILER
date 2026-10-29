#pragma once
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace xq
{
constexpr int numBands = 8;
constexpr int fftOrder = 11;
constexpr int fftSize  = 1 << fftOrder;

enum BandType { Bell = 0, LowShelf, HighShelf, LowCut, HighCut, Notch };

inline juce::String paramID (int band, const char* name)
{
    return "b" + juce::String (band) + "_" + name;
}

juce::StringArray typeNames();
bool usesGain (int type);
juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients (int type, double sampleRate,
                                                           float freq, float gainDb, float q);
} // namespace xq

class XQuilerAudioProcessor : public juce::AudioProcessor
{
public:
    XQuilerAudioProcessor();
    ~XQuilerAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "xQUILER"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    juce::AudioProcessorValueTreeState apvts;

    // Analizador (el audio escribe, el editor lee cuando fftReady == true)
    std::array<float, 2 * xq::fftSize> fftData {};
    std::atomic<bool> fftReady { false };
    std::atomic<double> currentSampleRate { 44100.0 };

private:
    struct BandState
    {
        bool on = false;
        float freq = 0.f, gain = 0.f, q = 0.f;
        int type = 0;
    };

    void updateFilters();
    void pushSample (float s);

    juce::dsp::IIR::Filter<float> filters[xq::numBands][2];
    bool bandActive[xq::numBands] {};
    BandState lastState[xq::numBands];

    std::atomic<float>* onP[xq::numBands] {};
    std::atomic<float>* freqP[xq::numBands] {};
    std::atomic<float>* gainP[xq::numBands] {};
    std::atomic<float>* qP[xq::numBands] {};
    std::atomic<float>* typeP[xq::numBands] {};
    std::atomic<float>* outP = nullptr;

    float lastOutGain = 1.0f;

    std::array<float, xq::fftSize> fifo {};
    int fifoIndex = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XQuilerAudioProcessor)
};
