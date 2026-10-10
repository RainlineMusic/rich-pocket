#pragma once
#include <array>
#include <algorithm>
#include <cmath>

namespace rich {
// Independent implementation of Pines, DAFx 2025, equations 5 and 16.
// Wright omega solves w + log(w) = z on the positive real branch.
inline double omega(double z) noexcept {
    if(z < -40.) return std::exp(z);
    double w=z>1.?z-std::log(z):std::exp(z);
    w=std::max(1.e-30,w);
    for(int i=0;i<8;++i){
        const double e=w+std::log(w)-z;
        const double candidate=w-e*w/(w+1.);
        w=candidate>0.?candidate:w*.5;
    }
    return w;
}
inline double curve(int colour,double x) noexcept {
    if(colour==0){
        constexpr double alpha=4.,beta=1.; // omega(1)=1, small signal gain=.5
        return (omega(alpha*x+beta)-1.)/(alpha*.5);
    }
    if(colour==1){
        constexpr double alpha=6.,beta=-2.;
        const double gain=1./(omega(beta)+1.);
        return (x-(omega(alpha*x+beta)-omega(-alpha*x+beta))/(2.*alpha))/gain;
    }
    // Integrated nonnegative derivative (1-u^2)^2: C2 soft knee.
    const double drive=colour==2?1.8:1.2,u=drive*x;
    const double q=std::abs(u)<=1.?u-(2./3.)*u*u*u+u*u*u*u*u/5.:
        std::copysign(8./15.,u);
    return q/drive;
}
struct Tables {
    static constexpr int count=32769;
    static constexpr double limit=16.;
    std::array<std::array<float,count>,4> data{};
    Tables(){for(int c=0;c<4;++c)for(int n=0;n<count;++n){const double x=-limit+2.*limit*n/(count-1);data[size_t(c)][size_t(n)]=float(curve(c,x)-x);}}
    float residual(int c,float x) const noexcept {
        if(!std::isfinite(x))return 0.f;
        if(std::abs(x)>limit)return float(curve(c,double(x))-x);
        const double position=(double(x)+limit)*(count-1)/(2.*limit);
        const int n=std::min(count-2,int(position));
        const float t=float(position-n);
        const auto& row=data[size_t(c)];
        return row[size_t(n)]+t*(row[size_t(n+1)]-row[size_t(n)]);
    }
};
// Construct tables during processor construction, never on the audio thread.
inline const Tables& tables(){static const Tables value;return value;}
struct Channel {
    double airLow=0,tintLow=0,tintHigh=0,dcInput=0,dcOutput=0;
};
class Colours {
public:
    void prepare(double rate){
        sampleRate=rate;
        airPole=std::exp(-2.*pi*900./rate);
        lowPole=std::exp(-2.*pi*350./rate);
        highPole=std::exp(-2.*pi*5000./rate);
        dcPole=std::exp(-2.*pi*3./rate);
        reset();(void)tables();
    }
    void reset(){channels={};}
    float process(int channel,float input,const std::array<float,4>& amounts) noexcept {
        const double x=std::isfinite(input)?input:0.;auto& s=channels[size_t(channel)];
        s.airLow=airPole*s.airLow+(1.-airPole)*x;
        const double upper=x-s.airLow;
        const auto& lut=tables();
        const double a=amounts[0],b=amounts[1],c=amounts[2],d=amounts[3];
        // Separate modest residuals; a gentle feed into Body creates interactions.
        const double warm=.23*a*lut.residual(0,float(x));
        const double body=.65*b*lut.residual(1,float(x+.18*warm));
        const double silk=.30*c*lut.residual(2,float(x));
        const double air=.35*d*lut.residual(3,float(upper));
        // Bound total residual mix to preserve a positive transfer slope at
        // extreme input levels even when every colour is fully enabled.
        const double density=.23*a+.65*b+.30*c+.35*d;
        const double budget=density>.90?.90/density:1.;
        double residual=budget*(warm+body+silk+air);
        // DC block only the colour residual. Dry stays untouched at zero Amount.
        const double blocked=residual-s.dcInput+dcPole*s.dcOutput;
        s.dcInput=residual;s.dcOutput=blocked;residual=blocked;
        const double coloured=x+residual;
        s.tintLow=lowPole*s.tintLow+(1.-lowPole)*coloured;
        s.tintHigh=highPole*s.tintHigh+(1.-highPole)*coloured;
        // Shared +/-1.5 dB shelf asymptote budget, including combined settings.
        const double sum=std::max(1.,a+b+c+d);
        const double lowDb=(a+.45*b-.20*d)/sum;
        const double highDb=(-.65*c+1.5*d-.20*a)/sum;
        const double out=coloured+(std::pow(10.,lowDb/20.)-1.)*s.tintLow+
            (std::pow(10.,highDb/20.)-1.)*(coloured-s.tintHigh);
        return std::isfinite(out)?float(out):0.f;
    }
private:
    static constexpr double pi=3.14159265358979323846;
    double sampleRate=48000.,airPole=0,lowPole=0,highPole=0,dcPole=0;
    std::array<Channel,2> channels{};
};
}
