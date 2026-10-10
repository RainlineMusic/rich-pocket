#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "BinaryData.h"
DuckPocketAudioProcessor::DuckPocketAudioProcessor()
:AudioProcessor(BusesProperties().withInput("Input",juce::AudioChannelSet::stereo(),true)
.withOutput("Output",juce::AudioChannelSet::stereo(),true)),parameters(*this,nullptr,"RICH_PARAMETERS",layout()) {
#if RICH_ENABLE_LICENSE
    license->checkOnOpen();
#endif
    constexpr const char* ids[]{"warm","body","silk","air"};
    for(int i=0;i<4;++i)amountParameters[size_t(i)]=parameters.getRawParameterValue(ids[i]);
    inputGain=parameters.getRawParameterValue("inputGain");outputGain=parameters.getRawParameterValue("outputGain");bypass=parameters.getRawParameterValue("bypass");
    (void)rich::tables();
}
juce::AudioProcessorValueTreeState::ParameterLayout DuckPocketAudioProcessor::layout(){
    juce::AudioProcessorValueTreeState::ParameterLayout result;
    constexpr const char* ids[]{"warm","body","silk","air"};constexpr const char* names[]{"Warm","Body","Silk","Air"};
    for(int i=0;i<4;++i)result.add(std::make_unique<juce::AudioParameterFloat>(ids[i],names[i],juce::NormalisableRange<float>(0.f,100.f,.1f),0.f));
    result.add(std::make_unique<juce::AudioParameterFloat>("inputGain","Input",juce::NormalisableRange<float>(-12.f,12.f,.01f),0.f));
    result.add(std::make_unique<juce::AudioParameterFloat>("outputGain","Output",juce::NormalisableRange<float>(-12.f,12.f,.01f),0.f));
    result.add(std::make_unique<juce::AudioParameterBool>("bypass","Bypass",false));
    return result;
}
void DuckPocketAudioProcessor::prepareDuck(double sampleRate)
{
    struct Asset { double rate; const char* data; int size; };
    const Asset assets[]{
        {44100.,BinaryData::Duck44100_wav,BinaryData::Duck44100_wavSize},
        {48000.,BinaryData::Duck48000_wav,BinaryData::Duck48000_wavSize},
        {88200.,BinaryData::Duck88200_wav,BinaryData::Duck88200_wavSize},
        {96000.,BinaryData::Duck96000_wav,BinaryData::Duck96000_wavSize},
        {176400.,BinaryData::Duck176400_wav,BinaryData::Duck176400_wavSize},
        {192000.,BinaryData::Duck192000_wav,BinaryData::Duck192000_wavSize},
    };
    const Asset* chosen=&assets[0];
    for(const auto& asset:assets)if(std::abs(asset.rate-sampleRate)<std::abs(chosen->rate-sampleRate))chosen=&asset;
    juce::WavAudioFormat format;
    std::unique_ptr<juce::AudioFormatReader> reader(format.createReaderFor(new juce::MemoryInputStream(chosen->data,size_t(chosen->size),false),true));
    duckAudio.setSize(1,0);
    if(reader){
        juce::AudioBuffer<float> source(1,int(reader->lengthInSamples));
        if(reader->read(&source,0,source.getNumSamples(),0,true,false)){
            if(sampleRate==chosen->rate)duckAudio=std::move(source);
            else {
                // Unusual rates: inexpensive linear conversion once at preparation.
                const int count=juce::jmax(1,juce::roundToInt(source.getNumSamples()*sampleRate/chosen->rate));
                duckAudio.setSize(1,count);
                const auto* input=source.getReadPointer(0);auto* output=duckAudio.getWritePointer(0);
                for(int i=0;i<count;++i){const double position=i*chosen->rate/sampleRate;const int a=juce::jmin(source.getNumSamples()-1,int(position)),b=juce::jmin(a+1,source.getNumSamples()-1);output[i]=input[a]+float(position-a)*(input[b]-input[a]);}
            }
        }
    }
    duckPosition=duckAudio.getNumSamples();duckRequested.store(false,std::memory_order_release);
}
void DuckPocketAudioProcessor::mixDuck(juce::AudioBuffer<float>& output)
{
    if(duckRequested.exchange(false,std::memory_order_acq_rel))duckPosition=0;
    const int count=juce::jmin(output.getNumSamples(),duckAudio.getNumSamples()-duckPosition);
    if(count<=0)return;
    for(int channel=0;channel<output.getNumChannels();++channel)output.addFrom(channel,0,duckAudio,0,duckPosition,count);
    duckPosition+=count;
}


void DuckPocketAudioProcessor::prepareToPlay(double sr,int maximumBlock){
    rate=std::isfinite(sr)&&sr>1000.?sr:48000.;blockCapacity=juce::jmax(1,maximumBlock);
    work.setSize(2,blockCapacity);dry.setSize(2,blockCapacity);base.setSize(2,blockCapacity);
    oversampling=std::make_unique<juce::dsp::Oversampling<float>>(2,3,juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,true,true);
    oversampling->initProcessing(size_t(blockCapacity));
    latency=juce::roundToInt(oversampling->getLatencyInSamples());setLatencySamples(latency);
    for(auto& d:delay)d.assign(size_t(juce::jmax(1,latency)),0.f);
    for(auto& d:baseDelay)d.assign(size_t(juce::jmax(1,latency)),0.f);
    colours.prepare(rate*8.);prepareDuck(rate);decimation=juce::jmax(1,int(rate/2400.));reset();
}
void DuckPocketAudioProcessor::reset(){
    if(oversampling)oversampling->reset();
    colours.reset();
    for(auto& d:delay)std::fill(d.begin(),d.end(),0.f);
    for(auto& d:baseDelay)std::fill(d.begin(),d.end(),0.f);
    delayPosition=0;
    for(size_t i=0;i<4;++i){amountSmooth[i].reset(rate*8.,.025);amountSmooth[i].setCurrentAndTargetValue(amountParameters[i]->load()*.01f);}
    inputSmooth.reset(rate,.025);outputSmooth.reset(rate,.025);wetSmooth.reset(rate,.005);
    inputSmooth.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(inputGain->load()));
    outputSmooth.setCurrentAndTargetValue(juce::Decibels::decibelsToGain(outputGain->load()));
    wetSmooth.setCurrentAndTargetValue(isActivated()&&bypass->load()<.5f?1.f:0.f);
    duckRequested.store(false);duckPosition=duckAudio.getNumSamples();captured=0;capture={};
}
bool DuckPocketAudioProcessor::isBusesLayoutSupported(const BusesLayout& l) const{
    return l.inputBuses.size()==1&&l.outputBuses.size()==1&&l.getMainInputChannelSet()==l.getMainOutputChannelSet()&&
        (l.getMainOutputChannelSet()==juce::AudioChannelSet::mono()||l.getMainOutputChannelSet()==juce::AudioChannelSet::stereo());
}
void DuckPocketAudioProcessor::processBlock(juce::AudioBuffer<float>& b,juce::MidiBuffer& m){processAudio(b,m,false);}
void DuckPocketAudioProcessor::processAudio(juce::AudioBuffer<float>& buffer,juce::MidiBuffer&,bool hostBypass){
    juce::ScopedNoDenormals guard;
    if(!oversampling||buffer.getNumChannels()<1)return;
    const int channels=juce::jmin(2,buffer.getNumChannels());
    const bool activated=isActivated();displayBypass.store(hostBypass,std::memory_order_relaxed);
    inputSmooth.setTargetValue(juce::Decibels::decibelsToGain(inputGain->load()));
    outputSmooth.setTargetValue(juce::Decibels::decibelsToGain(outputGain->load()));
    wetSmooth.setTargetValue(activated&&!hostBypass&&bypass->load()<.5f?1.f:0.f);
    for(size_t i=0;i<4;++i)amountSmooth[i].setTargetValue(juce::jlimit(0.f,1.f,amountParameters[i]->load()*.01f));
    // Hosts may exceed their advertised maximum: process in preallocated chunks.
    for(int offset=0;offset<buffer.getNumSamples();offset+=blockCapacity){
        const int count=juce::jmin(blockCapacity,buffer.getNumSamples()-offset);
        for(int n=0;n<count;++n){
            const float gain=inputSmooth.getNextValue();
            for(int ch=0;ch<2;++ch){
                float original=buffer.getSample(juce::jmin(ch,channels-1),offset+n);
                if(!std::isfinite(original))original=0.f;
                work.setSample(ch,n,original*gain);
                const float delayed=latency>0?delay[size_t(ch)][delayPosition]:original;
                if(latency>0)delay[size_t(ch)][delayPosition]=original;
                dry.setSample(ch,n,delayed);
                base.setSample(ch,n,latency>0?baseDelay[size_t(ch)][delayPosition]:original*gain);
                if(latency>0)baseDelay[size_t(ch)][delayPosition]=original*gain;
            }
            if(latency>0)delayPosition=(delayPosition+1)%size_t(latency);
        }
        juce::dsp::AudioBlock<float> block(work);auto active=block.getSubBlock(0,size_t(count));
        auto up=oversampling->processSamplesUp(active);
        for(size_t n=0;n<up.getNumSamples();++n){
            std::array<float,4> amounts{};for(size_t i=0;i<4;++i)amounts[i]=amountSmooth[i].getNextValue();
            for(int ch=0;ch<2;++ch){auto* samples=up.getChannelPointer(size_t(ch));const float x=samples[n];samples[n]=colours.process(ch,x,amounts)-x;}
        }
        oversampling->processSamplesDown(active);
        for(int n=0;n<count;++n){
            const float output=outputSmooth.getNextValue(),wet=wetSmooth.getNextValue();
            float inLo=0,inHi=0,outLo=0,outHi=0,activity=0;
            for(int ch=0;ch<channels;++ch){
                const float original=dry.getSample(ch,n);
                const float processed=(base.getSample(ch,n)+work.getSample(ch,n))*output;
                const float y=original+wet*(processed-original);
                buffer.setSample(ch,offset+n,y);
                if(ch==0){inLo=inHi=original;outLo=outHi=y;}else{inLo=juce::jmin(inLo,original);inHi=juce::jmax(inHi,original);outLo=juce::jmin(outLo,y);outHi=juce::jmax(outHi,y);}
                activity=juce::jmax(activity,juce::jlimit(0.f,1.f,std::abs(work.getSample(ch,n))*wet));
            }
            traceTime+=1./rate;
            if(!editorOpen.load(std::memory_order_relaxed)){captured=0;continue;}
            if(!captured)capture={inLo,inHi,outLo,outHi,1.f-activity,traceTime,traceGeneration.load()};
            else{capture.keyLo=juce::jmin(capture.keyLo,inLo);capture.keyHi=juce::jmax(capture.keyHi,inHi);capture.outLo=juce::jmin(capture.outLo,outLo);capture.outHi=juce::jmax(capture.outHi,outHi);capture.gain=juce::jmin(capture.gain,1.f-activity);capture.time=traceTime;}
            if(++captured>=decimation){int a,s,c,d;fifo.prepareToWrite(1,a,s,c,d);if(s){traces[size_t(a)]=capture;fifo.finishedWrite(1);}captured=0;}
        }
    }
    if(activated&&!hostBypass&&bypass->load()<.5f)mixDuck(buffer);
}
bool DuckPocketAudioProcessor::popTrace(PocketTrace& v){int a,s,b,c;fifo.prepareToRead(1,a,s,b,c);if(!s)return false;v=traces[size_t(a)];fifo.finishedRead(1);return true;}
void DuckPocketAudioProcessor::getStateInformation(juce::MemoryBlock& d){auto state=parameters.copyState();state.setProperty("uiWidth",editorWidth.load(),nullptr);state.setProperty("schemaVersion",1,nullptr);if(auto x=state.createXml())copyXmlToBinary(*x,d);}
void DuckPocketAudioProcessor::setStateInformation(const void* d,int n){
    if(auto x=getXmlFromBinary(d,n))if(x->hasTagName(parameters.state.getType())){
        auto state=juce::ValueTree::fromXml(*x);
        for(auto* parameter:getParameters())if(auto* ranged=dynamic_cast<juce::RangedAudioParameter*>(parameter)){
            auto node=state.getChildWithProperty("id",ranged->getParameterID());
            if(!node.isValid()){node=juce::ValueTree("PARAM");node.setProperty("id",ranged->getParameterID(),nullptr);state.appendChild(node,nullptr);node.setProperty("value",ranged->convertFrom0to1(ranged->getDefaultValue()),nullptr);}
            const float value=float(node.getProperty("value"));const auto range=ranged->getNormalisableRange();
            node.setProperty("value",std::isfinite(value)?range.snapToLegalValue(juce::jlimit(range.start,range.end,value)):ranged->convertFrom0to1(ranged->getDefaultValue()),nullptr);
        }
        editorWidth.store(juce::jlimit(400,1500,int(state.getProperty("uiWidth",800))));parameters.replaceState(state);
    }
}
juce::AudioProcessorEditor* DuckPocketAudioProcessor::createEditor(){
#if RICH_ENABLE_LICENSE
    license->checkOnOpen();
#endif
    return new DuckPocketAudioProcessorEditor(*this);
}
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter(){return new DuckPocketAudioProcessor();}
