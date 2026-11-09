#pragma once

#include "PluginProcessor.h"
#include <array>

class SlateCompAudioProcessorEditor final : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit SlateCompAudioProcessorEditor(SlateCompAudioProcessor&);
    ~SlateCompAudioProcessorEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    struct Control
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    struct ButtonControl
    {
        juce::ToggleButton button;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> attachment;
    };

    void timerCallback() override;
    void configureRotary(Control&, const juce::String&, const juce::String&, const juce::String&);
    void configureToggle(ButtonControl&, const juce::String&, const juce::String&);

    SlateCompAudioProcessor& processor;
    juce::LookAndFeel_V4 lookAndFeel;
    std::array<Control, 11> controls;
    std::array<ButtonControl, 3> toggles;
    juce::ComboBox shapeBox, toneBox, meterBox, presetBox;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> shapeAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> toneAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> meterAttachment;
    juce::Label grReadout;
    float inputPeak = 0.0f, outputPeak = 0.0f, reduction = 0.0f;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(SlateCompAudioProcessorEditor)
};
