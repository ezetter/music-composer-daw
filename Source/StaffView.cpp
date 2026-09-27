#include "StaffView.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace
{
    // The size of a staff space in pixels. All the other dimensions here are in staff spaces.
    constexpr float staffSpace = 12.0f;

    // Horizontal layout
    constexpr float systemLeft = 3.0f;    // where the staff lines start, just right of the brace
    constexpr float clefLeft = systemLeft + 0.8f;
    constexpr float timeSignatureLeft = clefLeft + 4.0f;
    constexpr float firstMeasureLeft = timeSignatureLeft + 3.0f;
    constexpr float measureWidth = 20.0f;
    constexpr float beatWidth = measureWidth / (float) Score::beatsPerMeasure;
    constexpr float rightMargin = 2.0f;

    // Vertical layout
    constexpr float staffHeight = 4.0f;
    constexpr float staffGap = 7.0f;      // from the treble staff's bottom line to the bass staff's top line
    constexpr float marginAbove = 6.0f;
    constexpr float marginBelow = 6.0f;

    // Line thicknesses and notehead metrics, from Bravura's metadata
    constexpr float staffLineThickness = 0.13f;
    constexpr float thinBarlineThickness = 0.16f;
    constexpr float thickBarlineThickness = 0.5f;
    constexpr float thinThickBarlineSeparation = 0.4f;
    constexpr float legerLineThickness = 0.16f;
    constexpr float legerLineExtension = 0.4f;
    constexpr float stemThickness = 0.12f;
    constexpr float stemLength = 3.5f;
    constexpr float noteheadWidth = 1.18f;
    constexpr float stemAttachmentOffset = 0.168f;    // from a notehead's centre to where its stem joins

    // A staff position counts lines and spaces up from the staff's bottom line: 0 is the bottom
    // line, 1 the space above it, 8 the top line, and negative positions are below the staff.
    constexpr int middleLine = 4;
    constexpr int topLine = 8;

    // How far clicks can reach on each staff. There's room for five ledger lines above and below
    // the system, and clicks between the staves go to the nearer one.
    constexpr int maxLedgerLines = 5;
    constexpr int highestTreblePosition = topLine + 2 * maxLedgerLines;    // B6
    constexpr int lowestTreblePosition = -(int) staffGap;                   // E3
    constexpr int highestBassPosition = topLine + (int) staffGap;           // A4
    constexpr int lowestBassPosition = -2 * maxLedgerLines;                 // D1

    const juce::Colour inkColour { 0xff1b1b1b };
    const juce::Colour hoverColour { 0x992f7de1 };
    const juce::Colour measureNumberColour { 0xff8c8c8c };

    int getBottomLinePitch (Staff staff)
    {
        return staff == Staff::treble ? 30     // E4
                                      : 18;    // G2
    }

    float getMeasureX (int measure)
    {
        return (firstMeasureLeft + (float) measure * measureWidth) * staffSpace;
    }

    float getBeatX (int measure, int beat)
    {
        return getMeasureX (measure) + ((float) beat + 0.5f) * beatWidth * staffSpace;
    }

    int getMeasureAtX (float x)
    {
        return (int) std::floor ((x / staffSpace - firstMeasureLeft) / measureWidth);
    }
}

StaffView::StaffView (Score& scoreToShow)
    : score (scoreToShow),
      glyphs (staffSpace)
{
    setOpaque (true);
    score.addChangeListener (this);
    setSize (getContentWidth(), getContentHeight());
}

StaffView::~StaffView()
{
    score.removeChangeListener (this);
}

int StaffView::getContentWidth() const
{
    return (int) std::ceil (getMeasureX (score.getNumMeasures()) + rightMargin * staffSpace);
}

int StaffView::getContentHeight()
{
    return (int) std::ceil ((marginAbove + staffHeight + staffGap + staffHeight + marginBelow) * staffSpace);
}

float StaffView::getStaffTop (Staff staff) const
{
    const auto trebleTop = std::round ((float) juce::jmax (0, getHeight() - getContentHeight()) / 2.0f)
                         + marginAbove * staffSpace;

    return staff == Staff::treble ? trebleTop
                                  : trebleTop + (staffHeight + staffGap) * staffSpace;
}

float StaffView::getY (Staff staff, int staffPosition) const
{
    return getStaffTop (staff) + (float) (topLine - staffPosition) * staffSpace / 2.0f;
}

std::optional<Note> StaffView::getNoteAt (juce::Point<float> point) const
{
    const auto measure = getMeasureAtX (point.x);

    if (! juce::isPositiveAndBelow (measure, score.getNumMeasures()))
        return {};

    const auto trebleTop = getStaffTop (Staff::treble);
    const auto bassBottom = getStaffTop (Staff::bass) + staffHeight * staffSpace;

    if (point.y < trebleTop - marginAbove * staffSpace || point.y >= bassBottom + marginBelow * staffSpace)
        return {};

    const auto beat = juce::jmin ((int) ((point.x - getMeasureX (measure)) / (beatWidth * staffSpace)),
                                  Score::beatsPerMeasure - 1);

    const auto staffDivide = trebleTop + (staffHeight + staffGap / 2.0f) * staffSpace;
    const auto staff = point.y < staffDivide ? Staff::treble : Staff::bass;
    const auto position = juce::roundToInt ((getY (staff, 0) - point.y) / (staffSpace / 2.0f));

    const auto clampedPosition = staff == Staff::treble
                                     ? juce::jlimit (lowestTreblePosition, highestTreblePosition, position)
                                     : juce::jlimit (lowestBassPosition, highestBassPosition, position);

    return Note { staff, measure, beat, getBottomLinePitch (staff) + clampedPosition };
}

//==============================================================================
void StaffView::paint (juce::Graphics& g)
{
    g.fillAll (paperColour);
    g.setColour (inkColour);

    const auto lineThickness = staffLineThickness * staffSpace;
    const auto linesLeft = systemLeft * staffSpace;
    const auto linesRight = getMeasureX (score.getNumMeasures());

    for (auto staff : { Staff::treble, Staff::bass })
        for (int line = 0; line <= topLine; line += 2)
            g.fillRect (juce::Rectangle<float> (linesLeft, getY (staff, line) - lineThickness / 2.0f,
                                                linesRight - linesLeft, lineThickness));

    drawSystemStart (g);

    // Only draw the measures that need repainting, allowing for symbols that overhang a little.
    const auto clip = g.getClipBounds().toFloat().expanded (2.0f * staffSpace, 0.0f);
    const auto firstMeasure = juce::jmax (0, getMeasureAtX (clip.getX()));
    const auto lastMeasure = juce::jmin (score.getNumMeasures() - 1, getMeasureAtX (clip.getRight()));

    for (auto measure = firstMeasure; measure <= lastMeasure; ++measure)
        drawMeasure (g, measure);

    drawHoverNote (g);
}

void StaffView::drawSystemStart (juce::Graphics& g) const
{
    const auto top = getStaffTop (Staff::treble);
    const auto bottom = getStaffTop (Staff::bass) + staffHeight * staffSpace;

    // The brace, scaled to reach from the top of the treble staff to the bottom of the bass
    const auto& brace = glyphs.getPath (Smufl::brace);
    const auto braceBounds = brace.getBounds();

    g.fillPath (brace, juce::AffineTransform::translation (-braceBounds.getRight(), -braceBounds.getBottom())
                           .scaled ((bottom - top) / braceBounds.getHeight())
                           .translated ((systemLeft - 0.4f) * staffSpace, bottom));

    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (systemLeft * staffSpace, top,
                                                            (systemLeft + thinBarlineThickness) * staffSpace, bottom));

    glyphs.draw (g, Smufl::gClef, { clefLeft * staffSpace, getY (Staff::treble, 2) });
    glyphs.draw (g, Smufl::fClef, { clefLeft * staffSpace, getY (Staff::bass, 6) });

    static_assert (Score::beatsPerMeasure == 4, "The time signature is always 4/4");

    for (auto staff : { Staff::treble, Staff::bass })
        for (auto position : { 6, 2 })
            glyphs.draw (g, Smufl::timeSig4, { timeSignatureLeft * staffSpace, getY (staff, position) });
}

void StaffView::drawMeasure (juce::Graphics& g, int measure) const
{
    const auto top = getStaffTop (Staff::treble);
    const auto bottom = getStaffTop (Staff::bass) + staffHeight * staffSpace;
    const auto right = getMeasureX (measure + 1);

    if (measure == score.getNumMeasures() - 1)
    {
        // A final barline: a thin line, then a thick one
        const auto thickLeft = right - thickBarlineThickness * staffSpace;
        const auto thinRight = thickLeft - thinThickBarlineSeparation * staffSpace;

        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (thickLeft, top, right, bottom));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (thinRight - thinBarlineThickness * staffSpace, top,
                                                                thinRight, bottom));
    }
    else
    {
        const auto thickness = thinBarlineThickness * staffSpace;
        g.fillRect (juce::Rectangle<float> (right - thickness / 2.0f, top, thickness, bottom - top));
    }

    for (auto staff : { Staff::treble, Staff::bass })
    {
        for (int beat = 0; beat < Score::beatsPerMeasure; ++beat)
            if (const auto& chord = score.getChord (staff, measure, beat); ! chord.empty())
                drawChord (g, staff, getBeatX (measure, beat), chord);

        drawRests (g, staff, measure);
    }

    g.setColour (measureNumberColour);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (measure + 1),
                juce::Rectangle<float> (getMeasureX (measure) + 2.0f, top - 2.0f * staffSpace, 40.0f, 1.5f * staffSpace),
                juce::Justification::bottomLeft, false);
    g.setColour (inkColour);
}

void StaffView::drawRests (juce::Graphics& g, Staff staff, int measure) const
{
    static_assert (Score::beatsPerMeasure == 4, "Rests are grouped for 4/4 time");

    const auto isEmpty = [&] (int beat) { return score.getChord (staff, measure, beat).empty(); };
    const auto middleY = getY (staff, middleLine);

    // A whole rest, hanging from the fourth line, fills an empty measure.
    if (isEmpty (0) && isEmpty (1) && isEmpty (2) && isEmpty (3))
    {
        drawCentred (g, Smufl::restWhole, (getMeasureX (measure) + getMeasureX (measure + 1)) / 2.0f,
                     getY (staff, middleLine + 2));
        return;
    }

    // Otherwise an empty half of the measure gets a half rest, and any other empty beat a quarter rest.
    for (auto firstBeat : { 0, 2 })
    {
        if (isEmpty (firstBeat) && isEmpty (firstBeat + 1))
        {
            drawCentred (g, Smufl::restHalf, getBeatX (measure, firstBeat), middleY);
            continue;
        }

        for (auto beat : { firstBeat, firstBeat + 1 })
            if (isEmpty (beat))
                drawCentred (g, Smufl::restQuarter, getBeatX (measure, beat), middleY);
    }
}

void StaffView::drawChord (juce::Graphics& g, Staff staff, float centreX, const std::vector<int>& pitches) const
{
    std::vector<int> positions;

    for (auto pitch : pitches)
        positions.push_back (pitch - getBottomLinePitch (staff));

    const auto lowest = positions.front();
    const auto highest = positions.back();

    // The stem points away from whichever note is furthest from the middle line.
    const auto stemUp = middleLine - lowest > highest - middleLine;

    // The two notes of a second can't share a column: the lower goes left of the stem and the
    // upper goes right. So with the stem up the upper note moves across, and with it down the lower.
    std::vector<bool> movedAcrossStem (positions.size(), false);

    if (stemUp)
    {
        for (size_t i = 1; i < positions.size(); ++i)
            movedAcrossStem[i] = positions[i] - positions[i - 1] == 1 && ! movedAcrossStem[i - 1];
    }
    else
    {
        for (auto i = positions.size() - 1; i-- > 0;)
            movedAcrossStem[i] = positions[i + 1] - positions[i] == 1 && ! movedAcrossStem[i + 1];
    }

    const auto headWidth = noteheadWidth * staffSpace;
    const auto stemWidth = stemThickness * staffSpace;
    const auto headLeft = centreX - headWidth / 2.0f;
    const auto moveAcross = stemUp ? headWidth - stemWidth : stemWidth - headWidth;

    std::vector<float> headLefts;

    for (size_t i = 0; i < positions.size(); ++i)
        headLefts.push_back (headLeft + (movedAcrossStem[i] ? moveAcross : 0.0f));

    drawLedgerLines (g, staff, positions, headLefts);

    for (size_t i = 0; i < positions.size(); ++i)
        glyphs.draw (g, Smufl::noteheadBlack, { headLefts[i], getY (staff, positions[i]) });

    // The stem reaches at least an octave past the outermost note, and at least to the middle line.
    const auto middleY = getY (staff, middleLine);

    if (stemUp)
    {
        const auto stemLeft = headLeft + headWidth - stemWidth;
        const auto stemTop = juce::jmin (getY (staff, highest) - stemLength * staffSpace, middleY);
        const auto stemBottom = getY (staff, lowest) - stemAttachmentOffset * staffSpace;

        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (stemLeft, stemTop, stemLeft + stemWidth, stemBottom));
    }
    else
    {
        const auto stemTop = getY (staff, highest) + stemAttachmentOffset * staffSpace;
        const auto stemBottom = juce::jmax (getY (staff, lowest) + stemLength * staffSpace, middleY);

        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (headLeft, stemTop, headLeft + stemWidth, stemBottom));
    }
}

void StaffView::drawLedgerLines (juce::Graphics& g, Staff staff, const std::vector<int>& staffPositions,
                                 const std::vector<float>& noteheadLefts) const
{
    const auto thickness = legerLineThickness * staffSpace;
    const auto extension = legerLineExtension * staffSpace;
    const auto headWidth = noteheadWidth * staffSpace;

    // Each ledger line runs under every notehead that's on it or further from the staff.
    const auto drawLedgerLine = [&] (int linePosition, auto isBeyondLine)
    {
        auto left = std::numeric_limits<float>::max();
        auto right = std::numeric_limits<float>::lowest();

        for (size_t i = 0; i < staffPositions.size(); ++i)
        {
            if (isBeyondLine (staffPositions[i], linePosition))
            {
                left = juce::jmin (left, noteheadLefts[i]);
                right = juce::jmax (right, noteheadLefts[i] + headWidth);
            }
        }

        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (left - extension, getY (staff, linePosition) - thickness / 2.0f,
                                                                right + extension, getY (staff, linePosition) + thickness / 2.0f));
    };

    const auto [lowest, highest] = std::minmax_element (staffPositions.begin(), staffPositions.end());

    for (auto line = topLine + 2; line <= *highest; line += 2)
        drawLedgerLine (line, [] (int position, int linePosition) { return position >= linePosition; });

    for (auto line = -2; line >= *lowest; line -= 2)
        drawLedgerLine (line, [] (int position, int linePosition) { return position <= linePosition; });
}

void StaffView::drawHoverNote (juce::Graphics& g) const
{
    if (! hoverNote.has_value() || hoverNote->measure >= score.getNumMeasures())
        return;

    const auto& chord = score.getChord (hoverNote->staff, hoverNote->measure, hoverNote->beat);

    if (std::find (chord.begin(), chord.end(), hoverNote->pitch) != chord.end())
        return;

    const auto position = hoverNote->pitch - getBottomLinePitch (hoverNote->staff);
    const auto left = getBeatX (hoverNote->measure, hoverNote->beat) - noteheadWidth * staffSpace / 2.0f;

    g.setColour (hoverColour);
    drawLedgerLines (g, hoverNote->staff, { position }, { left });
    glyphs.draw (g, Smufl::noteheadBlack, { left, getY (hoverNote->staff, position) });
}

void StaffView::drawCentred (juce::Graphics& g, juce::juce_wchar glyph, float centreX, float y) const
{
    glyphs.draw (g, glyph, { centreX - glyphs.getPath (glyph).getBounds().getCentreX(), y });
}

//==============================================================================
void StaffView::mouseMove (const juce::MouseEvent& e)
{
    setHoverNote (getNoteAt (e.position));
}

void StaffView::mouseExit (const juce::MouseEvent&)
{
    setHoverNote ({});
}

void StaffView::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        return;

    if (const auto note = getNoteAt (e.position))
        score.addNote (*note);
}

void StaffView::setHoverNote (std::optional<Note> note)
{
    if (note == hoverNote)
        return;

    if (hoverNote.has_value())
        repaint (getBeatArea (*hoverNote));

    hoverNote = note;

    if (hoverNote.has_value())
        repaint (getBeatArea (*hoverNote));
}

juce::Rectangle<int> StaffView::getBeatArea (const Note& note) const
{
    const auto width = beatWidth * staffSpace;

    return juce::Rectangle<float> (getBeatX (note.measure, note.beat) - width / 2.0f, 0.0f, width, (float) getHeight())
               .getSmallestIntegerContainer();
}

void StaffView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    setSize (getContentWidth(), getHeight());
    repaint();
}
