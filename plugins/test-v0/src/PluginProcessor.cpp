#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
// Monter ce numero a chaque changement de format de l'etat, et gerer l'ancien dans setStateInformation.
constexpr int stateVersion = 1;
}

PluginProcessor::PluginProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput ("Input", juce::AudioChannelSet::stereo(), true)
                          .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "ETAT", createLayout())
{
    gainDb = apvts.getRawParameterValue ("gain");
}

juce::AudioProcessorValueTreeState::ParameterLayout PluginProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // L'identifiant ("gain") et son numero de version (1) ne changent JAMAIS apres une sortie :
    // les projets et les automations des DAW en dependent. Le libelle, lui, peut changer.
    layout.add (std::make_unique<juce::AudioParameterFloat> (
        juce::ParameterID { "gain", 1 },
        "Gain",
        juce::NormalisableRange<float> (-60.0f, 12.0f, 0.01f),
        0.0f,
        juce::AudioParameterFloatAttributes().withLabel ("dB")));

    return layout;
}

void PluginProcessor::prepareToPlay (double sampleRate, int)
{
    gain.reset (sampleRate, 20.0f);
    gain.snap (droz::dbToGain (gainDb->load(), -60.0f));
}

bool PluginProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();
    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;
    return layouts.getMainInputChannelSet() == out;
}

void PluginProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    for (int ch = getTotalNumInputChannels(); ch < getTotalNumOutputChannels(); ++ch)
        buffer.clear (ch, 0, buffer.getNumSamples());

    gain.setTarget (droz::dbToGain (gainDb->load(), -60.0f));

    const int numChannels = buffer.getNumChannels();
    const int numSamples = buffer.getNumSamples();
    for (int i = 0; i < numSamples; ++i)
    {
        const float g = gain.next();
        for (int ch = 0; ch < numChannels; ++ch)
            buffer.getWritePointer (ch)[i] *= g;
    }
}

juce::AudioProcessorEditor* PluginProcessor::createEditor()
{
    return new PluginEditor (*this);
}

void PluginProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    state.setProperty ("version_etat", stateVersion, nullptr);
    state.setProperty ("largeur", editorWidth.load(), nullptr);
    state.setProperty ("hauteur", editorHeight.load(), nullptr);

    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void PluginProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    const auto xml = getXmlFromBinary (data, sizeInBytes);
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return; // etat illisible : on garde les reglages courants plutot que de planter

    auto state = juce::ValueTree::fromXml (*xml);
    // Ici : convertir les anciens formats si state["version_etat"] < stateVersion.
    editorWidth = juce::jlimit (320, 1600, static_cast<int> (state.getProperty ("largeur", 560)));
    editorHeight = juce::jlimit (200, 1200, static_cast<int> (state.getProperty ("hauteur", 300)));
    apvts.replaceState (state);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new PluginProcessor();
}
