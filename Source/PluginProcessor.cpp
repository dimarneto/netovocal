#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

namespace
{
    juce::String T (const char* s) { return juce::String::fromUTF8 (s); }

    std::unique_ptr<juce::AudioParameterFloat> makePercent (const char* id, const char* name, float defaultValue)
    {
        auto attrs = juce::AudioParameterFloatAttributes()
                        .withStringFromValueFunction ([] (float v, int) { return juce::String (juce::roundToInt (v)) + " %"; })
                        .withValueFromStringFunction ([] (const juce::String& t) { return t.getFloatValue(); });

        return std::make_unique<juce::AudioParameterFloat> (juce::ParameterID { id, 1 }, name,
                                                            juce::NormalisableRange<float> (0.0f, 100.0f, 0.1f),
                                                            defaultValue, attrs);
    }
}

//==============================================================================
// ProfileBuilder: analisa a voz em quadros de 2048 amostras (FFT) e guarda as medias
//==============================================================================
ProfileBuilder::ProfileBuilder()
    : fft (order), window ((size_t) fftSize, juce::dsp::WindowingFunction<float>::hann)
{
    for (auto& v : live) v.store (-90.0f);
}

float ProfileBuilder::bandEdgeHz (int i)
{
    return 60.0f * std::pow (14000.0f / 60.0f, (float) i / (float) numBands);
}

void ProfileBuilder::prepare (double sr)
{
    sampleRate = sr;
    const double binHz = sr / (double) fftSize;

    for (int b = 0; b < numBands; ++b)
    {
        const int s = juce::jlimit (1, fftSize / 2 - 2, (int) std::ceil ((double) bandEdgeHz (b) / binHz));
        const int e = juce::jlimit (s + 1, fftSize / 2 - 1, (int) std::ceil ((double) bandEdgeHz (b + 1) / binHz));
        bandStart[(size_t) b] = s;
        bandEnd[(size_t) b]   = e;
    }

    ring.fill (0.0f);
    pos = hopCount = 0;
    hopMid = hopSide = hopPeak = 0.0f;
    for (auto& v : live) v.store (-90.0f);
    levelDb.store (-100.0f);
    accumulating.store (false);
    pendingStart.store (false);
    finished.store (false);
}

void ProfileBuilder::startAccumulating (float seconds)
{
    pendingSeconds.store (seconds);
    finished.store (false);
    pendingStart.store (true);
}

float ProfileBuilder::progress() const
{
    const float secs = (float) voicedFrames.load() * (float) hop / (float) sampleRate;
    return juce::jlimit (0.0f, 1.0f, secs / pendingSeconds.load());
}

void ProfileBuilder::push (float mid, float side) noexcept
{
    ring[(size_t) pos] = mid;
    pos = (pos + 1) & (fftSize - 1);

    hopMid  += mid * mid;
    hopSide += side * side;
    hopPeak  = std::max (hopPeak, std::abs (mid));

    if (++hopCount >= hop)
    {
        hopCount = 0;
        processFrame();
        hopMid = hopSide = hopPeak = 0.0f;
    }
}

void ProfileBuilder::processFrame()
{
    for (int i = 0; i < fftSize; ++i)
        fftData[(size_t) i] = ring[(size_t) ((pos + i) & (fftSize - 1))];

    std::fill (fftData.begin() + fftSize, fftData.end(), 0.0f);
    window.multiplyWithWindowingTable (fftData.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftData.data());

    std::array<float, numBands> pw {};
    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        float acc = 0.0f;
        for (int k = bandStart[b]; k < bandEnd[b]; ++k)
            acc += fftData[(size_t) k] * fftData[(size_t) k];
        pw[b] = acc / (float) (bandEnd[b] - bandStart[b]);
    }

    const float rmsDb = 10.0f * std::log10 (hopMid / (float) hop + 1.0e-12f);
    const bool  voiced = rmsDb > -50.0f;
    levelDb.store (rmsDb);

    // curva ao vivo (suavizada) para o desenho
    for (size_t b = 0; b < (size_t) numBands; ++b)
    {
        const float db  = 10.0f * std::log10 (pw[b] + 1.0e-12f);
        const float old = live[b].load();
        live[b].store (old + (db - old) * 0.4f);
    }

    if (pendingStart.exchange (false))
    {
        bandPow.fill (0.0);
        frameCount = totalFrames = 0;
        sumMid = sumSide = 0.0;
        peak = 0.0f;
        voicedFrames.store (0);
        targetSeconds = pendingSeconds.load();
        timeoutFrames = (int) ((targetSeconds * 3.0f + 5.0f) * (float) sampleRate / (float) hop);
        accumulating.store (true);
    }

    if (accumulating.load())
    {
        ++totalFrames;

        if (voiced && frameCount < maxFrames)
        {
            for (size_t b = 0; b < (size_t) numBands; ++b)
                bandPow[b] += (double) pw[b];

            frameDb[(size_t) frameCount++] = rmsDb;
            sumMid  += (double) hopMid;
            sumSide += (double) hopSide;
            peak = std::max (peak, hopPeak);
            voicedFrames.store (frameCount);
        }

        const float secs = (float) frameCount * (float) hop / (float) sampleRate;
        if (secs >= targetSeconds || frameCount >= maxFrames || totalFrames > timeoutFrames)
        {
            accumulating.store (false);
            finished.store (true);
        }
    }
}

VoiceProfile ProfileBuilder::build() const
{
    VoiceProfile p;
    const int n = frameCount;

    if (n < (int) (1.5 * sampleRate / (double) hop))      // menos de ~1,5 s de voz
        return p;

    for (int b = 0; b < numBands; ++b)
        p.bandsDb[(size_t) b] = 10.0f * std::log10 ((float) (bandPow[(size_t) b] / (double) n) + 1.0e-12f);

    // normaliza pelo nivel medio de 300 Hz a 2 kHz: so o FORMATO da curva importa, nao o volume
    float ref = 0.0f;
    int cnt = 0;
    for (int b = 0; b < numBands; ++b)
    {
        const float f = bandCentreHz (b);
        if (f >= 300.0f && f <= 2000.0f) { ref += p.bandsDb[(size_t) b]; ++cnt; }
    }
    ref /= (float) std::max (1, cnt);
    for (auto& v : p.bandsDb) v -= ref;

    std::vector<float> sorted (frameDb.begin(), frameDb.begin() + n);
    std::sort (sorted.begin(), sorted.end());
    p.dynRange = sorted[(size_t) (0.95 * (n - 1))] - sorted[(size_t) (0.10 * (n - 1))];

    const float rmsDb = 10.0f * std::log10 ((float) (sumMid / ((double) n * (double) hop)) + 1.0e-12f);
    p.crest = 20.0f * std::log10 (peak + 1.0e-9f) - rmsDb;
    p.width = (float) std::sqrt (sumSide / (sumMid + 1.0e-12));
    p.valid = true;
    return p;
}

//==============================================================================
//==============================================================================
// Presets: valores em % (presenca, corpo, claridade, glue, harmonia, espaco, naturalidade, mix),
// modo (0 Padrao, 1 Suave, 2 Intenso) e Ajuste Fino (de-esser)
//==============================================================================
namespace
{
    struct PresetDef
    {
        const char* name;
        float presence, body, clarity, glue, harmony, space, natural, mix;
        int mode;
        bool fine;
    };

    const PresetDef kPresets[NetoVocalAudioProcessor::numPresets] =
    {
        { "Padrão",                68, 54, 62, 72, 70, 48, 76, 100, 0, true },   // o som de fábrica
        { "Voz Natural",           45, 40, 45, 35, 25, 25, 95,  85, 1, true },   // quase transparente
        { "Pop Brilhante",         78, 45, 85, 65, 55, 40, 60, 100, 0, true },   // brilho e ar
        { "Rap Forte",             82, 65, 60, 88, 60, 18, 50, 100, 2, true },   // voz na cara, bem comprimida
        { "Rock Potente",          75, 60, 55, 80, 85, 30, 45, 100, 2, true },   // garra e saturação
        { "Sertanejo Quente",      62, 72, 58, 60, 55, 45, 75, 100, 0, true },   // calor e corpo
        { "Balada Suave",          50, 62, 48, 45, 40, 70, 85, 100, 1, true },   // macia, com espaço
        { "Locução / Podcast",     70, 66, 50, 70, 20,  5, 90, 100, 0, true },   // clara e seca
        { "Estúdio Espacial",      66, 48, 72, 58, 60, 85, 70, 100, 1, true },   // grande e aberta
        { "Vocal Moderno (Trap)",  72, 50, 90, 78, 92, 55, 35, 100, 2, true }    // brilho e saturação fortes
    };
}

juce::String NetoVocalAudioProcessor::getPresetName (int index)
{
    if (index < 0 || index >= numPresets)
        return {};

    return juce::String::fromUTF8 (kPresets[index].name);
}

void NetoVocalAudioProcessor::applyPreset (int index)
{
    if (index < 0 || index >= numPresets)
        return;

    const auto& p = kPresets[index];
    setParam (NetoIDs::presence, p.presence);
    setParam (NetoIDs::body,     p.body);
    setParam (NetoIDs::clarity,  p.clarity);
    setParam (NetoIDs::glue,     p.glue);
    setParam (NetoIDs::harmony,  p.harmony);
    setParam (NetoIDs::space,    p.space);
    setParam (NetoIDs::natural,  p.natural);
    setParam (NetoIDs::mix,      p.mix);
    setParam (NetoIDs::mode,     (float) p.mode);
    setParam (NetoIDs::fine,     p.fine ? 1.0f : 0.0f);
}

int NetoVocalAudioProcessor::findMatchingPreset() const
{
    const float cur[8] = { pPresence->load(), pBody->load(), pClarity->load(), pGlue->load(),
                           pHarmony->load(), pSpace->load(), pNatural->load(), pMix->load() };
    const int  mode = (int) (pMode->load() + 0.5f);
    const bool fine = pFine->load() > 0.5f;

    for (int i = 0; i < numPresets; ++i)
    {
        const auto& p = kPresets[i];
        const float v[8] = { p.presence, p.body, p.clarity, p.glue, p.harmony, p.space, p.natural, p.mix };

        bool ok = (mode == p.mode) && (fine == p.fine);
        for (int k = 0; k < 8 && ok; ++k)
            ok = std::abs (cur[k] - v[k]) < 0.6f;

        if (ok)
            return i;
    }
    return -1;
}

juce::AudioProcessorValueTreeState::ParameterLayout NetoVocalAudioProcessor::createParameterLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add (makePercent (NetoIDs::presence, "Presenca Vocal", 68.0f));
    layout.add (makePercent (NetoIDs::body,     "Corpo",          54.0f));
    layout.add (makePercent (NetoIDs::clarity,  "Claridade",      62.0f));
    layout.add (makePercent (NetoIDs::glue,     "Glue Vocal",     72.0f));
    layout.add (makePercent (NetoIDs::harmony,  "Harmonia com o Mix", 70.0f));
    layout.add (makePercent (NetoIDs::space,    "Espacialidade",  48.0f));
    layout.add (makePercent (NetoIDs::natural,  "Naturalidade",   76.0f));
    layout.add (makePercent (NetoIDs::mix,      "Mix",            100.0f));
    layout.add (makePercent (NetoIDs::matchAmt, "Intensidade da Copia", 100.0f));

    layout.add (std::make_unique<juce::AudioParameterChoice> (
        juce::ParameterID { NetoIDs::mode, 1 }, "Modo de Processamento",
        juce::StringArray { T ("Padrão"), T ("Suave"), T ("Intenso") }, 0));

    layout.add (std::make_unique<juce::AudioParameterBool> (
        juce::ParameterID { NetoIDs::fine, 1 }, "Ajuste Fino", true));

    return layout;
}

NetoVocalAudioProcessor::NetoVocalAudioProcessor()
    : AudioProcessor (BusesProperties()
                        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
                        .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NETO_VOCAL_STATE", createParameterLayout())
{
    pPresence = apvts.getRawParameterValue (NetoIDs::presence);
    pBody     = apvts.getRawParameterValue (NetoIDs::body);
    pClarity  = apvts.getRawParameterValue (NetoIDs::clarity);
    pGlue     = apvts.getRawParameterValue (NetoIDs::glue);
    pHarmony  = apvts.getRawParameterValue (NetoIDs::harmony);
    pSpace    = apvts.getRawParameterValue (NetoIDs::space);
    pNatural  = apvts.getRawParameterValue (NetoIDs::natural);
    pMix      = apvts.getRawParameterValue (NetoIDs::mix);
    pMode     = apvts.getRawParameterValue (NetoIDs::mode);
    pFine     = apvts.getRawParameterValue (NetoIDs::fine);
}

bool NetoVocalAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto out = layouts.getMainOutputChannelSet();

    if (out != juce::AudioChannelSet::mono() && out != juce::AudioChannelSet::stereo())
        return false;

    return out == layouts.getMainInputChannelSet();
}

void NetoVocalAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    dryBuffer.setSize (2, juce::jmax (1, samplesPerBlock));

    reverb.setSampleRate (sampleRate);
    reverb.reset();

    auto bp = juce::dsp::IIR::Coefficients<float>::makeBandPass (sampleRate, 6500.0f, 1.2f);
    for (auto& f : sibFilter) { f.coefficients = bp; f.reset(); }

    for (auto& f : presenceFilter) f.reset();
    for (auto& f : bodyFilter)     f.reset();
    for (auto& f : clarityFilter)  f.reset();

    lastPresence = lastBody = lastClarity = lastEff = -1.0f;

    grDb  = 0.0f;
    deEnv = -100.0f;

    liveBuilder.prepare (sampleRate);
    capturing.store (false);
}

void NetoVocalAudioProcessor::updateEq (float presence, float body, float clarity, float eff)
{
    if (presence == lastPresence && body == lastBody && clarity == lastClarity && eff == lastEff)
        return;

    lastPresence = presence; lastBody = body; lastClarity = clarity; lastEff = eff;

    using Coeffs = juce::dsp::IIR::Coefficients<float>;
    const auto sr = currentSampleRate;

    // Ganhos maximos: +8 dB (presenca), +7 dB (corpo), +7 dB (claridade)
    auto p = Coeffs::makePeakFilter (sr, 3000.0f, 0.8f, juce::Decibels::decibelsToGain (presence * 8.0f * eff));
    auto b = Coeffs::makeLowShelf   (sr, 200.0f,  0.7f, juce::Decibels::decibelsToGain (body     * 7.0f * eff));
    auto c = Coeffs::makeHighShelf  (sr, 7000.0f, 0.7f, juce::Decibels::decibelsToGain (clarity  * 7.0f * eff));

    for (size_t ch = 0; ch < 2; ++ch)
    {
        presenceFilter[ch].coefficients = p;
        bodyFilter[ch].coefficients     = b;
        clarityFilter[ch].coefficients  = c;
    }
}

void NetoVocalAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    const int numCh = juce::jmin (buffer.getNumChannels(), 2);
    const int n     = buffer.getNumSamples();

    for (int c = getTotalNumInputChannels(); c < getTotalNumOutputChannels(); ++c)
        buffer.clear (c, 0, n);

    if (numCh == 0 || n == 0)
        return;

    if (dryBuffer.getNumSamples() < n)
        dryBuffer.setSize (2, n, false, false, true);

    for (int c = 0; c < numCh; ++c)
        dryBuffer.copyFrom (c, 0, buffer, c, 0, n);

    // ---- Analisador (Voice Match): escuta a ENTRADA, antes de processar ----
    {
        const float* il = buffer.getReadPointer (0);
        const float* ir = numCh > 1 ? buffer.getReadPointer (1) : il;
        for (int i = 0; i < n; ++i)
            liveBuilder.push (0.5f * (il[i] + ir[i]), 0.5f * (il[i] - ir[i]));
    }

    // ---- Parametros (0..1) ----
    const float presence = pPresence->load() * 0.01f;
    const float body     = pBody->load()     * 0.01f;
    const float clarity  = pClarity->load()  * 0.01f;
    const float glue     = pGlue->load()     * 0.01f;
    const float harmony  = pHarmony->load()  * 0.01f;
    const float space    = pSpace->load()    * 0.01f;
    const float natural  = pNatural->load()  * 0.01f;
    const float mix      = pMix->load()      * 0.01f;
    const int   mode     = juce::jlimit (0, 2, (int) pMode->load());
    const bool  fine     = pFine->load() > 0.5f;

    const float eff = 1.0f - 0.15f * natural;

    updateEq (presence, body, clarity, eff);

    // ---- Glue (compressor com sidechain linkado L/R) ----
    static constexpr float modeRatio[3] = { 1.0f, 0.7f, 1.4f };
    static constexpr float modeAtkMs[3] = { 25.0f, 40.0f, 10.0f };
    static constexpr float modeRelMs[3] = { 150.0f, 200.0f, 100.0f };

    const float thresholdDb = -10.0f - glue * 24.0f;
    const float ratio       = juce::jmax (1.0f, (2.0f + glue * 4.0f) * modeRatio[mode]);
    const float makeupDb    = -thresholdDb * (1.0f - 1.0f / ratio) * 0.4f;
    const float kneeW       = 4.0f + natural * 10.0f;

    const auto sr = (float) currentSampleRate;
    const float atk = std::exp (-1.0f / (sr * modeAtkMs[mode] * 0.001f));
    const float rel = std::exp (-1.0f / (sr * modeRelMs[mode] * 0.001f));

    // ---- Saturacao suave (Harmonia) ----
    const float drive   = 1.0f + harmony * 9.0f;
    const float satNorm = 4.0f * std::tanh (drive * 0.25f);
    const float satMix  = harmony * 0.7f * eff;

    // ---- De-Esser (ligado pelo "Ajuste Fino") ----
    const float deThreshold = -28.0f;
    const float deAtk = std::exp (-1.0f / (sr * 0.001f));
    const float deRel = std::exp (-1.0f / (sr * 0.05f));

    // ---- Largura estereo (Espacialidade) ----
    const float widthGain = 1.0f + space * 0.8f;

    auto softClip = [] (float v)
    {
        const float a = std::abs (v);
        if (a < 0.8f) return v;
        return std::copysign (0.8f + 0.2f * std::tanh ((a - 0.8f) / 0.2f), v);
    };

    auto* l = buffer.getWritePointer (0);
    auto* r = numCh > 1 ? buffer.getWritePointer (1) : nullptr;

    for (int i = 0; i < n; ++i)
    {
        float xl = l[i];
        float xr = r != nullptr ? r[i] : xl;

        // EQ
        xl = clarityFilter[0].processSample (bodyFilter[0].processSample (presenceFilter[0].processSample (xl)));
        if (r != nullptr)
            xr = clarityFilter[1].processSample (bodyFilter[1].processSample (presenceFilter[1].processSample (xr)));
        else
            xr = xl;

        // Saturacao
        xl = xl * (1.0f - satMix) + (std::tanh (drive * xl) / satNorm) * satMix;
        xr = xr * (1.0f - satMix) + (std::tanh (drive * xr) / satNorm) * satMix;

        // De-Esser
        if (fine)
        {
            const float sl = sibFilter[0].processSample (xl);
            const float sRight = r != nullptr ? sibFilter[1].processSample (xr) : sl;

            const float det   = juce::jmax (std::abs (sl), std::abs (sRight));
            const float detDb = juce::Decibels::gainToDecibels (det, -100.0f);
            const float c     = detDb > deEnv ? deAtk : deRel;
            deEnv = c * deEnv + (1.0f - c) * detDb;

            const float over = deEnv - deThreshold;
            const float red  = over > 0.0f ? over * 0.75f : 0.0f;
            const float g    = juce::Decibels::decibelsToGain (-red);

            xl -= sl * (1.0f - g);
            xr -= sRight * (1.0f - g);
        }

        // Glue
        {
            const float level   = juce::jmax (std::abs (xl), std::abs (xr));
            const float levelDb = juce::Decibels::gainToDecibels (level, -100.0f);
            const float over    = levelDb - thresholdDb;
            const float slope   = 1.0f - 1.0f / ratio;
            float target = 0.0f;

            if (2.0f * over > kneeW)
                target = over * slope;
            else if (2.0f * std::abs (over) <= kneeW)
            {
                const float t = over + kneeW * 0.5f;
                target = slope * t * t / (2.0f * kneeW);
            }
            const float c = target > grDb ? atk : rel;
            grDb = c * grDb + (1.0f - c) * target;

            const float g = juce::Decibels::decibelsToGain (makeupDb - grDb);
            xl *= g;
            xr *= g;
        }

        // Largura (M/S)
        if (r != nullptr)
        {
            const float mid  = 0.5f * (xl + xr);
            const float side = 0.5f * (xl - xr) * widthGain;
            xl = mid + side;
            xr = mid - side;
        }

        l[i] = softClip (xl);
        if (r != nullptr) r[i] = softClip (xr);
    }

    grMeter.store (grDb);

    // ---- Reverberacao natural (Espacialidade) ----
    juce::Reverb::Parameters rp;
    rp.roomSize   = 0.3f + 0.5f * space;
    rp.damping    = 0.5f;
    rp.wetLevel   = space * 0.15f;
    rp.dryLevel   = 0.5f;
    rp.width      = 1.0f;
    rp.freezeMode = 0.0f;
    reverb.setParameters (rp);

    if (r != nullptr) reverb.processStereo (l, r, n);
    else              reverb.processMono (l, n);

    // ---- Mix dry/wet ----
    for (int c = 0; c < numCh; ++c)
    {
        buffer.applyGain (c, 0, n, mix);
        buffer.addFrom (c, 0, dryBuffer, c, 0, n, 1.0f - mix);
    }

    // ---- Medidor de saida ----
    const float peakL = buffer.getMagnitude (0, 0, n);
    const float peakR = numCh > 1 ? buffer.getMagnitude (1, 0, n) : peakL;
    levelL.store (juce::jmax (peakL, levelL.load() * 0.85f));
    levelR.store (juce::jmax (peakR, levelR.load() * 0.85f));
}

//==============================================================================
// Voice Match
//==============================================================================
bool NetoVocalAudioProcessor::loadReference (const juce::File& file, juce::String& error)
{
    juce::AudioFormatManager fm;
    fm.registerBasicFormats();

    std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (file));
    if (reader == nullptr || reader->sampleRate < 8000.0)
    {
        error = T ("Não consegui abrir esse arquivo. Use WAV, MP3 ou FLAC.");
        return false;
    }

    auto pb = std::make_unique<ProfileBuilder>();
    pb->prepare (reader->sampleRate);
    pb->startAccumulating (45.0f);

    constexpr int blockSize = 16384;
    const bool stereo = reader->numChannels > 1;
    const juce::int64 total = juce::jmin (reader->lengthInSamples, (juce::int64) (reader->sampleRate * 90.0));
    juce::AudioBuffer<float> buf (2, blockSize);

    for (juce::int64 pos = 0; pos < total && ! pb->finished.load(); pos += blockSize)
    {
        const int n = (int) juce::jmin ((juce::int64) blockSize, total - pos);
        buf.clear();
        reader->read (&buf, 0, n, pos, true, stereo);

        const float* l = buf.getReadPointer (0);
        const float* r = stereo ? buf.getReadPointer (1) : l;
        for (int i = 0; i < n; ++i)
            pb->push (0.5f * (l[i] + r[i]), 0.5f * (l[i] - r[i]));
    }

    auto p = pb->build();
    if (! p.valid)
    {
        error = T ("Quase não encontrei voz nesse arquivo. Use um trecho com a voz bem audível.");
        return false;
    }

    refProfile = p;
    refName = file.getFileName();
    return true;
}

void NetoVocalAudioProcessor::startCapture()
{
    myProfile = VoiceProfile();
    capturing.store (true);
    liveBuilder.startAccumulating (8.0f);
}

void NetoVocalAudioProcessor::cancelCapture()
{
    liveBuilder.stopAccumulating();
    capturing.store (false);
}

bool NetoVocalAudioProcessor::pollCaptureFinished()
{
    if (capturing.load() && liveBuilder.finished.exchange (false))
    {
        capturing.store (false);
        myProfile = liveBuilder.build();
        return true;
    }
    return false;
}

void NetoVocalAudioProcessor::setParam (const char* id, float value)
{
    if (auto* p = apvts.getParameter (id))
    {
        p->beginChangeGesture();
        p->setValueNotifyingHost (p->convertTo0to1 (value));
        p->endChangeGesture();
    }
}

void NetoVocalAudioProcessor::applyMatch()
{
    if (! refProfile.valid || ! myProfile.valid)
        return;

    const float amt = juce::jlimit (0.0f, 1.0f, apvts.getRawParameterValue (NetoIDs::matchAmt)->load() * 0.01f);

    auto region = [] (const VoiceProfile& p, float f0, float f1)
    {
        float sum = 0.0f;
        int cnt = 0;
        for (int b = 0; b < VoiceProfile::numBands; ++b)
        {
            const float f = ProfileBuilder::bandCentreHz (b);
            if (f >= f0 && f <= f1) { sum += p.bandsDb[(size_t) b]; ++cnt; }
        }
        return cnt > 0 ? sum / (float) cnt : 0.0f;
    };

    // Quanto a referencia tem a MAIS que a sua voz em cada regiao (dB): graves, medios, agudos
    const float d[3] = { region (refProfile,   80.0f,  250.0f) - region (myProfile,   80.0f,  250.0f),
                         region (refProfile, 2000.0f, 4500.0f) - region (myProfile, 2000.0f, 4500.0f),
                         region (refProfile, 6500.0f, 12000.0f) - region (myProfile, 6500.0f, 12000.0f) };

    // Os knobs de EQ so somam (0 = neutro). Mantemos o FORMATO da curva tirando a menor diferenca.
    const float lo = std::min ({ d[0], d[1], d[2] });
    const float bodyK     = juce::jlimit (0.0f, 1.0f, (d[0] - lo) / (7.0f * 0.9f));
    const float presenceK = juce::jlimit (0.0f, 1.0f, (d[1] - lo) / (8.0f * 0.9f));
    const float clarityK  = juce::jlimit (0.0f, 1.0f, (d[2] - lo) / (7.0f * 0.9f));

    // Menos variacao de volume na referencia -> mais Glue
    const float glueK    = juce::jlimit (0.0f, 1.0f, 0.35f + (myProfile.dynRange - refProfile.dynRange) / 16.0f);
    // Pico/RMS menor na referencia -> mais Harmonia (saturacao)
    const float harmonyK = juce::jlimit (0.0f, 1.0f, 0.40f + (myProfile.crest - refProfile.crest) / 12.0f);
    // Mais largura na referencia -> mais Espacialidade
    const float spaceK   = juce::jlimit (0.0f, 1.0f, 0.30f + (refProfile.width - myProfile.width) * 2.0f);

    setParam (NetoIDs::body,     bodyK     * amt * 100.0f);
    setParam (NetoIDs::presence, presenceK * amt * 100.0f);
    setParam (NetoIDs::clarity,  clarityK  * amt * 100.0f);
    setParam (NetoIDs::glue,     glueK     * amt * 100.0f);
    setParam (NetoIDs::harmony,  harmonyK  * amt * 100.0f);
    setParam (NetoIDs::space,    spaceK    * amt * 100.0f);
}

//==============================================================================
juce::AudioProcessorEditor* NetoVocalAudioProcessor::createEditor()
{
    return new NetoVocalAudioProcessorEditor (*this);
}

void NetoVocalAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, destData);
}

void NetoVocalAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
        if (xml->hasTagName (apvts.state.getType()))
            apvts.replaceState (juce::ValueTree::fromXml (*xml));
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NetoVocalAudioProcessor();
}
