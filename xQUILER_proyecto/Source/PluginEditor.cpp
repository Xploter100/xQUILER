#include "PluginEditor.h"

XQuilerAudioProcessorEditor::XQuilerAudioProcessorEditor (XQuilerAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), display (p)
{
    dark = (bool) proc.apvts.state.getProperty ("dark", true);

    setLookAndFeel (&laf);

    addAndMakeVisible (display);
    display.onSelectionChanged = [this] (int b) { selectBand (b); };

    addAndMakeVisible (themeButton);
    themeButton.onClick = [this]
    {
        dark = ! dark;
        proc.apvts.state.setProperty ("dark", dark, nullptr);
        applyTheme();
    };

    typeBox.addItemList (xq::typeNames(), 1);
    addChildComponent (typeBox);

    for (auto* s : { &freqSlider, &gainSlider, &qSlider, &outSlider })
    {
        s->setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s->setTextBoxStyle (juce::Slider::TextBoxBelow, false, 90, 20);
        addChildComponent (s);
    }

    freqSlider.setTextValueSuffix (" Hz");
    freqSlider.setNumDecimalPlacesToDisplay (0);
    gainSlider.setTextValueSuffix (" dB");
    gainSlider.setNumDecimalPlacesToDisplay (1);
    qSlider.setNumDecimalPlacesToDisplay (2);
    outSlider.setTextValueSuffix (" dB");
    outSlider.setNumDecimalPlacesToDisplay (1);

    outA = std::make_unique<SA> (proc.apvts, "out_gain", outSlider);
    outSlider.setVisible (true);

    setSize (900, 580);
    applyTheme();
    startTimerHz (10);
}

XQuilerAudioProcessorEditor::~XQuilerAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void XQuilerAudioProcessorEditor::applyTheme()
{
    const auto pal = xq::getPalette (dark);
    laf.setPalette (pal);
    display.setDark (dark);
    themeButton.setButtonText (dark ? "CLARO" : "OSCURO");
    sendLookAndFeelChange();
    repaint();
}

void XQuilerAudioProcessorEditor::selectBand (int band)
{
    freqA.reset();
    gainA.reset();
    qA.reset();
    typeA.reset();

    selected = band;
    const bool has = band >= 0;

    typeBox.setVisible (has);
    freqSlider.setVisible (has);
    gainSlider.setVisible (has);
    qSlider.setVisible (has);

    if (has)
    {
        typeA = std::make_unique<CA> (proc.apvts, xq::paramID (band, "type"), typeBox);
        freqA = std::make_unique<SA> (proc.apvts, xq::paramID (band, "freq"), freqSlider);
        gainA = std::make_unique<SA> (proc.apvts, xq::paramID (band, "gain"), gainSlider);
        qA    = std::make_unique<SA> (proc.apvts, xq::paramID (band, "q"), qSlider);
    }

    repaint();
}

void XQuilerAudioProcessorEditor::timerCallback()
{
    if (selected < 0)
        return;

    const bool on = proc.apvts.getRawParameterValue (xq::paramID (selected, "on"))->load() > 0.5f;
    if (! on)
    {
        display.setSelected (-1);
        return;
    }

    const int type = (int) std::round (proc.apvts.getRawParameterValue (xq::paramID (selected, "type"))->load());
    gainSlider.setEnabled (xq::usesGain (type));
}

void XQuilerAudioProcessorEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    auto top = area.removeFromTop (32);
    themeButton.setBounds (top.removeFromRight (90).reduced (0, 3));

    area.removeFromTop (6);
    auto bottom = area.removeFromBottom (124);
    display.setBounds (area);

    auto ctrl = bottom.withTrimmedTop (18);

    typeBox.setBounds (ctrl.removeFromLeft (150).withSizeKeepingCentre (150, 26));
    ctrl.removeFromLeft (16);
    freqSlider.setBounds (ctrl.removeFromLeft (100));
    ctrl.removeFromLeft (8);
    gainSlider.setBounds (ctrl.removeFromLeft (100));
    ctrl.removeFromLeft (8);
    qSlider.setBounds (ctrl.removeFromLeft (100));
    outSlider.setBounds (ctrl.removeFromRight (100));
}

void XQuilerAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto pal = xq::getPalette (dark);
    g.fillAll (pal.bg);

    g.setColour (pal.text);
    g.setFont (juce::Font (20.0f, juce::Font::bold));
    g.drawText ("xQUILER", 16, 10, 200, 28, juce::Justification::centredLeft);

    g.setColour (pal.dim);
    g.setFont (11.0f);
    g.drawText ("Doble clic: agregar/quitar punto   |   Rueda: Q   |   Clic derecho: tipo / eliminar",
                juce::Rectangle<int> (190, 10, getWidth() - 190 - 120, 28),
                juce::Justification::centredLeft);

    auto drawLabel = [&] (juce::Component& c, const juce::String& text)
    {
        g.drawText (text, c.getX(), c.getY() - 16, c.getWidth(), 14, juce::Justification::centred);
    };

    g.setColour (pal.dim);
    g.setFont (10.0f);
    drawLabel (outSlider, "SALIDA");

    if (selected >= 0)
    {
        drawLabel (typeBox, "TIPO");
        drawLabel (freqSlider, "FRECUENCIA");
        drawLabel (gainSlider, "GANANCIA");
        drawLabel (qSlider, "Q");
    }
    else
    {
        g.setFont (12.0f);
        g.drawText ("Selecciona un punto para editar sus valores",
                    juce::Rectangle<int> (16, outSlider.getY(), outSlider.getX() - 32, outSlider.getHeight()),
                    juce::Justification::centredLeft);
    }
}
