#pragma once

#include <JuceHeader.h>

//==============================================================================
// EQ Pro - ecualizador paramétrico de 8 bandas
//==============================================================================
class EQProProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numBands = 8;
    static constexpr int fftOrder = 11;
    static constexpr int fftSize  = 1 << fftOrder;

    using Filter = juce::dsp::IIR::Filter<float>;
    using Coeffs = juce::dsp::IIR::Coefficients<float>;

    EQProProcessor();
    ~EQProProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "EQ Pro"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    //==========================================================================
    static const juce::StringArray& typeNames();
    static bool typeUsesGain (int type) { return type <= 2; }
    static Coeffs::Ptr makeCoeffs (int type, double sampleRate, float freq, float gainDb, float q);

    juce::AudioProcessorValueTreeState apvts;

    std::atomic<double> currentSampleRate { 44100.0 };

    // Analizador de espectro
    std::array<float, fftSize> fftSnapshot {};
    std::atomic<bool> snapshotReady { false };

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    struct BandParams
    {
        std::atomic<float>* on   = nullptr;
        std::atomic<float>* type = nullptr;
        std::atomic<float>* freq = nullptr;
        std::atomic<float>* gain = nullptr;
        std::atomic<float>* q    = nullptr;
    };

    struct BandCache
    {
        float freq = -1.0f, gain = -999.0f, q = -1.0f;
        int type = -1;
    };

    std::array<BandParams, numBands> bandParams;
    std::array<BandCache, numBands> cache;
    std::array<std::array<Filter, 2>, numBands> filters;
    std::atomic<float>* outGainParam = nullptr;

    std::array<float, fftSize> fifo {};
    int fifoPos = 0;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQProProcessor)
};
