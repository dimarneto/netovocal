#pragma once

#include "PluginProcessor.h"
#include <array>

// Visual: knobs com brilho, botoes luminosos, barra de intensidade e chave liga/desliga
class NetoLookAndFeel : public juce::LookAndFeel_V4
{
public:
    NetoLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPosProportional, float rotaryStartAngle,
                           float rotaryEndAngle, juce::Slider&) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawLinearSlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;
};

// "Orbe de Voz": esfera de vidro com anel de espectro ao redor (knob do Glue Vocal)
class OrbSlider : public juce::Slider
{
public:
    OrbSlider();
    void paint (juce::Graphics&) override;

    std::array<float, 48> halo {};   // espectro ao vivo, 0..1
    float pulse = 0.0f;              // nivel de saida, 0..1
    float phase = 0.0f;              // animacao
};

// Mostra a curva da sua voz (ao vivo) e a da referencia
class SpectrumView : public juce::Component
{
public:
    explicit SpectrumView (NetoVocalAudioProcessor& p) : proc (p) { setInterceptsMouseClicks (false, false); }
    void paint (juce::Graphics&) override;

private:
    NetoVocalAudioProcessor& proc;
};

class NetoVocalAudioProcessorEditor : public juce::AudioProcessorEditor,
                                      private juce::Timer
{
public:
    explicit NetoVocalAudioProcessorEditor (NetoVocalAudioProcessor&);
    ~NetoVocalAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    struct Knob
    {
        juce::Slider slider;
        juce::Label  title, desc;
        std::unique_ptr<SliderAttachment> attachment;
    };

    void setupKnob (Knob&, const char* paramID, const juce::String& title, const juce::String& desc, bool alignRight);
    void layoutKnob (Knob&, juce::Rectangle<int> area, bool knobOnLeft);
    void timerCallback() override;

    NetoVocalAudioProcessor& proc;
    NetoLookAndFeel laf;

    Knob presence, body, clarity, harmony, space, natural, mix;

    OrbSlider glueOrb;
    std::unique_ptr<SliderAttachment> glueAttachment;
    juce::Label glueTitle, glueSub;

    SpectrumView spectrum;
    juce::TextButton loadBtn, captureBtn, applyBtn;
    juce::Slider matchSlider;
    std::unique_ptr<SliderAttachment> matchAttachment;
    juce::Label matchLabel, statusLabel;

    juce::ComboBox presetBox;

    std::array<juce::TextButton, 3> modeButtons;
    juce::ToggleButton fineTune;
    std::unique_ptr<ButtonAttachment> fineAttachment;
    juce::AudioParameterChoice* modeParam = nullptr;

    std::unique_ptr<juce::FileChooser> chooser;

    juce::Rectangle<float> headerB, metersB, voiceB, rightB;
    juce::Rectangle<int> meterBars, grBar, modeTitleB, vmTitleB;

    float phase = 0.0f, smLevel = 0.0f, meterL = 0.0f, meterR = 0.0f;
    juce::String statusText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetoVocalAudioProcessorEditor)
};
