#include "PluginEditor.h"

namespace
{
const juce::Colour background { 8, 11, 18 };
const juce::Colour border { 99, 101, 100 };
const juce::Colour blue { 0, 87, 190 };
const juce::Colour gold { 231, 190, 81 };
const juce::Colour ink { 8, 10, 14 };
constexpr int editorWidth = 694;
constexpr int editorHeight = 400;

float gainToDecibels(float gain)
{
    return juce::Decibels::gainToDecibels(gain, -100.0f);
}
}

SlateCompAudioProcessorEditor::SlateCompAudioProcessorEditor(SlateCompAudioProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);
    setSize(editorWidth, editorHeight);

    const std::array<juce::String, 11> ids {{
        "threshold", "attack", "ratio", "release", "variableRelease",
        "output", "knee", "thrust", "link", "mix", "trim"
    }};
    const std::array<juce::String, 11> labels {{
        "THRESH", "ATTACK", "RATIO", "RELEASE", "VAR RELEASE",
        "OUTPUT", "KNEE", "THRUST", "L/R LINK", "MIX", "TRIM"
    }};
    const std::array<juce::String, 11> suffixes {{
        " dB", " ms", ":1", " ms", " ms", " dB", " dB", "", "%", "%", " dB"
    }};
    for (size_t i = 0; i < controls.size(); ++i)
        configureRotary(controls[i], ids[i], labels[i], suffixes[i]);
    controls[5].label.setVisible(false);

    configureToggle(toggles[0], "analog", "ANALOG");
    configureToggle(toggles[1], "autoMakeup", "AUTO");
    configureToggle(toggles[2], "variableOn", "VAR REL");

    shapeBox.addItemList({ "FULL", "HP", "LP", "BP" }, 1);
    toneBox.addItemList({ "FWD", "FBCK", "OLD" }, 1);
    meterBox.addItemList({ "IN", "OUT", "GR" }, 1);
    for (auto* box : { &shapeBox, &toneBox, &meterBox })
    {
        box->setColour(juce::ComboBox::backgroundColourId, ink);
        box->setColour(juce::ComboBox::outlineColourId, border);
        box->setColour(juce::ComboBox::textColourId, juce::Colours::white);
        addAndMakeVisible(box);
    }
    shapeAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "shape", shapeBox);
    toneAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "tone", toneBox);
    meterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.parameters, "meter", meterBox);

    presetBox.addItemList({ "Default", "Vocal Leveling", "Drum Bus", "Mix Bus", "Peak Catcher" }, 1);
    presetBox.setColour(juce::ComboBox::backgroundColourId, ink);
    presetBox.setColour(juce::ComboBox::outlineColourId, border);
    presetBox.setColour(juce::ComboBox::textColourId, juce::Colours::white);
    presetBox.onChange = [this]
    {
        const auto selected = presetBox.getSelectedId();
        if (selected > 0)
            processor.selectPreset(selected - 1);
    };
    addAndMakeVisible(presetBox);

    grReadout.setJustificationType(juce::Justification::centred);
    grReadout.setColour(juce::Label::textColourId, juce::Colours::white);
    grReadout.setFont(juce::Font(10.0f, juce::Font::bold));
    addAndMakeVisible(grReadout);
    grReadout.setText("0", juce::dontSendNotification);
    presetBox.setSelectedId(processor.getCurrentProgram() + 1, juce::dontSendNotification);
    startTimerHz(30);
}

SlateCompAudioProcessorEditor::~SlateCompAudioProcessorEditor()
{
    setLookAndFeel(nullptr);
}

void SlateCompAudioProcessorEditor::configureRotary(Control& control, const juce::String& id,
                                                       const juce::String& label, const juce::String& suffix)
{
    control.slider.setSliderStyle(juce::Slider::RotaryHorizontalVerticalDrag);
    control.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 54, 15);
    control.slider.setTextValueSuffix(suffix);
    control.slider.setColour(juce::Slider::rotarySliderFillColourId, blue);
    control.slider.setColour(juce::Slider::rotarySliderOutlineColourId, juce::Colour(44, 49, 61));
    control.slider.setColour(juce::Slider::thumbColourId, juce::Colour(225, 228, 230));
    control.slider.setColour(juce::Slider::textBoxTextColourId, juce::Colours::white);
    control.slider.setColour(juce::Slider::textBoxBackgroundColourId, ink);
    control.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    control.label.setText(label, juce::dontSendNotification);
    control.label.setJustificationType(juce::Justification::centred);
    control.label.setColour(juce::Label::textColourId, juce::Colours::white);
    control.label.setFont(juce::Font(10.0f, juce::Font::bold));
    addAndMakeVisible(control.slider);
    addAndMakeVisible(control.label);
    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.parameters, id, control.slider);
}

void SlateCompAudioProcessorEditor::configureToggle(ButtonControl& control, const juce::String& id,
                                                      const juce::String& label)
{
    control.button.setButtonText(label);
    control.button.setColour(juce::ToggleButton::textColourId, juce::Colours::white);
    control.button.setColour(juce::ToggleButton::tickColourId, blue.brighter(0.7f));
    addAndMakeVisible(control.button);
    control.attachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.parameters, id, control.button);
}

void SlateCompAudioProcessorEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);
    g.setColour(juce::Colour(6, 9, 15));
    g.fillRoundedRectangle(8.0f, 125.0f, 523.0f, 129.0f, 15.0f);
    g.fillRoundedRectangle(8.0f, 261.0f, 306.0f, 127.0f, 15.0f);
    g.fillRoundedRectangle(322.0f, 261.0f, 209.0f, 127.0f, 15.0f);
    g.setColour(border);
    g.drawRoundedRectangle(8.0f, 125.0f, 523.0f, 129.0f, 15.0f, 1.0f);
    g.drawRoundedRectangle(8.0f, 261.0f, 306.0f, 127.0f, 15.0f, 1.0f);
    g.drawRoundedRectangle(322.0f, 261.0f, 209.0f, 127.0f, 15.0f, 1.0f);
    g.setColour(juce::Colour(7, 23, 49));
    g.fillRect(541, 24, 153, 376);
    g.setColour(blue);
    g.fillRect(541, 24, 153, 376);
    g.setColour(juce::Colour(10, 16, 28));
    g.fillRect(541, 24, 153, 376);
    g.setColour(blue.withAlpha(0.42f));
    g.fillRect(543, 25, 149, 374);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(15.0f, juce::Font::bold));
    g.drawFittedText("SLATE", 30, 37, 46, 37, juce::Justification::centred, 1);
    g.setColour(blue.brighter(0.5f));
    juce::Path logoMark;
    logoMark.addTriangle(9.0f, 39.0f, 9.0f, 73.0f, 29.0f, 56.0f);
    g.fillPath(logoMark);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText("VU", 393, 30, 136, 18, juce::Justification::centred, 1);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText("COMPRESSOR", 143, 126, 258, 20, juce::Justification::centred, 1);
    g.drawFittedText("TONE", 90, 264, 145, 18, juce::Justification::centred, 1);
    g.drawFittedText("LINK", 354, 264, 148, 18, juce::Justification::centred, 1);
    g.setFont(juce::Font(10.0f, juce::Font::bold));
    g.drawFittedText("METER", 450, 44, 57, 16, juce::Justification::centred, 1);
    g.drawFittedText("ANALOG", 554, 30, 126, 18, juce::Justification::centred, 1);
    g.drawFittedText("OUTPUT", 560, 118, 119, 18, juce::Justification::centred, 1);

    for (int i = 0; i < 2; ++i)
    {
        const auto x = i == 0 ? 83 : 240;
        g.setColour(juce::Colour(35, 38, 40));
        g.fillRoundedRectangle(static_cast<float>(x), 30.0f, 144.0f, 89.0f, 5.0f);
        g.setColour(juce::Colour(5, 6, 7));
        g.fillRoundedRectangle(static_cast<float>(x + 7), 36.0f, 130.0f, 63.0f, 4.0f);
        g.setColour(gold);
        g.fillRoundedRectangle(static_cast<float>(x + 12), 40.0f, 120.0f, 53.0f, 4.0f);
        g.setColour(ink);
        g.setFont(juce::Font(8.0f));
        g.drawFittedText("-24      -12       -6        0        +6", x + 12, 42, 120, 10,
                         juce::Justification::centred, 1);
        const auto meterValue = i == 0 ? inputPeak : outputPeak;
        const auto needleX = x + 25 + juce::jlimit(0.0f, 86.0f, (gainToDecibels(meterValue) + 24.0f) * 2.7f);
        g.setColour(juce::Colour(32, 25, 17));
        g.drawLine(static_cast<float>(x + 72), 87.0f, needleX, 52.0f, 2.0f);
        g.setColour(juce::Colour(13, 15, 18));
        g.fillEllipse(static_cast<float>(x + 66), 100.0f, 13.0f, 13.0f);
    }

    const auto meterChoice = static_cast<int>(processor.parameters.getRawParameterValue("meter")->load());
    const auto value = meterChoice == 0 ? inputPeak : meterChoice == 1 ? outputPeak : reduction;
    g.setFont(juce::Font(10.0f));
    g.setColour(juce::Colour(116, 22, 23));
    g.fillEllipse(472, 54, 9, 9);
    g.setColour(juce::Colour(252, 53, 47));
    g.fillEllipse(474, 56, 5, 5);
    g.setColour(value > 0.1f ? juce::Colour(64, 230, 82) : juce::Colour(66, 75, 58));
    g.fillEllipse(472, 91, 8, 8);

    g.setColour(juce::Colour(11, 46, 99));
    g.fillRoundedRectangle(587.0f, 157.0f, 56.0f, 52.0f, 21.0f);
    g.setColour(juce::Colour(207, 204, 194));
    g.fillRoundedRectangle(583.0f, 164.0f, 63.0f, 37.0f, 12.0f);
    g.setColour(juce::Colour(117, 18, 19));
    g.fillEllipse(586.0f, 158.0f, 55.0f, 55.0f);
    g.setColour(juce::Colour(231, 45, 37));
    g.fillEllipse(591.0f, 163.0f, 45.0f, 45.0f);
    g.setColour(juce::Colour(255, 160, 112));
    g.drawEllipse(591.0f, 163.0f, 45.0f, 45.0f, 2.0f);
    g.setColour(juce::Colours::white);
    g.setFont(juce::Font(12.0f, juce::Font::bold));
    g.drawFittedText("-24", 547, 211, 37, 18, juce::Justification::centred, 1);
    g.drawFittedText("+24", 645, 211, 41, 18, juce::Justification::centred, 1);
    g.setColour(juce::Colour(6, 9, 15));
    g.fillRect(8, 22, 684, 1);
    g.setColour(juce::Colour(77, 84, 99));
    g.drawRect(1, 1, editorWidth - 2, editorHeight - 2, 1);
}

void SlateCompAudioProcessorEditor::resized()
{
    presetBox.setBounds(98, 3, 367, 17);
    grReadout.setBounds(439, 91, 77, 18);
    shapeBox.setBounds(103, 295, 92, 22);
    toneBox.setBounds(211, 295, 101, 22);
    meterBox.setBounds(450, 65, 57, 22);

    const std::array<juce::Rectangle<int>, 11> bounds {{
        { 47, 150, 53, 65 }, { 144, 150, 53, 65 }, { 241, 150, 53, 65 },
        { 338, 150, 53, 65 }, { 434, 150, 53, 65 }, { 583, 134, 63, 70 },
        { 26, 332, 55, 48 }, { 128, 332, 55, 48 }, { 441, 294, 54, 65 },
        { 561, 292, 39, 47 }, { 629, 292, 39, 47 }
    }};
    for (size_t i = 0; i < controls.size(); ++i)
    {
        controls[i].slider.setBounds(bounds[i]);
        const auto labelY = i < 5 ? 218 : i == 5 ? 205 : i < 8 ? 380 : i == 8 ? 359 : 339;
        const auto labelHeight = i == 5 ? 16 : 13;
        controls[i].label.setBounds(bounds[i].getX() - 8, labelY, bounds[i].getWidth() + 16, labelHeight);
    }
    toggles[0].button.setBounds(588, 46, 60, 20);
    toggles[1].button.setBounds(605, 235, 75, 24);
    toggles[2].button.setBounds(16, 293, 84, 20);
}

void SlateCompAudioProcessorEditor::timerCallback()
{
    const auto decay = 0.82f;
    inputPeak = juce::jmax(processor.inputLevel.load(), inputPeak * decay);
    outputPeak = juce::jmax(processor.outputLevel.load(), outputPeak * decay);
    reduction = juce::jmax(processor.gainReduction.load(), reduction * decay);
    const auto meterChoice = static_cast<int>(processor.parameters.getRawParameterValue("meter")->load());
    const auto value = meterChoice == 0 ? gainToDecibels(inputPeak)
                      : meterChoice == 1 ? gainToDecibels(outputPeak) : reduction;
    const auto text = (meterChoice == 2 ? juce::String(value, 1) : juce::String(value, 0)) + " dB";
    grReadout.setText(text, juce::dontSendNotification);
    repaint();
}
