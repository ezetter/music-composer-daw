#pragma once

#include <juce_graphics/juce_graphics.h>

#include <array>
#include <map>

/** SMuFL code points of the music symbols we draw. */
namespace Smufl
{
    constexpr juce::juce_wchar brace                 = 0xE000;
    constexpr juce::juce_wchar gClef                 = 0xE050;
    constexpr juce::juce_wchar fClef                 = 0xE062;
    constexpr juce::juce_wchar timeSig0              = 0xE080;
    constexpr juce::juce_wchar noteheadWhole         = 0xE0A2;
    constexpr juce::juce_wchar noteheadHalf          = 0xE0A3;
    constexpr juce::juce_wchar noteheadBlack         = 0xE0A4;
    constexpr juce::juce_wchar augmentationDot       = 0xE1E7;
    constexpr juce::juce_wchar flag8thUp             = 0xE240;
    constexpr juce::juce_wchar flag8thDown           = 0xE241;
    constexpr juce::juce_wchar flag16thUp            = 0xE242;
    constexpr juce::juce_wchar flag16thDown          = 0xE243;
    constexpr juce::juce_wchar flag32ndUp            = 0xE244;
    constexpr juce::juce_wchar flag32ndDown          = 0xE245;
    constexpr juce::juce_wchar accidentalFlat        = 0xE260;
    constexpr juce::juce_wchar accidentalNatural     = 0xE261;
    constexpr juce::juce_wchar accidentalSharp       = 0xE262;
    constexpr juce::juce_wchar accidentalDoubleSharp = 0xE263;
    constexpr juce::juce_wchar accidentalDoubleFlat  = 0xE264;
    constexpr juce::juce_wchar restWhole             = 0xE4E3;
    constexpr juce::juce_wchar restHalf              = 0xE4E4;
    constexpr juce::juce_wchar restQuarter           = 0xE4E5;
    constexpr juce::juce_wchar rest8th               = 0xE4E6;
    constexpr juce::juce_wchar rest16th              = 0xE4E7;
    constexpr juce::juce_wchar rest32nd              = 0xE4E8;
    constexpr juce::juce_wchar wiggleArpeggiatoUp    = 0xEAA9;

    /** The dynamics, from ppp to fff, in the order of music::Dynamic */
    constexpr std::array<juce::juce_wchar, 8> dynamics { 0xE52A, 0xE52B, 0xE520, 0xE52C, 0xE52D, 0xE522, 0xE52F, 0xE530 };
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
