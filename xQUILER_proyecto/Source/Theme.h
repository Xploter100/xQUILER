#pragma once
#include <juce_gui_basics/juce_gui_basics.h>

namespace xq
{
struct Palette
{
    juce::Colour bg, plot, grid, gridStrong, text, dim;
};

inline Palette getPalette (bool dark)
{
    Palette p;
    if (dark)
    {
        p.bg         = juce::Colour (0xff0b0b0b);
        p.plot       = juce::Colour (0xff141414);
        p.grid       = juce::Colour (0xff262626);
        p.gridStrong = juce::Colour (0xff3d3d3d);
        p.text       = juce::Colour (0xfff0f0f0);
        p.dim        = juce::Colour (0xff7d7d7d);
    }
    else
    {
        p.bg         = juce::Colour (0xffeeeeee);
        p.plot       = juce::Colour (0xffffffff);
        p.grid       = juce::Colour (0xffe2e2e2);
        p.gridStrong = juce::Colour (0xffc4c4c4);
        p.text       = juce::Colour (0xff111111);
        p.dim        = juce::Colour (0xff808080);
    }
    return p;
}

class XQLookAndFeel : public juce::LookAndFeel_V4
{
public:
    void setPalette (const Palette& p)
    {
        using namespace juce;

        setColour (ResizableWindow::backgroundColourId, p.bg);

        setColour (ComboBox::backgroundColourId, p.plot);
        setColour (ComboBox::textColourId, p.text);
        setColour (ComboBox::outlineColourId, p.gridStrong);
        setColour (ComboBox::arrowColourId, p.text);
        setColour (ComboBox::buttonColourId, p.plot);
        setColour (ComboBox::focusedOutlineColourId, p.text);

        setColour (PopupMenu::backgroundColourId, p.plot);
        setColour (PopupMenu::textColourId, p.text);
        setColour (PopupMenu::headerTextColourId, p.dim);
        setColour (PopupMenu::highlightedBackgroundColourId, p.text);
        setColour (PopupMenu::highlightedTextColourId, p.plot);

        setColour (Slider::rotarySliderFillColourId, p.text);
        setColour (Slider::rotarySliderOutlineColourId, p.gridStrong);
        setColour (Slider::thumbColourId, p.text);
        setColour (Slider::trackColourId, p.text);
        setColour (Slider::backgroundColourId, p.gridStrong);
        setColour (Slider::textBoxTextColourId, p.text);
        setColour (Slider::textBoxBackgroundColourId, Colours::transparentBlack);
        setColour (Slider::textBoxOutlineColourId, Colours::transparentBlack);
        setColour (Slider::textBoxHighlightColourId, p.dim);

        setColour (Label::textColourId, p.text);
        setColour (Label::textWhenEditingColourId, p.text);
        setColour (Label::backgroundWhenEditingColourId, p.plot);
        setColour (Label::outlineWhenEditingColourId, p.text);

        setColour (TextEditor::textColourId, p.text);
        setColour (TextEditor::backgroundColourId, p.plot);
        setColour (TextEditor::highlightColourId, p.dim);
        setColour (TextEditor::highlightedTextColourId, p.text);
        setColour (TextEditor::outlineColourId, p.text);
        setColour (TextEditor::focusedOutlineColourId, p.text);
        setColour (CaretComponent::caretColourId, p.text);

        setColour (TextButton::buttonColourId, p.plot);
        setColour (TextButton::buttonOnColourId, p.plot);
        setColour (TextButton::textColourOffId, p.text);
        setColour (TextButton::textColourOnId, p.text);
    }
};
} // namespace xq
