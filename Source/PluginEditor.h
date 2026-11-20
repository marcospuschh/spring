#pragma once
#include <juce_audio_utils/juce_audio_utils.h>
#include "PluginProcessor.h"

//==============================================================================
struct PTZRTheme
{
    juce::Colour accent, accentDim, bgTop, bgBot, glow;
    bool modern = false;

    static PTZRTheme forType (int type)
    {
        PTZRTheme t;
        switch (type)
        {
            case 0:  // VINTAGE: fundo âmbar/sépia
                t.accent = juce::Colour (0xffffb347); t.accentDim = juce::Colour (0xff8a5a1e);
                t.bgTop = juce::Colour (0xff2a1a0d); t.bgBot = juce::Colour (0xff0f0905); t.glow = juce::Colour (0xffffc266);
                break;
            case 2:  // MODERN: ciano/azul gelo, visual digital
                t.accent = juce::Colour (0xff38d5ff); t.accentDim = juce::Colour (0xff146a85);
                t.bgTop = juce::Colour (0xff0c1722); t.bgBot = juce::Colour (0xff05090f); t.glow = juce::Colour (0xff38d5ff);
                t.modern = true;
                break;
            default: // CLASSIC: laranja do design original
                t.accent = juce::Colour (0xffff7a1a); t.accentDim = juce::Colour (0xff8a3a0a);
                t.bgTop = juce::Colour (0xff151414); t.bgBot = juce::Colour (0xff0a0a0a); t.glow = juce::Colour (0xffff6a00);
                break;
        }
        return t;
    }
};

//==============================================================================
class PTZRLookAndFeel : public juce::LookAndFeel_V4
{
public:
    PTZRTheme theme = PTZRTheme::forType (1);

    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
};

//==============================================================================
class PTZRSpringEditor : public juce::AudioProcessorEditor, private juce::Timer
{
public:
    explicit PTZRSpringEditor (PTZRSpringProcessor&);
    ~PTZRSpringEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

private:
    void timerCallback() override;
    void setType (int t);
    void rebuildBackground();
    void drawStatic (juce::Graphics&);
    void drawSpringWindow (juce::Graphics&);
    void drawMeters (juce::Graphics&);
    void drawTypePanel (juce::Graphics&);
    void drawValues (juce::Graphics&);
    void drawTypeIcon (juce::Graphics&, int type, juce::Rectangle<float> r, bool selected);
    void askSaveName();

    PTZRSpringProcessor& proc;
    PTZRLookAndFeel laf;
    PTZRTheme theme;
    int currentType = 1;

    juce::Slider sMix, sDecay, sTone, sPre, sDrive, sDamp, sLow;
    using SA = juce::AudioProcessorValueTreeState::SliderAttachment;
    std::unique_ptr<SA> aMix, aDecay, aTone, aPre, aDrive, aDamp, aLow;
    juce::ParameterAttachment typeAttach;

    juce::Image background;
    float scale = 1.f;
    float inDisp = 0.f, outDisp = 0.f;
    double animTime = 0.0;
    juce::String presetName = "INIT";

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PTZRSpringEditor)
};
