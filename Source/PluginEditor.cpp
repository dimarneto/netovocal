#include "PluginEditor.h"
#include <cmath>

namespace
{
    const juce::Colour kBg    (0xff03060d);
    const juce::Colour kBlue  (0xff1a8cff);
    const juce::Colour kCyan  (0xff5fe0ff);
    const juce::Colour kMag   (0xffff4fd8);
    const juce::Colour kGold  (0xffffc857);
    const juce::Colour kGreen (0xff1ed6a0);
    const juce::Colour kText  (0xffeaf1fb);
    const juce::Colour kMuted (0xff8fa6c2);
    const float kPi = juce::MathConstants<float>::pi;

    juce::String T (const char* s) { return juce::String::fromUTF8 (s); }

    juce::Font F (float size, bool bold = false)
    {
        return juce::Font (juce::FontOptions (size, bold ? juce::Font::bold : juce::Font::plain));
    }

    // Fundo com "aurora" azul que se move devagar e acende com o nivel do audio
    void paintAurora (juce::Graphics& g, juce::Rectangle<float> r, float phase, float level)
    {
        g.fillAll (kBg);
        const float w = r.getWidth(), h = r.getHeight(), k = 0.75f + 0.5f * level;

        auto blob = [&] (float cx, float cy, float rad, juce::Colour c, float a)
        {
            g.setGradientFill (juce::ColourGradient (c.withAlpha (a), cx, cy, c.withAlpha (0.0f), cx + rad, cy, true));
            g.fillEllipse (cx - rad, cy - rad, rad * 2.0f, rad * 2.0f);
        };

        blob (w * 0.50f + 90.0f * std::sin (phase * 0.7f), h * 0.36f + 30.0f * std::cos (phase * 0.5f), 420.0f, kBlue, 0.34f * k);
        blob (w * 0.10f + 70.0f * std::cos (phase * 0.4f), h * 0.80f, 320.0f, kCyan, 0.14f * k);
        blob (w * 0.92f + 60.0f * std::sin (phase * 0.55f), h * 0.10f, 340.0f, juce::Colour (0xff7a5cff), 0.17f * k);
        blob (w * 0.78f, h * 0.92f + 20.0f * std::sin (phase * 0.6f), 280.0f, kMag, 0.07f * k);
    }

    void drawPanel (juce::Graphics& g, juce::Rectangle<float> r, juce::Colour glow)
    {
        g.setColour (glow.withAlpha (0.07f));
        g.fillRoundedRectangle (r.expanded (2.0f), 16.0f);
        g.setColour (juce::Colour (0xff08111d).withAlpha (0.80f));
        g.fillRoundedRectangle (r, 14.0f);
        g.setGradientFill (juce::ColourGradient (glow.withAlpha (0.60f), r.getX(), r.getY(),
                                                 glow.withAlpha (0.10f), r.getRight(), r.getBottom(), false));
        g.drawRoundedRectangle (r.reduced (0.5f), 14.0f, 1.0f);
    }
}

//==============================================================================
NetoLookAndFeel::NetoLookAndFeel()
{
    setColour (juce::Slider::textBoxTextColourId,       kText);
    setColour (juce::Slider::textBoxOutlineColourId,    juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Label::textColourId,               kText);

    setColour (juce::ComboBox::backgroundColourId,      juce::Colour (0xff0a1424));
    setColour (juce::ComboBox::textColourId,            kText);
    setColour (juce::ComboBox::outlineColourId,         kBlue.withAlpha (0.55f));
    setColour (juce::ComboBox::arrowColourId,           kCyan);
    setColour (juce::PopupMenu::backgroundColourId,     juce::Colour (0xff0a1424));
    setColour (juce::PopupMenu::textColourId,           kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kBlue.withAlpha (0.55f));
    setColour (juce::PopupMenu::highlightedTextColourId, juce::Colours::white);
}

void NetoLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float pos, float a0, float a1, juce::Slider&)
{
    auto b = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f;
    const auto c = b.getCentre();
    const float lw = juce::jmax (3.0f, R * 0.11f);
    const float arcR = R - lw * 1.2f;
    const float ang = a0 + pos * (a1 - a0);

    // brilho de fundo que cresce com o valor
    g.setGradientFill (juce::ColourGradient (kBlue.withAlpha (0.08f + 0.26f * pos), c.x, c.y,
                                             kBlue.withAlpha (0.0f), c.x + R, c.y, true));
    g.fillEllipse (c.x - R, c.y - R, R * 2.0f, R * 2.0f);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    g.setColour (juce::Colour (0xff10213a));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    juce::Path value;
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, ang, true);
    g.setColour (kBlue.withAlpha (0.25f));
    g.strokePath (value, juce::PathStrokeType (lw * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (kBlue, c.x - arcR, c.y + arcR, kCyan, c.x + arcR, c.y - arcR, false));
    g.strokePath (value, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float tx = c.x + arcR * std::sin (ang);
    const float ty = c.y - arcR * std::cos (ang);
    g.setColour (kCyan.withAlpha (0.35f));
    g.fillEllipse (tx - lw * 1.1f, ty - lw * 1.1f, lw * 2.2f, lw * 2.2f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (tx - lw * 0.55f, ty - lw * 0.55f, lw * 1.1f, lw * 1.1f);

    const float kr = arcR - lw * 1.5f;
    g.setGradientFill (juce::ColourGradient (juce::Colour (0xff3b4656), c.x - kr * 0.4f, c.y - kr * 0.6f,
                                             juce::Colour (0xff070a10), c.x + kr * 0.5f, c.y + kr, true));
    g.fillEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f);
    g.setColour (juce::Colour (0xff46566c));
    g.drawEllipse (c.x - kr, c.y - kr, kr * 2.0f, kr * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.6f, -kr + 4.0f, 3.2f, kr * 0.38f, 1.6f);
    g.setColour (kCyan);
    g.fillPath (pointer, juce::AffineTransform::rotation (ang).translated (c.x, c.y));
}

void NetoLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& btn, const juce::Colour& bg,
                                            bool over, bool down)
{
    auto r = btn.getLocalBounds().toFloat().reduced (2.0f);
    const bool on = btn.getToggleState();
    const float en = btn.isEnabled() ? 1.0f : 0.35f;

    if (on)
    {
        g.setColour (bg.withAlpha (0.20f));
        g.fillRoundedRectangle (r.expanded (2.0f), 11.0f);
    }

    g.setGradientFill (juce::ColourGradient (bg.withAlpha (((on ? 0.60f : 0.24f) + (over ? 0.12f : 0.0f)) * en), r.getX(), r.getY(),
                                             bg.withAlpha ((on ? 0.30f : 0.08f) * en), r.getX(), r.getBottom(), false));
    g.fillRoundedRectangle (r, 9.0f);

    g.setColour (bg.withAlpha ((on || over ? 1.0f : 0.55f) * en));
    g.drawRoundedRectangle (r, 9.0f, down ? 2.2f : 1.2f);
}

void NetoLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float, float, juce::Slider::SliderStyle, juce::Slider&)
{
    const float cy = (float) y + (float) height * 0.5f;
    juce::Rectangle<float> trk ((float) x + 2.0f, cy - 3.0f, (float) width - 4.0f, 6.0f);

    g.setColour (juce::Colour (0xff0e1d33));
    g.fillRoundedRectangle (trk, 3.0f);

    g.setGradientFill (juce::ColourGradient (kBlue, trk.getX(), cy, kCyan, trk.getRight(), cy, false));
    g.fillRoundedRectangle (trk.withRight (sliderPos), 3.0f);

    g.setColour (kCyan.withAlpha (0.28f));
    g.fillEllipse (sliderPos - 11.0f, cy - 11.0f, 22.0f, 22.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (sliderPos - 6.0f, cy - 6.0f, 12.0f, 12.0f);
}

void NetoLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool over, bool)
{
    juce::ignoreUnused (over);
    auto r = b.getLocalBounds().toFloat();
    juce::Rectangle<float> sw (r.getRight() - 46.0f, r.getCentreY() - 12.0f, 44.0f, 24.0f);
    const bool on = b.getToggleState();

    if (on)
    {
        g.setColour (kBlue.withAlpha (0.25f));
        g.fillRoundedRectangle (sw.expanded (3.0f), 15.0f);
    }
    g.setColour (on ? kBlue : juce::Colour (0xff1a2740));
    g.fillRoundedRectangle (sw, 12.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (on ? sw.getRight() - 21.0f : sw.getX() + 3.0f, sw.getY() + 3.0f, 18.0f, 18.0f);

    g.setColour (kText);
    g.setFont (13.0f);
    g.drawText (b.getButtonText(), r.withTrimmedRight (54.0f).toNearestInt(), juce::Justification::centredLeft);
}

//==============================================================================
OrbSlider::OrbSlider()
{
    setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    setRotaryParameters (kPi * 1.25f, kPi * 2.75f, true);
    setMouseDragSensitivity (220);
}

void OrbSlider::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();
    const auto c = b.getCentre();
    const float R = juce::jmin (b.getWidth(), b.getHeight()) * 0.5f - 2.0f;
    const float a0 = kPi * 1.25f, a1 = kPi * 2.75f;
    const float pos = (float) valueToProportionOfLength (getValue());
    const float ang = a0 + pos * (a1 - a0);
    const float orbR = R * 0.50f * (1.0f + 0.04f * pulse);

    // brilho atras da esfera
    g.setGradientFill (juce::ColourGradient (kBlue.withAlpha (0.42f + 0.28f * pulse), c.x, c.y,
                                             kBlue.withAlpha (0.0f), c.x + orbR * 1.75f, c.y, true));
    g.fillEllipse (c.x - orbR * 1.75f, c.y - orbR * 1.75f, orbR * 3.5f, orbR * 3.5f);

    // anel de espectro (graves no topo, agudos embaixo, espelhado)
    const float r0 = R * 0.78f, maxLen = R * 0.20f;
    for (int i = 0; i < 96; ++i)
    {
        const float th = juce::MathConstants<float>::twoPi * (float) i / 96.0f;
        const float t  = std::abs ((float) i / 48.0f - 1.0f);
        const float fi = t * 47.0f;
        const int i0 = (int) fi, i1 = juce::jmin (47, i0 + 1);
        float v = juce::jmap (fi - (float) i0, halo[(size_t) i0], halo[(size_t) i1]);
        v = juce::jmax (v, 0.07f + 0.04f * std::sin (phase * 2.0f + fi * 0.5f));

        const float len = 3.0f + v * maxLen;
        const float s = std::sin (th), co = std::cos (th);
        const auto col = kCyan.interpolatedWith (kMag, t * 0.85f);

        g.setColour (col.withAlpha (0.16f));
        g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * (r0 + len), c.y - co * (r0 + len), 5.0f);
        g.setColour (col.withAlpha (0.50f + 0.50f * v));
        g.drawLine (c.x + s * r0, c.y - co * r0, c.x + s * (r0 + len), c.y - co * (r0 + len), 2.2f);
    }

    // arco do valor
    const float arcR = R * 0.67f, lw = juce::jmax (4.0f, R * 0.045f);
    juce::Path track, value;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, a1, true);
    value.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, a0, ang, true);

    g.setColour (juce::Colour (0xff0f2038));
    g.strokePath (track, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setColour (kBlue.withAlpha (0.22f));
    g.strokePath (value, juce::PathStrokeType (lw * 2.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    g.setGradientFill (juce::ColourGradient (kBlue, c.x - arcR, c.y + arcR, kCyan, c.x + arcR, c.y - arcR, false));
    g.strokePath (value, juce::PathStrokeType (lw, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    const float tx = c.x + arcR * std::sin (ang), ty = c.y - arcR * std::cos (ang);
    g.setColour (juce::Colours::white.withAlpha (0.25f));
    g.fillEllipse (tx - lw * 2.0f, ty - lw * 2.0f, lw * 4.0f, lw * 4.0f);
    g.setColour (juce::Colours::white);
    g.fillEllipse (tx - lw * 1.0f, ty - lw * 1.0f, lw * 2.0f, lw * 2.0f);

    // esfera de vidro
    const juce::Rectangle<float> orb (c.x - orbR, c.y - orbR, orbR * 2.0f, orbR * 2.0f);
    juce::ColourGradient sphere (juce::Colours::white.withAlpha (0.60f), c.x - orbR * 0.35f, c.y - orbR * 0.45f,
                                 juce::Colour (0xff020c1c), c.x + orbR * 0.55f, c.y + orbR * 0.95f, true);
    sphere.addColour (0.22, kCyan.withAlpha (0.65f));
    sphere.addColour (0.55, juce::Colour (0xff0e5ccf));
    g.setGradientFill (sphere);
    g.fillEllipse (orb);

    g.saveState();
    juce::Path clip;
    clip.addEllipse (orb);
    g.reduceClipRegion (clip);
    for (int w = 0; w < 2; ++w)
    {
        juce::Path wp;
        const float amp = orbR * (0.10f + 0.40f * pulse) * (w == 0 ? 1.0f : 0.55f);
        bool first = true;
        for (float x = orb.getX(); x <= orb.getRight(); x += 3.0f)
        {
            const float t = (x - c.x) / orbR;
            const float y = c.y - orbR * 0.10f
                          + std::sin (x * 0.045f * (w == 0 ? 1.0f : 1.6f) + phase * (w == 0 ? 2.0f : -2.2f)) * amp * (1.0f - t * t);
            if (first) { wp.startNewSubPath (x, y); first = false; }
            else         wp.lineTo (x, y);
        }
        g.setColour (juce::Colours::white.withAlpha (w == 0 ? 0.85f : 0.40f));
        g.strokePath (wp, juce::PathStrokeType (w == 0 ? 2.4f : 1.4f));
    }
    g.restoreState();

    // reflexo no topo + borda
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.50f), c.x, c.y - orbR * 0.95f,
                                             juce::Colours::white.withAlpha (0.0f), c.x, c.y - orbR * 0.20f, false));
    g.fillEllipse (c.x - orbR * 0.62f, c.y - orbR * 0.94f, orbR * 1.24f, orbR * 0.72f);
    g.setColour (juce::Colours::white.withAlpha (0.28f));
    g.drawEllipse (orb, 1.2f);

    g.setColour (juce::Colours::white);
    g.setFont (F (orbR * 0.38f, true));
    g.drawText (juce::String (juce::roundToInt ((float) getValue())) + "%",
                juce::Rectangle<float> (c.x - orbR, c.y + orbR * 0.28f, orbR * 2.0f, orbR * 0.5f).toNearestInt(),
                juce::Justification::centred);
}

//==============================================================================
void SpectrumView::paint (juce::Graphics& g)
{
    constexpr int N = VoiceProfile::numBands;
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff050b16));
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (kBlue.withAlpha (0.28f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    auto a = r.reduced (12.0f, 16.0f);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    for (int i = 0; i <= 4; ++i)
        g.drawHorizontalLine ((int) (a.getY() + a.getHeight() * (float) i / 4.0f), a.getX(), a.getRight());

    const float freqs[3] = { 100.0f, 1000.0f, 10000.0f };
    const char* names[3] = { "100", "1k", "10k" };
    g.setFont (10.0f);
    for (int i = 0; i < 3; ++i)
    {
        const float x = a.getX() + a.getWidth() * std::log (freqs[i] / 60.0f) / std::log (14000.0f / 60.0f);
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine ((int) x, a.getY(), a.getBottom());
        g.setColour (kMuted);
        g.drawText (names[i], (int) x - 20, (int) a.getBottom() + 1, 40, 12, juce::Justification::centred);
    }

    auto yFor = [&] (float db) { return a.getCentreY() - juce::jlimit (-1.1f, 1.1f, db / 22.0f) * a.getHeight() * 0.5f; };
    auto xFor = [&] (int b) { return a.getX() + a.getWidth() * (float) b / (float) (N - 1); };
    auto makePath = [&] (auto getDb)
    {
        juce::Path p;
        for (int b = 0; b < N; ++b)
        {
            if (b == 0) p.startNewSubPath (xFor (b), yFor (getDb (b)));
            else        p.lineTo (xFor (b), yFor (getDb (b)));
        }
        return p.createPathWithRoundedCorners (8.0f);
    };

    // referencia (magenta)
    const auto& ref = proc.getReference();
    if (ref.valid)
    {
        auto p = makePath ([&] (int b) { return ref.bandsDb[(size_t) b]; });
        g.setColour (kMag.withAlpha (0.20f));
        g.strokePath (p, juce::PathStrokeType (7.0f));
        g.setColour (kMag);
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

    // sua voz ao vivo (ciano)
    const auto& lb = proc.liveBuilder;
    float mean = 0.0f;
    int cnt = 0;
    for (int b = 0; b < N; ++b)
    {
        const float f = ProfileBuilder::bandCentreHz (b);
        if (f >= 300.0f && f <= 2000.0f) { mean += lb.live[(size_t) b].load(); ++cnt; }
    }
    mean /= (float) juce::jmax (1, cnt);

    const float gate = juce::jlimit (0.0f, 1.0f, (lb.levelDb.load() + 62.0f) / 16.0f);
    if (gate > 0.02f)
    {
        auto p = makePath ([&] (int b) { return lb.live[(size_t) b].load() - mean; });
        juce::Path fill (p);
        fill.lineTo (xFor (N - 1), a.getBottom());
        fill.lineTo (xFor (0), a.getBottom());
        fill.closeSubPath();
        g.setGradientFill (juce::ColourGradient (kCyan.withAlpha (0.32f * gate), 0.0f, a.getY(),
                                                 kBlue.withAlpha (0.0f), 0.0f, a.getBottom(), false));
        g.fillPath (fill);
        g.setColour (kCyan.withAlpha (0.22f * gate));
        g.strokePath (p, juce::PathStrokeType (7.0f));
        g.setColour (kCyan.withAlpha (gate));
        g.strokePath (p, juce::PathStrokeType (2.0f));
    }

    // sua voz analisada (branca, fina)
    const auto& mine = proc.getMine();
    if (mine.valid)
    {
        auto p = makePath ([&] (int b) { return mine.bandsDb[(size_t) b]; });
        g.setColour (juce::Colours::white.withAlpha (0.55f));
        g.strokePath (p, juce::PathStrokeType (1.2f));
    }

    // legenda
    g.setFont (11.0f);
    g.setColour (kCyan);
    g.fillEllipse (a.getX() + 2.0f, r.getY() + 7.0f, 7.0f, 7.0f);
    g.setColour (kMuted);
    g.drawText (T ("Sua voz"), (int) a.getX() + 14, (int) r.getY() + 4, 70, 14, juce::Justification::centredLeft);
    g.setColour (kMag);
    g.fillEllipse (a.getX() + 82.0f, r.getY() + 7.0f, 7.0f, 7.0f);
    g.setColour (kMuted);
    g.drawText (T ("Referência"), (int) a.getX() + 94, (int) r.getY() + 4, 90, 14, juce::Justification::centredLeft);

    if (! ref.valid && gate < 0.05f)
    {
        g.setColour (kMuted.withAlpha (0.8f));
        g.setFont (12.0f);
        g.drawText (T ("Carregue uma referência e toque a sua voz"), a.toNearestInt(), juce::Justification::centred);
    }
}

//==============================================================================
NetoVocalAudioProcessorEditor::NetoVocalAudioProcessorEditor (NetoVocalAudioProcessor& p)
    : AudioProcessorEditor (&p), proc (p), spectrum (p)
{
    setLookAndFeel (&laf);

    setupKnob (presence, NetoIDs::presence, T ("Presença Vocal"), T ("Mantém a voz na frente sem ficar artificial"), false);
    setupKnob (body,     NetoIDs::body,     "Corpo",               T ("Deixa a voz mais cheia e consistente"), false);
    setupKnob (clarity,  NetoIDs::clarity,  "Claridade",           T ("Dá definição sem exagero"), false);
    setupKnob (harmony,  NetoIDs::harmony,  "Harmonia com o Mix",  T ("Ajusta a voz para o ambiente da música"), true);
    setupKnob (space,    NetoIDs::space,    "Espacialidade",       T ("Amplia e posiciona a voz no palco sonoro"), true);
    setupKnob (natural,  NetoIDs::natural,  "Naturalidade",        T ("Mantém o timbre original com mais vida"), true);
    setupKnob (mix,      NetoIDs::mix,      "Mix",                 {}, false);
    mix.title.setJustificationType (juce::Justification::centred);

    addAndMakeVisible (glueOrb);
    glueOrb.setDoubleClickReturnValue (true, 72.0);
    glueAttachment = std::make_unique<SliderAttachment> (proc.apvts, NetoIDs::glue, glueOrb);

    addAndMakeVisible (glueTitle);
    glueTitle.setText ("Glue Vocal", juce::dontSendNotification);
    glueTitle.setFont (F (20.0f, true));
    glueTitle.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (glueSub);
    glueSub.setText (T ("Une sua voz ao instrumental"), juce::dontSendNotification);
    glueSub.setFont (F (13.0f));
    glueSub.setColour (juce::Label::textColourId, kMuted);
    glueSub.setJustificationType (juce::Justification::centred);

    // ---- Voice Match ----
    addAndMakeVisible (spectrum);

    auto styleBtn = [] (juce::TextButton& b, const juce::String& text, juce::Colour col)
    {
        b.setButtonText (text);
        b.setColour (juce::TextButton::buttonColourId,   col);
        b.setColour (juce::TextButton::textColourOffId,  juce::Colours::white);
        b.setColour (juce::TextButton::textColourOnId,   juce::Colours::white);
    };
    addAndMakeVisible (loadBtn);    styleBtn (loadBtn,    T ("Carregar referência"), kMag);
    addAndMakeVisible (captureBtn); styleBtn (captureBtn, T ("Analisar minha voz"),  kCyan);
    addAndMakeVisible (applyBtn);   styleBtn (applyBtn,   T ("Aplicar ao meu som"),  kGold);

    addAndMakeVisible (matchSlider);
    matchSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    matchSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    matchSlider.setDoubleClickReturnValue (true, 100.0);
    matchAttachment = std::make_unique<SliderAttachment> (proc.apvts, NetoIDs::matchAmt, matchSlider);

    addAndMakeVisible (matchLabel);
    matchLabel.setText (T ("Intensidade da cópia"), juce::dontSendNotification);
    matchLabel.setFont (F (11.0f));
    matchLabel.setColour (juce::Label::textColourId, kMuted);

    addAndMakeVisible (statusLabel);
    statusText = T ("Passo 1: carregue a voz de referência (de preferência só a voz, sem música).");
    statusLabel.setFont (F (12.0f));
    statusLabel.setColour (juce::Label::textColourId, kGold);
    statusLabel.setJustificationType (juce::Justification::centredRight);

    loadBtn.onClick = [this]
    {
        chooser = std::make_unique<juce::FileChooser> (T ("Escolha a voz de referência"), juce::File(),
                                                       "*.wav;*.mp3;*.flac;*.aif;*.aiff;*.ogg");
        juce::Component::SafePointer<NetoVocalAudioProcessorEditor> safe (this);

        chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [safe] (const juce::FileChooser& fc)
        {
            auto* ed = safe.getComponent();
            const auto f = fc.getResult();
            if (ed == nullptr || ! f.existsAsFile())
                return;

            juce::String err;
            if (ed->proc.loadReference (f, err))
                ed->statusText = T ("Referência pronta. Passo 2: toque o seu vocal e clique em Analisar.");
            else
                ed->statusText = err;
        });
    };

    captureBtn.onClick = [this]
    {
        proc.startCapture();
        statusText = T ("Escutando... toque o seu vocal (cerca de 8 s de voz).");
    };

    applyBtn.onClick = [this]
    {
        proc.applyMatch();
        statusText = T ("Cópia aplicada! Os knobs se moveram; afine à mão se quiser.");
    };
    applyBtn.setEnabled (false);

    // ---- Modos e Ajuste Fino ----
    modeParam = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (NetoIDs::mode));
    const juce::String modeNames[3] = { T ("Padrão"), T ("Suave"), T ("Intenso") };

    for (int i = 0; i < 3; ++i)
    {
        auto& btn = modeButtons[(size_t) i];
        addAndMakeVisible (btn);
        btn.setButtonText (modeNames[i]);
        btn.setColour (juce::TextButton::buttonColourId,  kBlue);
        btn.setColour (juce::TextButton::textColourOffId, kMuted);
        btn.setColour (juce::TextButton::textColourOnId,  juce::Colours::white);
        btn.onClick = [this, i]
        {
            if (modeParam != nullptr)
            {
                modeParam->beginChangeGesture();
                *modeParam = i;
                modeParam->endChangeGesture();
            }
        };
    }

    addAndMakeVisible (fineTune);
    fineTune.setButtonText ("Ajuste Fino");
    fineAttachment = std::make_unique<ButtonAttachment> (proc.apvts, NetoIDs::fine, fineTune);

    // ---- Presets ----
    addAndMakeVisible (presetBox);
    for (int i = 0; i < NetoVocalAudioProcessor::numPresets; ++i)
        presetBox.addItem (NetoVocalAudioProcessor::getPresetName (i), i + 1);
    presetBox.setJustificationType (juce::Justification::centredLeft);
    presetBox.onChange = [this]
    {
        const int idx = presetBox.getSelectedItemIndex();
        if (idx >= 0)
            proc.applyPreset (idx);
    };

    setSize (1120, 740);
    startTimerHz (30);
}

NetoVocalAudioProcessorEditor::~NetoVocalAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void NetoVocalAudioProcessorEditor::setupKnob (Knob& k, const char* paramID, const juce::String& title,
                                               const juce::String& desc, bool alignRight)
{
    addAndMakeVisible (k.slider);
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 18);
    k.slider.setRotaryParameters (kPi * 1.25f, kPi * 2.75f, true);
    k.slider.setDoubleClickReturnValue (true, 50.0);

    const auto just = alignRight ? juce::Justification::centredRight : juce::Justification::centredLeft;

    addAndMakeVisible (k.title);
    k.title.setText (title, juce::dontSendNotification);
    k.title.setFont (F (15.0f, true));
    k.title.setColour (juce::Label::textColourId, kText);
    k.title.setJustificationType (just);

    addAndMakeVisible (k.desc);
    k.desc.setText (desc, juce::dontSendNotification);
    k.desc.setFont (F (12.0f));
    k.desc.setColour (juce::Label::textColourId, kMuted);
    k.desc.setJustificationType (alignRight ? juce::Justification::topRight : juce::Justification::topLeft);

    k.attachment = std::make_unique<SliderAttachment> (proc.apvts, paramID, k.slider);
}

void NetoVocalAudioProcessorEditor::layoutKnob (Knob& k, juce::Rectangle<int> area, bool knobOnLeft)
{
    k.slider.setBounds (knobOnLeft ? area.removeFromLeft (112) : area.removeFromRight (112));
    auto tb = area.reduced (6, 0).withSizeKeepingCentre (area.getWidth() - 12, 74);
    k.title.setBounds (tb.removeFromTop (22));
    k.desc.setBounds (tb);
}

void NetoVocalAudioProcessorEditor::resized()
{
    auto r = getLocalBounds().reduced (16);
    headerB = r.removeFromTop (70).toFloat();
    {
        auto hb = headerB.toNearestInt();
        presetBox.setBounds (hb.removeFromRight (210).reduced (10, 18));
    }
    r.removeFromTop (10);
    auto bottom = r.removeFromBottom (176);
    r.removeFromBottom (10);

    auto left      = r.removeFromLeft (232);
    auto metersCol = r.removeFromRight (86);
    auto right     = r.removeFromRight (232);
    auto centre    = r;

    const int cellH = left.getHeight() / 3;
    layoutKnob (presence, left.removeFromTop (cellH), true);
    layoutKnob (body,     left.removeFromTop (cellH), true);
    layoutKnob (clarity,  left,                       true);
    layoutKnob (harmony,  right.removeFromTop (cellH), false);
    layoutKnob (space,    right.removeFromTop (cellH), false);
    layoutKnob (natural,  right,                       false);

    glueTitle.setBounds (centre.removeFromTop (26));
    glueSub.setBounds (centre.removeFromTop (20));
    const int s = juce::jmin (centre.getWidth(), centre.getHeight());
    glueOrb.setBounds (centre.withSizeKeepingCentre (s, s));

    metersB = metersCol.toFloat();
    auto m = metersCol.reduced (10, 8);
    m.removeFromTop (26);
    meterBars = m.removeFromTop (240);
    mix.title.setBounds (m.removeFromTop (20));
    mix.slider.setBounds (m.removeFromTop (112));

    voiceB = bottom.removeFromLeft (700).toFloat();
    bottom.removeFromLeft (12);
    rightB = bottom.toFloat();

    auto vm = voiceB.toNearestInt().reduced (14, 10);
    vmTitleB = vm.removeFromTop (22);
    statusLabel.setBounds (vmTitleB.withTrimmedLeft (150));
    vm.removeFromTop (4);
    spectrum.setBounds (vm.removeFromLeft (404));
    vm.removeFromLeft (14);
    loadBtn.setBounds (vm.removeFromTop (26));    vm.removeFromTop (5);
    captureBtn.setBounds (vm.removeFromTop (26)); vm.removeFromTop (5);
    applyBtn.setBounds (vm.removeFromTop (26));   vm.removeFromTop (6);
    matchLabel.setBounds (vm.removeFromTop (14));
    matchSlider.setBounds (vm.removeFromTop (24));

    auto rb = rightB.toNearestInt().reduced (14, 10);
    modeTitleB = rb.removeFromTop (18);
    rb.removeFromTop (4);
    auto modes = rb.removeFromTop (34);
    const int bw = modes.getWidth() / 3;
    for (auto& btn : modeButtons)
        btn.setBounds (modes.removeFromLeft (bw).reduced (3, 0));
    rb.removeFromTop (14);
    auto row = rb.removeFromTop (30);
    fineTune.setBounds (row.removeFromLeft (150));
    row.removeFromLeft (14);
    grBar = row.removeFromBottom (8);
}

void NetoVocalAudioProcessorEditor::paint (juce::Graphics& g)
{
    paintAurora (g, getLocalBounds().toFloat(), phase, smLevel);

    // ---- cabecalho ----
    drawPanel (g, headerB, kBlue);

    for (int i = 0; i < 5; ++i)
    {
        const float h = 10.0f + 22.0f * (0.35f + 0.65f * smLevel) * std::abs (std::sin (phase * 1.3f + (float) i * 0.9f));
        const float x = headerB.getX() + 24.0f + (float) i * 9.0f, cy = headerB.getCentreY();
        g.setColour (kBlue.withAlpha (0.25f));
        g.fillRoundedRectangle (x - 2.0f, cy - h * 0.5f - 2.0f, 8.0f, h + 4.0f, 4.0f);
        g.setGradientFill (juce::ColourGradient (kCyan, x, cy - h * 0.5f, kBlue, x, cy + h * 0.5f, false));
        g.fillRoundedRectangle (x, cy - h * 0.5f, 4.0f, h, 2.0f);
    }

    juce::AttributedString title;
    title.append ("Neto ", F (34.0f, true), kText);
    title.append ("Vocal", F (34.0f, true), kCyan);
    title.draw (g, juce::Rectangle<float> (headerB.getX() + 84.0f, headerB.getY() + 8.0f, 300.0f, 42.0f));

    g.setColour (kMuted);
    g.setFont (11.0f);
    g.drawText ("COLA SUA VOZ NO MIX   |   VST3", juce::Rectangle<int> ((int) headerB.getX() + 86, (int) headerB.getBottom() - 22, 320, 14),
                juce::Justification::centredLeft);

    g.setColour (kText);
    g.setFont (14.0f);
    g.drawFittedText (T ("Mais presença. Mais naturalidade.\nSua voz no lugar certo."),
                      juce::Rectangle<int> ((int) headerB.getRight() - 560, (int) headerB.getY() + 12, 330, 46),
                      juce::Justification::centredRight, 2);

    // ---- medidor de saida ----
    drawPanel (g, metersB, kCyan);
    g.setColour (kText);
    g.setFont (F (14.0f, true));
    g.drawText (T ("Saída"), (int) metersB.getX(), (int) metersB.getY() + 8, (int) metersB.getWidth(), 20, juce::Justification::centred);

    const float levels[2] = { meterL, meterR };
    for (int ch = 0; ch < 2; ++ch)
    {
        const float bw = 16.0f;
        juce::Rectangle<float> bar ((float) meterBars.getCentreX() - bw - 4.0f + (float) ch * (bw + 8.0f),
                                    (float) meterBars.getY(), bw, (float) meterBars.getHeight() - 18.0f);
        g.setColour (juce::Colour (0xff060d18));
        g.fillRoundedRectangle (bar, 4.0f);

        const float db   = juce::Decibels::gainToDecibels (levels[(size_t) ch], -60.0f);
        const float frac = juce::jlimit (0.0f, 1.0f, (db + 60.0f) / 60.0f);
        auto fill = bar.withTop (bar.getBottom() - bar.getHeight() * frac);

        g.setColour (kBlue.withAlpha (0.28f));
        g.fillRoundedRectangle (fill.expanded (3.0f, 0.0f), 5.0f);
        g.setGradientFill (juce::ColourGradient (kCyan, bar.getX(), bar.getY(), kGreen, bar.getX(), bar.getBottom(), false));
        g.fillRoundedRectangle (fill, 4.0f);

        g.setColour (kMuted);
        g.setFont (11.0f);
        g.drawText (ch == 0 ? "L" : "R", (int) bar.getX() - 4, (int) bar.getBottom() + 2, (int) bar.getWidth() + 8, 14, juce::Justification::centred);
    }

    // ---- Voice Match ----
    drawPanel (g, voiceB, kMag);
    g.setColour (kText);
    g.setFont (F (15.0f, true));
    g.drawText ("Voice Match", vmTitleB.withWidth (140), juce::Justification::centredLeft);

    // ---- modos / ganho ----
    drawPanel (g, rightB, kBlue);
    g.setColour (kMuted);
    g.setFont (12.0f);
    g.drawText (T ("Modo de Processamento"), modeTitleB, juce::Justification::centredLeft);

    const float gr = juce::jmax (0.0f, proc.grMeter.load());
    g.setColour (kMuted);
    g.setFont (11.0f);
    g.drawText (T ("Redução de ganho"), grBar.getX(), grBar.getY() - 16, grBar.getWidth(), 14, juce::Justification::centredLeft);
    g.setColour (kGold);
    g.drawText ("-" + juce::String (gr, 1) + " dB", grBar.getX(), grBar.getY() - 16, grBar.getWidth(), 14, juce::Justification::centredRight);
    g.setColour (juce::Colour (0xff060d18));
    g.fillRoundedRectangle (grBar.toFloat(), 4.0f);
    auto grFill = grBar.toFloat().withWidth (grBar.getWidth() * juce::jlimit (0.0f, 1.0f, gr / 18.0f));
    g.setGradientFill (juce::ColourGradient (kGold, grBar.getX(), 0.0f, kMag, grBar.getRight(), 0.0f, false));
    g.fillRoundedRectangle (grFill, 4.0f);

    g.setColour (kMuted.withAlpha (0.8f));
    g.setFont (12.0f);
    g.drawText ("Neto Studio", rightB.toNearestInt().reduced (16, 12), juce::Justification::bottomRight);
}

void NetoVocalAudioProcessorEditor::timerCallback()
{
    phase += 0.045f;
    meterL = proc.levelL.load();
    meterR = proc.levelR.load();
    smLevel = juce::jmax (juce::jmax (meterL, meterR), smLevel * 0.9f);

    // anel de espectro do Orbe: mesma curva ao vivo, normalizada
    const auto& lb = proc.liveBuilder;
    float mean = 0.0f;
    int cnt = 0;
    for (int b = 0; b < VoiceProfile::numBands; ++b)
    {
        const float f = ProfileBuilder::bandCentreHz (b);
        if (f >= 300.0f && f <= 2000.0f) { mean += lb.live[(size_t) b].load(); ++cnt; }
    }
    mean /= (float) juce::jmax (1, cnt);

    const float gate = juce::jlimit (0.0f, 1.0f, (lb.levelDb.load() + 62.0f) / 16.0f);
    for (size_t b = 0; b < glueOrb.halo.size(); ++b)
    {
        const float v = juce::jlimit (0.0f, 1.0f, (lb.live[b].load() - mean + 22.0f) / 40.0f);
        glueOrb.halo[b] += (v * gate - glueOrb.halo[b]) * 0.35f;
    }
    glueOrb.pulse = juce::jlimit (0.0f, 1.0f, smLevel * 1.3f);
    glueOrb.phase = phase;

    if (proc.pollCaptureFinished())
    {
        statusText = proc.getMine().valid ? T ("Sua voz foi analisada. Passo 3: clique em Aplicar!")
                                          : T ("Pouca voz detectada. Toque o vocal mais alto e tente de novo.");
    }
    else if (proc.isCapturing())
    {
        statusText = T ("Escutando sua voz... ") + juce::String (juce::roundToInt (proc.getCaptureProgress() * 100.0f)) + " %";
    }

    // menu de presets acompanha os knobs ("Personalizado" quando voce mexe)
    {
        const int idx = proc.findMatchingPreset();
        if (idx >= 0)
        {
            if (presetBox.getSelectedItemIndex() != idx)
                presetBox.setSelectedItemIndex (idx, juce::dontSendNotification);
        }
        else if (presetBox.getText() != T ("Personalizado"))
        {
            presetBox.setText (T ("Personalizado"), juce::dontSendNotification);
        }
    }

    captureBtn.setToggleState (proc.isCapturing(), juce::dontSendNotification);
    applyBtn.setEnabled (proc.getReference().valid && proc.getMine().valid);
    statusLabel.setText (statusText, juce::dontSendNotification);

    if (modeParam != nullptr)
    {
        const int idx = modeParam->getIndex();
        for (int i = 0; i < 3; ++i)
            modeButtons[(size_t) i].setToggleState (i == idx, juce::dontSendNotification);
    }

    repaint();
}
