#pragma once
#include "PluginProcessor.h"
#include "EQDisplay.h"
#include "Theme.h"

class XQuilerAudioProcessorEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit XQuilerAudioProcessorEditor (XQuilerAudioProcessor&);
    ~XQuilerAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    using CA = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void timerCallback() override;
    void applyTheme();
    void selectBand (int band);

    XQuilerAudioProcessor& proc;
    bool dark = true;
    int selected = -1;

    xq::XQLookAndFeel laf;
    EQDisplay display;
    juce::TextButton themeButton;
    juce::ComboBox typeBox;
    juce::Slider freqSlider, gainSlider, qSlider, outSlider;

    // Los attachments se declaran despues de los componentes (se destruyen primero)
    std::unique_ptr<SA> freqA, gainA, qA, outA;
    std::unique_ptr<CA> typeA;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (XQuilerAudioProcessorEditor)
};
