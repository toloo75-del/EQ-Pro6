#pragma once

#include <JuceHeader.h>
#include "PluginProcessor.h"

//==============================================================================
class EQProEditor : public juce::AudioProcessorEditor,
                    private juce::Timer
{
public:
    explicit EQProEditor (EQProProcessor&);
    ~EQProEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;
    void mouseDoubleClick (const juce::MouseEvent&) override;
    void mouseWheelMove (const juce::MouseEvent&, const juce::MouseWheelDetails&) override;

private:
    using APVTS = juce::AudioProcessorValueTreeState;

    void timerCallback() override;
    void selectBand (int band);
    void updateSpectrum();

    int  hitBand (juce::Point<float> p) const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    float gainToY (float g) const;
    float yToGain (float y) const;
    juce::Colour bandColour (int b) const;

    float getParam (const juce::String& id) const;
    void  setParam (const juce::String& id, float value);
    void  gesture (int band, bool begin);
    juce::String pid (int band, const char* suffix) const { return "b" + juce::String (band) + "_" + suffix; }

    EQProProcessor& proc;

    juce::Rectangle<float> plot;
    int selected = 0;
    int dragging = -1;

    // Controles de la banda seleccionada
    juce::ToggleButton onButton { "On" };
    juce::ComboBox typeBox;
    juce::Slider freqSlider, gainSlider, qSlider, outSlider;
    juce::Label freqLabel, gainLabel, qLabel, outLabel;
    std::unique_ptr<APVTS::ButtonAttachment>   onAtt;
    std::unique_ptr<APVTS::ComboBoxAttachment> typeAtt;
    std::unique_ptr<APVTS::SliderAttachment>   freqAtt, gainAtt, qAtt, outAtt;

    // Analizador
    juce::dsp::FFT fft { EQProProcessor::fftOrder };
    juce::dsp::WindowingFunction<float> window { (size_t) EQProProcessor::fftSize,
                                                 juce::dsp::WindowingFunction<float>::hann };
    std::array<float, EQProProcessor::fftSize / 2> spectrum {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EQProEditor)
};
