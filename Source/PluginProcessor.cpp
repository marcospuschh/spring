#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace juce;

//==============================================================================
AudioProcessorValueTreeState::ParameterLayout PTZRSpringProcessor::createLayout()
{
    using P = AudioParameterFloat;
    using R = NormalisableRange<float>;
    AudioProcessorValueTreeState::ParameterLayout l;

    auto pct = [] (float v, int) { return String (roundToInt (v)) + "%"; };

    l.add (std::make_unique<P> (ParameterID { "mix", 1 }, "Mix", R (0.f, 100.f, 0.1f), 35.f,
                                AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { "decay", 1 }, "Decay", R (0.5f, 10.f, 0.01f), 2.8f,
                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (v, 1) + "s"; })));
    l.add (std::make_unique<P> (ParameterID { "tone", 1 }, "Tone", R (0.f, 100.f, 0.1f), 50.f,
                                AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { "predelay", 1 }, "Pre-Delay", R (0.f, 200.f, 0.1f, 0.333f), 25.f,
                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int) { return String (roundToInt (v)) + "ms"; })));
    l.add (std::make_unique<P> (ParameterID { "drive", 1 }, "Drive", R (0.f, 100.f, 0.1f), 30.f,
                                AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<AudioParameterChoice> (ParameterID { "type", 1 }, "Spring Type",
                                                   StringArray { "Vintage", "Classic", "Modern" }, 1));
    l.add (std::make_unique<P> (ParameterID { "damping", 1 }, "Damping", R (0.f, 100.f, 0.1f), 60.f,
                                AudioParameterFloatAttributes().withStringFromValueFunction (pct)));
    l.add (std::make_unique<P> (ParameterID { "lowcut", 1 }, "Low Cut", R (20.f, 1000.f, 1.f, 0.25f), 120.f,
                                AudioParameterFloatAttributes().withStringFromValueFunction ([] (float v, int)
                                { return v >= 1000.f ? String ("1kHz") : String (roundToInt (v)) + "Hz"; })));
    return l;
}

//==============================================================================
PTZRSpringProcessor::PTZRSpringProcessor()
    : AudioProcessor (BusesProperties()
                          .withInput  ("Input",  AudioChannelSet::stereo(), true)
                          .withOutput ("Output", AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "PTZR_SPRING", createLayout())
{
    pMix = apvts.getRawParameterValue ("mix");       pDecay = apvts.getRawParameterValue ("decay");
    pTone = apvts.getRawParameterValue ("tone");     pPre = apvts.getRawParameterValue ("predelay");
    pDrive = apvts.getRawParameterValue ("drive");   pType = apvts.getRawParameterValue ("type");
    pDamp = apvts.getRawParameterValue ("damping");  pLow = apvts.getRawParameterValue ("lowcut");

    apvts.state.setProperty ("presetName", "INIT", nullptr);
    rebuildPresetList();
}

bool PTZRSpringProcessor::isBusesLayoutSupported (const BusesLayout& l) const
{
    // Saída sempre estéreo. Entrada mono (mono -> estéreo) ou estéreo (estéreo -> estéreo).
    if (l.getMainOutputChannelSet() != AudioChannelSet::stereo())
        return false;
    return l.getMainInputChannelSet() == AudioChannelSet::mono()
        || l.getMainInputChannelSet() == AudioChannelSet::stereo();
}

void PTZRSpringProcessor::prepareToPlay (double sampleRate, int)
{
    engine.prepare (sampleRate, (int) pType->load());
}

void PTZRSpringProcessor::processBlock (AudioBuffer<float>& buffer, MidiBuffer&)
{
    ScopedNoDenormals noDenormals;

    const int n = buffer.getNumSamples();
    const int nIn = getTotalNumInputChannels();
    if (n == 0 || buffer.getNumChannels() < 2) return;

    // entrada mono: duplica o canal 0 nos dois lados do motor
    const float* inL = buffer.getReadPointer (0);
    const float* inR = nIn >= 2 ? buffer.getReadPointer (1) : inL;
    float* outL = buffer.getWritePointer (0);
    float* outR = buffer.getWritePointer (1);

    float pk = 0.f;
    for (int i = 0; i < n; ++i) pk = jmax (pk, std::abs (inL[i]), std::abs (inR[i]));
    inPeak.store (jmax (inPeak.load(), pk));

    ptzr::SpringReverbEngine::Params p;
    p.mix = pMix->load() * 0.01f;
    p.decaySec = pDecay->load();
    p.tone = pTone->load() * 0.01f;
    p.preDelayMs = pPre->load();
    p.drive = pDrive->load() * 0.01f;
    p.damping = pDamp->load() * 0.01f;
    p.lowCutHz = pLow->load();
    p.type = (int) pType->load();
    engine.setParams (p);

    engine.process (inL, inR, outL, outR, n);

    pk = 0.f;
    for (int i = 0; i < n; ++i) pk = jmax (pk, std::abs (outL[i]), std::abs (outR[i]));
    outPeak.store (jmax (outPeak.load(), pk));
}

AudioProcessorEditor* PTZRSpringProcessor::createEditor() { return new PTZRSpringEditor (*this); }

//==============================================================================
// Estado
void PTZRSpringProcessor::getStateInformation (MemoryBlock& dest)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, dest);
}

void PTZRSpringProcessor::setStateInformation (const void* data, int size)
{
    if (auto xml = getXmlFromBinary (data, size))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (ValueTree::fromXml (*xml));
}

//==============================================================================
// Presets
File PTZRSpringProcessor::userPresetDir()
{
    return File::getSpecialLocation (File::userDocumentsDirectory)
        .getChildFile ("PTZR.PLUGINS").getChildFile ("PTZR Spring").getChildFile ("Presets");
}

void PTZRSpringProcessor::rebuildPresetList()
{
    const ScopedLock sl (presetLock);
    presets.clear();

    auto add = [this] (const char* name, float mix, float decay, float tone, float pre, float drive, int type, float damp, float low)
    {
        Preset p; p.name = name;
        p.v[0] = mix; p.v[1] = decay; p.v[2] = tone; p.v[3] = pre; p.v[4] = drive; p.v[5] = (float) type; p.v[6] = damp; p.v[7] = low;
        presets.push_back (p);
    };
    //  nome                 mix  decay tone  pre  drive type damp  lowcut     (type: 0 Vintage, 1 Classic, 2 Modern)
    add ("INIT",             35.f, 2.8f, 50.f, 25.f, 30.f, 1, 60.f, 120.f);
    add ("Surf Garage",      38.f, 1.8f, 55.f, 10.f, 45.f, 0, 45.f, 150.f);
    add ("Clean Drums",      22.f, 1.1f, 60.f,  5.f, 15.f, 1, 55.f, 200.f);
    add ("Vocal Air",        28.f, 2.2f, 68.f, 40.f, 10.f, 2, 40.f, 250.f);
    add ("Dub Space",        45.f, 6.5f, 40.f, 60.f, 35.f, 1, 65.f, 180.f);
    add ("Cosmic Wash",      55.f, 9.5f, 52.f, 90.f, 20.f, 2, 50.f, 300.f);

    auto dir = userPresetDir();
    if (dir.isDirectory())
        for (auto& f : dir.findChildFiles (File::findFiles, false, "*.xml"))
        {
            Preset p; p.user = true; p.file = f; p.name = f.getFileNameWithoutExtension();
            presets.push_back (p);
        }
}

void PTZRSpringProcessor::setParam (const String& id, float v)
{
    if (auto* prm = apvts.getParameter (id))
    {
        prm->beginChangeGesture();
        prm->setValueNotifyingHost (prm->convertTo0to1 (v));
        prm->endChangeGesture();
    }
}

void PTZRSpringProcessor::applyPreset (const Preset& p)
{
    if (p.user)
    {
        if (auto xml = XmlDocument::parse (p.file))
            for (auto* e : xml->getChildWithTagNameIterator ("PARAM"))
                setParam (e->getStringAttribute ("id"), (float) e->getDoubleAttribute ("value"));
    }
    else
    {
        setParam ("mix", p.v[0]);   setParam ("decay", p.v[1]); setParam ("tone", p.v[2]);
        setParam ("predelay", p.v[3]); setParam ("drive", p.v[4]); setParam ("type", p.v[5]);
        setParam ("damping", p.v[6]); setParam ("lowcut", p.v[7]);
    }
    apvts.state.setProperty ("presetName", p.name, nullptr);
}

int PTZRSpringProcessor::getNumPrograms() { const ScopedLock sl (presetLock); return (int) presets.size(); }

const String PTZRSpringProcessor::getProgramName (int i)
{
    const ScopedLock sl (presetLock);
    return isPositiveAndBelow (i, (int) presets.size()) ? presets[(size_t) i].name : String();
}

void PTZRSpringProcessor::setCurrentProgram (int i)
{
    Preset p;
    {
        const ScopedLock sl (presetLock);
        if (! isPositiveAndBelow (i, (int) presets.size())) return;
        p = presets[(size_t) i];
    }
    currentProgram = i;
    applyPreset (p);
}

String PTZRSpringProcessor::getPresetName() const
{
    return apvts.state.getProperty ("presetName", "INIT").toString();
}

void PTZRSpringProcessor::loadPresetRelative (int delta)
{
    int count, idx = -1;
    {
        const ScopedLock sl (presetLock);
        count = (int) presets.size();
        auto cur = getPresetName();
        for (int i = 0; i < count; ++i)
            if (presets[(size_t) i].name == cur) { idx = i; break; }
    }
    if (count == 0) return;
    idx = idx < 0 ? 0 : (idx + delta + count) % count;
    setCurrentProgram (idx);
}

bool PTZRSpringProcessor::saveUserPreset (const String& rawName)
{
    auto name = File::createLegalFileName (rawName.trim());
    if (name.isEmpty()) return false;

    auto dir = userPresetDir();
    if (! dir.createDirectory().wasOk()) return false;

    auto file = dir.getChildFile (name + ".xml");
    auto xml = apvts.copyState().createXml();
    if (xml == nullptr || ! xml->writeTo (file)) return false;

    apvts.state.setProperty ("presetName", name, nullptr);
    rebuildPresetList();
    return true;
}

AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new PTZRSpringProcessor(); }
