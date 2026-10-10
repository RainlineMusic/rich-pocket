#pragma once
#include <JuceHeader.h>
#include "RichDSP.h"
#include "License.h"
#ifndef RICH_ENABLE_LICENSE
#define RICH_ENABLE_LICENSE 0
#endif
struct PocketTrace {
    float keyLo=0,keyHi=0,outLo=0,outHi=0,gain=1;
    double time=0;
    std::uint32_t generation=0;
};
class DuckPocketAudioProcessor final : public juce::AudioProcessor {
public:
    DuckPocketAudioProcessor();
    ~DuckPocketAudioProcessor() override=default;
    void playDuck() noexcept {duckRequested.store(true,std::memory_order_release);}
    bool popTrace(PocketTrace&);
    bool isActivated() const noexcept {
#if RICH_ENABLE_LICENSE
        return license->active.load(std::memory_order_acquire);
#else
        return true;
#endif
    }
    bool activateLicense(const juce::String& key,juce::String& error){return license->activate(key,error);}
    juce::String licenseDeviceCode() const{return license->deviceCode;}
    bool importLicenseFile(const juce::File& file,juce::String& error){return license->importFile(file,error);}
    bool startOnlineActivation(const juce::String& key,juce::String& error){return license->startOnline(key,error);}
    bool onlineActivationBusy() const{return license->onlineBusy();}
    juce::String onlineActivationMessage() const{return license->onlineMessage();}
    std::atomic<bool> listenSidechain{false},displayBypass{false},editorOpen{false};
    std::atomic<int> editorWidth{0};
    std::atomic<std::uint32_t> traceGeneration{0};
    void prepareToPlay(double,int) override;
    void releaseResources() override{}
    void reset() override;
    bool isBusesLayoutSupported(const BusesLayout&) const override;
    using juce::AudioProcessor::processBlock;
    using juce::AudioProcessor::processBlockBypassed;
    void processBlock(juce::AudioBuffer<float>&,juce::MidiBuffer&) override;
    void processBlockBypassed(juce::AudioBuffer<float>& b,juce::MidiBuffer& m) override{processAudio(b,m,true);}
    juce::AudioProcessorEditor* createEditor() override;
    juce::AudioProcessorParameter* getBypassParameter() const override{return parameters.getParameter("bypass");}
    bool hasEditor() const override{return true;}
    const juce::String getName() const override{return JucePlugin_Name;}
    bool acceptsMidi() const override{return false;}
    bool producesMidi() const override{return false;}
    bool isMidiEffect() const override{return false;}
    double getTailLengthSeconds() const override{return .5;}
    int getNumPrograms() override{return 1;}
    int getCurrentProgram() override{return 0;}
    void setCurrentProgram(int) override{}
    const juce::String getProgramName(int) override{return {};}
    void changeProgramName(int,const juce::String&) override{}
    void getStateInformation(juce::MemoryBlock&) override;
    void setStateInformation(const void*,int) override;
    static juce::AudioProcessorValueTreeState::ParameterLayout layout();
    juce::AudioProcessorValueTreeState parameters;
private:
    friend struct DuckLicenseTestAccess;
    juce::SharedResourcePointer<pocket::LicenseState> license;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;
    rich::Colours colours;
    juce::AudioBuffer<float> work,dry,base;
    std::array<std::vector<float>,2> delay,baseDelay;
    size_t delayPosition=0;
    int latency=0,blockCapacity=1;
    double rate=48000.;
    std::array<juce::SmoothedValue<float>,4> amountSmooth;
    juce::SmoothedValue<float> inputSmooth,outputSmooth,wetSmooth;
    std::array<std::atomic<float>*,4> amountParameters{};
    std::atomic<float>* inputGain=nullptr,*outputGain=nullptr,*bypass=nullptr;
    void processAudio(juce::AudioBuffer<float>&,juce::MidiBuffer&,bool);
    void prepareDuck(double);
    void mixDuck(juce::AudioBuffer<float>&);
    juce::AudioBuffer<float> duckAudio;
    std::atomic<bool> duckRequested{false};
    int duckPosition=0;
    juce::AbstractFifo fifo{8192};
    std::array<PocketTrace,8192> traces{};
    PocketTrace capture;
    double traceTime=0;
    int captured=0,decimation=20;
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(DuckPocketAudioProcessor)
};
