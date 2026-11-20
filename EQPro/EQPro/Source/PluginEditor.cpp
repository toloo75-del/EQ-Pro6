#include "PluginEditor.h"

namespace
{
    constexpr float minF = 20.0f, maxF = 20000.0f;
    constexpr float dbRange = 24.0f;
    constexpr int stripHeight = 120;
}

//==============================================================================
EQProEditor::EQProEditor (EQProProcessor& p)
    : AudioProcessorEditor (&p), proc (p)
{
    setSize (900, 560);
    setResizable (true, true);
    setResizeLimits (700, 420, 1800, 1100);

    auto setupKnob = [this] (juce::Slider& s, juce::Label& l, const juce::String& name)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
        addAndMakeVisible (s);
        l.setText (name, juce::dontSendNotification);
        l.setJustificationType (juce::Justification::centred);
        l.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
        addAndMakeVisible (l);
    };
    setupKnob (freqSlider, freqLabel, "FREQ");
    setupKnob (gainSlider, gainLabel, "GAIN");
    setupKnob (qSlider,    qLabel,    "Q");
    setupKnob (outSlider,  outLabel,  "SALIDA");

    freqSlider.setTextValueSuffix (" Hz");
    gainSlider.setTextValueSuffix (" dB");
    outSlider.setTextValueSuffix (" dB");

    typeBox.addItemList (EQProProcessor::typeNames(), 1);
    addAndMakeVisible (typeBox);
    addAndMakeVisible (onButton);

    outAtt = std::make_unique<APVTS::SliderAttachment> (proc.apvts, "out_gain", outSlider);

    selectBand (0);
    startTimerHz (30);
}

EQProEditor::~EQProEditor()
{
    stopTimer();
}

//==============================================================================
void EQProEditor::selectBand (int band)
{
    selected = juce::jlimit (0, EQProProcessor::numBands - 1, band);

    onAtt.reset(); typeAtt.reset(); freqAtt.reset(); gainAtt.reset(); qAtt.reset();

    onAtt   = std::make_unique<APVTS::ButtonAttachment>   (proc.apvts, pid (selected, "on"),   onButton);
    typeAtt = std::make_unique<APVTS::ComboBoxAttachment> (proc.apvts, pid (selected, "type"), typeBox);
    freqAtt = std::make_unique<APVTS::SliderAttachment>   (proc.apvts, pid (selected, "freq"), freqSlider);
    gainAtt = std::make_unique<APVTS::SliderAttachment>   (proc.apvts, pid (selected, "gain"), gainSlider);
    qAtt    = std::make_unique<APVTS::SliderAttachment>   (proc.apvts, pid (selected, "q"),    qSlider);

    onButton.setButtonText ("Banda " + juce::String (selected + 1));
    const auto c = bandColour (selected);
    for (auto* s : { &freqSlider, &gainSlider, &qSlider })
        s->setColour (juce::Slider::rotarySliderFillColourId, c);

    repaint();
}

juce::Colour EQProEditor::bandColour (int b) const
{
    return juce::Colour::fromHSV ((float) b / (float) EQProProcessor::numBands, 0.65f, 0.95f, 1.0f);
}

//==============================================================================
float EQProEditor::freqToX (float f) const
{
    const float t = std::log (f / minF) / std::log (maxF / minF);
    return plot.getX() + juce::jlimit (0.0f, 1.0f, t) * plot.getWidth();
}

float EQProEditor::xToFreq (float x) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return minF * std::pow (maxF / minF, t);
}

float EQProEditor::gainToY (float g) const
{
    return juce::jmap (g, -dbRange, dbRange, plot.getBottom(), plot.getY());
}

float EQProEditor::yToGain (float y) const
{
    return juce::jlimit (-dbRange, dbRange, juce::jmap (y, plot.getBottom(), plot.getY(), -dbRange, dbRange));
}

float EQProEditor::getParam (const juce::String& id) const
{
    return proc.apvts.getRawParameterValue (id)->load();
}

void EQProEditor::setParam (const juce::String& id, float v)
{
    if (auto* p = proc.apvts.getParameter (id))
        p->setValueNotifyingHost (p->convertTo0to1 (v));
}

void EQProEditor::gesture (int band, bool begin)
{
    for (auto* suffix : { "freq", "gain" })
        if (auto* p = proc.apvts.getParameter (pid (band, suffix)))
        {
            if (begin) p->beginChangeGesture();
            else       p->endChangeGesture();
        }
}

int EQProEditor::hitBand (juce::Point<float> pt) const
{
    int best = -1;
    float bestDist = 14.0f;

    for (int b = 0; b < EQProProcessor::numBands; ++b)
    {
        if (getParam (pid (b, "on")) < 0.5f)
            continue;

        const int type = (int) getParam (pid (b, "type"));
        const float x = freqToX (getParam (pid (b, "freq")));
        const float y = gainToY (EQProProcessor::typeUsesGain (type) ? getParam (pid (b, "gain")) : 0.0f);
        const float d = pt.getDistanceFrom ({ x, y });

        if (d < bestDist) { bestDist = d; best = b; }
    }
    return best;
}

//==============================================================================
void EQProEditor::mouseDown (const juce::MouseEvent& e)
{
    const int hit = hitBand (e.position);
    if (hit >= 0)
    {
        selectBand (hit);
        dragging = hit;
        gesture (hit, true);
    }
}

void EQProEditor::mouseDrag (const juce::MouseEvent& e)
{
    if (dragging < 0)
        return;

    const int type = (int) getParam (pid (dragging, "type"));
    setParam (pid (dragging, "freq"), xToFreq (e.position.x));
    if (EQProProcessor::typeUsesGain (type))
        setParam (pid (dragging, "gain"), yToGain (e.position.y));
}

void EQProEditor::mouseUp (const juce::MouseEvent&)
{
    if (dragging >= 0)
        gesture (dragging, false);
    dragging = -1;
}

void EQProEditor::mouseDoubleClick (const juce::MouseEvent& e)
{
    const int hit = hitBand (e.position);

    if (hit >= 0)
    {
        // Doble clic sobre un nodo: desactivar banda
        if (auto* p = proc.apvts.getParameter (pid (hit, "on")))
        {
            p->beginChangeGesture();
            p->setValueNotifyingHost (0.0f);
            p->endChangeGesture();
        }
        return;
    }

    if (! plot.contains (e.position))
        return;

    // Doble clic en vacío: activar primera banda libre en esa posición
    for (int b = 0; b < EQProProcessor::numBands; ++b)
    {
        if (getParam (pid (b, "on")) < 0.5f)
        {
            setParam (pid (b, "type"), 0.0f);
            setParam (pid (b, "freq"), xToFreq (e.position.x));
            setParam (pid (b, "gain"), yToGain (e.position.y));
            setParam (pid (b, "q"), 1.0f);
            setParam (pid (b, "on"), 1.0f);
            selectBand (b);
            return;
        }
    }
}

void EQProEditor::mouseWheelMove (const juce::MouseEvent& e, const juce::MouseWheelDetails& w)
{
    int b = hitBand (e.position);
    if (b < 0) b = selected;
    if (getParam (pid (b, "on")) < 0.5f)
        return;

    const float q = juce::jlimit (0.1f, 18.0f, getParam (pid (b, "q")) * std::exp (w.deltaY * 1.5f));
    if (auto* p = proc.apvts.getParameter (pid (b, "q")))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (q));
        p->endChangeGesture();
    }
}

//==============================================================================
void EQProEditor::updateSpectrum()
{
    if (! proc.snapshotReady.exchange (false))
        return;

    std::array<float, EQProProcessor::fftSize * 2> data {};
    std::copy (proc.fftSnapshot.begin(), proc.fftSnapshot.end(), data.begin());

    window.multiplyWithWindowingTable (data.data(), (size_t) EQProProcessor::fftSize);
    fft.performFrequencyOnlyForwardTransform (data.data());

    const float norm = (float) EQProProcessor::fftSize * 0.25f;
    for (size_t i = 0; i < spectrum.size(); ++i)
    {
        const float db = juce::Decibels::gainToDecibels (data[i] / norm, -120.0f);
        spectrum[i] = spectrum[i] * 0.75f + db * 0.25f;
    }
}

void EQProEditor::timerCallback()
{
    updateSpectrum();

    const int type = (int) getParam (pid (selected, "type"));
    gainSlider.setEnabled (EQProProcessor::typeUsesGain (type));

    repaint();
}

//==============================================================================
void EQProEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff14161a));

    auto area = plot;
    g.setColour (juce::Colour (0xff1c1f26));
    g.fillRoundedRectangle (area, 6.0f);

    g.saveState();
    g.reduceClipRegion (area.toNearestInt());

    // Rejilla
    g.setFont (11.0f);
    const float freqs[] = { 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000 };
    for (float f : freqs)
    {
        const float x = freqToX (f);
        g.setColour (juce::Colour (0xff2c303a));
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }
    for (int db = -24; db <= 24; db += 6)
    {
        const float y = gainToY ((float) db);
        g.setColour (db == 0 ? juce::Colour (0xff4a505e) : juce::Colour (0xff2c303a));
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    }

    // Espectro
    {
        const double sr = proc.currentSampleRate.load();
        juce::Path sp;
        bool started = false;
        for (float x = area.getX(); x <= area.getRight(); x += 2.0f)
        {
            const float f = xToFreq (x);
            const float bin = f * (float) EQProProcessor::fftSize / (float) sr;
            const int i0 = juce::jlimit (0, (int) spectrum.size() - 1, (int) bin);
            const float db = spectrum[(size_t) i0];
            const float y = juce::jmap (juce::jlimit (-90.0f, -6.0f, db), -90.0f, -6.0f,
                                        area.getBottom(), area.getY());
            if (! started) { sp.startNewSubPath (x, area.getBottom()); sp.lineTo (x, y); started = true; }
            else sp.lineTo (x, y);
        }
        if (started)
        {
            sp.lineTo (area.getRight(), area.getBottom());
            sp.closeSubPath();
            g.setColour (juce::Colour (0x3340b0ff));
            g.fillPath (sp);
            g.setColour (juce::Colour (0x6640b0ff));
            g.strokePath (sp, juce::PathStrokeType (1.0f));
        }
    }

    // Curva de respuesta
    {
        const double sr = proc.currentSampleRate.load();
        const int n = juce::jmax (2, (int) area.getWidth());
        std::vector<double> freqs ((size_t) n), mags ((size_t) n, 1.0), tmp ((size_t) n);
        for (int i = 0; i < n; ++i)
            freqs[(size_t) i] = xToFreq (area.getX() + (float) i);

        std::vector<double> selMags ((size_t) n, 1.0);
        bool selActive = false;

        for (int b = 0; b < EQProProcessor::numBands; ++b)
        {
            if (getParam (pid (b, "on")) < 0.5f)
                continue;

            auto c = EQProProcessor::makeCoeffs ((int) getParam (pid (b, "type")), sr,
                                                 getParam (pid (b, "freq")),
                                                 getParam (pid (b, "gain")),
                                                 getParam (pid (b, "q")));
            c->getMagnitudeForFrequencyArray (freqs.data(), tmp.data(), (size_t) n, sr);

            for (size_t i = 0; i < (size_t) n; ++i)
            {
                mags[i] *= tmp[i];
                if (b == selected) selMags[i] = tmp[i];
            }
            if (b == selected) selActive = true;
        }

        auto toY = [this] (double mag)
        {
            return gainToY (juce::jlimit (-dbRange, dbRange, (float) juce::Decibels::gainToDecibels (mag)));
        };

        if (selActive)
        {
            juce::Path bp;
            bp.startNewSubPath (area.getX(), gainToY (0.0f));
            for (int i = 0; i < n; ++i)
                bp.lineTo (area.getX() + (float) i, toY (selMags[(size_t) i]));
            bp.lineTo (area.getRight(), gainToY (0.0f));
            bp.closeSubPath();
            g.setColour (bandColour (selected).withAlpha (0.22f));
            g.fillPath (bp);
        }

        juce::Path rp;
        for (int i = 0; i < n; ++i)
        {
            const float x = area.getX() + (float) i;
            const float y = toY (mags[(size_t) i]);
            if (i == 0) rp.startNewSubPath (x, y); else rp.lineTo (x, y);
        }
        g.setColour (juce::Colours::white);
        g.strokePath (rp, juce::PathStrokeType (2.0f));
    }

    g.restoreState();

    // Etiquetas de ejes
    g.setFont (11.0f);
    g.setColour (juce::Colours::grey);
    for (float f : freqs)
    {
        juce::String s = f >= 1000.0f ? juce::String (f / 1000.0f) + "k" : juce::String ((int) f);
        g.drawText (s, (int) freqToX (f) - 20, (int) area.getBottom() + 3, 40, 14, juce::Justification::centred);
    }
    for (int db = -24; db <= 24; db += 12)
        g.drawText (juce::String (db), 2, (int) gainToY ((float) db) - 7, 30, 14, juce::Justification::right);

    // Nodos
    for (int b = 0; b < EQProProcessor::numBands; ++b)
    {
        if (getParam (pid (b, "on")) < 0.5f)
            continue;

        const int type = (int) getParam (pid (b, "type"));
        const float x = freqToX (getParam (pid (b, "freq")));
        const float y = gainToY (EQProProcessor::typeUsesGain (type) ? getParam (pid (b, "gain")) : 0.0f);
        const float r = b == selected ? 10.0f : 8.0f;

        g.setColour (bandColour (b));
        g.fillEllipse (x - r, y - r, r * 2, r * 2);
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawEllipse (x - r, y - r, r * 2, r * 2, b == selected ? 2.0f : 1.0f);
        g.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        g.drawText (juce::String (b + 1), (int) (x - r), (int) (y - r), (int) (r * 2), (int) (r * 2),
                    juce::Justification::centred);
    }

    // Franja inferior: selector de bandas
    auto strip = getLocalBounds().removeFromBottom (stripHeight).toFloat();
    g.setColour (juce::Colour (0xff1c1f26));
    g.fillRect (strip);

    g.setFont (11.0f);
    g.setColour (juce::Colours::grey);
    g.drawText ("Doble clic: añadir/quitar banda  |  Arrastrar: frecuencia y ganancia  |  Rueda: Q",
                getLocalBounds().removeFromTop (14).reduced (10, 0), juce::Justification::centredRight);
}

void EQProEditor::resized()
{
    auto bounds = getLocalBounds();
    auto strip = bounds.removeFromBottom (stripHeight).reduced (10, 6);
    plot = bounds.withTrimmedLeft (34).withTrimmedRight (10).withTrimmedTop (16).withTrimmedBottom (22).toFloat();

    auto left = strip.removeFromLeft (150);
    onButton.setBounds (left.removeFromTop (30));
    typeBox.setBounds (left.removeFromTop (30).reduced (0, 2));

    auto out = strip.removeFromRight (100);
    outLabel.setBounds (out.removeFromTop (16));
    outSlider.setBounds (out);

    const int w = strip.getWidth() / 3;
    for (auto [s, l] : { std::pair<juce::Slider*, juce::Label*> { &freqSlider, &freqLabel },
                         { &gainSlider, &gainLabel }, { &qSlider, &qLabel } })
    {
        auto cell = strip.removeFromLeft (w);
        l->setBounds (cell.removeFromTop (16));
        s->setBounds (cell);
    }
}
