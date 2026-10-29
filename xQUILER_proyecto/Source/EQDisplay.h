#pragma once
#include "PluginProcessor.h"
#include "Theme.h"
#include <functional>

class EQDisplay : public juce::Component, private juce::Timer
{
public:
    explicit EQDisplay (XQuilerAudioProcessor& p);
    ~EQDisplay() override { stopTimer(); }

    void setDark (bool d) { dark = d; repaint(); }
    int getSelected() const { return selected; }
    void setSelected (int b);

    std::function<void (int)> onSelectionChanged;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseUp (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;
    void mouseDoubleClick (const juce::MouseEvent& e) override;
    void mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w) override;

private:
    void timerCallback() override;

    float freqToX (float f) const;
    float xToFreq (float x) const;
    float dbToY (float db) const;
    float yToDb (float y) const;

    bool isOn (int b) const;
    float val (int b, const char* name) const;
    int getType (int b) const;
    void setParam (const juce::String& id, float plainValue);
    juce::Point<float> nodePos (int b) const;
    int hitTest (juce::Point<float> p) const;
    void addBandAt (juce::Point<float> p);
    void removeBand (int b);
    void showBandMenu (int b);

    XQuilerAudioProcessor& proc;
    bool dark = true;
    int selected = -1;
    int dragging = -1;
    int hover = -1;
    int idleFrames = 0;
    juce::Rectangle<float> plot;

    juce::dsp::FFT fft { xq::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) xq::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann,
                                                 false };
    std::array<float, 2 * xq::fftSize> scratch {};
    std::array<float, xq::fftSize / 2> spec {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQDisplay)
};
