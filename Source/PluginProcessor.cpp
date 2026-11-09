#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

namespace
{
constexpr float minimumDecibels = -100.0f;

float decibelsToGain(float decibels)
{
    return juce::Decibels::decibelsToGain(decibels);
}

float gainToDecibels(float gain)
{
    return juce::Decibels::gainToDecibels(gain, minimumDecibels);
}

float compressDecibels(float input, float threshold, float ratio, float knee)
{
    const auto over = input - threshold;
    if (knee <= 0.001f)
        return over > 0.0f ? over * (1.0f / ratio - 1.0f) : 0.0f;

    const auto halfKnee = knee * 0.5f;
    if (over <= -halfKnee)
        return 0.0f;
    if (over >= halfKnee)
        return over * (1.0f / ratio - 1.0f);

    const auto curved = over + halfKnee;
    return (1.0f / ratio - 1.0f) * curved * curved / (2.0f * knee);
}
}

SlateCompAudioProcessor::SlateCompAudioProcessor()
    : AudioProcessor(BusesProperties()
        .withInput("Input", juce::AudioChannelSet::stereo(), true)
        .withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      parameters(*this, nullptr, "SlateCompState", createParameterLayout())
{
}

juce::AudioProcessorValueTreeState::ParameterLayout SlateCompAudioProcessor::createParameterLayout()
{
    using namespace juce;
    std::vector<std::unique_ptr<RangedAudioParameter>> layout;
    const auto floatParameter = [&layout](const char* id, const char* name, NormalisableRange<float> range,
                                          float initial, const char* unit = "")
    {
        const auto attributes = AudioParameterFloatAttributes().withLabel(unit);
        layout.push_back(std::make_unique<AudioParameterFloat>(
            ParameterID { id, 1 }, name, range, initial, attributes));
    };

    floatParameter("threshold", "Threshold", { -36.0f, 0.0f, 0.1f }, -12.0f, " dB");
    floatParameter("attack", "Attack", { 0.1f, 100.0f, 0.01f, 0.45f }, 10.0f, " ms");
    floatParameter("ratio", "Ratio", { 1.0f, 20.0f, 0.01f, 0.5f }, 4.0f, ":1");
    floatParameter("release", "Release", { 10.0f, 3000.0f, 1.0f, 0.4f }, 120.0f, " ms");
    floatParameter("variableRelease", "Variable Release", { 10.0f, 3000.0f, 1.0f, 0.4f }, 300.0f, " ms");
    floatParameter("output", "Output", { -24.0f, 24.0f, 0.1f }, 0.0f, " dB");
    floatParameter("knee", "Knee", { 0.0f, 18.0f, 0.1f }, 6.0f, " dB");
    floatParameter("thrust", "Thrust", { 0.0f, 1.0f, 0.01f }, 0.0f);
    floatParameter("link", "Stereo Link", { 0.0f, 100.0f, 1.0f }, 100.0f, "%");
    floatParameter("mix", "Mix", { 0.0f, 100.0f, 1.0f }, 100.0f, "%");
    floatParameter("trim", "Trim", { -18.0f, 18.0f, 0.1f }, 0.0f, " dB");

    layout.push_back(std::make_unique<AudioParameterBool>(ParameterID { "analog", 1 }, "Analog", false));
    layout.push_back(std::make_unique<AudioParameterBool>(ParameterID { "autoMakeup", 1 }, "Auto Makeup", false));
    layout.push_back(std::make_unique<AudioParameterBool>(ParameterID { "variableOn", 1 }, "Variable Release", false));
    layout.push_back(std::make_unique<AudioParameterChoice>(ParameterID { "shape", 1 }, "Detector Shape",
        StringArray { "Full", "HP", "LP", "BP" }, 0));
    layout.push_back(std::make_unique<AudioParameterChoice>(ParameterID { "tone", 1 }, "Tone Type",
        StringArray { "FWD", "FBCK", "OLD" }, 0));
    layout.push_back(std::make_unique<AudioParameterChoice>(ParameterID { "meter", 1 }, "Meter Source",
        StringArray { "IN", "OUT", "GR" }, 2));
    return { layout.begin(), layout.end() };
}

void SlateCompAudioProcessor::prepareToPlay(double sampleRate, int)
{
    currentSampleRate = sampleRate;
    envelope.fill(0.0f);
    lowPassState.fill(0.0f);
    bandPassState.fill(0.0f);
    previousGain.fill(1.0f);
    inputLevel.store(0.0f);
    outputLevel.store(0.0f);
    gainReduction.store(0.0f);
}

void SlateCompAudioProcessor::releaseResources()
{
}

bool SlateCompAudioProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::mono() || output == juce::AudioChannelSet::stereo();
}

void SlateCompAudioProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    const auto channels = juce::jmin(buffer.getNumChannels(), 2);
    const auto samples = buffer.getNumSamples();
    if (channels == 0 || samples == 0)
        return;

    const auto threshold = parameters.getRawParameterValue("threshold")->load();
    const auto attackMs = parameters.getRawParameterValue("attack")->load();
    const auto releaseMs = parameters.getRawParameterValue("release")->load();
    const auto variableReleaseMs = parameters.getRawParameterValue("variableRelease")->load();
    const auto ratio = parameters.getRawParameterValue("ratio")->load();
    const auto knee = parameters.getRawParameterValue("knee")->load();
    const auto outputDb = parameters.getRawParameterValue("output")->load();
    const auto trimDb = parameters.getRawParameterValue("trim")->load();
    const auto mix = parameters.getRawParameterValue("mix")->load() * 0.01f;
    const auto link = parameters.getRawParameterValue("link")->load() * 0.01f;
    const auto thrust = parameters.getRawParameterValue("thrust")->load();
    const auto analog = parameters.getRawParameterValue("analog")->load() > 0.5f;
    const auto autoMakeup = parameters.getRawParameterValue("autoMakeup")->load() > 0.5f;
    const auto variableOn = parameters.getRawParameterValue("variableOn")->load() > 0.5f;
    const auto shape = static_cast<int>(parameters.getRawParameterValue("shape")->load());
    const auto tone = static_cast<int>(parameters.getRawParameterValue("tone")->load());

    const auto attackCoefficient = std::exp(-1.0f / static_cast<float>(currentSampleRate * attackMs * 0.001));
    const auto highPassCoefficient = std::exp(-1.0f / static_cast<float>(currentSampleRate * 0.001));
    const auto bandPassCoefficient = std::exp(-1.0f / static_cast<float>(currentSampleRate * 0.00012));
    const auto outputGain = decibelsToGain(outputDb + trimDb);
    float peakInput = 0.0f;
    float peakOutput = 0.0f;
    float maximumReduction = 0.0f;

    for (int sample = 0; sample < samples; ++sample)
    {
        std::array<float, 2> dry {};
        std::array<float, 2> detector {};
        std::array<float, 2> gains {};
        float linkedDetector = 0.0f;

        for (int channel = 0; channel < channels; ++channel)
        {
            dry[static_cast<size_t>(channel)] = buffer.getSample(channel, sample);
            const auto absolute = std::abs(dry[static_cast<size_t>(channel)]);
            peakInput = juce::jmax(peakInput, absolute);
            const auto low = (1.0f - highPassCoefficient) * dry[static_cast<size_t>(channel)]
                           + highPassCoefficient * lowPassState[static_cast<size_t>(channel)];
            lowPassState[static_cast<size_t>(channel)] = low;
            const auto high = dry[static_cast<size_t>(channel)] - low;
            const auto band = (1.0f - bandPassCoefficient) * high
                            + bandPassCoefficient * bandPassState[static_cast<size_t>(channel)];
            bandPassState[static_cast<size_t>(channel)] = band;
            auto shaped = dry[static_cast<size_t>(channel)];
            if (shape == 1) shaped = high;
            if (shape == 2) shaped = low;
            if (shape == 3) shaped = band;
            const auto thrustGain = decibelsToGain(thrust * 12.0f);
            detector[static_cast<size_t>(channel)] = std::abs(shaped * thrustGain);
            linkedDetector = juce::jmax(linkedDetector, detector[static_cast<size_t>(channel)]);
        }

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto index = static_cast<size_t>(channel);
            const auto linked = linkedDetector * link + detector[index] * (1.0f - link);
            const auto detectionSignal = tone == 1 ? linked * previousGain[index] : linked;
            const auto coefficient = detectionSignal > envelope[index]
                ? attackCoefficient
                : std::exp(-1.0f / static_cast<float>(currentSampleRate
                    * (variableOn ? variableReleaseMs * (1.0f + envelope[index]) : releaseMs) * 0.001));
            envelope[index] = coefficient * envelope[index] + (1.0f - coefficient) * detectionSignal;
            const auto levelDb = gainToDecibels(envelope[index]);
            auto reductionDb = compressDecibels(levelDb, threshold, ratio, knee);
            if (tone == 2)
                reductionDb *= 0.92f;
            const auto gain = decibelsToGain(reductionDb);
            previousGain[index] = gain;
            gains[index] = gain;
            maximumReduction = juce::jmax(maximumReduction, -reductionDb);
        }

        for (int channel = 0; channel < channels; ++channel)
        {
            const auto index = static_cast<size_t>(channel);
            const auto makeupGain = autoMakeup
                ? decibelsToGain(juce::jmin(-gainToDecibels(gains[index]), 12.0f) * 0.5f)
                : 1.0f;
            const auto compressed = dry[index] * gains[index] * makeupGain;
            auto result = (dry[index] * (1.0f - mix) + compressed * mix) * outputGain;
            if (analog)
                result = std::tanh(result * 1.12f) / std::tanh(1.12f);
            buffer.setSample(channel, sample, result);
            peakOutput = juce::jmax(peakOutput, std::abs(result));
        }
    }

    inputLevel.store(peakInput);
    outputLevel.store(peakOutput);
    gainReduction.store(maximumReduction);
}

void SlateCompAudioProcessor::setCurrentProgram(int index)
{
    selectPreset(juce::jlimit(0, getNumPrograms() - 1, index));
}

const juce::String SlateCompAudioProcessor::getProgramName(int index)
{
    static const juce::StringArray names { "Default", "Vocal Leveling", "Drum Bus", "Mix Bus", "Peak Catcher" };
    return names[index >= 0 && index < names.size() ? index : 0];
}

void SlateCompAudioProcessor::selectPreset(int index)
{
    struct Preset
    {
        float threshold, attack, ratio, release, variableRelease, output, knee, thrust, link, mix, trim;
        bool analog, autoMakeup, variableOn;
        int shape, tone;
    };
    static constexpr std::array<Preset, 5> presets {{
        { -12.0f, 10.0f, 4.0f, 120.0f, 300.0f, 0.0f, 6.0f, 0.0f, 100.0f, 100.0f, 0.0f, false, false, false, 0, 0 },
        { -20.0f, 18.0f, 2.5f, 180.0f, 420.0f, 2.0f, 8.0f, 0.15f, 100.0f, 100.0f, 0.0f, true, false, true, 1, 0 },
        { -16.0f, 8.0f, 4.0f, 90.0f, 240.0f, 1.0f, 4.0f, 0.3f, 70.0f, 75.0f, 0.0f, true, false, false, 1, 2 },
        { -10.0f, 30.0f, 2.0f, 250.0f, 550.0f, 1.5f, 10.0f, 0.0f, 100.0f, 100.0f, 0.0f, false, false, true, 0, 0 },
        { -18.0f, 0.5f, 8.0f, 60.0f, 160.0f, 3.0f, 1.0f, 0.1f, 100.0f, 100.0f, 0.0f, true, false, false, 0, 1 }
    }};
    const auto& preset = presets[static_cast<size_t>(juce::jlimit(0, 4, index))];
    const auto set = [this](const char* id, float value)
    {
        if (auto* parameter = parameters.getParameter(id))
            parameter->setValueNotifyingHost(parameter->convertTo0to1(value));
    };

    set("threshold", preset.threshold); set("attack", preset.attack); set("ratio", preset.ratio);
    set("release", preset.release); set("variableRelease", preset.variableRelease); set("output", preset.output);
    set("knee", preset.knee); set("thrust", preset.thrust); set("link", preset.link);
    set("mix", preset.mix); set("trim", preset.trim); set("analog", preset.analog ? 1.0f : 0.0f);
    set("autoMakeup", preset.autoMakeup ? 1.0f : 0.0f); set("variableOn", preset.variableOn ? 1.0f : 0.0f);
    set("shape", static_cast<float>(preset.shape)); set("tone", static_cast<float>(preset.tone));
    currentProgram = juce::jlimit(0, 4, index);
}

void SlateCompAudioProcessor::getStateInformation(juce::MemoryBlock& destination)
{
    if (auto xml = parameters.copyState().createXml())
        copyXmlToBinary(*xml, destination);
}

void SlateCompAudioProcessor::setStateInformation(const void* data, int size)
{
    if (auto xml = getXmlFromBinary(data, size))
        if (xml->hasTagName(parameters.state.getType()))
            parameters.replaceState(juce::ValueTree::fromXml(*xml));
}

juce::AudioProcessorEditor* SlateCompAudioProcessor::createEditor()
{
    return new SlateCompAudioProcessorEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new SlateCompAudioProcessor();
}
