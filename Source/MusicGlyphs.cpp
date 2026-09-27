#include "MusicGlyphs.h"

#include <BinaryData.h>

MusicGlyphs::MusicGlyphs (float staffSpace)
    : font (juce::FontOptions (juce::Typeface::createSystemTypefaceFor (BinaryData::Bravura_otf,
                                                                        BinaryData::Bravura_otfSize))
                .withPointHeight (4.0f * staffSpace))
{
}

const juce::Path& MusicGlyphs::getPath (juce::juce_wchar glyph) const
{
    const auto [entry, isNew] = paths.try_emplace (glyph);

    if (isNew)
    {
        juce::GlyphArrangement arrangement;
        arrangement.addLineOfText (font, juce::String::charToString (glyph), 0.0f, 0.0f);
        arrangement.createPath (entry->second);
    }

    return entry->second;
}

void MusicGlyphs::draw (juce::Graphics& g, juce::juce_wchar glyph, juce::Point<float> origin) const
{
    g.fillPath (getPath (glyph), juce::AffineTransform::translation (origin));
}
