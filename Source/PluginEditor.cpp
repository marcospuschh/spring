#include "PluginEditor.h"

using namespace juce;

namespace
{
constexpr float kW = 1536.f, kH = 1024.f;   // coordenadas do design de referência

struct KnobInfo { const char* name; const char* lo; const char* hi; const char* id; float cx, cy; bool small; };
const KnobInfo kKnobs[7] = {
    { "MIX",       "0%",   "100%",  "mix",      211.f,  492.f, false },
    { "DECAY",     "0.5s", "10s",   "decay",    484.f,  492.f, false },
    { "TONE",      "DARK", "BRIGHT","tone",     758.f,  492.f, false },
    { "PRE-DELAY", "0ms",  "200ms", "predelay", 1043.f, 492.f, false },
    { "DRIVE",     "0",    "100%",  "drive",    1355.f, 493.f, false },
    { "DAMPING",   "0%",   "100%",  "damping",  1037.f, 803.f, true  },
    { "LOW CUT",   "20Hz", "1kHz",  "lowcut",   1340.f, 803.f, true  }
};

const Rectangle<float> kTiles[3] = { { 120.f, 754.f, 212.f, 100.f }, { 353.f, 754.f, 213.f, 100.f }, { 586.f, 754.f, 212.f, 100.f } };
const char* kTypeNames[3] = { "VINTAGE", "CLASSIC", "MODERN" };
const Rectangle<float> kPrev { 568.f, 27.f, 42.f, 45.f }, kNext { 931.f, 27.f, 42.f, 45.f },
                       kName { 618.f, 27.f, 302.f, 45.f }, kSave { 1022.f, 27.f, 100.f, 45.f };

Font makeFont (float h, bool bold = false, float kern = 0.f)
{
    Font f (FontOptions (h, bold ? Font::bold : Font::plain));
    f.setExtraKerningFactor (kern);
    return f;
}

void txt (Graphics& g, const String& s, float x, float y, float h, Colour c, bool bold = false, float kern = 0.f,
          Justification j = Justification::centred, float boxW = 420.f)
{
    g.setColour (c);
    g.setFont (makeFont (h, bold, kern));
    Rectangle<float> r (boxW, h * 1.7f);
    if (j == Justification::centred)          r.setCentre (x, y);
    else if (j == Justification::centredLeft) r.setPosition (x, y - r.getHeight() * 0.5f);
    else                                      r.setPosition (x - boxW, y - r.getHeight() * 0.5f);
    g.drawText (s, r, j, false);
}

void drawScrew (Graphics& g, float cx, float cy, float r)
{
    g.setGradientFill (ColourGradient (Colour (0xff4a4a4c), cx - r, cy - r, Colour (0xff111112), cx + r, cy + r, false));
    g.fillEllipse (cx - r, cy - r, 2 * r, 2 * r);
    g.setColour (Colour (0xff050505));
    g.drawEllipse (cx - r, cy - r, 2 * r, 2 * r, 1.2f);
    g.drawLine (cx - r * 0.55f, cy + r * 0.3f, cx + r * 0.55f, cy - r * 0.3f, 1.6f);
}

void drawMetal (Graphics& g, Rectangle<float> r, float corner = 4.f)
{
    g.setGradientFill (ColourGradient (Colour (0xff77777a), r.getX(), r.getY(), Colour (0xff18181a), r.getRight(), r.getBottom(), false));
    g.fillRoundedRectangle (r, corner);
    g.setColour (Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (r, corner, 1.5f);
    g.setColour (Colour (0x33ffffff));
    g.drawLine (r.getX() + 3, r.getY() + 2.f, r.getRight() - 3, r.getY() + 2.f, 1.f);
}

void drawWaveIcon (Graphics& g, float cx, float cy, float h, Colour c)
{
    static const float hs[9] = { 0.25f, 0.5f, 0.85f, 0.6f, 1.f, 0.7f, 0.45f, 0.65f, 0.25f };
    g.setColour (c);
    for (int i = 0; i < 9; ++i)
    {
        const float x = cx + ((float) i - 4.f) * 4.2f, hh = h * hs[i];
        g.drawLine (x, cy - hh * 0.5f, x, cy + hh * 0.5f, 1.8f);
    }
}

float dbToY (float db)
{
    static const float d[6] = { 0.f, -6.f, -12.f, -18.f, -24.f, -48.f };
    static const float y[6] = { 197.f, 225.f, 252.f, 280.f, 308.f, 336.f };
    if (db >= 0.f) return y[0];
    for (int i = 0; i < 5; ++i)
        if (db >= d[i + 1]) return y[i] + (y[i + 1] - y[i]) * (db - d[i]) / (d[i + 1] - d[i]);
    return jmin (345.f, y[5] + (-48.f - db) * 0.6f);
}
} // namespace

//==============================================================================
void PTZRLookAndFeel::drawRotarySlider (Graphics& g, int x, int y, int w, int h, float pos,
                                        float a0, float a1, Slider&)
{
    const float size = (float) jmin (w, h);
    const float cx = x + w * 0.5f, cy = y + h * 0.5f;
    const float R = size * 0.44f, body = size * 0.36f, s = body / 72.f;
    const float ang = a0 + pos * (a1 - a0);

    // trilho + arco de valor
    if (! theme.modern)
    {
        Path track; track.addCentredArc (cx, cy, R, R, 0.f, a0, a1, true);
        g.setColour (theme.accentDim.withAlpha (0.30f));
        g.strokePath (track, PathStrokeType (3.5f * s, PathStrokeType::curved, PathStrokeType::rounded));

        if (pos > 0.002f)
        {
            Path va; va.addCentredArc (cx, cy, R, R, 0.f, a0, ang, true);
            g.setColour (theme.accent.withAlpha (0.20f));
            g.strokePath (va, PathStrokeType (10.f * s, PathStrokeType::curved, PathStrokeType::rounded));
            g.setColour (theme.accent);
            g.strokePath (va, PathStrokeType (4.5f * s, PathStrokeType::curved, PathStrokeType::rounded));
        }
    }
    else    // MODERN: anel de LEDs
    {
        const int n = 31;
        for (int i = 0; i < n; ++i)
        {
            const float t = (float) i / (float) (n - 1), a = a0 + (a1 - a0) * t;
            const float px = cx + std::sin (a) * R, py = cy - std::cos (a) * R;
            const bool lit = t <= pos + 0.001f;
            if (lit) { g.setColour (theme.accent.withAlpha (0.25f)); g.fillEllipse (px - 4.f * s, py - 4.f * s, 8.f * s, 8.f * s); }
            g.setColour (lit ? theme.accent : theme.accentDim.withAlpha (0.35f));
            g.fillEllipse (px - 2.2f * s, py - 2.2f * s, 4.4f * s, 4.4f * s);
        }
    }

    // sombra + corpo
    g.setGradientFill (ColourGradient (Colours::black.withAlpha (0.75f), cx, cy + 3.f * s, Colours::transparentBlack, cx + body * 1.35f, cy + 3.f * s, true));
    g.fillEllipse (cx - body * 1.35f, cy - body * 1.35f + 3.f * s, body * 2.7f, body * 2.7f);

    g.setGradientFill (ColourGradient (Colour (0xff48484b), cx - body, cy - body, Colour (0xff0b0b0c), cx + body, cy + body, false));
    g.fillEllipse (cx - body - 3.f * s, cy - body - 3.f * s, 2.f * (body + 3.f * s), 2.f * (body + 3.f * s));

    const Colour face1 = theme.modern ? Colour (0xff3d4a55) : Colour (0xff4c4c4f);
    g.setGradientFill (ColourGradient (face1, cx - body * 0.35f, cy - body * 0.40f, Colour (0xff131314), cx + body * 0.9f, cy + body * 0.2f, true));
    g.fillEllipse (cx - body, cy - body, 2.f * body, 2.f * body);

    g.setColour (Colours::white.withAlpha (0.035f));
    for (float r = body * 0.2f; r < body; r += body * 0.06f)
        g.drawEllipse (cx - r, cy - r, 2.f * r, 2.f * r, 0.9f);

    Path sheen; sheen.addCentredArc (cx, cy, body * 0.86f, body * 0.86f, 0.f, -2.5f, -1.0f, true);
    g.setColour (Colours::white.withAlpha (0.10f));
    g.strokePath (sheen, PathStrokeType (3.f * s, PathStrokeType::curved, PathStrokeType::rounded));

    // ponteiro
    const float dx = std::sin (ang), dy = -std::cos (ang);
    const Point<float> p1 (cx + dx * body * 0.20f, cy + dy * body * 0.20f), p2 (cx + dx * body * 0.68f, cy + dy * body * 0.68f);
    const float pw = theme.modern ? 3.2f * s : 5.f * s;
    g.setColour (theme.accent.withAlpha (0.25f));
    g.drawLine ({ p1, p2 }, pw * 2.2f);
    g.setColour (theme.accent);
    g.drawLine ({ p1, p2 }, pw);
    g.setColour (Colours::white.withAlpha (0.55f));
    g.drawLine ({ p1, p2 }, pw * 0.35f);
}

//==============================================================================
PTZRSpringEditor::PTZRSpringEditor (PTZRSpringProcessor& p)
    : AudioProcessorEditor (&p), proc (p),
      typeAttach (*p.apvts.getParameter ("type"), [this] (float v) { setType (roundToInt (v)); })
{
    auto setup = [this] (Slider& s, const char* id, std::unique_ptr<SA>& att, double def)
    {
        s.setSliderStyle (Slider::RotaryVerticalDrag);
        s.setTextBoxStyle (Slider::NoTextBox, false, 0, 0);
        s.setRotaryParameters (MathConstants<float>::pi * 1.25f, MathConstants<float>::pi * 2.75f, true);
        s.setMouseDragSensitivity (220);
        s.setLookAndFeel (&laf);
        addAndMakeVisible (s);
        att = std::make_unique<SA> (proc.apvts, id, s);
        s.setDoubleClickReturnValue (true, def);
    };
    setup (sMix, "mix", aMix, 35.0);      setup (sDecay, "decay", aDecay, 2.8);
    setup (sTone, "tone", aTone, 50.0);   setup (sPre, "predelay", aPre, 25.0);
    setup (sDrive, "drive", aDrive, 30.0); setup (sDamp, "damping", aDamp, 60.0);
    setup (sLow, "lowcut", aLow, 120.0);

    setResizable (true, true);
    getConstrainer()->setFixedAspectRatio (1.5);
    setResizeLimits (720, 480, 1536, 1024);
    setSize (960, 640);

    typeAttach.sendInitialUpdate();
    presetName = proc.getPresetName();
    startTimerHz (30);
}

PTZRSpringEditor::~PTZRSpringEditor()
{
    stopTimer();
    for (auto* s : { &sMix, &sDecay, &sTone, &sPre, &sDrive, &sDamp, &sLow }) s->setLookAndFeel (nullptr);
}

void PTZRSpringEditor::setType (int t)
{
    currentType = jlimit (0, 2, t);
    theme = PTZRTheme::forType (currentType);
    laf.theme = theme;
    rebuildBackground();
    repaint();
}

void PTZRSpringEditor::resized()
{
    scale = (float) getWidth() / kW;
    Slider* sl[7] = { &sMix, &sDecay, &sTone, &sPre, &sDrive, &sDamp, &sLow };
    for (int i = 0; i < 7; ++i)
    {
        const float half = kKnobs[i].small ? 70.f : 100.f;
        sl[i]->setBounds (Rectangle<float> ((kKnobs[i].cx - half) * scale, (kKnobs[i].cy - half) * scale,
                                            2.f * half * scale, 2.f * half * scale).toNearestInt());
    }
    rebuildBackground();
}

void PTZRSpringEditor::timerCallback()
{
    animTime = Time::getMillisecondCounterHiRes() * 0.001;
    inDisp  = jmax (proc.inPeak.exchange (0.f),  inDisp  * 0.86f);
    outDisp = jmax (proc.outPeak.exchange (0.f), outDisp * 0.86f);
    presetName = proc.getPresetName();
    repaint();
}

void PTZRSpringEditor::mouseDown (const MouseEvent& e)
{
    const auto p = e.position / scale;
    if (kPrev.contains (p))      { proc.loadPresetRelative (-1); return; }
    if (kNext.contains (p))      { proc.loadPresetRelative (+1); return; }
    if (kSave.contains (p))      { askSaveName(); return; }
    for (int t = 0; t < 3; ++t)
        if (kTiles[t].contains (p)) { typeAttach.setValueAsCompleteGesture ((float) t); return; }
}

void PTZRSpringEditor::askSaveName()
{
    auto* w = new AlertWindow ("Salvar preset", "Nome do preset:", MessageBoxIconType::NoIcon, this);
    w->addTextEditor ("name", presetName == "INIT" ? String() : presetName);
    w->addButton ("Salvar", 1, KeyPress (KeyPress::returnKey));
    w->addButton ("Cancelar", 0, KeyPress (KeyPress::escapeKey));
    Component::SafePointer<PTZRSpringEditor> safe (this);
    w->enterModalState (true, ModalCallbackFunction::create ([safe, w] (int r)
    {
        if (r == 1 && safe != nullptr)
            safe->proc.saveUserPreset (w->getTextEditorContents ("name"));
    }), true);
}

void PTZRSpringEditor::rebuildBackground()
{
    const int w = getWidth(), h = getHeight();
    if (w <= 0 || h <= 0) return;
    float px = 1.f;
    if (auto* d = Desktop::getInstance().getDisplays().getDisplayForRect (getScreenBounds()))
        px = (float) d->scale;
    background = Image (Image::ARGB, roundToInt (w * px), roundToInt (h * px), true);
    Graphics g (background);
    g.addTransform (AffineTransform::scale (px * scale));
    drawStatic (g);
}

//==============================================================================
void PTZRSpringEditor::paint (Graphics& g)
{
    if (background.isValid())
        g.drawImage (background, 0, 0, getWidth(), getHeight(), 0, 0, background.getWidth(), background.getHeight());

    g.addTransform (AffineTransform::scale (scale));
    drawSpringWindow (g);
    drawMeters (g);
    drawTypePanel (g);
    drawValues (g);
}

//==============================================================================
void PTZRSpringEditor::drawStatic (Graphics& g)
{
    // fundo (muda de cor com o tipo de mola)
    g.setGradientFill (ColourGradient (theme.bgTop, 0.f, 0.f, theme.bgBot, 0.f, kH, false));
    g.fillRect (0.f, 0.f, kW, kH);

    const Rectangle<float> plate (15.f, 8.f, 1507.f, 997.f);
    g.setGradientFill (ColourGradient (theme.bgTop.brighter (0.12f), 0.f, 8.f, theme.bgBot, 0.f, 1005.f, false));
    g.fillRoundedRectangle (plate, 22.f);
    g.setGradientFill (ColourGradient (Colours::transparentBlack, 768.f, 512.f, Colours::black.withAlpha (0.55f), 0.f, 0.f, true));
    g.fillRoundedRectangle (plate, 22.f);
    g.setColour (Colour (0xff2c2c2e));
    g.drawRoundedRectangle (plate, 22.f, 2.f);
    for (auto pt : { Point<float> (38, 26), { 1500, 26 }, { 38, 986 }, { 1500, 986 } }) drawScrew (g, pt.x, pt.y, 9.f);

    // cabeçalho
    txt (g, "PTZR", 95.f, 46.f, 40.f, Colour (0xfff2f2f2), false, 0.45f, Justification::centredLeft, 200.f);
    txt (g, "®", 262.f, 36.f, 14.f, Colour (0xffdddddd), false, 0.f, Justification::centredLeft, 30.f);
    txt (g, "SPRING REVERB", 95.f, 75.f, 17.f, Colour (0xffd8d8d8), false, 0.42f, Justification::centredLeft, 260.f);
    g.setColour (Colour (0xff444446));
    g.drawLine (297.f, 33.f, 297.f, 66.f, 1.5f);
    txt (g, "PLUGINS", 312.f, 47.f, 30.f, Colour (0xff8c8c8e), false, 0.55f, Justification::centredLeft, 260.f);

    auto button = [&] (Rectangle<float> r)
    {
        g.setGradientFill (ColourGradient (Colour (0xff1e1e20), r.getX(), r.getY(), Colour (0xff111112), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.f);
        g.setColour (Colour (0xff3a3a3d));
        g.drawRoundedRectangle (r, 6.f, 1.5f);
    };
    button (kPrev); button (kNext); button (kSave);
    g.setColour (Colour (0xff060606)); g.fillRoundedRectangle (kName, 7.f);
    g.setColour (Colour (0xff343437)); g.drawRoundedRectangle (kName, 7.f, 1.5f);
    g.setColour (Colour (0xff9a9a9c));
    Path tl; tl.addTriangle (kPrev.getCentreX() + 8.f, kPrev.getCentreY() - 11.f, kPrev.getCentreX() + 8.f, kPrev.getCentreY() + 11.f, kPrev.getCentreX() - 10.f, kPrev.getCentreY());
    g.fillPath (tl);
    Path tr; tr.addTriangle (kNext.getCentreX() - 8.f, kNext.getCentreY() - 11.f, kNext.getCentreX() - 8.f, kNext.getCentreY() + 11.f, kNext.getCentreX() + 10.f, kNext.getCentreY());
    g.fillPath (tr);
    txt (g, "SAVE", kSave.getCentreX(), kSave.getCentreY(), 16.f, Colour (0xffb0b0b2), false, 0.3f);

    txt (g, "ptzr.plugins", 1270.f, 50.f, 20.f, Colour (0xff8c8c8e), false, 0.12f, Justification::centredLeft, 160.f);
    txt (g, "®", 1393.f, 42.f, 11.f, Colour (0xff8c8c8e), false, 0.f, Justification::centredLeft, 20.f);
    g.setColour (Colour (0xff3e3e40)); g.drawLine (1417.f, 36.f, 1417.f, 62.f, 1.5f);
    drawWaveIcon (g, 1463.f, 50.f, 34.f, Colour (0xffbdbdbf));

    // janela da mola (moldura; o interior é animado)
    const Rectangle<float> win (160.f, 112.f, 1218.f, 256.f);
    g.setGradientFill (ColourGradient (Colour (0xff2a2a2c), win.getX(), win.getY(), Colour (0xff0c0c0d), win.getX(), win.getBottom(), false));
    g.fillRoundedRectangle (win, 8.f);
    g.setColour (Colour (0xff050505)); g.fillRoundedRectangle (win.reduced (5.f), 5.f);
    g.setColour (Colour (0xff3b3b3e)); g.drawRoundedRectangle (win, 8.f, 1.6f);
    for (auto pt : { Point<float> (182, 132), { 1356, 132 }, { 182, 348 }, { 1356, 348 } }) drawScrew (g, pt.x, pt.y, 5.f);

    // painéis de medidores
    for (auto r : { Rectangle<float> (40.f, 105.f, 108.f, 262.f), Rectangle<float> (1380.f, 105.f, 120.f, 262.f) })
    {
        g.setColour (Colour (0xff0b0b0c)); g.fillRoundedRectangle (r, 8.f);
        g.setColour (Colour (0xff262628)); g.drawRoundedRectangle (r, 8.f, 1.5f);
    }
    txt (g, "IN", 77.f, 171.f, 15.f, Colour (0xffd0d0d0), true, 0.15f);
    txt (g, "OUT", 1436.f, 171.f, 15.f, Colour (0xffd0d0d0), true, 0.15f);
    const char* lab[6] = { "0", "-6", "-12", "-18", "-24", "-48" };
    const float ly[6] = { 197.f, 225.f, 252.f, 280.f, 308.f, 336.f };
    for (int i = 0; i < 6; ++i)
    {
        txt (g, lab[i], 118.f, ly[i], 14.f, Colour (0xff8e8e90), false, 0.f, Justification::centredRight, 60.f);
        txt (g, lab[i], 1447.f, ly[i], 14.f, Colour (0xff8e8e90), false, 0.f, Justification::centredLeft, 60.f);
    }

    // painel dos knobs principais
    const Rectangle<float> kp (40.f, 383.f, 1460.f, 272.f);
    g.setColour (Colour (0xff0b0b0c)); g.fillRoundedRectangle (kp, 10.f);
    g.setColour (Colour (0xff262628)); g.drawRoundedRectangle (kp, 10.f, 1.5f);
    g.drawLine (1222.f, 385.f, 1222.f, 653.f, 1.5f);

    // painéis inferiores
    const Rectangle<float> sp (58.f, 692.f, 799.f, 220.f);
    g.setColour (Colour (0xff0a0a0b)); g.fillRoundedRectangle (sp, 10.f);
    g.setColour (theme.accent.withAlpha (0.25f)); g.drawRoundedRectangle (sp.expanded (2.f), 11.f, 5.f);
    g.setColour (theme.accent); g.drawRoundedRectangle (sp, 10.f, 2.f);
    for (auto pt : { Point<float> (77, 711), { 838, 711 }, { 77, 893 }, { 838, 893 } }) drawScrew (g, pt.x, pt.y, 6.f);
    txt (g, "SPRING TYPE", 459.f, 719.f, 19.f, Colour (0xffc8c8ca), false, 0.38f);
    g.setColour (Colour (0xff3a3a3c)); g.drawLine (127.f, 736.f, 790.f, 736.f, 1.2f);
    g.drawLine (343.f, 762.f, 343.f, 848.f, 1.2f);
    g.drawLine (576.f, 762.f, 576.f, 848.f, 1.2f);

    for (auto r : { Rectangle<float> (872.f, 690.f, 310.f, 228.f), Rectangle<float> (1184.f, 690.f, 303.f, 228.f) })
    {
        g.setColour (Colour (0xff0b0b0c)); g.fillRoundedRectangle (r, 10.f);
        g.setColour (Colour (0xff262628)); g.drawRoundedRectangle (r, 10.f, 1.5f);
    }

    // rótulos dos knobs
    for (auto& k : kKnobs)
    {
        const float lo = k.small ? 70.f : 94.f, ty = k.small ? 57.f : 70.f;
        txt (g, k.lo, k.cx - lo, k.cy + ty, 15.f, Colour (0xffb4b4b6), false, 0.05f);
        txt (g, k.hi, k.cx + lo - 2.f, k.cy + ty, 15.f, Colour (0xffb4b4b6), false, 0.05f);
        if (k.small) txt (g, k.name, k.cx, k.cy - 84.f, 18.f, Colour (0xffe6e6e8), false, 0.18f);
        else         txt (g, k.name, k.cx, k.cy + 99.f, 19.f, Colour (0xffe6e6e8), false, 0.20f);
    }

    // rodapé
    txt (g, "PTZR", 66.f, 972.f, 19.f, Colour (0xffcfcfd1), false, 0.4f, Justification::centredLeft, 100.f);
    txt (g, "®", 139.f, 966.f, 9.f, Colour (0xffcfcfd1), false, 0.f, Justification::centredLeft, 20.f);
    g.setColour (Colour (0xff4a4a4c)); g.drawLine (160.f, 972.f, 252.f, 972.f, 1.4f);
    txt (g, "ptzr.plugins", 1380.f, 973.f, 15.f, Colour (0xff77777a), false, 0.1f, Justification::centredRight, 140.f);
    g.setColour (Colour (0xff3e3e40)); g.drawLine (1387.f, 962.f, 1387.f, 984.f, 1.2f);
    drawWaveIcon (g, 1411.f, 973.f, 24.f, Colour (0xff8e8e90));
    txt (g, "v1.0.0", 1443.f, 973.f, 15.f, Colour (0xff77777a), false, 0.05f, Justification::centredLeft, 80.f);
}

//==============================================================================
void PTZRSpringEditor::drawSpringWindow (Graphics& g)
{
    const Rectangle<float> win (166.f, 119.f, 1206.f, 242.f);
    g.saveState();
    g.reduceClipRegion (win.toNearestInt());

    g.setGradientFill (ColourGradient (Colour (0xff060606), 0.f, win.getY(), Colour (0xff0d0b0a), 0.f, win.getBottom(), false));
    g.fillRect (win);

    const float lvl = jlimit (0.f, 1.f, outDisp * 1.6f);
    const float glowA = 0.38f + 0.45f * lvl;
    auto glow = [&] (float cx, float cy, float r, float a)
    {
        g.setGradientFill (ColourGradient (theme.glow.withAlpha (jlimit (0.f, 1.f, a)), cx, cy, theme.glow.withAlpha (0.f), cx + r, cy, true));
        g.fillRect (win);
    };
    glow (440.f, 132.f, 340.f, glowA);
    glow (1010.f, 138.f, 300.f, glowA * 0.45f);
    glow (760.f, 340.f, 420.f, glowA * 0.22f);

    const float t = (float) animTime;
    const float cy = 250.f;

    // trilhos
    g.setColour (Colour (0xff222224)); g.fillRect (270.f, 176.f, 992.f, 6.f); g.fillRect (270.f, 318.f, 992.f, 6.f);
    g.setColour (Colour (0x30ffffff)); g.fillRect (270.f, 176.f, 992.f, 1.f); g.fillRect (270.f, 318.f, 992.f, 1.f);

    if (! theme.modern)
    {
        const bool vint = currentType == 0;
        const float xs = 275.f, xe = 1245.f;
        const int N = vint ? 92 : 66;
        const float pitch = (xe - xs) / (float) N, ry = vint ? 40.f : 46.f;

        for (int k = 0; k < N; ++k)
        {
            const float x = xs + ((float) k + 0.5f) * pitch;
            const float wob = std::sin (t * 14.f - (float) k * 0.30f) * lvl * 0.10f;
            const float r = ry * (1.f + wob);
            const float f = (x - xs) / (xe - xs);
            const float heat = std::exp (-std::pow ((x - 470.f) / 330.f, 2.f)) * (0.55f + 0.45f * lvl);
            const Colour steel = Colour (0xff7e7e82).interpolatedWith (Colour (0xff38383b), f);
            const Colour col = steel.interpolatedWith (theme.accent, jlimit (0.f, 1.f, heat));

            Path p;
            p.addEllipse (-pitch * 0.55f, -r, pitch * 1.1f, 2.f * r);
            p.applyTransform (AffineTransform::rotation (-0.22f).translated (x, cy));
            g.setColour (Colours::black.withAlpha (0.6f));
            g.strokePath (p, PathStrokeType (vint ? 3.8f : 4.6f));
            g.setColour (col);
            g.strokePath (p, PathStrokeType (vint ? 2.4f : 3.2f));
            g.setColour (col.brighter (0.6f).withAlpha (0.45f));
            g.strokePath (p, PathStrokeType (0.9f));
        }
    }
    else    // MODERN: grade digital + fitas de onda
    {
        g.setColour (theme.accent.withAlpha (0.06f));
        for (float y = win.getY() + 20.f; y < win.getBottom(); y += 30.f) g.drawLine (win.getX(), y, win.getRight(), y, 1.f);
        for (float x = win.getX() + 20.f; x < win.getRight(); x += 60.f) g.drawLine (x, win.getY(), x, win.getBottom(), 1.f);

        // lâmina plana (mola moderna)
        Path blade;
        blade.startNewSubPath (285.f, cy - 7.f); blade.lineTo (1240.f, cy - 3.f); blade.lineTo (1240.f, cy + 3.f); blade.lineTo (285.f, cy + 7.f); blade.closeSubPath();
        g.setGradientFill (ColourGradient (Colour (0xffc9d6de), 0.f, cy - 7.f, Colour (0xff4b5a64), 0.f, cy + 7.f, false));
        g.fillPath (blade);

        for (int r = 0; r < 3; ++r)
        {
            Path p;
            const float amp = 18.f + lvl * 62.f + 6.f * (float) r;
            bool first = true;
            for (float x = 170.f; x <= 1370.f; x += 5.f)
            {
                const float u = (x - 170.f) / 1200.f, env = std::sin (u * MathConstants<float>::pi);
                const float y = cy + amp * env * std::sin (MathConstants<float>::twoPi * x / (240.f + 70.f * (float) r) + t * (1.7f + 0.8f * (float) r) + (float) r * 1.3f);
                if (first) { p.startNewSubPath (x, y); first = false; } else p.lineTo (x, y);
            }
            g.setColour (theme.accent.withAlpha (0.16f)); g.strokePath (p, PathStrokeType (7.f - (float) r));
            g.setColour (theme.accent.withAlpha (0.95f - 0.28f * (float) r)); g.strokePath (p, PathStrokeType (2.4f - 0.5f * (float) r));
        }
    }

    // suportes metálicos
    drawMetal (g, { 205.f, 168.f, 66.f, 164.f });
    drawMetal (g, { 1262.f, 168.f, 74.f, 164.f });
    drawMetal (g, { 735.f, 178.f, 60.f, 144.f });
    drawScrew (g, 765.f, 250.f, 9.f);
    drawScrew (g, 236.f, 190.f, 5.f); drawScrew (g, 236.f, 310.f, 5.f);
    drawScrew (g, 1298.f, 190.f, 5.f); drawScrew (g, 1298.f, 310.f, 5.f);

    g.restoreState();
}

void PTZRSpringEditor::drawMeters (Graphics& g)
{
    auto meter = [&] (float x, float level)
    {
        const float db = level > 1.0e-5f ? Decibels::gainToDecibels (level) : -100.f;
        const float yLvl = dbToY (db);
        for (float yy = 341.f; yy > 192.f; yy -= 5.5f)
        {
            const bool lit = yy >= yLvl;
            const float hot = jlimit (0.f, 1.f, (341.f - yy) / 150.f);
            g.setColour (lit ? theme.accent.interpolatedWith (Colours::white, hot * 0.35f) : theme.accent.withAlpha (0.10f));
            g.fillRect (x, yy - 3.4f, 14.f, 3.4f);
        }
    };
    meter (67.f, inDisp);
    meter (1418.f, outDisp);
}

void PTZRSpringEditor::drawTypeIcon (Graphics& g, int type, Rectangle<float> r, bool selected)
{
    const float cy = r.getCentreY();
    const Colour hi = selected ? theme.accent : Colour (0xff8a8a8e);
    const Colour lo = selected ? theme.accentDim : Colour (0xff3e3e41);

    if (type == 2)   // MODERN: lâmina plana
    {
        Path b;
        b.startNewSubPath (r.getX() + 34.f, cy - 12.f); b.lineTo (r.getRight() - 20.f, cy - 5.f);
        b.lineTo (r.getRight() - 20.f, cy + 5.f);       b.lineTo (r.getX() + 34.f, cy + 12.f); b.closeSubPath();
        g.setGradientFill (ColourGradient (hi.brighter (0.4f), 0.f, cy - 12.f, lo, 0.f, cy + 12.f, false));
        g.fillPath (b);
        drawMetal (g, { r.getX() + 16.f, cy - 22.f, 20.f, 44.f }, 3.f);
    }
    else
    {
        const bool vint = type == 0;
        const int n = vint ? 30 : 20;
        const float x0 = r.getX() + 34.f, x1 = r.getRight() - 30.f, pitch = (x1 - x0) / (float) n;
        const float ry = vint ? 20.f : 26.f;
        drawMetal (g, { r.getX() + 14.f, cy - 30.f, 22.f, 60.f }, 3.f);
        drawMetal (g, { r.getRight() - 34.f, cy - 30.f, 22.f, 60.f }, 3.f);
        for (int k = 0; k < n; ++k)
        {
            const float x = x0 + ((float) k + 0.5f) * pitch;
            Path p; p.addEllipse (-pitch * 0.55f, -ry, pitch * 1.1f, 2.f * ry);
            p.applyTransform (AffineTransform::rotation (-0.22f).translated (x, cy));
            g.setColour (Colours::black.withAlpha (0.6f));  g.strokePath (p, PathStrokeType (vint ? 2.6f : 3.4f));
            g.setColour (k < n / 2 ? hi : hi.interpolatedWith (lo, 0.6f)); g.strokePath (p, PathStrokeType (vint ? 1.5f : 2.2f));
        }
    }
}

void PTZRSpringEditor::drawTypePanel (Graphics& g)
{
    for (int i = 0; i < 3; ++i)
    {
        const bool sel = i == currentType;
        const auto r = kTiles[i];
        g.setGradientFill (ColourGradient (Colour (0xff141415), r.getX(), r.getY(), Colour (0xff080809), r.getX(), r.getBottom(), false));
        g.fillRoundedRectangle (r, 6.f);
        if (sel)
        {
            g.setColour (theme.accent.withAlpha (0.22f)); g.drawRoundedRectangle (r.expanded (2.f), 7.f, 6.f);
            g.setColour (theme.accent);                   g.drawRoundedRectangle (r, 6.f, 2.2f);
        }
        drawTypeIcon (g, i, r, sel);
        txt (g, kTypeNames[i], r.getCentreX(), 868.f, 17.f, sel ? Colour (0xfff2ece6) : Colour (0xff8e8e90), false, 0.28f);
        if (sel)
        {
            g.setColour (theme.accent.withAlpha (0.3f)); g.fillEllipse (r.getCentreX() - 9.f, 888.f, 18.f, 18.f);
            g.setColour (theme.accent);                  g.fillEllipse (r.getCentreX() - 4.5f, 892.5f, 9.f, 9.f);
        }
    }
}

void PTZRSpringEditor::drawValues (Graphics& g)
{
    for (auto& k : kKnobs)
        if (auto* prm = proc.apvts.getParameter (k.id))
            txt (g, prm->getCurrentValueAsText(), k.cx, k.cy + (k.small ? 89.f : 132.f), 21.f, theme.accent, true, 0.05f);

    txt (g, presetName, kName.getCentreX(), kName.getCentreY(), 19.f, Colour (0xffe8e8ea), true, 0.18f, Justification::centred, 280.f);
}
