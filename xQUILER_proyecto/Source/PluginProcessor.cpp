#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace xq
{
juce::StringArray typeNames()
{
    return { "Bell", "Low Shelf", "High Shelf", "Low Cut", "High Cut", "Notch" };
}

bool usesGain (int type)
{
    return type == Bell || type == LowShelf || type == HighShelf;
}

juce::dsp::IIR::Coefficients<float>::Ptr makeCoefficients (int type, double sr,
                                                           float freq, float gainDb, float q)
{
    using C = juce::dsp::IIR::Coefficients<float>;

    freq = juce::jlimit (20.0f, (float) (sr * 0.49), freq);
    q = juce::jmax (0.05f, q);
    const float gain = juce::Decibels::decibelsToGain (gainDb);

    switch (type)
    {
        case LowShelf:  return C::makeLowShelf (sr, freq, q, gain);
        case HighShelf: return C::makeHighShelf (sr, freq, q, gain);
        case LowCut:    return C::makeHighPass (sr, freq, q);
        case HighCut:   return C::makeLowPass (sr, freq, q);
        case Notch:     return C::makeNotch (sr, freq, q);
        case Bell:
        default:        return C::makePeakFilter (sr, freq, q, gain);
    }
}
} // namespace xq

//==============================================================================
XQuilerAudioProcessor::XQuilerAudioProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "XQUILER", createLayout())
{
    for (int b = 0; b < xq::numBands; ++b)
    {
        onP[b]   = apvts.getRawParameterValue (xq::paramID (b, "on"));
        freqP[b] = apvts.getRawParameterValue (xq::paramID (b, "freq"));
        gainP[b] = apvts.getRawParameterValue (xq::paramID (b, "gain"));
        qP[b]    = apvts.getRawParameterValue (xq::paramID (b, "q"));
        typeP[b] = apvts.getRawParameterValue (xq::paramID (b, "type"));
    }
    outP = apvts.getRawParameterValue ("out_gain");
}

juce::AudioProcessorValueTreeState::ParameterLayout XQuilerAudioProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    const float defFreqs[xq::numBands] = { 60.f, 150.f, 400.f, 1000.f, 2500.f, 5000.f, 9000.f, 14000.f };

    for (int b = 0; b < xq::numBands; ++b)
    {
        const String n = "Band " + String (b + 1);

        layout.add (std::make_unique<AudioParameterBool> (
            ParameterID { xq::paramID (b, "on"), 1 }, n + " On", false));

        NormalisableRange<float> fr (20.0f, 20000.0f, 0.01f);
        fr.setSkewForCentre (1000.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { xq::paramID (b, "freq"), 1 }, n + " Freq", fr, defFreqs[b]));

        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { xq::paramID (b, "gain"), 1 }, n + " Gain",
            NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));

        NormalisableRange<float> qr (0.1f, 18.0f, 0.01f);
        qr.setSkewForCentre (1.0f);
        layout.add (std::make_unique<AudioParameterFloat> (
            ParameterID { xq::paramID (b, "q"), 1 }, n + " Q", qr, 1.0f));

        layout.add (std::make_unique<AudioParameterChoice> (
            ParameterID { xq::paramID (b, "type"), 1 }, n + " Type", xq::typeNames(), 0));
    }

    layout.add (std::make_unique<AudioParameterFloat> (
        ParameterID { "out_gain", 1 }, "Output",
        NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f));

    return layout;
}

//==============================================================================
void XQuilerAudioProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    for (int b = 0; b < xq::numBands; ++b)
    {
        lastState[b].on = false;
        bandActive[b] = false;
        for (int ch = 0; ch < 2; ++ch)
            filters[b][ch].reset();
    }

    lastOutGain = juce::Decibels::decibelsToGain (outP->load());
    fifoIndex = 0;
    fftReady = false;
}

bool XQuilerAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return out == layouts.getMainInputChannelSet();
}

void XQuilerAudioProcessor::updateFilters()
{
    const double sr = currentSampleRate.load();

    for (int b = 0; b < xq::numBands; ++b)
    {
        const bool on = onP[b]->load() > 0.5f;

        if (! on)
        {
            bandActive[b] = false;
            lastState[b].on = false;
            continue;
        }

        BandState s;
        s.on = true;
        s.freq = freqP[b]->load();
        s.gain = gainP[b]->load();
        s.q = qP[b]->load();
        s.type = (int) std::round (typeP[b]->load());

        const auto& l = lastState[b];
        const bool changed = ! l.on || s.freq != l.freq || s.gain != l.gain
                             || s.q != l.q || s.type != l.type;

        if (changed)
        {
            auto c = xq::makeCoefficients (s.type, sr, s.freq, s.gain, s.q);
            for (int ch = 0; ch < 2; ++ch)
            {
                if (! l.on)
                    filters[b][ch].reset();
                filters[b][ch].coefficients = c;
            }
        }

        lastState[b] = s;
        bandActive[b] = true;
    }
}

void XQuilerAudioProcessor::pushSample (float s)
{
    if (fifoIndex == xq::fftSize)
    {
        if (! fftReady.load())
        {
            std::fill (fftData.begin(), fftData.end(), 0.0f);
            std::copy (fifo.begin(), fifo.end(), fftData.begin());
            fftReady = true;
        }
        fifoIndex = 0;
    }
    fifo[(size_t) fifoIndex++] = s;
}

void XQuilerAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numIn = getTotalNumInputChannels();
    const int numOut = getTotalNumOutputChannels();
    const int n = buffer.getNumSamples();

    for (int i = numIn; i < numOut; ++i)
        buffer.clear (i, 0, n);

    updateFilters();

    const int chs = juce::jmin (buffer.getNumChannels(), 2);

    for (int b = 0; b < xq::numBands; ++b)
    {
        if (! bandActive[b])
            continue;
        for (int ch = 0; ch < chs; ++ch)
            filters[b][ch].processSamples (buffer.getWritePointer (ch), n);
    }

    const float outGain = juce::Decibels::decibelsToGain (outP->load());
    buffer.applyGainRamp (0, n, lastOutGain, outGain);
    lastOutGain = outGain;

    // Analizador: mezcla a mono
    const int total = buffer.getNumChannels();
    if (total > 0)
    {
        for (int i = 0; i < n; ++i)
        {
            float s = 0.0f;
            for (int ch = 0; ch < total; ++ch)
                s += buffer.getReadPointer (ch)[i];
            pushSample (s / (float) total);
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* XQuilerAudioProcessor::createEditor()
{
    return new XQuilerAudioProcessorEditor (*this);
}

void XQuilerAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void XQuilerAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new XQuilerAudioProcessor();
}
