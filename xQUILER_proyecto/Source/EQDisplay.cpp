#include "EQDisplay.h"

namespace
{
constexpr float minFreq = 20.0f;
constexpr float maxFreq = 20000.0f;
constexpr float dbRange = 24.0f;
constexpr float specMin = -90.0f;
constexpr float specMax = -10.0f;
constexpr int numBins = xq::fftSize / 2;

juce::String freqText (float f)
{
    if (f >= 1000.0f)
        return juce::String (f / 1000.0f, 2) + " kHz";
    return juce::String ((int) std::round (f)) + " Hz";
}
} // namespace

EQDisplay::EQDisplay (XQuilerAudioProcessor& p) : proc (p)
{
    spec.fill (-120.0f);
    startTimerHz (30);
}

//==============================================================================
void EQDisplay::resized()
{
    plot = getLocalBounds().toFloat()
               .withTrimmedLeft (34.0f)
               .withTrimmedBottom (20.0f)
               .withTrimmedTop (6.0f)
               .withTrimmedRight (8.0f);
}

float EQDisplay::freqToX (float f) const
{
    return plot.getX() + plot.getWidth() * std::log (f / minFreq) / std::log (maxFreq / minFreq);
}

float EQDisplay::xToFreq (float x) const
{
    const float p = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / juce::jmax (1.0f, plot.getWidth()));
    return minFreq * std::pow (maxFreq / minFreq, p);
}

float EQDisplay::dbToY (float db) const
{
    return plot.getY() + plot.getHeight() * (1.0f - (db + dbRange) / (2.0f * dbRange));
}

float EQDisplay::yToDb (float y) const
{
    const float p = juce::jlimit (0.0f, 1.0f, (y - plot.getY()) / juce::jmax (1.0f, plot.getHeight()));
    return dbRange * (1.0f - 2.0f * p);
}

//==============================================================================
bool EQDisplay::isOn (int b) const
{
    return proc.apvts.getRawParameterValue (xq::paramID (b, "on"))->load() > 0.5f;
}

float EQDisplay::val (int b, const char* name) const
{
    return proc.apvts.getRawParameterValue (xq::paramID (b, name))->load();
}

int EQDisplay::getType (int b) const
{
    return (int) std::round (val (b, "type"));
}

void EQDisplay::setParam (const juce::String& id, float plainValue)
{
    if (auto* p = proc.apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (plainValue));
        p->endChangeGesture();
    }
}

juce::Point<float> EQDisplay::nodePos (int b) const
{
    const float x = freqToX (val (b, "freq"));
    const float db = xq::usesGain (getType (b)) ? val (b, "gain") : 0.0f;
    return { x, dbToY (db) };
}

int EQDisplay::hitTest (juce::Point<float> p) const
{
    // Prioriza el punto seleccionado, luego los de arriba
    if (selected >= 0 && isOn (selected) && nodePos (selected).getDistanceFrom (p) < 11.0f)
        return selected;

    for (int b = xq::numBands - 1; b >= 0; --b)
        if (isOn (b) && nodePos (b).getDistanceFrom (p) < 11.0f)
            return b;

    return -1;
}

void EQDisplay::setSelected (int b)
{
    if (selected == b)
        return;
    selected = b;
    if (onSelectionChanged)
        onSelectionChanged (b);
    repaint();
}

void EQDisplay::addBandAt (juce::Point<float> p)
{
    if (! plot.contains (p))
        return;

    int free = -1;
    for (int b = 0; b < xq::numBands; ++b)
        if (! isOn (b)) { free = b; break; }

    if (free < 0)
        return;

    setParam (xq::paramID (free, "type"), (float) xq::Bell);
    setParam (xq::paramID (free, "q"), 1.0f);
    setParam (xq::paramID (free, "freq"), xToFreq (p.x));
    setParam (xq::paramID (free, "gain"), std::round (yToDb (p.y) * 2.0f) / 2.0f);
    setParam (xq::paramID (free, "on"), 1.0f);
    setSelected (free);
}

void EQDisplay::removeBand (int b)
{
    setParam (xq::paramID (b, "on"), 0.0f);
    if (selected == b)
        setSelected (-1);
}

void EQDisplay::showBandMenu (int b)
{
    juce::PopupMenu m;
    const auto names = xq::typeNames();
    const int cur = getType (b);

    for (int i = 0; i < names.size(); ++i)
        m.addItem (i + 1, names[i], true, i == cur);

    m.addSeparator();
    m.addItem (100, "Eliminar punto");

    juce::Component::SafePointer<EQDisplay> safe (this);
    m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                     [safe, b] (int result)
                     {
                         if (safe == nullptr || result == 0)
                             return;
                         if (result == 100)
                             safe->removeBand (b);
                         else
                             safe->setParam (xq::paramID (b, "type"), (float) (result - 1));
                     });
}

//==============================================================================
void EQDisplay::mouseDown (const juce::MouseEvent& e)
{
    const int b = hitTest (e.position);

    if (e.mods.isPopupMenu())
    {
        if (b >= 0)
        {
            setSelected (b);
            showBandMenu (b);
        }
        return;
    }

    dragging = b;
    setSelected (b);
}

void EQDisplay::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0 || ! e.mods.isLeftButtonDown())
        return;

    setParam (xq::paramID (dragging, "freq"), xToFreq (e.position.x));

    if (xq::usesGain (getType (dragging)))
        setParam (xq::paramID (dragging, "gain"), std::round (yToDb (e.position.y) * 10.0f) / 10.0f);
}

void EQDisplay::mouseUp (const juce::MouseEvent&)
{
    dragging = -1;
}

void EQDisplay::mouseMove (const juce::MouseEvent& e)
{
    const int h = hitTest (e.position);
    if (h != hover)
    {
        hover = h;
        setMouseCursor (h >= 0 ? juce::MouseCursor::PointingHandCursor
                               : juce::MouseCursor::NormalCursor);
    }
}

void EQDisplay::mouseExit (const juce::MouseEvent&)
{
    hover = -1;
}

void EQDisplay::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int b = hitTest (e.position);
    if (b >= 0)
        removeBand (b);
    else
        addBandAt (e.position);
}

void EQDisplay::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = hitTest (e.position);
    if (b < 0)
        b = selected;
    if (b < 0 || ! isOn (b))
        return;

    const float q = val (b, "q");
    const float newQ = juce::jlimit (0.1f, 18.0f, q * std::exp (w.deltaY * 0.5f));
    setParam (xq::paramID (b, "q"), newQ);
}

//==============================================================================
void EQDisplay::timerCallback()
{
    if (proc.fftReady.load())
    {
        std::copy (proc.fftData.begin(), proc.fftData.end(), scratch.begin());
        proc.fftReady = false;

        window.multiplyWithWindowingTable (scratch.data(), (size_t) xq::fftSize);
        fft.performFrequencyOnlyForwardTransform (scratch.data());

        const float norm = juce::Decibels::gainToDecibels ((float) xq::fftSize) - 12.0f;

        for (int i = 0; i < numBins; ++i)
        {
            const float db = juce::jmax (-120.0f, juce::Decibels::gainToDecibels (scratch[(size_t) i]) - norm);
            auto& s = spec[(size_t) i];

            if (db > s)
                s = s * 0.4f + db * 0.6f;
            else
                s = s * 0.85f + db * 0.15f;
        }
        idleFrames = 0;
    }
    else if (++idleFrames > 15)
    {
        for (auto& s : spec)
            s = juce::jmax (-120.0f, s - 1.0f);
    }

    repaint();
}

//==============================================================================
void EQDisplay::paint (juce::Graphics& g)
{
    const auto pal = xq::getPalette (dark);
    const float sr = (float) proc.currentSampleRate.load();

    if (plot.isEmpty())
        return;

    g.setColour (pal.plot);
    g.fillRoundedRectangle (plot, 4.0f);

    // ---- Rejilla ----
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.toNearestInt());

        const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (float f : freqs)
        {
            const float x = freqToX (f);
            g.setColour (pal.grid);
            g.drawLine (x, plot.getY(), x, plot.getBottom(), 1.0f);
        }

        for (int db = -24; db <= 24; db += 6)
        {
            const float y = dbToY ((float) db);
            g.setColour (db == 0 ? pal.gridStrong : pal.grid);
            g.drawLine (plot.getX(), y, plot.getRight(), y, 1.0f);
        }
    }

    // Etiquetas
    g.setFont (10.0f);
    g.setColour (pal.dim);
    {
        const char* labels[] = { "20", "50", "100", "200", "500", "1k", "2k", "5k", "10k", "20k" };
        const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
        for (int i = 0; i < 10; ++i)
        {
            const float x = freqToX (freqs[i]);
            g.drawText (labels[i], juce::Rectangle<float> (x - 18.0f, plot.getBottom() + 3.0f, 36.0f, 14.0f),
                        juce::Justification::centred);
        }
        for (int db = -24; db <= 24; db += 6)
        {
            const float y = dbToY ((float) db);
            const juce::String t = db > 0 ? "+" + juce::String (db) : juce::String (db);
            g.drawText (t, juce::Rectangle<float> (0.0f, y - 6.0f, 30.0f, 12.0f),
                        juce::Justification::centredRight);
        }
    }

    // ---- Espectro ----
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.toNearestInt());

        juce::Path sp;
        bool started = false;

        for (float x = plot.getX(); x <= plot.getRight() + 0.5f; x += 2.0f)
        {
            const float f = xToFreq (x);
            const float bin = f * (float) xq::fftSize / sr;
            const int i0 = juce::jlimit (0, numBins - 2, (int) bin);
            const float fr = juce::jlimit (0.0f, 1.0f, bin - (float) i0);

            float db = spec[(size_t) i0] * (1.0f - fr) + spec[(size_t) i0 + 1] * fr;
            db += 3.0f * std::log2 (f / 1000.0f); // inclinacion para que el agudo se vea

            const float t = juce::jlimit (0.0f, 1.0f, (db - specMin) / (specMax - specMin));
            const float y = plot.getBottom() - plot.getHeight() * t;

            if (! started) { sp.startNewSubPath (x, y); started = true; }
            else           sp.lineTo (x, y);
        }

        juce::Path fill (sp);
        fill.lineTo (plot.getRight(), plot.getBottom());
        fill.lineTo (plot.getX(), plot.getBottom());
        fill.closeSubPath();

        g.setColour (pal.text.withAlpha (0.12f));
        g.fillPath (fill);
        g.setColour (pal.text.withAlpha (0.35f));
        g.strokePath (sp, juce::PathStrokeType (1.0f));
    }

    // ---- Curva del EQ ----
    {
        juce::Graphics::ScopedSaveState ss (g);
        g.reduceClipRegion (plot.toNearestInt());

        std::vector<juce::dsp::IIR::Coefficients<float>::Ptr> cs;
        for (int b = 0; b < xq::numBands; ++b)
            if (isOn (b))
                cs.push_back (xq::makeCoefficients (getType (b), sr, val (b, "freq"),
                                                    val (b, "gain"), val (b, "q")));

        juce::Path curve;
        bool started = false;

        for (float x = plot.getX(); x <= plot.getRight() + 0.5f; x += 2.0f)
        {
            const double f = (double) xToFreq (x);
            double mag = 1.0;
            for (auto& c : cs)
                mag *= c->getMagnitudeForFrequency (f, (double) sr);

            const float db = juce::jlimit (-dbRange, dbRange,
                                           juce::Decibels::gainToDecibels ((float) mag));
            const float y = dbToY (db);

            if (! started) { curve.startNewSubPath (x, y); started = true; }
            else           curve.lineTo (x, y);
        }

        const float y0 = dbToY (0.0f);
        juce::Path fill (curve);
        fill.lineTo (plot.getRight(), y0);
        fill.lineTo (plot.getX(), y0);
        fill.closeSubPath();

        g.setColour (pal.text.withAlpha (0.08f));
        g.fillPath (fill);
        g.setColour (pal.text);
        g.strokePath (curve, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved,
                                                   juce::PathStrokeType::rounded));
    }

    // ---- Nodos ----
    for (int b = 0; b < xq::numBands; ++b)
    {
        if (! isOn (b))
            continue;

        const auto p = nodePos (b);
        const bool sel = (b == selected);
        const bool hot = (b == hover || b == dragging);
        const float r = (sel || hot) ? 7.5f : 6.0f;

        g.setColour (sel ? pal.text : pal.plot);
        g.fillEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f);
        g.setColour (pal.text);
        g.drawEllipse (p.x - r, p.y - r, r * 2.0f, r * 2.0f, 1.5f);

        g.setFont (10.0f);
        g.setColour (pal.dim);
        g.drawText (juce::String (b + 1), juce::Rectangle<float> (p.x - 10.0f, p.y - r - 14.0f, 20.0f, 12.0f),
                    juce::Justification::centred);
    }

    // ---- Lectura del punto seleccionado ----
    if (selected >= 0 && isOn (selected))
    {
        const int t = getType (selected);
        juce::String s = freqText (val (selected, "freq"));
        if (xq::usesGain (t))
        {
            const float gdb = val (selected, "gain");
            s += "   " + juce::String (gdb >= 0 ? "+" : "") + juce::String (gdb, 1) + " dB";
        }
        s += "   Q " + juce::String (val (selected, "q"), 2);

        g.setFont (12.0f);
        g.setColour (pal.text);
        g.drawText (s, plot.reduced (10.0f, 6.0f), juce::Justification::topLeft);
    }
}
