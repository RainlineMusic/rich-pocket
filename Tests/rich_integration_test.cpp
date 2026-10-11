#include "PluginEditor.h"
#include "PocketSnapshot.h"
#include <iostream>
#include <cstdlib>
#include <vector>
struct DuckUiTestAccess {
 static void theme(DuckPocketAudioProcessorEditor& e,PocketTheme t){e.setTheme(t,false);}
 static void tick(DuckPocketAudioProcessorEditor& e){e.resizeStamp=0;e.frameTick();}
 static bool layout(DuckPocketAudioProcessorEditor& e){
  juce::Component* controls[]{&e.influence,&e.duration,&e.attack,&e.midSide,&e.inputDial,&e.outputDial,&e.settingsButton,&e.bypassButton};
  for(auto* c:controls)if(!c->isVisible()||!e.getLocalBounds().contains(c->getBounds()))return false;
  for(size_t i=0;i<8;++i)for(size_t j=i+1;j<8;++j)if(controls[i]->getBounds().intersects(controls[j]->getBounds()))return false;
  return !e.activationPanel.isVisible()&&e.influence.getMaximum()==100&&e.inputDial.getMinimum()==-12&&e.outputDial.getMaximum()==12;
 }
};
void check(bool ok,const char* msg){if(!ok){std::cerr<<"FAIL: "<<msg<<'\n';std::exit(1);}}
void set(DuckPocketAudioProcessor& p,const char* id,float value){auto* param=p.parameters.getParameter(id);param->setValueNotifyingHost(param->convertTo0to1(value));}
int main(int argc,char** argv){
 std::cout<<std::unitbuf;std::cerr<<std::unitbuf;
 juce::ScopedJuceInitialiser_GUI gui;
 DuckPocketAudioProcessor p;check(p.isActivated(),"development authorization disabled");
 const char* colours[]{"warm","body","silk","air"};
 for(double rate:{44100.,48000.,96000.,192000.}){
  p.prepareToPlay(rate,64);int latency=p.getLatencySamples();check(latency>=0&&latency<128,"oversampling latency");
  juce::AudioBuffer<float> b(2,257);juce::MidiBuffer midi;std::vector<float> input;
  for(int block=0;block<4;++block){for(int n=0;n<257;++n){float v=float(std::sin((block*257+n)*.17)*.8);input.push_back(v);b.setSample(0,n,v);b.setSample(1,n,v);}p.processBlock(b,midi);for(int n=0;n<257;++n){int source=block*257+n-latency;float expected=source>=0?input[size_t(source)]:0.f;const bool matches=std::abs(b.getSample(0,n)-expected)<=2.f*std::numeric_limits<float>::epsilon()*juce::jmax(1.e-4f,std::abs(expected));if(!matches)std::cerr<<"Dry mismatch: rate="<<rate<<" index="<<block*257+n<<" latency="<<latency<<" expected="<<std::hexfloat<<expected<<" actual="<<b.getSample(0,n)<<std::defaultfloat<<"\n";check(matches,"zero colours transparent within float roundoff with latency and oversized blocks");}}
  for(auto* id:colours)set(p,id,100);
  for(int block=0;block<30;++block){for(int n=0;n<257;++n){float v=float(std::sin((block*257+n)*.07)*4.);b.setSample(0,n,v);b.setSample(1,n,v);}p.processBlock(b,midi);for(int n=0;n<257;++n){check(std::isfinite(b.getSample(0,n)),"finite full colours");check(b.getSample(0,n)==b.getSample(1,n),"stereo identity");}}
  for(auto* id:colours)set(p,id,0);
 }
 // Spectral calibration through actual JUCE 8x oversampling, including tint.
 constexpr int fs=48000,N=48000;juce::MidiBuffer midi;
 for(int c=0;c<5;++c){for(int j=0;j<4;++j)set(p,colours[j],c==4||j==c?100.f:0.f);p.prepareToPlay(fs,128);
  std::array<double,21> re{},im{};
  juce::AudioBuffer<float> b(2,128);
  for(int offset=0;offset<2*N;offset+=128){int count=juce::jmin(128,2*N-offset);b.setSize(2,count,false,false,true);for(int n=0;n<count;++n){float x=float(std::cos(juce::MathConstants<double>::twoPi*1000.*(offset+n)/fs));b.setSample(0,n,x);b.setSample(1,n,x);}p.processBlock(b,midi);for(int n=0;n<count;++n)if(offset+n>=N)for(int k=1;k<=20;++k){double phase=juce::MathConstants<double>::twoPi*k*1000.*(offset+n)/fs;re[size_t(k)]+=b.getSample(0,n)*std::cos(phase);im[size_t(k)]-=b.getSample(0,n)*std::sin(phase);}}
  double fundamental=std::hypot(re[1],im[1]),strongest=0.;int order=0;for(int k=2;k<=20;++k){double v=std::hypot(re[size_t(k)],im[size_t(k)])/fundamental;if(v>strongest){strongest=v;order=k;}}
  double db=20.*std::log10(strongest);std::cout<<(c<4?colours[c]:"Combined")<<" H"<<order<<": "<<db<<" dBc\n";if(c<4)check(db<=-23.,"single colour spectral ceiling");
 }
 // Both bypass paths must ignore non-unity input/output gains after settling.
 set(p,"inputGain",12);set(p,"outputGain",-12);
 for(bool host:{false,true}){
  set(p,"bypass",host?0.f:1.f);p.prepareToPlay(48000.,128);
  juce::AudioBuffer<float> test(2,128);std::vector<float> originals;
  for(int block=0;block<20;++block){for(int n=0;n<128;++n){float x=float(std::sin((block*128+n)*.13)*.5);originals.push_back(x);test.setSample(0,n,x);test.setSample(1,n,x);}if(host)p.processBlockBypassed(test,midi);else p.processBlock(test,midi);
   if(block>5)for(int n=0;n<128;++n)check(test.getSample(0,n)==originals[size_t(block*128+n-p.getLatencySamples())],"bypass preserves unity delayed dry");
  }
 }
 set(p,"bypass",0);
 DuckPocketAudioProcessor mono;auto buses=mono.getBusesLayout();buses.inputBuses.set(0,juce::AudioChannelSet::mono());buses.outputBuses.set(0,juce::AudioChannelSet::mono());check(mono.setBusesLayout(buses),"mono layout supported");mono.prepareToPlay(48000.,64);set(mono,"warm",100);juce::AudioBuffer<float> monoBuffer(1,64);for(int n=0;n<64;++n)monoBuffer.setSample(0,n,float(std::sin(n*.15)));mono.processBlock(monoBuffer,midi);for(int n=0;n<64;++n)check(std::isfinite(monoBuffer.getSample(0,n)),"mono processing finite");
 set(p,"inputGain",6);set(p,"outputGain",-3);juce::MemoryBlock state;p.getStateInformation(state);DuckPocketAudioProcessor restored;restored.setStateInformation(state.getData(),int(state.getSize()));check(std::abs(restored.parameters.getRawParameterValue("inputGain")->load()-6.f)<1.e-5f,"input state");check(std::abs(restored.parameters.getRawParameterValue("outputGain")->load()+3.f)<1.e-5f,"output state");
 set(p,"inputGain",0);set(p,"outputGain",0);for(int i=0;i<4;++i)set(p,colours[i],float(25+i*15));
 p.prepareToPlay(48000.,128);std::unique_ptr<DuckPocketAudioProcessorEditor> editor(static_cast<DuckPocketAudioProcessorEditor*>(p.createEditor()));
 juce::File output=argc>1?juce::File(argv[1]):juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("rich-ui-captures");check(output.createDirectory().wasOk(),"create UI capture directory");std::cout<<"UI captures: "<<output.getFullPathName()<<"\n";
 const char* themes[]{"dark","neon","amber"};PocketTheme values[]{PocketTheme::SolidDark,PocketTheme::Neon,PocketTheme::Amber};
 for(int t=0;t<3;++t){DuckUiTestAccess::theme(*editor,values[t]);for(int width:{400,800,1200}){editor->setSize(width,juce::roundToInt(width*792./800.));check(DuckUiTestAccess::layout(*editor),"six dials fit without overlap");auto image=pocket::softwareSnapshot(*editor,editor->getLocalBounds());auto stream=output.getChildFile(juce::String(themes[t])+"-"+juce::String(width)+".png").createOutputStream();check(stream&&juce::PNGImageFormat().writeImageToStream(image,*stream),"UI capture");}}
 set(p,"bypass",1);juce::AudioBuffer<float> b(2,128);for(int block=0;block<40;++block){b.clear();p.processBlock(b,midi);}DuckUiTestAccess::tick(*editor);
 {auto image=pocket::softwareSnapshot(*editor,editor->getLocalBounds());auto stream=output.getChildFile("bypass.png").createOutputStream();check(stream&&juce::PNGImageFormat().writeImageToStream(image,*stream),"bypass capture");}
 std::cout<<"Rich integration and UI checks passed\n";
}
