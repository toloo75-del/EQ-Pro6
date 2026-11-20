#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
const juce::StringArray& EQProProcessor::typeNames()
{
    static const juce::StringArray names { "Bell", "Low Shelf", "High Shelf",
                                           "Low Cut", "High Cut", "Notch", "Band Pass" };
    return names;
}

EQProProcessor::Coeffs::Ptr EQProProcessor::makeCoeffs (int type, double sr, float f, float gainDb, float q)
{
    f = juce::jlimit (10.0f, (float) (sr * 0.49), f);
    q = juce::jmax (0.05f, q);
    const float g = juce::Decibels::decibelsToGain (gainDb);

    switch (type)
    {
        case 0:  return Coeffs::makePeakFilter (sr, f, q, g);
        case 1:  return Coeffs::makeLowShelf   (sr, f, q, g);
        case 2:  return Coeffs::makeHighShelf  (sr, f, q, g);
        case 3:  return Coeffs::makeHighPass   (sr, f, q);
        case 4:  return Coeffs::makeLowPass    (sr, f, q);
        case 5:  return Coeffs::makeNotch      (sr, f, q);
        default: return Coeffs::makeBandPass   (sr, f, q);
    }
}

juce::AudioProcessorValueTreeState::ParameterLayout EQProProcessor::createLayout()
{
    std::vector<std::unique_ptr<juce::RangedAudioParameter>> params;

    const float defFreq[numBands] = { 60.f, 150.f, 400.f, 1000.f, 2500.f, 5000.f, 10000.f, 16000.f };
    const int   defType[numBands] = { 1, 0, 0, 0, 0, 0, 0, 2 };

    for (int i = 0; i < numBands; ++i)
    {
        const auto s = juce::String (i);
        const auto n = "Band " + juce::String (i + 1) + " ";

        params.push_back (std::make_unique<juce::AudioParameterBool> (
            juce::ParameterID { "b" + s + "_on", 1 }, n + "On", false));

        params.push_back (std::make_unique<juce::AudioParameterChoice> (
            juce::ParameterID { "b" + s + "_type", 1 }, n + "Type", typeNames(), defType[i]));

        juce::NormalisableRange<float> fr (20.0f, 20000.0f, 0.01f);
        fr.setSkewForCentre (1000.0f);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "b" + s + "_freq", 1 }, n + "Freq", fr, defFreq[i],
            juce::AudioParameterFloatAttributes().withLabel ("Hz")));

        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "b" + s + "_gain", 1 }, n + "Gain",
            juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
            juce::AudioParameterFloatAttributes().withLabel ("dB")));

        juce::NormalisableRange<float> qr (0.1f, 18.0f, 0.01f);
        qr.setSkewForCentre (1.0f);
        params.push_back (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { "b" + s + "_q", 1 }, n + "Q", qr, 1.0f));
    }

    params.push_back (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "out_gain", 1 }, "Output Gain",
        juce::NormalisableRange<float> (-24.0f, 24.0f, 0.01f), 0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return { params.begin(), params.end() };
}

//==============================================================================
EQProProcessor::EQProProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "STATE", createLayout())
{
    for (int i = 0; i < numBands; ++i)
    {
        const auto s = juce::String (i);
        bandParams[(size_t) i].on   = apvts.getRawParameterValue ("b" + s + "_on");
        bandParams[(size_t) i].type = apvts.getRawParameterValue ("b" + s + "_type");
        bandParams[(size_t) i].freq = apvts.getRawParameterValue ("b" + s + "_freq");
        bandParams[(size_t) i].gain = apvts.getRawParameterValue ("b" + s + "_gain");
        bandParams[(size_t) i].q    = apvts.getRawParameterValue ("b" + s + "_q");
    }
    outGainParam = apvts.getRawParameterValue ("out_gain");
}

bool EQProProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& in  = layouts.getMainInputChannelSet();
    const auto& out = layouts.getMainOutputChannelSet();
    if (in != out) return false;
    return in == juce::AudioChannelSet::mono() || in == juce::AudioChannelSet::stereo();
}

void EQProProcessor::prepareToPlay (double sampleRate, int)
{
    currentSampleRate = sampleRate;

    for (auto& band : filters)
        for (auto& f : band)
            f.reset();

    for (auto& c : cache)
        c = BandCache();

    fifoPos = 0;
}

void EQProProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const double sr = getSampleRate();
    const int numSamples = buffer.getNumSamples();
    const int numCh = juce::jmin (buffer.getNumChannels(), 2);

    // Limpia canales extra de salida
    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, numSamples);

    for (int b = 0; b < numBands; ++b)
    {
        const auto& p = bandParams[(size_t) b];
        if (p.on->load() < 0.5f)
            continue;

        const int   type = (int) p.type->load();
        const float f    = p.freq->load();
        const float g    = p.gain->load();
        const float q    = p.q->load();

        auto& c = cache[(size_t) b];
        if (type != c.type || f != c.freq || g != c.gain || q != c.q)
        {
            auto coeffs = makeCoeffs (type, sr, f, g, q);
            for (auto& flt : filters[(size_t) b])
                flt.coefficients = coeffs;
            c = { f, g, q, type };
        }

        if (typeUsesGain (type) && std::abs (g) < 0.01f)
            continue;

        for (int ch = 0; ch < numCh; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            auto& flt = filters[(size_t) b][(size_t) ch];
            for (int i = 0; i < numSamples; ++i)
                data[i] = flt.processSample (data[i]);
        }
    }

    const float outGain = juce::Decibels::decibelsToGain (outGainParam->load());
    for (int ch = 0; ch < numCh; ++ch)
        buffer.applyGain (ch, 0, numSamples, outGain);

    // Alimenta el analizador con el canal izquierdo (post-EQ)
    if (numCh > 0)
    {
        const auto* data = buffer.getReadPointer (0);
        for (int i = 0; i < numSamples; ++i)
        {
            fifo[(size_t) fifoPos++] = data[i];
            if (fifoPos >= fftSize)
            {
                fifoPos = 0;
                if (! snapshotReady.load())
                {
                    fftSnapshot = fifo;
                    snapshotReady = true;
                }
            }
        }
    }
}

//==============================================================================
juce::AudioProcessorEditor* EQProProcessor::createEditor()
{
    return new EQProEditor (*this);
}

void EQProProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void EQProProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new EQProProcessor();
}
