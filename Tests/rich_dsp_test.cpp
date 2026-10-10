#include "../Source/RichDSP.h"
#include <iostream>
#include <cstdlib>
#include <vector>
void require(bool v,const char* message){if(!v){std::cerr<<"FAIL: "<<message<<'\n';std::exit(1);}}
int main(){
 const auto& lut=rich::tables();
 for(double z=-30.;z<100.;z+=.25){const auto w=rich::omega(z);require(std::abs(w+std::log(w)-z)<1.e-9,"Wright omega equation");}
 for(int c=0;c<4;++c){double previous=rich::curve(c,-8.);for(int n=1;n<=32000;++n){double x=-8.+n*.0005;double y=rich::curve(c,x);require(std::isfinite(y)&&y>=previous-1.e-10,"finite monotonic curves");require(std::abs(double(lut.residual(c,float(x)))-(y-x))<2.e-5,"LUT accuracy");previous=y;}}
 rich::Colours engine;engine.prepare(48000.);
 std::array<float,4> zero{};
 for(int n=0;n<48000;++n){float x=float(std::sin(n*.17)*4.);require(engine.process(0,x,zero)==x,"zero colours exact dry");}
 for(int c=0;c<5;++c){
  engine.prepare(48000.);std::array<float,4> amounts{};if(c<4)amounts[size_t(c)]=1.f;else amounts.fill(1.f);
  std::array<double,21> re{},im{};double mean=0.;
  for(int n=0;n<96000;++n){float x=float(std::cos(2.*3.14159265358979323846*1000.*n/48000.));float y=engine.process(0,x,amounts);require(std::isfinite(y),"finite output");if(n>=48000){mean+=y;for(int k=1;k<=20;++k){double phase=2.*3.14159265358979323846*k*1000.*n/48000.;re[size_t(k)]+=y*std::cos(phase);im[size_t(k)]-=y*std::sin(phase);}}}
  double fundamental=std::hypot(re[1],im[1]),strongest=0.;int order=0;
  for(int k=2;k<=20;++k){double amplitude=std::hypot(re[size_t(k)],im[size_t(k)])/fundamental;if(amplitude>strongest){strongest=amplitude;order=k;}}
  const double db=20.*std::log10(strongest);std::cout<<(c<4?std::array<const char*,4>{"Warm","Body","Silk","Air"}[size_t(c)]:"Combined")<<": H"<<order<<" "<<db<<" dBc; DC="<<mean/48000.<<'\n';
  if(c<4)require(db<=-23.,"single colour strongest harmonic <= -23 dBc at 0 dBFS");
  require(std::abs(mean/48000.)<1.e-4,"DC residual removed");
 }
 engine.prepare(192000.*8.);std::array<float,4> all{1,1,1,1};
 for(int n=0;n<100000;++n){float x=float(std::sin(n*.001)*16.);require(std::isfinite(engine.process(n%2,x,all)),"high amplitude stress");}
 std::cout<<"Rich DSP checks passed\n";
}
