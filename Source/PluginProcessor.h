#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>
#include <vector>

// IDs dos parametros (usados no Processor e no Editor)
namespace NetoIDs
{
    inline constexpr const char* presence = "presence";
    inline constexpr const char* body     = "body";
    inline constexpr const char* clarity  = "clarity";
    inline constexpr const char* glue     = "glue";
    inline constexpr const char* harmony  = "harmony";
    inline constexpr const char* space    = "space";
    inline constexpr const char* natural  = "natural";
    inline constexpr const char* mix      = "mix";
    inline constexpr const char* mode     = "mode";
    inline constexpr const char* fine     = "fine";
    inline constexpr const char* matchAmt = "matchamt";
}

// "Impressao digital" de uma voz: curva por bandas + dinamica + largura estereo
struct VoiceProfile
{
    static constexpr int numBands = 48;
    std::array<float, numBands> bandsDb {};   // curva normalizada (dB)
    float dynRange = 0.0f;                    // variacao de volume (dB)
    float crest    = 0.0f;                    // pico - RMS (dB)
    float width    = 0.0f;                    // lado / centro
    bool  valid    = false;
};

// Analisa o audio em quadros (FFT) e acumula as estatisticas da voz
class ProfileBuilder
{
public:
    static constexpr int numBands  = VoiceProfile::numBands;
    static constexpr int order     = 11;
    static constexpr int fftSize   = 1 << order;
    static constexpr int hop       = fftSize / 2;
    static constexpr int maxFrames = 4096;

    ProfileBuilder();

    void prepare (double sampleRate);
    void push (float mid, float side) noexcept;            // thread de audio (ou offline)
    void startAccumulating (float seconds);                // pode ser chamado de outra thread
    void stopAccumulating() { accumulating.store (false); pendingStart.store (false); }
    bool  isAccumulating() const { return accumulating.load(); }
    float progress() const;                                // 0..1
    VoiceProfile build() const;                            // chamar so depois de terminar

    static float bandEdgeHz (int i);
    static float bandCentreHz (int b) { return std::sqrt (bandEdgeHz (b) * bandEdgeHz (b + 1)); }

    std::array<std::atomic<float>, numBands> live;         // espectro ao vivo (dB), para o desenho
    std::atomic<float> levelDb { -100.0f };
    std::atomic<bool>  finished { false };

private:
    void processFrame();

    juce::dsp::FFT fft;
    juce::dsp::WindowingFunction<float> window;
    std::array<float, fftSize>     ring {};
    std::array<float, fftSize * 2> fftData {};
    std::array<int, numBands>      bandStart {}, bandEnd {};
    std::array<double, numBands>   bandPow {};
    std::array<float, maxFrames>   frameDb {};

    int pos = 0, hopCount = 0, frameCount = 0, totalFrames = 0, timeoutFrames = 1000;
    float hopMid = 0.0f, hopSide = 0.0f, hopPeak = 0.0f, peak = 0.0f, targetSeconds = 8.0f;
    double sumMid = 0.0, sumSide = 0.0, sampleRate = 44100.0;

    std::atomic<bool>  accumulating { false }, pendingStart { false };
    std::atomic<int>   voicedFrames { 0 };
    std::atomic<float> pendingSeconds { 8.0f };
};

class NetoVocalAudioProcessor : public juce::AudioProcessor
{
public:
    NetoVocalAudioProcessor();
    ~NetoVocalAudioProcessor() override = default;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Neto Vocal"; }
    bool acceptsMidi() const override  { return false; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.5; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();

    // ---- Presets ----
    static constexpr int numPresets = 10;
    static juce::String getPresetName (int index);
    void applyPreset (int index);
    int  findMatchingPreset() const;        // -1 se os knobs nao batem com nenhum preset

    // ---- Voice Match (so a thread da interface chama estas funcoes) ----
    bool  loadReference (const juce::File& file, juce::String& error);
    void  startCapture();
    void  cancelCapture();
    bool  pollCaptureFinished();            // true no momento em que a analise termina
    bool  isCapturing() const { return capturing.load(); }
    float getCaptureProgress() const { return liveBuilder.progress(); }
    void  applyMatch();

    const VoiceProfile& getReference() const { return refProfile; }
    const VoiceProfile& getMine() const { return myProfile; }
    const juce::String& getReferenceName() const { return refName; }

    juce::AudioProcessorValueTreeState apvts;
    ProfileBuilder liveBuilder;             // analisa a entrada ao vivo

    // Lidos pelo Editor
    std::atomic<float> levelL { 0.0f };
    std::atomic<float> levelR { 0.0f };
    std::atomic<float> grMeter { 0.0f };    // reducao de ganho do glue (dB)

private:
    using Filter = juce::dsp::IIR::Filter<float>;

    void updateEq (float presence, float body, float clarity, float eff);
    void setParam (const char* id, float value);

    double currentSampleRate = 44100.0;

    // Cadeia: EQ -> saturacao -> de-esser -> glue -> largura -> reverb -> mix
    std::array<Filter, 2> presenceFilter, bodyFilter, clarityFilter, sibFilter;
    juce::Reverb reverb;
    juce::AudioBuffer<float> dryBuffer;

    float grDb  = 0.0f;
    float deEnv = -100.0f;

    float lastPresence = -1.0f, lastBody = -1.0f, lastClarity = -1.0f, lastEff = -1.0f;

    VoiceProfile refProfile, myProfile;
    juce::String refName;
    std::atomic<bool> capturing { false };

    std::atomic<float>* pPresence = nullptr;
    std::atomic<float>* pBody     = nullptr;
    std::atomic<float>* pClarity  = nullptr;
    std::atomic<float>* pGlue     = nullptr;
    std::atomic<float>* pHarmony  = nullptr;
    std::atomic<float>* pSpace    = nullptr;
    std::atomic<float>* pNatural  = nullptr;
    std::atomic<float>* pMix      = nullptr;
    std::atomic<float>* pMode     = nullptr;
    std::atomic<float>* pFine     = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NetoVocalAudioProcessor)
};
