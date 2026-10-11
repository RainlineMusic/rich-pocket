#pragma once
#include <JuceHeader.h>
namespace pocket {
// Offscreen capture must not require a native Direct2D device/peer.
inline juce::Image softwareSnapshot(juce::Component& component,juce::Rectangle<int> area,float scale=1.f){
    area=area.getIntersection(component.getLocalBounds());
    if(area.isEmpty())return {};
    const int width=juce::jmax(1,juce::roundToInt(float(area.getWidth())*scale));
    const int height=juce::jmax(1,juce::roundToInt(float(area.getHeight())*scale));
    juce::Image image(component.isOpaque()?juce::Image::RGB:juce::Image::ARGB,width,height,true,juce::SoftwareImageType());
    juce::Graphics g(image);
    g.addTransform(juce::AffineTransform::scale(float(width)/float(area.getWidth()),float(height)/float(area.getHeight())));
    g.setOrigin(-area.getPosition());
    component.paintEntireComponent(g,true);
    return image;
}
}
