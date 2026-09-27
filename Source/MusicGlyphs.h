#pragma once

#include <juce_graphics/juce_graphics.h>

#include <map>

/** SMuFL code points of the music symbols we draw. */
namespace Smufl
{
    constexpr juce::juce_wchar brace         = 0xE000;
    constexpr juce::juce_wchar gClef         = 0xE050;
    constexpr juce::juce_wchar fClef         = 0xE062;
    constexpr juce::juce_wchar timeSig4      = 0xE084;
    constexpr juce::juce_wchar noteheadBlack = 0xE0A4;
    constexpr juce::juce_wchar restWhole     = 0xE4E3;
    constexpr juce::juce_wchar restHalf      = 0xE4E4;
    constexpr juce::juce_wchar restQuarter   = 0xE4E5;
}

/** Outlines of music symbols from the embedded Bravura font, sized for a given staff space.

    SMuFL fonts are drawn so that one em is four staff spaces, and each glyph's origin is where
    it sits on the staff: a notehead's origin is its vertical centre, a treble clef's is on the
    G line, and so on.
*/
class MusicGlyphs
{
public:
    explicit MusicGlyphs (float staffSpace);

    /** The glyph's outline, relative to its origin. */
    const juce::Path& getPath (juce::juce_wchar glyph) const;

    void draw (juce::Graphics&, juce::juce_wchar glyph, juce::Point<float> origin) const;

private:
    juce::Font font;
    mutable std::map<juce::juce_wchar, juce::Path> paths;
};
