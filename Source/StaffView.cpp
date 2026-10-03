#include "StaffView.h"

#include "EraserButton.h"

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
        return notesPerBeat >= 8 ? 17.0f : notesPerBeat >= 4 ? 10.0f : notesPerBeat == 2 ? 6.4f : 5.0f;
    }

    bool isLine (int staffPosition)
    {
        return music::mod (staffPosition, 2) == 0;
    }

    bool hasStem (Duration duration)
    {
        return duration != Duration::whole;
    }

    /** How many flags a note has on its own, or beams when it's beamed: 1 for an eighth, 2 for a 16th and 3 for a 32nd. */
    int getFlagCount (Duration duration)
    {
        return duration == Duration::eighth || duration == Duration::dottedEighth ? 1
             : duration == Duration::sixteenth ? 2
             : duration == Duration::thirtySecond ? 3
                                                  : 0;
    }

    juce::juce_wchar getNoteheadGlyph (Duration duration)
    {
        switch (duration)
        {
            case Duration::whole:        return Smufl::noteheadWhole;
            case Duration::dottedHalf:
            case Duration::half:         return Smufl::noteheadHalf;
            case Duration::dottedQuarter:
            case Duration::quarter:
            case Duration::dottedEighth:
            case Duration::eighth:
            case Duration::sixteenth:
            case Duration::thirtySecond: break;
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
            case Duration::dottedQuarter:
            case Duration::quarter:      return Smufl::restQuarter;
            case Duration::dottedEighth:
            case Duration::eighth:       return Smufl::rest8th;
            case Duration::sixteenth:    return Smufl::rest16th;
            case Duration::thirtySecond: return Smufl::rest32nd;
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
      partId (scoreToShow.getPartId (partToShow)),
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
    // Room for the numerals, and the pedal marks below them
    return juce::jmax (minMarginBelow, getNumeralOffset() + 2.4f);
}

void StaffView::setNoteLength (double beats)
{
    noteLength = juce::jlimit (1.0 / Score::slotsPerBeat, Score::maxNoteLength, beats);
    setHoverNote ({});
}

void StaffView::setMarking (std::optional<music::Marking> newMarking)
{
    marking = newMarking;
    erasing = false;
    hoverErasable = {};
    hoverHairpin.reset();
    setMouseCursor (juce::MouseCursor::NormalCursor);
    setHoverNote ({});
    setHoverMarkPoint ({});
    repaint();
}

void StaffView::setScalePlacement (std::optional<ScaleSettings> settings)
{
    scalePlacement = settings;
    scaleHover.reset();
    setHoverNote ({});

    if (scalePlacement.has_value())
        setMouseCursor (juce::MouseCursor::CrosshairCursor);
    else
        setMouseCursor (erasing ? EraserButton::createCursor() : juce::MouseCursor (juce::MouseCursor::NormalCursor));

    repaint();
}

std::vector<Note> StaffView::planScale (const Note& start, juce::Random* randomToUse) const
{
    const auto& settings = *scalePlacement;
    const auto keyAlterations = music::getKeyAlterations (score.getKey());
    const auto slotsPerMeasure = score.getBeatsPerMeasure() * Score::slotsPerBeat;

    // Random notes come from the staff and a little either side of it, never the same twice running.
    const auto lowest = getBottomLineStep (start.staff) - 2;
    const auto highest = getBottomLineStep (start.staff) + topLine + 2;

    std::vector<Note> notes;
    auto measure = start.measure;
    auto slot = (int) std::lround (start.beat * Score::slotsPerBeat);
    auto previousStep = std::numeric_limits<int>::min();

    for (int i = 0; i < settings.numNotes; ++i)
    {
        auto step = start.pitch.step + (settings.ascending ? i : -i);

        if (settings.random && randomToUse == nullptr)
            step = getBottomLineStep (start.staff) + middleLine;
        else if (settings.random)
            do step = lowest + randomToUse->nextInt (highest - lowest + 1); while (step == previousStep);

        previousStep = step;

        // The scale's notes are the key signature's.
        const auto length = Score::fitNoteLength (settings.length, slotsPerMeasure - slot);
        notes.push_back ({ start.staff, measure, (double) slot / Score::slotsPerBeat, { step, keyAlterations[(size_t) music::mod (step, 7)] },
                           part, (double) length / Score::slotsPerBeat });

        if ((slot += length) >= slotsPerMeasure)
        {
            ++measure;
            slot = 0;
        }
    }

    return notes;
}

void StaffView::placeScale (const Note& start)
{
    const auto notes = planScale (start, &random);

    while (notes.back().measure >= score.getNumMeasures())
        score.addMeasure();

    for (const auto& note : notes)
    {
        if (score.chordUsesStaff (part, note.measure, note.staff))
            continue;

        // In place of whatever starts where it goes
        const auto from = (int) std::lround (note.beat * Score::slotsPerBeat);
        const auto to = from + (int) std::lround (note.length * Score::slotsPerBeat);

        for (auto slot = from; slot < to; ++slot)
        {
            const auto beat = (double) slot / Score::slotsPerBeat;
            const auto existing = score.getNotes (part, note.staff, note.measure, beat);

            for (const auto& pitch : existing)
                score.removeNotesAt (part, note.staff, note.measure, beat, pitch.step);
        }

        score.addNote (note);
    }

    if (onScalePlaced != nullptr)
        onScalePlaced();
}

void StaffView::drawScaleHint (juce::Graphics& g) const
{
    if (! scalePlacement.has_value() || ! scaleHover.has_value())
        return;

    g.setColour (hoverColour);

    for (const auto& note : planScale (*scaleHover, nullptr))
    {
        if (note.measure >= score.getNumMeasures())
            break;

        // Measures a chord has the staff in are left alone.
        if (score.chordUsesStaff (part, note.measure, note.staff))
            continue;

        const auto duration = getDurationForBeats (note.length);
        const auto width = getNoteheadWidth (duration) * staffSpace;
        const auto position = note.pitch.step - getBottomLineStep (note.staff);
        const auto left = getOnsetX (note.measure, note.beat) - width / 2.0f;

        drawLedgerLines (g, note.staff, { position }, { left }, width);
        glyphs.draw (g, getNoteheadGlyph (duration), { left, getY (note.staff, position) });
    }

    g.setColour (inkColour);
}

void StaffView::setErasing (bool shouldErase)
{
    setMarking ({});
    erasing = shouldErase;

    if (erasing)
        setMouseCursor (EraserButton::createCursor());
}

std::optional<music::Dynamic> StaffView::getChosenDynamic() const
{
    if (marking.has_value())
        if (const auto* dynamic = std::get_if<music::Dynamic> (&*marking))
            return *dynamic;

    return {};
}

bool StaffView::isPedalChosen() const
{
    return marking.has_value() && std::holds_alternative<music::Pedal> (*marking);
}

std::optional<music::Hairpin> StaffView::getChosenHairpin() const
{
    if (marking.has_value())
        if (const auto* hairpin = std::get_if<music::Hairpin> (&*marking))
            return *hairpin;

    return {};
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

        for (int spacedPart = 0; spacedPart < score.getNumParts(); ++spacedPart)
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

void StaffView::followPart()
{
    part = score.findPart (partId);
}

void StaffView::changeListenerCallback (juce::ChangeBroadcaster*)
{
    // A part that's been taken out shows nothing, until its view goes too.
    followPart();

    if (part < 0)
        return;

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

juce::Rectangle<int> StaffView::getBeatArea (int measure, double beat) const
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

int StaffView::getFinalBarlineX() const
{
    return measureLayouts.empty() ? 0 : juce::roundToInt (measureLayouts.back().x + measureLayouts.back().width);
}

std::optional<Note> StaffView::getNoteAt (juce::Point<float> point) const
{
    const auto measure = findMeasure (point.x);

    if (measure < 0 || ! isInClickRange (point))
        return {};

    const auto staffDivide = getStaffTop (Staff::treble) + (staffHeight + staffGap / 2.0f) * staffSpace;
    const auto staff = point.y < staffDivide ? Staff::treble : Staff::bass;

    const auto position = juce::roundToInt ((getY (staff, 0) - point.y) / (staffSpace / 2.0f));
    const auto clampedPosition = staff == Staff::treble
                                     ? juce::jlimit (lowestTreblePosition, highestTreblePosition, position)
                                     : juce::jlimit (lowestBassPosition, highestBassPosition, position);

    // The nearest beat, or for a shorter note, the nearest step of its length without its dot: an
    // eighth, 16th or 32nd. Notes already there can be clicked wherever they start.
    // Adding from the scale, the notes going on are the ones of its length.
    const auto length = scalePlacement.has_value() ? (double) scalePlacement->length / Score::slotsPerBeat : noteLength;
    auto grid = Score::slotsPerBeat;

    while (grid > 1 && grid > (int) std::lround (length * Score::slotsPerBeat))
        grid /= 2;
    const auto slot = getSlotAt (measure, point.x, grid, [&] (double beat)
    {
        return ! score.getNotes (part, staff, measure, beat).empty();
    });

    // The note takes its sharp or flat from the key signature.
    const auto step = getBottomLineStep (staff) + clampedPosition;
    const auto alter = music::getKeyAlterations (score.getKey())[(size_t) music::mod (step, 7)];

    return Note { staff, measure, (double) slot / Score::slotsPerBeat, { step, alter }, part, noteLength };
}

std::optional<std::pair<int, double>> StaffView::getDynamicPointAt (juce::Point<float> point) const
{
    const auto measure = findMeasure (point.x);

    if (measure < 0 || ! isInClickRange (point))
        return {};

    // Between beats, only where a note starts, on either staff
    const auto slot = getSlotAt (measure, point.x, Score::slotsPerBeat, [&] (double beat) { return hasNoteStartingAt (measure, beat); });
    return std::pair { measure, (double) slot / Score::slotsPerBeat };
}

bool StaffView::hasNoteStartingAt (int measure, double beat) const
{
    for (const auto& staff : measureLayouts[(size_t) measure].content.staves)
        for (const auto& event : staff.events)
            if (! event.isRest() && juce::exactlyEqual (event.onset, beat))
                return true;

    return false;
}

double StaffView::getHairpinEndAt (float x) const
{
    const auto beats = score.getBeatsPerMeasure();
    const auto measure = findMeasure (x);

    if (measure < 0)
        return x < measureLayouts.front().x ? 0.0 : (double) (score.getNumMeasures() * beats);

    // The nearest beat, counting the barline after the last, or 32nd note with a note starting there
    const auto& layout = measureLayouts[(size_t) measure];
    const auto inBeats = (x - layout.x - layout.padding) / layout.beatWidth;
    const auto slot = juce::jlimit (0, beats * Score::slotsPerBeat, juce::roundToInt (inBeats * (float) Score::slotsPerBeat));
    const auto between = (double) slot / Score::slotsPerBeat;
    const auto beat = slot % Score::slotsPerBeat != 0 && hasNoteStartingAt (measure, between)
                          ? between
                          : (double) juce::jlimit (0, beats, juce::roundToInt (inBeats));

    return toScoreBeats (measure, beat);
}

double StaffView::toScoreBeats (int measure, double beat) const
{
    return measure * score.getBeatsPerMeasure() + beat;
}

std::pair<int, double> StaffView::fromScoreBeats (double beats) const
{
    const auto beatsPerMeasure = score.getBeatsPerMeasure();
    const auto measure = juce::jmin ((int) std::floor (beats / beatsPerMeasure), score.getNumMeasures());
    return { measure, beats - measure * beatsPerMeasure };
}

std::optional<std::pair<HairpinMark, bool>> StaffView::findHairpinAt (juce::Point<float> point) const
{
    if (std::abs (point.y - getMarkingCentreY()) > 1.3f * staffSpace)
        return {};

    for (const auto& hairpin : score.getHairpins (part))
    {
        const auto span = getHairpinSpan (hairpin.measure, hairpin.beat, toScoreBeats (hairpin.measure, hairpin.beat) + hairpin.length);

        if (std::abs (point.x - span.getEnd()) <= 0.8f * staffSpace)
            return std::pair { hairpin, true };

        if (span.contains (point.x))
            return std::pair { hairpin, false };
    }

    return {};
}

bool StaffView::isInClickRange (juce::Point<float> point) const
{
    const auto trebleTop = getStaffTop (Staff::treble);
    const auto bassBottom = getStaffTop (Staff::bass) + staffHeight * staffSpace;

    return point.y >= trebleTop - noteRoom * staffSpace && point.y < bassBottom + noteRoom * staffSpace;
}

int StaffView::getSlotAt (int measure, float x, int grid, const std::function<bool (double beat)>& hasNotesAt) const
{
    const auto& layout = measureLayouts[(size_t) measure];
    const auto slotWidth = layout.beatWidth / (float) Score::slotsPerBeat;
    const auto slots = score.getBeatsPerMeasure() * Score::slotsPerBeat;
    const auto inSlots = (x - layout.x - layout.padding) / slotWidth;

    // The nearest 32nd note, if notes start there
    if (const auto slot = juce::jlimit (0, slots - 1, juce::roundToInt (inSlots)); hasNotesAt ((double) slot / Score::slotsPerBeat))
        return slot;

    // Or else the nearest step of the grid
    grid = juce::jlimit (1, Score::slotsPerBeat, grid);
    return juce::jlimit (0, slots - grid, juce::roundToInt (inSlots / (float) grid) * grid);
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

    if (part < 0)
        return;

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

    g.setColour (inkColour);
    drawTies (g, juce::jmax (0, firstMeasure - 1), lastMeasure);
    drawTieDrag (g);
    drawHoverNote (g);
    drawHairpins (g);
    drawPedals (g);
    drawHoverDynamic (g);
    drawScaleHint (g);
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
    drawDynamics (g, measure);

    g.setColour (measureNumberColour);
    g.setFont (juce::FontOptions (11.0f));
    g.drawText (juce::String (measure + 1),
                juce::Rectangle<float> (layout.x + 2.0f, top - 1.7f * staffSpace, 40.0f, 1.4f * staffSpace),
                juce::Justification::bottomLeft, false);
}

std::vector<std::vector<size_t>> StaffView::groupBeams (const std::vector<const StaffEvent*>& notes)
{
    std::vector<std::vector<size_t>> groups;
    const auto beamable = [&] (size_t i) { return getFlagCount (notes[i]->duration) > 0; };
    const auto isEighth = [&] (size_t i) { return notes[i]->duration == Duration::eighth; };

    for (size_t first = 0; first < notes.size();)
    {
        if (! beamable (first))
        {
            ++first;
            continue;
        }

        // A run of eighths and shorter notes, each starting as the one before ends
        auto end = first + 1;

        while (end < notes.size() && beamable (end)
               && juce::exactlyEqual (notes[end]->onset, notes[end - 1]->onset + getBeats (notes[end - 1]->duration)))
            ++end;

        // Every four eighths in a row share a beam, counting from the first of them; the rest of
        // the run is beamed a beat at a time.
        const auto startsFourEighths = [&] (size_t i)
        {
            if (i + 3 >= end || ! (isEighth (i) && isEighth (i + 1) && isEighth (i + 2) && isEighth (i + 3)))
                return false;

            auto stretchStart = i;

            while (stretchStart > first && isEighth (stretchStart - 1))
                --stretchStart;

            return (i - stretchStart) % 4 == 0;
        };

        auto addingToBeat = false;      // whether the last group is a beat's, which the next note may join

        for (auto i = first; i < end;)
        {
            if (startsFourEighths (i))
            {
                groups.push_back ({ i, i + 1, i + 2, i + 3 });
                i += 4;
                addingToBeat = false;
                continue;
            }

            if (addingToBeat && (int) std::floor (notes[i]->onset) == (int) std::floor (notes[groups.back().back()]->onset))
                groups.back().push_back (i);
            else
                groups.push_back ({ i });

            addingToBeat = true;
            ++i;
        }

        first = end;
    }

    return groups;
}

void StaffView::drawStaff (juce::Graphics& g, int measure, Staff staff) const
{
    const auto& content = measureLayouts[(size_t) measure].content.staves[(size_t) staff];
    const auto keyAlterations = music::getKeyAlterations (score.getKey());
    const auto beamable = content.notesPerBeat > 1;

    std::map<int, int> alterationsInForce;
    std::vector<NoteLayout> notes;
    std::vector<bool> rolled;
    std::vector<const StaffEvent*> noteEvents;

    notes.reserve (content.events.size());

    for (const auto& event : content.events)
    {
        if (event.isRest())
        {
            drawCentred (g, getRestGlyph (event.duration), getEventX (measure, event),
                         getY (staff, event.duration == Duration::whole ? middleLine + 2 : middleLine));
            continue;
        }

        notes.emplace_back (event, staff, getEventX (measure, event), alterationsInForce, keyAlterations);
        rolled.push_back (event.rolled);
        noteEvents.push_back (&event);
    }

    const auto beamGroups = beamable ? groupBeams (noteEvents) : std::vector<std::vector<size_t>>();

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

        // Move the beam away from the notes until every stem is long enough, longer the more
        // beams there are.
        auto beamCount = 1;

        for (const auto* note : beamed)
            beamCount = juce::jmax (beamCount, getFlagCount (note->duration));

        const auto minStem = (beamCount > 1 ? 3.0f + 0.75f * (float) (beamCount - 2) : 2.5f) * staffSpace;
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
        // A Random chord's notes are taken out one at a time, so only the one under the pointer is red.
        const auto onItsOwn = content.source == StaffContent::Source::notes || hasRandomChord (measure);
        const auto isHovered = highlightedPosition.has_value()
                            && (! onItsOwn || juce::exactlyEqual (notes[i].onset, (double) hoverNote->beat));
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
    // note on a line. A flag hanging from a stem going up would run into it, so it's past the flag.
    if (isDotted (note.duration))
    {
        std::set<int> dotPositions;

        for (auto position : note.positions)
            dotPositions.insert (isLine (position) ? position + 1 : position);

        auto dotX = headsRight + 0.35f * staffSpace;

        if (note.stemUp && ! note.beamed && getFlagCount (note.duration) > 0)
            dotX = juce::jmax (dotX, note.getStemLeft() + glyphs.getPath (Smufl::flag8thUp).getBounds().getRight() + 0.25f * staffSpace);

        for (auto position : dotPositions)
            glyphs.draw (g, Smufl::augmentationDot, { dotX, getY (staff, position) });
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
        const auto length = (stemLength + (flags == 3 ? 1.25f : flags == 2 ? 0.5f : 0.0f)) * staffSpace;
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
        const auto glyph = note.stemUp ? (flags == 1 ? Smufl::flag8thUp : flags == 2 ? Smufl::flag16thUp : Smufl::flag32ndUp)
                                       : (flags == 1 ? Smufl::flag8thDown : flags == 2 ? Smufl::flag16thDown : Smufl::flag32ndDown);
        const auto anchor = note.stemUp ? (flags == 1 ? -0.04f : flags == 2 ? -0.088f : 0.376f)
                                        : (flags == 1 ? 0.132f : flags == 2 ? 0.128f : -0.448f);

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
    const auto thickness = towardsNotes * beamThickness * staffSpace;
    const auto beamY = [&] (float x) { return right > left ? first.stemEnd + (last.stemEnd - first.stemEnd) * (x - left) / (right - left) : first.stemEnd; };

    const auto drawBeam = [&] (int level, float from, float to)
    {
        const auto offset = towardsNotes * (float) level * (beamThickness + beamSpacing) * staffSpace;
        juce::Path path;
        path.startNewSubPath (from, beamY (from) + offset);
        path.lineTo (to, beamY (to) + offset);
        path.lineTo (to, beamY (to) + offset + thickness);
        path.lineTo (from, beamY (from) + offset + thickness);
        path.closeSubPath();
        g.fillPath (path);
    };

    // The main beam joins them all. Each beam after it joins the notes next to one another that
    // are short enough to have it: a second for 16ths and 32nds, and a third for 32nds. A note
    // without a neighbour to share it with has a short one, pointing into the group.
    drawBeam (0, left, right);

    for (int level = 1; level < 3; ++level)
    {
        for (size_t i = 0; i < beamed.size();)
        {
            if (getFlagCount (beamed[i]->duration) <= level)
            {
                ++i;
                continue;
            }

            auto end = i + 1;

            while (end < beamed.size() && getFlagCount (beamed[end]->duration) > level)
                ++end;

            const auto from = beamed[i]->getStemLeft();
            const auto to = beamed[end - 1]->getStemLeft() + stemThickness * staffSpace;
            const auto stub = 1.1f * staffSpace;

            if (end - i > 1)
                drawBeam (level, from, to);
            else if (i == 0)
                drawBeam (level, from, from + stub);
            else
                drawBeam (level, to - stub, to);

            i = end;
        }
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

    // The notehead of the length a click adds, once it's fitted into what's left of the measure:
    // filled for a dotted quarter or shorter, open for a half or longer
    const auto room = (int) std::lround ((score.getBeatsPerMeasure() - hoverNote->beat) * Score::slotsPerBeat);
    const auto slots = Score::fitNoteLength ((int) std::lround (hoverNote->length * Score::slotsPerBeat), room);
    const auto duration = getDurationForBeats ((double) slots / Score::slotsPerBeat);
    const auto notehead = getNoteheadGlyph (duration);
    const auto width = getNoteheadWidth (duration) * staffSpace;
    const auto position = hoverNote->pitch.step - getBottomLineStep (hoverNote->staff);
    const auto left = getOnsetX (hoverNote->measure, hoverNote->beat) - width / 2.0f;

    g.setColour (hoverColour);
    drawLedgerLines (g, hoverNote->staff, { position }, { left }, width);
    glyphs.draw (g, notehead, { left, getY (hoverNote->staff, position) });

    // A dotted note has its dot in a space.
    if (isDotted (duration))
        glyphs.draw (g, Smufl::augmentationDot, { left + width + 0.35f * staffSpace, getY (hoverNote->staff, isLine (position) ? position + 1 : position) });
}

float StaffView::getPedalLineY() const
{
    return getY (Staff::bass, 0) + (getNumeralOffset() + 1.6f) * staffSpace;
}

float StaffView::getScoreBeatsX (double beats) const
{
    // The end of the score is just before the final barline.
    const auto [measure, beat] = fromScoreBeats (beats);

    if (measure >= score.getNumMeasures())
        return measureLayouts.back().x + measureLayouts.back().width - 1.0f * staffSpace;

    return getOnsetX (measure, beat);
}

juce::Range<float> StaffView::getPedalSpan (const PedalSpan& pedal) const
{
    const auto start = getScoreBeatsX (pedal.start);
    return { start, juce::jmax (start + 0.5f * staffSpace, getScoreBeatsX (pedal.end)) };
}

void StaffView::drawPedals (juce::Graphics& g) const
{
    // A line under the lower staff, with a tick up where the pedal goes down, and another where
    // it comes up. One that's still down at the next mark, or the end, has no tick at its end.
    const auto y = getPedalLineY();
    const auto tick = 1.0f * staffSpace;
    const auto thickness = 0.16f * staffSpace;

    const auto draw = [&] (juce::Range<float> span, bool lifted, bool downTick)
    {
        juce::Path bracket;
        bracket.startNewSubPath (span.getStart(), downTick ? y - tick : y);
        bracket.lineTo (span.getStart(), y);
        bracket.lineTo (span.getEnd(), y);

        if (lifted)
            bracket.lineTo (span.getEnd(), y - tick);

        g.strokePath (bracket, juce::PathStrokeType (thickness, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
    };

    const auto pedals = score.getPedals (part);

    for (const auto& pedal : pedals)
    {
        g.setColour (erasing && hoverErasable.pedal == pedal ? removalColour : inkColour);
        draw (getPedalSpan (pedal), pedal.lifted, true);
    }

    // Where a click would put the pedal down, or lift the one that's down
    if (isPedalChosen() && hoverMarkPoint.has_value() && ! hoverHintHidden)
    {
        const auto at = toScoreBeats (hoverMarkPoint->first, hoverMarkPoint->second);
        const auto x = getScoreBeatsX (at);
        g.setColour (hoverColour);

        if (const auto open = std::find_if (pedals.begin(), pedals.end(), [] (const PedalSpan& p) { return ! p.lifted; });
            open != pedals.end() && ! juce::exactlyEqual (open->start, at))
            draw ({ juce::jmin (x, getScoreBeatsX (open->start)), juce::jmax (x, getScoreBeatsX (open->start)) }, true, true);
        else
            draw ({ x, x + 1.5f * staffSpace }, false, true);
    }

    g.setColour (inkColour);
}

std::optional<std::pair<int, double>> StaffView::getPedalPointAt (juce::Point<float> point) const
{
    // On or under the lower staff
    const auto staffDivide = getStaffTop (Staff::treble) + (staffHeight + staffGap / 2.0f) * staffSpace;

    if (point.y < staffDivide)
        return {};

    const auto measure = findMeasure (point.x);

    if (measure < 0)
        return {};

    const auto slot = getSlotAt (measure, point.x, Score::slotsPerBeat, [&] (double beat) { return hasNoteStartingAt (measure, beat); });
    return std::pair { measure, (double) slot / Score::slotsPerBeat };
}

void StaffView::clickPedal (std::pair<int, double> point)
{
    const auto at = toScoreBeats (point.first, point.second);
    auto pedals = score.getPedals (part);
    const auto open = std::find_if (pedals.begin(), pedals.end(), [] (const PedalSpan& p) { return ! p.lifted; });
    const auto within = std::find_if (pedals.begin(), pedals.end(), [at] (const PedalSpan& p) { return p.lifted && p.start < at && at < p.end; });

    if (open != pedals.end())
    {
        // Lifting the one that's down: from where it went down to here, either way round. Any
        // others in between make way.
        if (juce::exactlyEqual (open->start, at))
            return;

        const PedalSpan lifted { juce::jmin (open->start, at), juce::jmax (open->start, at), true };
        pedals.erase (open);
        pedals.erase (std::remove_if (pedals.begin(), pedals.end(), [&] (const PedalSpan& p) { return p.start < lifted.end && p.end > lifted.start; }),
                      pedals.end());
        pedals.push_back (lifted);
    }
    else if (within != pedals.end())
    {
        // Between where one goes down and comes up: lifted and put straight down again here
        const auto end = within->end;
        within->end = at;
        pedals.push_back ({ at, end, true });
    }
    else
    {
        pedals.push_back ({ at, at, false });
    }

    std::sort (pedals.begin(), pedals.end(), [] (const PedalSpan& a, const PedalSpan& b) { return a.start < b.start; });
    score.setPedals (part, pedals);
    hoverHintHidden = true;
}

float StaffView::getMarkingCentreY() const
{
    return (getY (Staff::treble, 0) + getY (Staff::bass, topLine)) / 2.0f;
}

float StaffView::getDynamicBaseline() const
{
    // With the letters' middles on the halfway line between the staves
    return getMarkingCentreY() + 0.45f * staffSpace;
}

juce::Range<float> StaffView::getHairpinSpan (int measure, double beat, double end) const
{
    const auto dynamicHalfWidth = [this] (music::Dynamic dynamic)
    {
        return glyphs.getPath (Smufl::dynamics[(size_t) dynamic]).getBounds().getWidth() / 2.0f;
    };

    auto start = getOnsetX (measure, beat) - 0.4f * staffSpace;

    if (const auto dynamic = score.getDynamic (part, measure, beat))
        start = getOnsetX (measure, beat) + dynamicHalfWidth (*dynamic) + 0.5f * staffSpace;

    // To the note it leads to, or the end of the score
    const auto [endMeasure, endBeat] = fromScoreBeats (end);
    auto finish = 0.0f;

    if (endMeasure >= score.getNumMeasures())
    {
        const auto& last = measureLayouts.back();
        finish = last.x + last.width - 1.2f * staffSpace;
    }
    else if (const auto dynamic = score.getDynamic (part, endMeasure, endBeat))
    {
        finish = getOnsetX (endMeasure, endBeat) - dynamicHalfWidth (*dynamic) - 0.5f * staffSpace;
    }
    else
    {
        finish = getOnsetX (endMeasure, endBeat) - 0.8f * staffSpace;
    }

    return { start, juce::jmax (finish, start + 1.5f * staffSpace) };
}

void StaffView::drawHairpin (juce::Graphics& g, music::Hairpin hairpin, juce::Range<float> span) const
{
    // Two lines meeting at one end and opening out to a staff space and a half at the other
    const auto centre = getMarkingCentreY();
    const auto halfOpening = 0.75f * staffSpace;
    const auto crescendo = hairpin == music::Hairpin::crescendo;
    const auto pointX = crescendo ? span.getStart() : span.getEnd();
    const auto openX = crescendo ? span.getEnd() : span.getStart();

    juce::Path wedge;
    wedge.startNewSubPath (openX, centre - halfOpening);
    wedge.lineTo (pointX, centre);
    wedge.lineTo (openX, centre + halfOpening);
    g.strokePath (wedge, juce::PathStrokeType (0.14f * staffSpace, juce::PathStrokeType::mitered, juce::PathStrokeType::butt));
}

void StaffView::drawHairpins (juce::Graphics& g) const
{
    for (const auto& hairpin : score.getHairpins (part))
    {
        // One being stretched is drawn where the drag has got to instead.
        if (hairpinDrag.has_value() && hairpinDrag->stretching == hairpin)
            continue;

        // Under the mouse, a click will take it out, or a drag stretch it, so it's highlighted.
        const auto hovered = getChosenHairpin().has_value() && ! hoverHintHidden && hoverHairpin == hairpin;
        const auto underEraser = erasing && hoverErasable.hairpin == hairpin;
        g.setColour (underEraser ? removalColour : hovered ? hoverColour : inkColour);
        drawHairpin (g, hairpin.type, getHairpinSpan (hairpin.measure, hairpin.beat, toScoreBeats (hairpin.measure, hairpin.beat) + hairpin.length));
    }

    // The one being dragged out, or stretched
    if (hairpinDrag.has_value())
    {
        const auto& start = hairpinDrag->start;
        g.setColour (hoverColour);
        drawHairpin (g, start.type, getHairpinSpan (start.measure, start.beat, hairpinDrag->end));
    }
    // Where a click would put a beat-long one
    else if (const auto chosen = getChosenHairpin(); chosen.has_value() && hoverMarkPoint.has_value() && ! hoverHairpin.has_value() && ! hoverHintHidden)
    {
        const auto [measure, beat] = *hoverMarkPoint;

        if (measure < score.getNumMeasures() && beat < score.getBeatsPerMeasure())
        {
            const auto start = toScoreBeats (measure, beat);
            g.setColour (hoverColour);
            drawHairpin (g, *chosen, getHairpinSpan (measure, beat, juce::jmin (start + 1.0, (double) (score.getNumMeasures() * score.getBeatsPerMeasure()))));
        }
    }

    g.setColour (inkColour);
}

void StaffView::drawDynamics (juce::Graphics& g, int measure) const
{
    g.setColour (inkColour);

    // Ones on beats a shorter time signature leaves out are kept, but not shown.
    for (int slot = 0; slot < score.getBeatsPerMeasure() * Score::slotsPerBeat; ++slot)
    {
        const auto beat = (double) slot / Score::slotsPerBeat;

        if (const auto marked = score.getDynamic (part, measure, beat))
        {
            // Over a dynamic, a click will replace it or take it out, so it's highlighted.
            const auto hovered = getChosenDynamic().has_value() && ! hoverHintHidden && hoverMarkPoint == std::pair { measure, beat };
            const auto underEraser = erasing && hoverErasable.dynamic == std::pair { measure, beat };
            g.setColour (underEraser ? removalColour : hovered ? hoverColour : inkColour);
            drawCentred (g, Smufl::dynamics[(size_t) *marked], getOnsetX (measure, beat), getDynamicBaseline());
        }
    }

    g.setColour (inkColour);
}

void StaffView::drawHoverDynamic (juce::Graphics& g) const
{
    const auto dynamic = getChosenDynamic();

    if (! dynamic.has_value() || ! hoverMarkPoint.has_value() || hoverHintHidden)
        return;

    const auto [measure, beat] = *hoverMarkPoint;

    if (measure >= score.getNumMeasures() || beat >= score.getBeatsPerMeasure() || score.getDynamic (part, measure, beat).has_value())
        return;

    g.setColour (hoverColour);
    drawCentred (g, Smufl::dynamics[(size_t) *dynamic], getOnsetX (measure, beat), getDynamicBaseline());
}

void StaffView::drawCentred (juce::Graphics& g, juce::juce_wchar glyph, float centreX, float y) const
{
    glyphs.draw (g, glyph, { centreX - glyphs.getPath (glyph).getBounds().getCentreX(), y });
}

//==============================================================================
void StaffView::mouseMove (const juce::MouseEvent& e)
{
    if (part < 0)
        return;

    if (scalePlacement.has_value())
    {
        if (const auto start = getNoteAt (e.position); start != scaleHover)
        {
            scaleHover = start;
            repaint();
        }

        return;
    }

    if (erasing)
    {
        setHoverErasable (findErasableAt (e.position));
        return;
    }

    if (! marking.has_value())
    {
        setHoverNote (getNoteToClickAt (e.position));
        return;
    }

    // With the pedal chosen, a pedal mark shows where a click would put it.
    if (isPedalChosen())
    {
        const auto point = getPedalPointAt (e.position);

        if (point != hoverMarkPoint)
        {
            hoverMarkPoint = point;
            hoverHintHidden = false;
            repaint();
        }

        return;
    }

    // With a hairpin chosen, one under the mouse is highlighted, and its end can be dragged.
    const auto overHairpin = getChosenHairpin().has_value() ? findHairpinAt (e.position) : std::nullopt;
    const auto newHoverHairpin = overHairpin.has_value() ? std::optional (overHairpin->first) : std::nullopt;

    if (newHoverHairpin != hoverHairpin)
    {
        hoverHairpin = newHoverHairpin;
        hoverHintHidden = false;
        repaint();
    }

    setMouseCursor (overHairpin.has_value() && overHairpin->second ? juce::MouseCursor::LeftRightResizeCursor : juce::MouseCursor::NormalCursor);
    setHoverMarkPoint (getDynamicPointAt (e.position));
}

void StaffView::mouseExit (const juce::MouseEvent&)
{
    if (! erasingStroke)
        setHoverErasable ({});

    setHoverNote ({});
    setHoverMarkPoint ({});

    if (hoverHairpin.has_value())
    {
        hoverHairpin.reset();
        repaint();
    }
}

void StaffView::mouseDown (const juce::MouseEvent& e)
{
    if (part < 0)
        return;

    pressedNote.reset();
    pressedDynamic.reset();
    pressedPedal.reset();
    pressedHairpin.reset();
    hairpinDrag.reset();
    tieFrom.reset();

    if (e.mods.isPopupMenu())
        return;

    if (onClicked != nullptr)
        onClicked();

    // Adding from the scale, a click says where, when the mouse is let go.
    if (scalePlacement.has_value())
    {
        pressedNote = getNoteAt (e.position);
        return;
    }

    // With the eraser, everything the mouse goes over until it's let go is taken out.
    if (erasing)
    {
        erasingStroke = true;
        lastErasePoint = e.position;

        if (onEraseStarted != nullptr)
            onEraseStarted();

        eraseAt (e.position);
        setHoverErasable (findErasableAt (e.position));
        return;
    }

    // With the pedal chosen, a click marks it, when the mouse is let go.
    if (isPedalChosen())
    {
        pressedPedal = getPedalPointAt (e.position);
        return;
    }

    // With a dynamic chosen, a click marks it, when the mouse is let go.
    if (getChosenDynamic().has_value())
    {
        pressedDynamic = getDynamicPointAt (e.position);
        return;
    }

    // With a hairpin chosen, dragging a hairpin's end stretches it, and anywhere else, dragging
    // marks a new one. A click on a hairpin takes it out.
    if (const auto chosen = getChosenHairpin())
    {
        if (const auto under = findHairpinAt (e.position))
        {
            const auto& [hairpin, onEnd] = *under;

            if (onEnd)
            {
                hairpinDrag = HairpinDrag { hairpin, toScoreBeats (hairpin.measure, hairpin.beat) + hairpin.length, hairpin };
                return;
            }

            pressedHairpin = hairpin;
        }

        if (const auto point = getDynamicPointAt (e.position))
            hairpinDrag = HairpinDrag { { point->first, point->second, *chosen, 1.0 }, toScoreBeats (point->first, point->second) + 1.0, std::nullopt };

        return;
    }

    // Nothing changes until the mouse is let go: a click adds or takes out a note, and a drag
    // from a note can tie it to the next one of the same pitch.
    pressedNote = getNoteToClickAt (e.position);
    tieFrom = getTieableNoteAt (e.position);
}

void StaffView::mouseDrag (const juce::MouseEvent& e)
{
    if (part < 0)
        return;

    if (erasingStroke)
    {
        eraseAlong (lastErasePoint, e.position);
        lastErasePoint = e.position;
        setHoverErasable (findErasableAt (e.position));
        return;
    }

    // A hairpin stretches to the beat nearest the mouse, at least an eighth note long.
    if (hairpinDrag.has_value())
    {
        if (e.mouseWasDraggedSinceMouseDown())
        {
            const auto start = toScoreBeats (hairpinDrag->start.measure, hairpinDrag->start.beat);
            hairpinDrag->end = juce::jmax (start + 0.5, getHairpinEndAt (e.position.x));
            hoverHintHidden = true;
            repaint();
        }

        return;
    }

    if (! tieFrom.has_value() || ! e.mouseWasDraggedSinceMouseDown())
        return;

    // A tie follows the mouse from the note.
    dragPoint = e.position;
    hoverHintHidden = true;
    repaint();
}

void StaffView::mouseUp (const juce::MouseEvent& e)
{
    if (erasingStroke)
    {
        erasingStroke = false;

        if (onEraseFinished != nullptr)
            onEraseFinished();

        return;
    }

    if (part < 0)
        return;

    if (scalePlacement.has_value())
    {
        const auto start = pressedNote;
        pressedNote.reset();

        if (start.has_value() && ! e.mods.isPopupMenu() && ! e.mouseWasDraggedSinceMouseDown())
            placeScale (*start);

        return;
    }

    const auto pressed = pressedNote;
    const auto from = tieFrom;
    const auto pressedPoint = pressedDynamic;
    const auto pedalPoint = pressedPedal;
    const auto clickedHairpin = pressedHairpin;
    const auto drag = hairpinDrag;
    pressedNote.reset();
    pressedDynamic.reset();
    pressedPedal.reset();
    pressedHairpin.reset();
    hairpinDrag.reset();
    tieFrom.reset();
    repaint();

    if (e.mods.isPopupMenu())
        return;

    if (pedalPoint.has_value() && isPedalChosen() && ! e.mouseWasDraggedSinceMouseDown())
    {
        clickPedal (*pedalPoint);
        return;
    }

    if (pressedPoint.has_value() && getChosenDynamic().has_value() && ! e.mouseWasDraggedSinceMouseDown())
    {
        clickDynamic (*pressedPoint);
        return;
    }

    if (drag.has_value())
    {
        hoverHintHidden = true;

        // A click on a hairpin takes it out, a click on its end leaves it as it is, and a click
        // anywhere else marks one a beat long.
        if (! e.mouseWasDraggedSinceMouseDown())
        {
            if (clickedHairpin.has_value())
                score.removeHairpin (part, clickedHairpin->measure, clickedHairpin->beat);
            else if (! drag->stretching.has_value())
                score.setHairpin (part, drag->start);

            hoverHairpin.reset();
            return;
        }

        auto hairpin = drag->start;
        hairpin.length = drag->end - toScoreBeats (hairpin.measure, hairpin.beat);
        score.setHairpin (part, hairpin);
        hoverHairpin.reset();
        return;
    }

    if (e.mods.isPopupMenu() || ! pressed.has_value())
        return;

    if (! e.mouseWasDraggedSinceMouseDown())
    {
        clickNote (*pressed);
        return;
    }

    // Let go over the next note of the same pitch, the drag ties them, or unties them. Anywhere
    // else, nothing happens.
    if (const auto to = getTieableNoteAt (e.position); from.has_value() && to.has_value() && to->staff == from->staff
                                                       && to->pitch == from->pitch && (to->measure != from->measure || ! juce::exactlyEqual (to->beat, from->beat)))
        score.toggleTie (part, from->staff, from->measure, from->beat, to->measure, to->beat, from->pitch);
}

bool StaffView::hasRandomChord (int measure) const
{
    const auto* chord = score.getChord (part, measure);
    return chord != nullptr && chord->hasNotes() && chord->style.type == music::ChordType::random;
}

std::optional<double> StaffView::findWrittenNoteOnset (const Note& spot, float x) const
{
    std::optional<double> nearest;
    auto nearestDistance = 1.4f * staffSpace;

    for (const auto& event : measureLayouts[(size_t) spot.measure].content.staves[(size_t) spot.staff].events)
        for (const auto& tone : event.tones)
            if (const auto distance = std::abs (x - getEventX (spot.measure, event)); tone.pitch.step == spot.pitch.step && distance <= nearestDistance)
            {
                nearest = event.onset;
                nearestDistance = distance;
            }

    return nearest;
}

std::optional<Note> StaffView::getNoteToClickAt (juce::Point<float> point) const
{
    auto note = getNoteAt (point);

    // A Random chord's notes are each taken out on their own, so it's the one under the point.
    if (note.has_value() && hasRandomChord (note->measure) && score.chordUsesStaff (part, note->measure, note->staff))
        if (const auto onset = findWrittenNoteOnset (*note, point.x))
            note->beat = *onset;

    return note;
}

void StaffView::writeOutRandomChordWithout (int measure, Staff staff, double beat, int midi)
{
    // Everything the measure shows, as it's written, but the one note
    const auto content = getMeasureContent (score, part, measure);
    std::vector<Note> notes;

    for (auto writtenStaff : { Staff::treble, Staff::bass })
        for (const auto& event : content.staves[(size_t) writtenStaff].events)
            for (const auto& tone : event.tones)
                if (! (writtenStaff == staff && juce::exactlyEqual (event.onset, beat) && tone.midi == midi))
                    notes.push_back ({ writtenStaff, measure, event.onset, tone.pitch, part, getBeats (event.duration) });

    score.replaceChordWithNotes (part, measure, notes);
}

void StaffView::clickNote (const Note& note)
{
    // In a measure with a Random chord, clicking one of its notes takes out just that note: the
    // chord goes, and its other notes stay as they are, as notes of their own.
    if (hasRandomChord (note.measure) && score.chordUsesStaff (part, note.measure, note.staff))
    {
        for (const auto& event : measureLayouts[(size_t) note.measure].content.staves[(size_t) note.staff].events)
            for (const auto& tone : event.tones)
                if (tone.pitch.step == note.pitch.step && juce::exactlyEqual (event.onset, note.beat))
                {
                    writeOutRandomChordWithout (note.measure, note.staff, event.onset, tone.midi);
                    hoverHintHidden = true;
                    return;
                }
    }

    // On a staff with a chord's notes, clicking one of them takes it out of the chord, and
    // clicking anywhere else adds a note to them.
    if (score.chordUsesStaff (part, note.measure, note.staff))
    {
        const auto existing = findNoteUnder (note);
        const auto* chord = score.getChord (part, note.measure);
        const music::KeyboardNote clicked { existing ? existing->midi : note.pitch.getMidiNoteNumber(),
                                            existing ? existing->pitch.getSpelling() : note.pitch.getSpelling() };

        if (note.staff == chord->style.staff)
            score.toggleChordNote (part, note.measure, clicked.midi, chord->style);
        else
            score.toggleAlternateNote (part, note.measure, clicked);
    }
    // Clicking a note takes it out, whatever its sharp or flat; clicking anywhere else adds one.
    else if (score.hasNoteAt (part, note.staff, note.measure, note.beat, note.pitch.step))
        score.removeNotesAt (part, note.staff, note.measure, note.beat, note.pitch.step);
    else
        score.addNote (note);

    // Show what the click did, rather than what another click would do.
    hoverHintHidden = true;
    repaint (getBeatArea (note.measure, note.beat));
}

StaffView::Erasable StaffView::findErasableAt (juce::Point<float> point) const
{
    Erasable found;

    // A notehead: on the line or space under the mouse, and near enough across
    if (const auto spot = getNoteAt (point))
    {
        const auto& content = measureLayouts[(size_t) spot->measure].content.staves[(size_t) spot->staff];

        for (const auto& event : content.events)
            for (const auto& tone : event.tones)
                if (tone.pitch.step == spot->pitch.step && ! found.note.has_value()
                    && std::abs (point.x - getEventX (spot->measure, event)) <= 1.4f * staffSpace)
                {
                    found.note = Note { spot->staff, spot->measure, event.onset, tone.pitch, part, 1.0 };
                    found.tone = tone;
                }
    }

    // A tie, near its curve
    const auto measure = findMeasure (point.x);
    const auto nearMeasure = measure >= 0 ? measure : score.getNumMeasures() - 1;

    for (const auto& tie : getTieShapes (nearMeasure - 1, nearMeasure))
    {
        for (int i = 0; i <= 20 && ! found.tie.has_value(); ++i)
            if (point.getDistanceFrom (getPointOnTie (tie, (float) i / 20.0f)) <= 0.5f * staffSpace)
                found.tie = tie.from;
    }

    // A dynamic's letters
    for (int m = juce::jmax (0, nearMeasure - 1); m <= nearMeasure + 1 && m < score.getNumMeasures() && ! found.dynamic.has_value(); ++m)
    {
        for (int slot = 0; slot < score.getBeatsPerMeasure() * Score::slotsPerBeat; ++slot)
        {
            const auto beat = (double) slot / Score::slotsPerBeat;

            if (const auto dynamic = score.getDynamic (part, m, beat))
            {
                const auto& glyph = glyphs.getPath (Smufl::dynamics[(size_t) *dynamic]);
                const auto bounds = glyph.getBounds().translated (getOnsetX (m, beat) - glyph.getBounds().getCentreX(), getDynamicBaseline());

                if (bounds.expanded (0.3f * staffSpace).contains (point))
                    found.dynamic = std::pair { m, beat };
            }
        }
    }

    // A hairpin, anywhere along it
    if (const auto hairpin = findHairpinAt (point))
        found.hairpin = hairpin->first;

    // A pedal mark, anywhere along its line or ticks
    if (const auto lineY = getPedalLineY(); point.y >= lineY - 1.4f * staffSpace && point.y <= lineY + 0.6f * staffSpace)
        for (const auto& pedal : score.getPedals (part))
            if (const auto span = getPedalSpan (pedal); point.x >= span.getStart() - 0.5f * staffSpace && point.x <= span.getEnd() + 0.5f * staffSpace)
                found.pedal = pedal;

    return found;
}

void StaffView::eraseAt (juce::Point<float> point)
{
    const auto found = findErasableAt (point);

    // The tie first, as taking out its note would let go of it anyway.
    if (found.tie.has_value())
        if (const auto next = score.getFollowingBeat (part, found.tie->staff, found.tie->measure, found.tie->beat))
            score.toggleTie (part, found.tie->staff, found.tie->measure, found.tie->beat, next->first, next->second, found.tie->pitch);

    if (found.hairpin.has_value())
        score.removeHairpin (part, found.hairpin->measure, found.hairpin->beat);

    if (found.pedal.has_value())
    {
        auto pedals = score.getPedals (part);
        pedals.erase (std::remove (pedals.begin(), pedals.end(), *found.pedal), pedals.end());
        score.setPedals (part, pedals);
    }

    if (found.dynamic.has_value())
        score.setDynamic (part, found.dynamic->first, found.dynamic->second, std::nullopt);

    // A note is taken out as a click takes it out: from the chord, if it's a chord's, or for a
    // Random chord, by writing the chord's other notes out on their own.
    if (found.note.has_value() && found.tone.has_value())
    {
        const auto& note = *found.note;

        if (hasRandomChord (note.measure) && score.chordUsesStaff (part, note.measure, note.staff))
            writeOutRandomChordWithout (note.measure, note.staff, note.beat, found.tone->midi);
        else if (! score.chordUsesStaff (part, note.measure, note.staff))
            score.removeNotesAt (part, note.staff, note.measure, note.beat, note.pitch.step);
        else if (const auto* chord = score.getChord (part, note.measure); chord != nullptr && chord->style.staff == note.staff)
            score.toggleChordNote (part, note.measure, found.tone->midi, chord->style);
        else
            score.toggleAlternateNote (part, note.measure, { found.tone->midi, found.tone->pitch.getSpelling() });
    }
}

void StaffView::eraseAlong (juce::Point<float> from, juce::Point<float> to)
{
    // Every little way along, so a quick stroke doesn't skip over anything
    const auto steps = juce::jmax (1, (int) std::ceil (from.getDistanceFrom (to) / (0.4f * staffSpace)));

    for (int i = 1; i <= steps; ++i)
        eraseAt (from + (to - from) * ((float) i / (float) steps));
}

void StaffView::setHoverErasable (Erasable erasable)
{
    if (erasable == hoverErasable)
        return;

    hoverErasable = std::move (erasable);

    // A note's highlighted as it is when a click would take it out.
    hoverHintHidden = false;
    setHoverNote (hoverErasable.note);
    repaint();
}

void StaffView::clickDynamic (std::pair<int, double> point)
{
    const auto [measure, beat] = point;
    const auto dynamic = getChosenDynamic();
    score.setDynamic (part, measure, beat, score.getDynamic (part, measure, beat) == dynamic ? std::nullopt : dynamic);

    // Show what the click did, rather than what another click would do.
    hoverHintHidden = true;
    repaint (getBeatArea (measure, beat));
}

std::optional<Note> StaffView::getTieableNoteAt (juce::Point<float> point) const
{
    // A note clicked into the staff, with its own pitch, sharp or flat, rather than the one a
    // click there would add
    auto note = getNoteAt (point);

    if (! note.has_value() || score.chordUsesStaff (part, note->measure, note->staff))
        return {};

    for (const auto& pitch : score.getNotes (part, note->staff, note->measure, note->beat))
    {
        if (pitch.step == note->pitch.step)
        {
            note->pitch = pitch;
            return note;
        }
    }

    return {};
}

//==============================================================================
std::optional<StaffView::TiePoint> StaffView::getTiePoint (int measure, Staff staff, double beat, music::Pitch pitch) const
{
    if (! juce::isPositiveAndBelow (measure, (int) measureLayouts.size()))
        return {};

    const auto& content = measureLayouts[(size_t) measure].content.staves[(size_t) staff];

    if (content.source != StaffContent::Source::notes)
        return {};

    for (const auto& event : content.events)
    {
        if (event.isRest() || ! juce::exactlyEqual (event.onset, beat))
            continue;

        std::map<int, int> alterations;
        const NoteLayout layout (event, staff, getEventX (measure, event), alterations, music::getKeyAlterations (score.getKey()));

        // The layout's notes are low to high, as the event's are.
        auto tones = event.tones;
        std::sort (tones.begin(), tones.end(), [] (const music::Tone& a, const music::Tone& b)
        {
            return a.pitch.step != b.pitch.step ? a.pitch.step < b.pitch.step : a.pitch.alter < b.pitch.alter;
        });

        for (size_t i = 0; i < tones.size(); ++i)
        {
            if (tones[i].pitch != pitch)
                continue;

            // A single note's tie curves away from its stem; a chord's upper notes' curve up and
            // its lower notes' down.
            const auto upwards = tones.size() == 1 ? ! layout.stemUp
                               : (tones.size() % 2 == 1 && i == tones.size() / 2) ? ! layout.stemUp
                                                                                  : i >= tones.size() / 2;

            return TiePoint { { layout.headLefts[i] + layout.headWidth / 2.0f, getY (staff, layout.positions[i]) },
                              layout.headWidth / 2.0f, upwards };
        }
    }

    return {};
}

void StaffView::drawTie (juce::Graphics& g, juce::Point<float> start, juce::Point<float> end, bool upwards) const
{
    // A slur-shaped curve, thickest in the middle and fine at its ends
    const auto direction = upwards ? -1.0f : 1.0f;
    const auto height = getTieHeight (start, end, upwards);
    const auto thickness = 0.16f * staffSpace * direction;
    const auto middle = juce::Point<float> ((start.x + end.x) / 2.0f, (start.y + end.y) / 2.0f);

    juce::Path tie;
    tie.startNewSubPath (start);
    tie.quadraticTo (middle.translated (0.0f, height * 2.0f), end);
    tie.quadraticTo (middle.translated (0.0f, (height - thickness) * 2.0f), start);
    tie.closeSubPath();
    g.fillPath (tie);
}

float StaffView::getTieHeight (juce::Point<float> start, juce::Point<float> end, bool upwards) const
{
    // Higher the longer it is, within limits
    return juce::jlimit (0.5f, 1.6f, 0.12f * (end.x - start.x) / staffSpace + 0.3f) * staffSpace * (upwards ? -1.0f : 1.0f);
}

std::vector<StaffView::TieShape> StaffView::getTieShapes (int firstMeasure, int lastMeasure) const
{
    std::vector<TieShape> shapes;

    for (auto measure = juce::jmax (0, firstMeasure); measure <= lastMeasure && measure < (int) measureLayouts.size(); ++measure)
    {
        for (auto staff : { Staff::treble, Staff::bass })
        {
            for (const auto& [onset, pitch] : measureLayouts[(size_t) measure].content.staves[(size_t) staff].tiedNotes)
            {
                const auto next = score.getFollowingBeat (part, staff, measure, onset);
                const auto from = getTiePoint (measure, staff, onset, pitch);
                const auto to = next.has_value() ? getTiePoint (next->first, staff, next->second, pitch) : std::nullopt;

                if (! from.has_value() || ! to.has_value())
                    continue;

                // From just after the first notehead to just before the second, a little away from their centres
                const auto offset = 0.45f * staffSpace * (from->upwards ? -1.0f : 1.0f);
                shapes.push_back ({ Note { staff, measure, onset, pitch, part, 1.0 },
                                    from->centre.translated (from->halfWidth + 0.15f * staffSpace, offset),
                                    to->centre.translated (-to->halfWidth - 0.15f * staffSpace, offset), from->upwards });
            }
        }
    }

    return shapes;
}

juce::Point<float> StaffView::getPointOnTie (const TieShape& tie, float proportion) const
{
    // The middle of the curve's thickness, along the quadratic curve drawTie draws
    const auto middle = (tie.start + tie.end) / 2.0f;
    const auto control = middle.translated (0.0f, getTieHeight (tie.start, tie.end, tie.upwards) * 2.0f);
    const auto t = proportion, u = 1.0f - proportion;
    return tie.start * (u * u) + control * (2.0f * u * t) + tie.end * (t * t);
}

void StaffView::drawTies (juce::Graphics& g, int firstMeasure, int lastMeasure) const
{
    for (const auto& tie : getTieShapes (firstMeasure, lastMeasure))
    {
        // Under the eraser, it's red, to show it would go.
        g.setColour (erasing && hoverErasable.tie == tie.from ? removalColour : inkColour);
        drawTie (g, tie.start, tie.end, tie.upwards);
    }

    g.setColour (inkColour);
}

void StaffView::drawTieDrag (juce::Graphics& g) const
{
    if (! tieFrom.has_value() || ! hoverHintHidden)
        return;

    const auto from = getTiePoint (tieFrom->measure, tieFrom->staff, tieFrom->beat, tieFrom->pitch);

    if (! from.has_value())
        return;

    // To the note it would tie to, if the mouse is over one, or else to the mouse.
    auto end = dragPoint;
    auto onTarget = false;

    if (const auto to = getTieableNoteAt (dragPoint); to.has_value() && to->staff == tieFrom->staff && to->pitch == tieFrom->pitch
                                                      && score.getFollowingBeat (part, to->staff, tieFrom->measure, tieFrom->beat)
                                                             == std::optional<std::pair<int, double>> ({ to->measure, to->beat }))
    {
        if (const auto point = getTiePoint (to->measure, to->staff, to->beat, to->pitch))
        {
            end = point->centre.translated (-point->halfWidth - 0.15f * staffSpace, 0.0f);
            onTarget = true;
        }
    }

    const auto offset = 0.45f * staffSpace * (from->upwards ? -1.0f : 1.0f);
    const auto start = from->centre.translated ((end.x >= from->centre.x ? 1.0f : -1.0f) * (from->halfWidth + 0.15f * staffSpace), offset);

    g.setColour (selectionColour.withAlpha (onTarget ? 1.0f : 0.5f));
    drawTie (g, end.x >= start.x ? start : end.translated (0.0f, offset), end.x >= start.x ? end.translated (0.0f, onTarget ? offset : 0.0f) : start,
             from->upwards);
}

void StaffView::setHoverMarkPoint (std::optional<std::pair<int, double>> point)
{
    if (point == hoverMarkPoint)
        return;

    hoverHintHidden = false;

    // A hairpin's hint reaches into the next beat.
    const auto area = [this] (std::pair<int, double> p)
    {
        return getBeatArea (p.first, p.second).getUnion (getBeatArea (p.first, juce::jmin (p.second + 1.0, (double) score.getBeatsPerMeasure() - 0.5)))
                   .expanded (juce::roundToInt (staffSpace), 0);
    };

    if (hoverMarkPoint.has_value())
        repaint (area (*hoverMarkPoint));

    hoverMarkPoint = point;

    if (hoverMarkPoint.has_value())
        repaint (area (*hoverMarkPoint));
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
    // As many as the measures laid out, which may be behind the score while it's changing
    if (part < 0)
        return;

    const auto numMeasures = juce::jmin (score.getNumMeasures(), (int) measureLayouts.size());

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
