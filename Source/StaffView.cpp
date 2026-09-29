#include "StaffView.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <set>

namespace
{
    // The size of a staff space in pixels. All the other dimensions here are in staff spaces.
    constexpr float staffSpace = 12.0f;

    // Horizontal layout
    constexpr float systemLeft = 3.0f;          // where the staff lines start, just right of the brace
    constexpr float clefLeft = systemLeft + 0.8f;
    constexpr float keySignatureLeft = clefLeft + 3.6f;
    constexpr float measurePadding = 2.5f;      // from a barline to the first beat's centre (at least), and after the last slot
    constexpr float minMeasureWidth = 12.0f;
    constexpr float rightMargin = 2.0f;

    // Vertical layout
    constexpr float staffHeight = 4.0f;
    constexpr float staffGap = 7.0f;            // from the treble staff's bottom line to the bass staff's top line
    constexpr float noteRoom = 6.0f;            // above and below the system, for clicking in notes on ledger lines

    // Above the staves: the chord names, just clear of the notes, with the chord buttons over them.
    // The margins grow when the music reaches further, and are always tall enough to click in
    // ledger-line notes.
    constexpr float minMarginAbove = noteRoom + 1.0f;
    constexpr float minMarginBelow = noteRoom;
    constexpr float chordNameHeight = 1.2f;     // from the chord name's baseline to just over its top
    constexpr float chordButtonHeight = 2.0f;

    // Line thicknesses and notehead metrics, from Bravura's metadata
    constexpr float staffLineThickness = 0.13f;
    constexpr float thinBarlineThickness = 0.16f;
    constexpr float thickBarlineThickness = 0.5f;
    constexpr float thinThickBarlineSeparation = 0.4f;
    constexpr float legerLineThickness = 0.16f;
    constexpr float legerLineExtension = 0.4f;
    constexpr float stemThickness = 0.12f;
    constexpr float stemAttachmentOffset = 0.168f;    // from a notehead's centre to where its stem joins
    constexpr float stemLength = 3.5f;
    constexpr float beamThickness = 0.5f;
    constexpr float beamSpacing = 0.25f;

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
    const juce::Colour removalColour { 0xffd8413a };
    const juce::Colour playbackColour { 0x2e4a8fe0 };
    const juce::Colour selectionColour { 0xff2f5fd0 };
    const juce::Colour numeralColour { 0xff2848b0 };
    const juce::Colour measureNumberColour { 0xff8c8c8c };

    int getBottomLineStep (Staff staff)
    {
        return staff == Staff::treble ? 30     // E4
                                      : 18;    // G2
    }

    float getBeatWidth (int notesPerBeat)
    {
        return notesPerBeat >= 4 ? 10.0f : notesPerBeat == 2 ? 6.4f : 5.0f;
    }

    bool isLine (int staffPosition)
    {
        return music::mod (staffPosition, 2) == 0;
    }

    bool hasStem (Duration duration)
    {
        return duration != Duration::whole;
    }

    int getFlagCount (Duration duration)
    {
        return duration == Duration::eighth ? 1 : duration == Duration::sixteenth ? 2 : 0;
    }

    juce::juce_wchar getNoteheadGlyph (Duration duration)
    {
        switch (duration)
        {
            case Duration::whole:        return Smufl::noteheadWhole;
            case Duration::dottedHalf:
            case Duration::half:         return Smufl::noteheadHalf;
            case Duration::quarter:
            case Duration::eighth:
            case Duration::sixteenth:    break;
        }

        return Smufl::noteheadBlack;
    }

    float getNoteheadWidth (Duration duration)
    {
        return duration == Duration::whole ? 1.688f : 1.18f;
    }

    juce::juce_wchar getRestGlyph (Duration duration)
    {
        switch (duration)
        {
            case Duration::whole:        return Smufl::restWhole;
            case Duration::dottedHalf:
            case Duration::half:         return Smufl::restHalf;
            case Duration::quarter:      return Smufl::restQuarter;
            case Duration::eighth:       return Smufl::rest8th;
            case Duration::sixteenth:    return Smufl::rest16th;
        }

        return Smufl::restQuarter;
    }

    juce::juce_wchar getAccidentalGlyph (int alter)
    {
        switch (alter)
        {
            case -2:  return Smufl::accidentalDoubleFlat;
            case -1:  return Smufl::accidentalFlat;
            case 1:   return Smufl::accidentalSharp;
            case 2:   return Smufl::accidentalDoubleSharp;
            default:  return Smufl::accidentalNatural;
        }
    }

}

//==============================================================================
struct StaffView::NoteLayout
{
    /** Lays out a note or chord. An accidental lasts to the end of the measure on its line or
        space, so a note there only shows one if it's different from what's in force.
    */
    NoteLayout (const StaffEvent& event, Staff staff, float centre, std::map<int, int>& alterationsInForce,
                const std::array<int, 7>& keyAlterations)
        : centreX (centre),
          onset (event.onset),
          duration (event.duration),
          headWidth (getNoteheadWidth (event.duration) * staffSpace)
    {
        auto tones = event.tones;
        std::sort (tones.begin(), tones.end(), [] (const music::Tone& a, const music::Tone& b)
        {
            return a.pitch.step != b.pitch.step ? a.pitch.step < b.pitch.step : a.pitch.alter < b.pitch.alter;
        });

        for (const auto& tone : tones)
        {
            const auto inForce = alterationsInForce.find (tone.pitch.step);
            const auto current = inForce != alterationsInForce.end() ? inForce->second
                                                                     : keyAlterations[(size_t) tone.pitch.getLetter()];

            positions.push_back (tone.pitch.step - getBottomLineStep (staff));
            accidentals.push_back (tone.pitch.alter != current ? std::optional<int> (tone.pitch.alter) : std::nullopt);
        }

        for (const auto& tone : tones)
            alterationsInForce[tone.pitch.step] = tone.pitch.alter;

        chooseStemDirection();
        placeNoteheads();
        placeAccidentals();
    }

    float centreX = 0.0f;
    double onset = 0.0;
    Duration duration = Duration::quarter;
    float headWidth = 0.0f;
    std::vector<int> positions;                     // low to high
    std::vector<std::optional<int>> accidentals;    // the alteration to show beside each note, if any
    std::vector<int> accidentalColumns;             // which column each accidental goes in, counting leftwards
    std::vector<float> headLefts;
    bool stemUp = true;
    bool beamed = false;
    float stemEnd = 0.0f;                           // the y where a beamed note's stem meets its beam

    float getNormalLeft() const  { return centreX - headWidth / 2.0f; }
    float getHeadsLeft() const   { return *std::min_element (headLefts.begin(), headLefts.end()); }

    /** The right edge of a column of accidentals. */
    float getAccidentalRight (int column) const
    {
        return getHeadsLeft() - (0.2f + 1.2f * (float) column) * staffSpace;
    }

    /** Roughly how far the note reaches left of its centre, with its accidentals and roll. */
    float getLeftReach (bool rolled) const
    {
        auto left = getHeadsLeft();

        if (const auto columns = *std::max_element (accidentalColumns.begin(), accidentalColumns.end()) + 1; columns > 0)
            left = getAccidentalRight (columns - 1) - staffSpace;

        if (rolled && positions.size() > 1)
            left -= 0.8f * staffSpace;

        return centreX - left;
    }

    float getStemLeft() const
    {
        return stemUp ? getNormalLeft() + headWidth - stemThickness * staffSpace : getNormalLeft();
    }

    /** The stem points away from whichever note is furthest from the middle line. */
    void chooseStemDirection()
    {
        stemUp = middleLine - positions.front() > positions.back() - middleLine;
    }

    /** The two notes of a second can't share a column: the lower goes left of the stem and the
        upper goes right. So with the stem up the upper note moves across, and with it down the
        lower one does. Whole notes, with no stem, move the upper note.
    */
    void placeNoteheads()
    {
        const auto upwards = stemUp || ! hasStem (duration);
        std::vector<bool> moved (positions.size(), false);

        if (upwards)
        {
            for (size_t i = 1; i < positions.size(); ++i)
                moved[i] = positions[i] - positions[i - 1] == 1 && ! moved[i - 1];
        }
        else
        {
            for (auto i = positions.size() - 1; i-- > 0;)
                moved[i] = positions[i + 1] - positions[i] == 1 && ! moved[i + 1];
        }

        const auto shift = ! hasStem (duration) ? headWidth
                         : stemUp ? headWidth - stemThickness * staffSpace
                                  : stemThickness * staffSpace - headWidth;

        headLefts.clear();

        for (size_t i = 0; i < positions.size(); ++i)
            headLefts.push_back (getNormalLeft() + (moved[i] ? shift : 0.0f));
    }

    /** Accidentals go in columns to the left, from the top note down, each in the first column
        where it clears the accidentals already there by at least a seventh.
    */
    void placeAccidentals()
    {
        std::vector<size_t> order;

        for (size_t i = 0; i < positions.size(); ++i)
            if (accidentals[i].has_value())
                order.push_back (i);

        std::sort (order.begin(), order.end(), [this] (size_t a, size_t b) { return positions[a] > positions[b]; });

        std::vector<std::vector<int>> columns;
        accidentalColumns.assign (positions.size(), -1);

        for (auto index : order)
        {
            const auto position = positions[index];
            size_t column = 0;

            while (column < columns.size()
                   && std::any_of (columns[column].begin(), columns[column].end(), [position] (int p) { return std::abs (p - position) < 6; }))
                ++column;

            if (column == columns.size())
                columns.emplace_back();

            columns[column].push_back (position);
            accidentalColumns[index] = (int) column;
        }
    }
};

//==============================================================================
StaffView::StaffView (Score& scoreToShow, int partToShow)
    : score (scoreToShow),
      part (partToShow),
      glyphs (staffSpace)
{
    setOpaque (true);
    score.addChangeListener (this);
    updateLayout();
    setSize (getContentWidth(), getContentHeight());
}

StaffView::~StaffView()
{
    score.removeChangeListener (this);
}

int StaffView::getContentWidth() const
{
    const auto& last = measureLayouts.back();
    return (int) std::ceil (last.x + last.width + rightMargin * staffSpace);
}

int StaffView::getContentHeight() const
{
    return (int) std::ceil ((getMarginAbove() + staffHeight + staffGap + staffHeight + getMarginBelow()) * staffSpace);
}

juce::Range<int> StaffView::getStavesRange() const
{
    return { juce::roundToInt (getStaffTop (Staff::treble)), juce::roundToInt (getStaffTop (Staff::bass) + staffHeight * staffSpace) };
}

float StaffView::getSymbolOffset() const
{
    return juce::jmax (2.0f, reachAbove + 1.8f);
}

float StaffView::getChordButtonOffset() const
{
    return getSymbolOffset() + chordNameHeight + chordButtonHeight;
}

float StaffView::getNumeralOffset() const
{
    return juce::jmax (4.0f, reachBelow + 3.4f);
}

float StaffView::getMarginAbove() const
{
    return juce::jmax (minMarginAbove, getChordButtonOffset() + 0.4f);
}

float StaffView::getMarginBelow() const
{
    return juce::jmax (minMarginBelow, getNumeralOffset() + 1.4f);
}

void StaffView::setNoteLength (int beats)
{
    noteLength = juce::jlimit (1, Score::maxNoteLength, beats);
    setHoverNote ({});
}

void StaffView::setActive (bool shouldBeActive)
{
    if (shouldBeActive == active)
        return;

    active = shouldBeActive;
    repaint();
}

void StaffView::setTitle (const juce::String& newTitle)
{
    if (newTitle == title)
        return;

    title = newTitle;
    repaint();
}

void StaffView::setSelectedMeasure (std::optional<int> measure)
{
    if (measure == selectedMeasure)
        return;

    if (selectedMeasure.has_value() && *selectedMeasure < (int) measureLayouts.size())
        repaint (getMeasureBox (*selectedMeasure).toNearestInt().expanded (2));

    selectedMeasure = measure;

    if (selectedMeasure.has_value())
        repaint (getMeasureBox (*selectedMeasure).toNearestInt().expanded (2));

    updateChordButtons();
}

//==============================================================================
void StaffView::updateLayout()
{
    const auto keyAlterations = music::getKeyAlterations (score.getKey());
    const auto numAccidentals = (int) std::count_if (keyAlterations.begin(), keyAlterations.end(), [] (int a) { return a != 0; });
    const auto sharps = std::any_of (keyAlterations.begin(), keyAlterations.end(), [] (int a) { return a > 0; });
    const auto keySignatureWidth = (float) numAccidentals * (sharps ? 1.05f : 0.95f);

    timeSignatureLeft = keySignatureLeft + (numAccidentals > 0 ? keySignatureWidth + 0.9f : 0.4f);
    firstMeasureLeft = timeSignatureLeft + 3.0f;

    const auto beats = (float) score.getBeatsPerMeasure();
    auto x = firstMeasureLeft * staffSpace;

    measureLayouts.clear();
    reachAbove = 0.0f;
    reachBelow = 0.0f;

    for (int measure = 0; measure < score.getNumMeasures(); ++measure)
    {
        MeasureLayout layout;
        layout.content = getMeasureContent (score, part, measure);

        // Every part's measure is spaced the same, to fit what's in all of them, so their beats line up.
        auto notesPerBeat = 1;
        layout.padding = measurePadding * staffSpace;

        for (int spacedPart = 0; spacedPart < Score::numParts; ++spacedPart)
        {
            const auto content = spacedPart == part ? layout.content : getMeasureContent (score, spacedPart, measure);
            notesPerBeat = juce::jmax (notesPerBeat, content.staves[0].notesPerBeat, content.staves[1].notesPerBeat);

            // Leave room after the barline for the accidentals, displaced notes and rolls of the
            // measure's first notes.
            for (auto staff : { Staff::treble, Staff::bass })
            {
                const auto& events = content.staves[(size_t) staff].events;

                if (! events.empty() && ! events.front().isRest() && ! events.front().centred)
                {
                    std::map<int, int> alterationsInForce;
                    const NoteLayout first (events.front(), staff, 0.0f, alterationsInForce, keyAlterations);
                    layout.padding = juce::jmax (layout.padding, first.getLeftReach (events.front().rolled) + 0.9f * staffSpace);
                }
            }
        }

        layout.beatWidth = getBeatWidth (notesPerBeat) * staffSpace;

        layout.width = juce::jmax (minMeasureWidth * staffSpace,
                                   layout.padding + measurePadding * staffSpace + (beats - 1.0f / (float) notesPerBeat) * layout.beatWidth);
        layout.x = x;
        x += layout.width;

        // How far the notes and their stems reach past the outer staves, which sets where the
        // chord names and numerals go. Stems run an octave from the note furthest in their
        // direction, and arpeggios may be beamed either way, so they get room for both.
        for (auto staff : { Staff::treble, Staff::bass })
        {
            const auto& content = layout.content.staves[(size_t) staff];
            const auto melodic = content.source == StaffContent::Source::chord && music::isMelodic (layout.content.chordStyle.type);

            for (const auto& event : content.events)
            {
                if (event.isRest())
                    continue;

                auto highest = std::numeric_limits<int>::min();
                auto lowest = std::numeric_limits<int>::max();

                for (const auto& tone : event.tones)
                {
                    highest = juce::jmax (highest, tone.pitch.step - getBottomLineStep (staff));
                    lowest = juce::jmin (lowest, tone.pitch.step - getBottomLineStep (staff));
                }

                if (melodic)
                {
                    highest += 7;
                    lowest -= 7;
                }
                else if (hasStem (event.duration))
                {
                    if (middleLine - lowest > highest - middleLine)
                        highest += 7;
                    else
                        lowest -= 7;
                }

                if (staff == Staff::treble)
                    reachAbove = juce::jmax (reachAbove, (float) (highest - topLine) / 2.0f);
                else
                    reachBelow = juce::jmax (reachBelow, (float) -lowest / 2.0f);
            }
        }

        measureLayouts.push_back (std::move (layout));
    }
}

void StaffView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    updateLayout();

    if (selectedMeasure.has_value() && *selectedMeasure >= score.getNumMeasures())
        selectedMeasure.reset();

    // The height follows how far the music reaches above and below the staves.
    setSize (getContentWidth(), getContentHeight());
    updateChordButtons();
    repaint();
}

//==============================================================================
float StaffView::getStaffTop (Staff staff) const
{
    const auto trebleTop = std::round ((float) juce::jmax (0, getHeight() - getContentHeight()) / 2.0f)
                         + getMarginAbove() * staffSpace;

    return staff == Staff::treble ? trebleTop
                                  : trebleTop + (staffHeight + staffGap) * staffSpace;
}

float StaffView::getY (Staff staff, int staffPosition) const
{
    return getStaffTop (staff) + (float) (topLine - staffPosition) * staffSpace / 2.0f;
}

float StaffView::getOnsetX (int measure, double onset) const
{
    const auto& layout = measureLayouts[(size_t) measure];
    return layout.x + layout.padding + (float) onset * layout.beatWidth;
}

float StaffView::getEventX (int measure, const StaffEvent& event) const
{
    const auto& layout = measureLayouts[(size_t) measure];
    return event.centred ? layout.x + layout.width / 2.0f : getOnsetX (measure, event.onset);
}

int StaffView::findMeasure (float x) const
{
    const auto after = std::upper_bound (measureLayouts.begin(), measureLayouts.end(), x,
                                         [] (float value, const MeasureLayout& layout) { return value < layout.x; });

    if (after == measureLayouts.begin())
        return -1;

    const auto measure = (int) std::distance (measureLayouts.begin(), after) - 1;
    const auto& layout = measureLayouts[(size_t) measure];
    return x < layout.x + layout.width ? measure : -1;
}

juce::Rectangle<int> StaffView::getBeatArea (int measure, int beat) const
{
    if (! juce::isPositiveAndBelow (measure, (int) measureLayouts.size()))
        return {};

    const auto width = measureLayouts[(size_t) measure].beatWidth;

    return juce::Rectangle<float> (getOnsetX (measure, beat) - width / 2.0f, 0.0f, width, (float) getHeight())
               .getSmallestIntegerContainer();
}

juce::Rectangle<float> StaffView::getMeasureBox (int measure) const
{
    const auto& layout = measureLayouts[(size_t) measure];
    const auto top = getStaffTop (Staff::treble) - (getMarginAbove() - 0.4f) * staffSpace;
    const auto bottom = getStaffTop (Staff::bass) + (staffHeight + getMarginBelow() - 0.4f) * staffSpace;

    return { layout.x + 0.3f * staffSpace, top, layout.width - 0.6f * staffSpace, bottom - top };
}

float StaffView::getSymbolBaseline() const
{
    return getStaffTop (Staff::treble) - getSymbolOffset() * staffSpace;
}

float StaffView::getNumeralBaseline() const
{
    return getStaffTop (Staff::bass) + (staffHeight + getNumeralOffset()) * staffSpace;
}

//==============================================================================
std::optional<int> StaffView::getMeasureAt (juce::Point<float> point) const
{
    const auto top = getStaffTop (Staff::treble) - getMarginAbove() * staffSpace;
    const auto bottom = getStaffTop (Staff::bass) + (staffHeight + getMarginBelow()) * staffSpace;

    if (point.y < top || point.y >= bottom)
        return {};

    if (const auto measure = findMeasure (point.x); measure >= 0)
        return measure;

    return {};
}

juce::Rectangle<int> StaffView::getMeasureArea (int measure) const
{
    const auto& layout = measureLayouts[(size_t) measure];
    return juce::Rectangle<float> (layout.x, 0.0f, layout.width, (float) getHeight()).getSmallestIntegerContainer();
}

std::optional<Note> StaffView::getNoteAt (juce::Point<float> point) const
{
    const auto measure = findMeasure (point.x);

    if (measure < 0)
        return {};

    const auto trebleTop = getStaffTop (Staff::treble);
    const auto bassBottom = getStaffTop (Staff::bass) + staffHeight * staffSpace;

    if (point.y < trebleTop - noteRoom * staffSpace || point.y >= bassBottom + noteRoom * staffSpace)
        return {};

    const auto& layout = measureLayouts[(size_t) measure];
    const auto beat = juce::jlimit (0, score.getBeatsPerMeasure() - 1,
                                    (int) std::floor ((point.x - layout.x - layout.padding + layout.beatWidth / 2.0f)
                                                      / layout.beatWidth));

    const auto staffDivide = trebleTop + (staffHeight + staffGap / 2.0f) * staffSpace;
    const auto staff = point.y < staffDivide ? Staff::treble : Staff::bass;

    const auto position = juce::roundToInt ((getY (staff, 0) - point.y) / (staffSpace / 2.0f));
    const auto clampedPosition = staff == Staff::treble
                                     ? juce::jlimit (lowestTreblePosition, highestTreblePosition, position)
                                     : juce::jlimit (lowestBassPosition, highestBassPosition, position);

    // The note takes its sharp or flat from the key signature.
    const auto step = getBottomLineStep (staff) + clampedPosition;
    const auto alter = music::getKeyAlterations (score.getKey())[(size_t) music::mod (step, 7)];

    return Note { staff, measure, beat, { step, alter }, part, noteLength };
}

void StaffView::setPlaybackPosition (std::optional<double> beats)
{
    const auto beat = beats.has_value() ? std::optional<int> ((int) *beats) : std::nullopt;

    if (beat == playingBeat)
        return;

    repaint (getPlaybackArea());
    playingBeat = beat;
    repaint (getPlaybackArea());
}

juce::Rectangle<int> StaffView::getPlaybackArea() const
{
    if (! playingBeat.has_value())
        return {};

    const auto beats = score.getBeatsPerMeasure();
    return getBeatArea (*playingBeat / beats, *playingBeat % beats);
}

//==============================================================================
void StaffView::paint (juce::Graphics& g)
{
    g.fillAll (paperColour);

    // Only the measures that need repainting are drawn, allowing for symbols that overhang a little.
    const auto clip = g.getClipBounds().toFloat().expanded (3.0f * staffSpace, 0.0f);
    const auto firstMeasure = juce::jmax (0, findMeasure (juce::jmax (clip.getX(), measureLayouts.front().x)));
    auto lastMeasure = findMeasure (clip.getRight());

    if (lastMeasure < 0)
        lastMeasure = score.getNumMeasures() - 1;

    for (auto measure = firstMeasure; measure <= lastMeasure; ++measure)
    {
        if (measure == selectedMeasure)
        {
            const auto box = getMeasureBox (measure);
            g.setColour (selectionColour.withAlpha (0.07f));
            g.fillRoundedRectangle (box, 0.7f * staffSpace);
            g.setColour (selectionColour);
            g.drawRoundedRectangle (box.reduced (1.0f), 0.7f * staffSpace, 2.0f);
        }
    }

    if (playingBeat.has_value())
    {
        const auto area = getPlaybackArea().toFloat()
                              .withTop (getStaffTop (Staff::treble) - 2.0f * staffSpace)
                              .withBottom (getStaffTop (Staff::bass) + (staffHeight + 2.0f) * staffSpace);

        g.setColour (playbackColour);
        g.fillRoundedRectangle (area.reduced (0.5f * staffSpace, 0.0f), 0.5f * staffSpace);
    }

    g.setColour (inkColour);

    const auto lineThickness = staffLineThickness * staffSpace;
    const auto linesLeft = systemLeft * staffSpace;
    const auto linesRight = measureLayouts.back().x + measureLayouts.back().width;

    for (auto staff : { Staff::treble, Staff::bass })
        for (int line = 0; line <= topLine; line += 2)
            g.fillRect (juce::Rectangle<float> (linesLeft, getY (staff, line) - lineThickness / 2.0f,
                                                linesRight - linesLeft, lineThickness));

    drawHeader (g);

    for (auto measure = firstMeasure; measure <= lastMeasure; ++measure)
        drawMeasure (g, measure);

    drawHoverNote (g);
}

void StaffView::drawHeader (juce::Graphics& g) const
{
    const auto top = getStaffTop (Staff::treble);
    const auto bottom = getStaffTop (Staff::bass) + staffHeight * staffSpace;

    // The part's name at the top left, over the clefs, and a bar beside the active part's staves
    const auto titleTop = top - getChordButtonOffset() * staffSpace;
    const auto titleRight = chordButtons.isEmpty() ? firstMeasureLeft * staffSpace : (float) chordButtons.getFirst()->getX() - 0.5f * staffSpace;
    g.setColour (active ? selectionColour : measureNumberColour);
    g.setFont (juce::FontOptions (13.0f, active ? juce::Font::bold : juce::Font::plain));
    g.drawFittedText (title, juce::Rectangle<float> (systemLeft * staffSpace, titleTop, titleRight - systemLeft * staffSpace, 2.0f * staffSpace).toNearestInt(),
                      juce::Justification::centredLeft, 1, 0.8f);

    if (active)
        g.fillRoundedRectangle (juce::Rectangle<float>::leftTopRightBottom (0.5f * staffSpace, top, 0.85f * staffSpace, bottom), 0.15f * staffSpace);

    g.setColour (inkColour);

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

    // The key signature, in the standard order and positions (these are the treble staff's;
    // the bass staff's are a third lower)
    const auto keyAlterations = music::getKeyAlterations (score.getKey());
    const auto sharps = std::any_of (keyAlterations.begin(), keyAlterations.end(), [] (int a) { return a > 0; });
    const std::array<int, 7> sharpLetters { 3, 0, 4, 1, 5, 2, 6 }, sharpPositions { 8, 5, 9, 6, 3, 7, 4 };
    const std::array<int, 7> flatLetters { 6, 2, 5, 1, 4, 0, 3 }, flatPositions { 4, 7, 3, 6, 2, 5, 1 };

    const auto& letters = sharps ? sharpLetters : flatLetters;
    const auto& positions = sharps ? sharpPositions : flatPositions;
    const auto advance = (sharps ? 1.05f : 0.95f) * staffSpace;

    for (auto staff : { Staff::treble, Staff::bass })
        for (size_t i = 0; i < letters.size() && keyAlterations[(size_t) letters[i]] != 0; ++i)
            glyphs.draw (g, sharps ? Smufl::accidentalSharp : Smufl::accidentalFlat,
                         { keySignatureLeft * staffSpace + (float) i * advance,
                           getY (staff, positions[i] - (staff == Staff::bass ? 2 : 0)) });

    for (auto staff : { Staff::treble, Staff::bass })
    {
        glyphs.draw (g, (juce::juce_wchar) (Smufl::timeSig0 + score.getBeatsPerMeasure()), { timeSignatureLeft * staffSpace, getY (staff, 6) });
        glyphs.draw (g, (juce::juce_wchar) (Smufl::timeSig0 + 4), { timeSignatureLeft * staffSpace, getY (staff, 2) });
    }
}

void StaffView::drawMeasure (juce::Graphics& g, int measure) const
{
    const auto& layout = measureLayouts[(size_t) measure];
    const auto top = getStaffTop (Staff::treble);
    const auto bottom = getStaffTop (Staff::bass) + staffHeight * staffSpace;
    const auto right = layout.x + layout.width;

    g.setColour (inkColour);

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
        drawStaff (g, measure, staff);

    drawLabels (g, measure);

    g.setColour (measureNumberColour);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (measure + 1),
                juce::Rectangle<float> (layout.x + 2.0f, top - 1.7f * staffSpace, 40.0f, 1.4f * staffSpace),
                juce::Justification::bottomLeft, false);
}

void StaffView::drawStaff (juce::Graphics& g, int measure, Staff staff) const
{
    const auto& content = measureLayouts[(size_t) measure].content.staves[(size_t) staff];
    const auto keyAlterations = music::getKeyAlterations (score.getKey());
    const auto beamable = content.notesPerBeat > 1;

    std::map<int, int> alterationsInForce;
    std::vector<NoteLayout> notes;
    std::vector<bool> rolled;
    std::vector<std::vector<size_t>> beamGroups;
    auto groupBeat = -1;

    notes.reserve (content.events.size());

    for (const auto& event : content.events)
    {
        if (event.isRest())
        {
            drawCentred (g, getRestGlyph (event.duration), getEventX (measure, event),
                         getY (staff, event.duration == Duration::whole ? middleLine + 2 : middleLine));
            groupBeat = -1;
            continue;
        }

        notes.emplace_back (event, staff, getEventX (measure, event), alterationsInForce, keyAlterations);
        rolled.push_back (event.rolled);

        // Eighths and sixteenths are beamed a beat at a time.
        if (beamable)
        {
            const auto beat = (int) std::floor (event.onset);

            if (beat == groupBeat)
                beamGroups.back().push_back (notes.size() - 1);
            else
                beamGroups.push_back ({ notes.size() - 1 });

            groupBeat = beat;
        }
    }

    const auto beamCount = content.notesPerBeat >= 4 ? 2 : 1;
    std::vector<std::vector<NoteLayout*>> beams;

    for (const auto& group : beamGroups)
    {
        if (group.size() < 2)
            continue;

        std::vector<NoteLayout*> beamed;

        for (auto index : group)
            beamed.push_back (&notes[index]);

        // The group's stems all point the same way, away from its note furthest from the middle
        // line, and the beam slopes at most a staff space.
        auto lowest = std::numeric_limits<int>::max();
        auto highest = std::numeric_limits<int>::min();

        for (const auto* note : beamed)
        {
            lowest = juce::jmin (lowest, note->positions.front());
            highest = juce::jmax (highest, note->positions.back());
        }

        const auto stemUp = middleLine - lowest > highest - middleLine;
        const auto direction = stemUp ? -1.0f : 1.0f;

        for (auto* note : beamed)
        {
            note->stemUp = stemUp;
            note->beamed = true;
            note->placeNoteheads();
        }

        const auto stemX = [] (const NoteLayout& note) { return note.getStemLeft() + stemThickness * staffSpace / 2.0f; };
        const auto nearestY = [&] (const NoteLayout& note)
        {
            return getY (staff, stemUp ? note.positions.back() : note.positions.front());
        };

        const auto& first = *beamed.front();
        const auto& last = *beamed.back();
        const auto x1 = stemX (first);
        const auto x2 = stemX (last);
        auto y1 = nearestY (first) + direction * stemLength * staffSpace;
        auto y2 = y1 + juce::jlimit (-staffSpace, staffSpace, nearestY (last) + direction * stemLength * staffSpace - y1);

        const auto beamY = [&] (float x) { return x2 > x1 ? y1 + (y2 - y1) * (x - x1) / (x2 - x1) : y1; };

        // Move the beam away from the notes until every stem is long enough.
        const auto minStem = (beamCount > 1 ? 3.0f : 2.5f) * staffSpace;
        auto shift = 0.0f;

        for (const auto* note : beamed)
            shift = juce::jmax (shift, minStem - (beamY (stemX (*note)) - nearestY (*note)) * direction);

        y1 += direction * shift;
        y2 += direction * shift;

        // A beam over notes far from the staff still reaches the middle line.
        const auto middleY = getY (staff, middleLine);
        const auto innerEdge = stemUp ? juce::jmax (y1, y2) - middleY : middleY - juce::jmin (y1, y2);

        if (innerEdge > 0.0f)
        {
            y1 += direction * innerEdge;
            y2 += direction * innerEdge;
        }

        for (auto* note : beamed)
            note->stemEnd = beamY (stemX (*note));

        beams.push_back (beamed);
    }

    // The note under the pointer is red, to show a click will take it out. A chord's
    // notes are taken out wherever they are in the measure; a quarter note only on its own beat.
    std::optional<int> highlightedPosition;

    if (hoverNote.has_value() && ! hoverHintHidden
        && hoverNote->staff == staff && hoverNote->measure == measure)
        highlightedPosition = hoverNote->pitch.step - getBottomLineStep (staff);

    for (size_t i = 0; i < notes.size(); ++i)
    {
        const auto isHovered = highlightedPosition.has_value()
                            && (content.source != StaffContent::Source::notes || juce::exactlyEqual (notes[i].onset, (double) hoverNote->beat));
        drawNote (g, staff, notes[i], rolled[i], isHovered ? highlightedPosition : std::nullopt);
    }

    for (const auto& note : notes)
        drawStem (g, staff, note);

    for (auto& beamed : beams)
        drawBeams (g, staff, beamed);
}

void StaffView::drawNote (juce::Graphics& g, Staff staff, const NoteLayout& note, bool rolled,
                          std::optional<int> highlightedPosition) const
{
    drawLedgerLines (g, staff, note.positions, note.headLefts, note.headWidth);

    for (size_t i = 0; i < note.positions.size(); ++i)
    {
        const auto highlighted = note.positions[i] == highlightedPosition;

        if (highlighted)
            g.setColour (removalColour);

        glyphs.draw (g, getNoteheadGlyph (note.duration), { note.headLefts[i], getY (staff, note.positions[i]) });

        if (highlighted)
            g.setColour (inkColour);
    }

    const auto headsLeft = *std::min_element (note.headLefts.begin(), note.headLefts.end());
    const auto headsRight = *std::max_element (note.headLefts.begin(), note.headLefts.end()) + note.headWidth;

    // A dotted note has its dot in a space: beside a note in a space, or in the space above a
    // note on a line.
    if (note.duration == Duration::dottedHalf)
    {
        std::set<int> dotPositions;

        for (auto position : note.positions)
            dotPositions.insert (isLine (position) ? position + 1 : position);

        for (auto position : dotPositions)
            glyphs.draw (g, Smufl::augmentationDot, { headsRight + 0.35f * staffSpace, getY (staff, position) });
    }

    auto leftmost = headsLeft;

    for (size_t i = 0; i < note.positions.size(); ++i)
    {
        if (! note.accidentals[i].has_value())
            continue;

        const auto glyph = getAccidentalGlyph (*note.accidentals[i]);
        const auto right = note.getAccidentalRight (note.accidentalColumns[i]);
        const auto bounds = glyphs.getPath (glyph).getBounds();

        glyphs.draw (g, glyph, { right - bounds.getRight(), getY (staff, note.positions[i]) });
        leftmost = juce::jmin (leftmost, right - bounds.getWidth());
    }

    if (rolled && note.positions.size() > 1)
        drawRollSign (g, leftmost - 0.25f * staffSpace,
                      getY (staff, note.positions.back()) - 0.6f * staffSpace,
                      getY (staff, note.positions.front()) + 0.6f * staffSpace);
}

void StaffView::drawStem (juce::Graphics& g, Staff staff, const NoteLayout& note) const
{
    if (! hasStem (note.duration))
        return;

    const auto flags = note.beamed ? 0 : getFlagCount (note.duration);
    const auto stemLeft = note.getStemLeft();
    auto end = note.stemEnd;

    if (! note.beamed)
    {
        // The stem reaches an octave past the outermost note, and at least to the middle line.
        const auto length = (stemLength + (flags == 2 ? 0.5f : 0.0f)) * staffSpace;
        const auto middleY = getY (staff, middleLine);

        end = note.stemUp ? juce::jmin (getY (staff, note.positions.back()) - length, middleY)
                          : juce::jmax (getY (staff, note.positions.front()) + length, middleY);
    }

    const auto start = note.stemUp ? getY (staff, note.positions.front()) - stemAttachmentOffset * staffSpace
                                   : getY (staff, note.positions.back()) + stemAttachmentOffset * staffSpace;

    g.fillRect (juce::Rectangle<float>::leftTopRightBottom (stemLeft, juce::jmin (start, end),
                                                            stemLeft + stemThickness * staffSpace, juce::jmax (start, end)));

    if (flags > 0)
    {
        // The flag's anchor, which meets the end of the stem, from Bravura's metadata
        const auto glyph = note.stemUp ? (flags == 1 ? Smufl::flag8thUp : Smufl::flag16thUp)
                                       : (flags == 1 ? Smufl::flag8thDown : Smufl::flag16thDown);
        const auto anchor = note.stemUp ? (flags == 1 ? -0.04f : -0.088f) : (flags == 1 ? 0.132f : 0.128f);

        glyphs.draw (g, glyph, { stemLeft, end + anchor * staffSpace });
    }
}

void StaffView::drawBeams (juce::Graphics& g, Staff, std::vector<NoteLayout*>& beamed) const
{
    const auto& first = *beamed.front();
    const auto& last = *beamed.back();
    const auto left = first.getStemLeft();
    const auto right = last.getStemLeft() + stemThickness * staffSpace;
    const auto towardsNotes = first.stemUp ? 1.0f : -1.0f;
    const auto beamCount = first.duration == Duration::sixteenth ? 2 : 1;

    for (int beam = 0; beam < beamCount; ++beam)
    {
        const auto offset = towardsNotes * (float) beam * (beamThickness + beamSpacing) * staffSpace;
        const auto thickness = towardsNotes * beamThickness * staffSpace;

        juce::Path path;
        path.startNewSubPath (left, first.stemEnd + offset);
        path.lineTo (right, last.stemEnd + offset);
        path.lineTo (right, last.stemEnd + offset + thickness);
        path.lineTo (left, first.stemEnd + offset + thickness);
        path.closeSubPath();
        g.fillPath (path);
    }
}

void StaffView::drawLedgerLines (juce::Graphics& g, Staff staff, const std::vector<int>& staffPositions,
                                 const std::vector<float>& noteheadLefts, float noteheadWidth) const
{
    const auto thickness = legerLineThickness * staffSpace;
    const auto extension = legerLineExtension * staffSpace;

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
                right = juce::jmax (right, noteheadLefts[i] + noteheadWidth);
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

void StaffView::drawRollSign (juce::Graphics& g, float right, float top, float bottom) const
{
    // A rolled chord's wavy line, built from Bravura's arpeggiato segments turned upright
    const auto& segment = glyphs.getPath (Smufl::wiggleArpeggiatoUp);
    const auto segmentLength = 1.02f * staffSpace;
    const auto count = juce::jmax (1, (int) std::ceil ((bottom - top) / segmentLength));

    for (int i = 0; i < count; ++i)
        g.fillPath (segment, juce::AffineTransform::rotation (-juce::MathConstants<float>::halfPi)
                                 .translated (right, bottom - (float) i * segmentLength));
}

void StaffView::drawLabels (juce::Graphics& g, int measure) const
{
    const auto& content = measureLayouts[(size_t) measure].content;

    if (! content.chord.has_value())
        return;

    const auto& chord = *content.chord;
    const auto& chordStaff = content.staves[(size_t) content.chordStyle.staff];

    if (chordStaff.events.empty())
        return;

    // Both labels are centred on the chord's first note.
    const auto x = getEventX (measure, chordStaff.events.front());

    if (chord.symbol.isNotEmpty())
    {
        g.setColour (inkColour);
        g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
        g.drawSingleLineText (chord.symbol, juce::roundToInt (x), juce::roundToInt (getSymbolBaseline()),
                              juce::Justification::horizontallyCentred);
    }

    if (chord.numeral.isEmpty())
        return;

    // The numeral, then any + or ° (or the M of a major 7th) at its top right, and its figures
    // stacked beside that. Times has no flat sign, so a ♭ before the numeral comes from the
    // sans-serif font.
    const juce::Font numeralFont (juce::FontOptions ("Times New Roman", 22.0f, juce::Font::plain));
    const juce::Font flatFont (juce::FontOptions (20.0f));
    const juce::Font figureFont (juce::FontOptions ("Times New Roman", 13.0f, juce::Font::plain));
    const auto baseline = getNumeralBaseline();

    const auto flatSign = juce::String::charToString (0x266D);
    const auto flat = chord.numeral.startsWith (flatSign) ? flatSign : juce::String();
    const auto roman = chord.numeral.substring (flat.length());
    const auto flatWidth = juce::GlyphArrangement::getStringWidth (flatFont, flat);
    const auto numeralWidth = flatWidth + juce::GlyphArrangement::getStringWidth (numeralFont, roman);
    const auto numeralLeft = x - numeralWidth / 2.0f;

    g.setColour (numeralColour);
    g.setFont (flatFont);
    g.drawSingleLineText (flat, juce::roundToInt (numeralLeft), juce::roundToInt (baseline));
    g.setFont (numeralFont);
    g.drawSingleLineText (roman, juce::roundToInt (numeralLeft + flatWidth), juce::roundToInt (baseline));

    g.setFont (figureFont);
    auto figureX = x + numeralWidth / 2.0f + 1.0f;

    for (const auto& mark : { chord.sign, chord.seventhMark })
    {
        if (mark.isEmpty())
            continue;

        g.drawSingleLineText (mark, juce::roundToInt (figureX), juce::roundToInt (baseline - 9.0f));
        figureX += juce::GlyphArrangement::getStringWidth (figureFont, mark) + 1.0f;
    }

    for (int i = 0; i < chord.figures.size(); ++i)
        g.drawSingleLineText (chord.figures[i], juce::roundToInt (figureX), juce::roundToInt (baseline - 9.0f + 11.0f * (float) i));
}

std::optional<music::Tone> StaffView::findNoteUnder (const Note& note) const
{
    if (! juce::isPositiveAndBelow (note.measure, (int) measureLayouts.size()))
        return {};

    // A chord's notes count wherever they are in the measure; a quarter note only on its own beat.
    const auto& content = measureLayouts[(size_t) note.measure].content.staves[(size_t) note.staff];

    for (const auto& event : content.events)
    {
        if (content.source == StaffContent::Source::notes && ! juce::exactlyEqual (event.onset, (double) note.beat))
            continue;

        for (const auto& tone : event.tones)
            if (tone.pitch.step == note.pitch.step)
                return tone;
    }

    return {};
}

void StaffView::drawHoverNote (juce::Graphics& g) const
{
    if (! hoverNote.has_value() || hoverHintHidden
        || hoverNote->measure >= score.getNumMeasures() || hoverNote->beat >= score.getBeatsPerMeasure())
        return;

    // Over a note, the note itself is highlighted instead.
    if (findNoteUnder (*hoverNote).has_value())
        return;

    // The notehead of the length a click adds: filled for a quarter note, open for a half or whole note
    const auto duration = getDurationForBeats (juce::jmin (hoverNote->length, score.getBeatsPerMeasure() - hoverNote->beat));
    const auto notehead = duration == Duration::whole ? Smufl::noteheadWhole
                        : duration == Duration::quarter ? Smufl::noteheadBlack
                                                        : Smufl::noteheadHalf;
    const auto width = getNoteheadWidth (duration) * staffSpace;
    const auto position = hoverNote->pitch.step - getBottomLineStep (hoverNote->staff);
    const auto left = getOnsetX (hoverNote->measure, hoverNote->beat) - width / 2.0f;

    g.setColour (hoverColour);
    drawLedgerLines (g, hoverNote->staff, { position }, { left }, width);
    glyphs.draw (g, notehead, { left, getY (hoverNote->staff, position) });
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

    if (onClicked != nullptr)
        onClicked();

    if (const auto note = getNoteAt (e.position))
    {
        // On a staff with a chord's notes, clicking one of them takes it out of the chord, and
        // clicking anywhere else adds a note to them.
        if (score.chordUsesStaff (part, note->measure, note->staff))
        {
            const auto existing = findNoteUnder (*note);
            const auto* chord = score.getChord (part, note->measure);
            const music::KeyboardNote clicked { existing ? existing->midi : note->pitch.getMidiNoteNumber(),
                                                existing ? existing->pitch.getSpelling() : note->pitch.getSpelling() };

            if (note->staff == chord->style.staff)
                score.toggleChordNote (part, note->measure, clicked.midi, chord->style);
            else
                score.toggleAlternateNote (part, note->measure, clicked);
        }
        // Clicking a note takes it out, whatever its sharp or flat; clicking anywhere else adds one.
        else if (score.hasNoteAt (part, note->staff, note->measure, note->beat, note->pitch.step))
            score.removeNotesAt (part, note->staff, note->measure, note->beat, note->pitch.step);
        else
            score.addNote (*note);

        // Show what the click did, rather than what another click would do.
        hoverHintHidden = true;
        repaint (getBeatArea (note->measure, note->beat));
    }
}

void StaffView::setHoverNote (std::optional<Note> note)
{
    if (note == hoverNote)
        return;

    hoverHintHidden = false;

    if (hoverNote.has_value())
        repaint (getBeatArea (hoverNote->measure, hoverNote->beat));

    hoverNote = note;

    if (hoverNote.has_value())
        repaint (getBeatArea (hoverNote->measure, hoverNote->beat));
}

//==============================================================================
void StaffView::resized()
{
    updateChordButtons();
}

void StaffView::updateChordButtons()
{
    const auto numMeasures = score.getNumMeasures();

    while (chordButtons.size() > numMeasures)
        chordButtons.removeLast();

    while (chordButtons.size() < numMeasures)
    {
        const auto measure = chordButtons.size();
        auto* button = chordButtons.add (std::make_unique<juce::TextButton>());
        button->setWantsKeyboardFocus (false);
        button->setColour (juce::TextButton::buttonColourId, paperColour);
        button->setColour (juce::TextButton::buttonOnColourId, selectionColour.withAlpha (0.15f));
        button->setColour (juce::TextButton::textColourOffId, selectionColour);
        button->setColour (juce::TextButton::textColourOnId, selectionColour);
        button->onClick = [this, measure]
        {
            if (onClicked != nullptr)
                onClicked();

            if (onChordButtonClicked != nullptr)
                onChordButtonClicked (measure);
        };
        addAndMakeVisible (button);
    }

    // Each button sits just over its measure's chord name.
    const auto top = getStaffTop (Staff::treble) - getChordButtonOffset() * staffSpace;

    for (int measure = 0; measure < numMeasures; ++measure)
    {
        auto* button = chordButtons[measure];
        const auto& layout = measureLayouts[(size_t) measure];
        const auto* chord = score.getChord (part, measure);
        const auto hasChord = chord != nullptr && chord->hasNotes();
        const auto width = juce::jmin (layout.width - 1.2f * staffSpace, 90.0f);

        button->setButtonText (hasChord ? "Edit Chord" : "Add Chord");
        button->setTooltip ((hasChord ? "Edit the chord in measure " : "Add a chord to measure ") + juce::String (measure + 1));
        button->setToggleState (measure == selectedMeasure, juce::dontSendNotification);
        button->setBounds (juce::Rectangle<float> (layout.x + (layout.width - width) / 2.0f, top, width, 2.0f * staffSpace).toNearestInt());
    }
}
