#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <utility>

/** Colours and helpers shared by the app's controls. */
namespace controls
{
    inline const juce::Colour accent { 0xff2f5fd0 };
    inline const juce::Colour accentLight { 0xffe3eafb };
    inline const juce::Colour secondaryText { 0xff6b7079 };
    inline const juce::Colour sidebarBackground { 0xfff4f4f6 };

    /** Joins buttons into a segmented control, with one of them on at a time. */
    inline void makeSegmented (std::initializer_list<juce::TextButton*> buttons, int radioGroupId)
    {
        const auto count = (int) buttons.size();
        auto index = 0;

        for (auto* button : buttons)
        {
            button->setClickingTogglesState (true);
            button->setRadioGroupId (radioGroupId);
            button->setWantsKeyboardFocus (false);
            button->setConnectedEdges ((index > 0 ? juce::Button::ConnectedOnLeft : 0)
                                       | (index < count - 1 ? juce::Button::ConnectedOnRight : 0));
            button->setColour (juce::TextButton::buttonOnColourId, accentLight);
            button->setColour (juce::TextButton::textColourOnId, accent);
            ++index;
        }
    }

    /** The tile behind one of the small symbol buttons over the score, such as a note length's,
        and the colour to draw its symbol in.
    */
    inline juce::Colour drawSymbolButtonTile (juce::Graphics& g, const juce::Button& button, bool highlighted, bool down)
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        const auto chosen = button.getToggleState();

        g.setColour (chosen ? accentLight : down ? juce::Colour (0xffe8e8ec) : highlighted ? juce::Colour (0xfff2f2f5) : juce::Colours::white);
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (chosen ? accent.withAlpha (0.6f) : juce::Colours::black.withAlpha (0.18f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);

        return chosen ? accent : juce::Colour (0xff1b1b1b);
    }

    /** A small capitalised heading over a control, such as "KEY". */
    inline void makeHeading (juce::Label& label, const juce::String& text)
    {
        label.setText (text.toUpperCase(), juce::dontSendNotification);
        label.setFont (juce::FontOptions (11.0f, juce::Font::bold));
        label.setColour (juce::Label::textColourId, secondaryText);
        label.setBorderSize ({});
    }

    /** Text with a flat or sharp sign in it, from a UTF-8 string. */
    inline juce::String fromUTF8 (const char* text)
    {
        return juce::String (juce::CharPointer_UTF8 (text));
    }

    /** Draws small rotary dials: a thin track with the value arced along it, and a knob with a
        pointer, and small value text under them. A dial whose "muted" property is set is grey.
    */
    struct DialLookAndFeel final : public juce::LookAndFeel_V4
    {
        DialLookAndFeel() : LookAndFeel_V4 (getLightColourScheme()) {}

        void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                               float startAngle, float endAngle, juce::Slider& slider) override
        {
            const auto muted = (bool) slider.getProperties().getWithDefault ("muted", false);
            const auto colour = muted ? juce::Colours::black.withAlpha (0.3f) : accent;
            const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat();
            const auto radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) / 2.0f - 2.0f;
            const auto centre = bounds.getCentre();
            const auto angle = startAngle + position * (endAngle - startAngle);
            const auto arcThickness = 2.5f;

            const auto arc = [&] (float from, float to)
            {
                juce::Path path;
                path.addCentredArc (centre.x, centre.y, radius, radius, 0.0f, from, to, true);
                return path;
            };

            const juce::PathStrokeType stroke (arcThickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);
            g.setColour (juce::Colours::black.withAlpha (0.12f));
            g.strokePath (arc (startAngle, endAngle), stroke);

            if (position > 0.0f)
            {
                g.setColour (colour);
                g.strokePath (arc (startAngle, angle), stroke);
            }

            const auto knobRadius = radius - arcThickness - 2.5f;
            g.setColour (juce::Colours::white);
            g.fillEllipse (juce::Rectangle<float> (2.0f * knobRadius, 2.0f * knobRadius).withCentre (centre));
            g.setColour (juce::Colours::black.withAlpha (0.2f));
            g.drawEllipse (juce::Rectangle<float> (2.0f * knobRadius, 2.0f * knobRadius).withCentre (centre), 1.0f);

            const auto tip = centre.getPointOnCircumference (knobRadius - 2.0f, angle);
            g.setColour (position > 0.0f ? colour : secondaryText);
            g.drawLine ({ centre.getPointOnCircumference (knobRadius * 0.3f, angle), tip }, 2.0f);
        }

        juce::Label* createSliderTextBox (juce::Slider& slider) override
        {
            auto* label = LookAndFeel_V4::createSliderTextBox (slider);
            label->setFont (juce::FontOptions (11.5f));
            return label;
        }
    };

    /** A rotary dial that can also be clicked, without turning it. */
    struct ClickableDial final : public juce::Slider
    {
        /** Called when the dial's clicked and let go without being dragged. */
        std::function<void()> onClick;

        void mouseUp (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseUp (e);

            if (isEnabled() && ! e.mouseWasDraggedSinceMouseDown() && ! e.mods.isPopupMenu() && onClick != nullptr)
                onClick();
        }
    };

    /** Draws buttons as small circles, for single symbols such as + and −. */
    struct RoundButtonLookAndFeel final : public juce::LookAndFeel_V4
    {
        RoundButtonLookAndFeel() : LookAndFeel_V4 (getLightColourScheme()) {}

        void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                                   bool highlighted, bool down) override
        {
            const auto bounds = button.getLocalBounds().toFloat().reduced (1.5f);
            const auto diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
            const auto circle = juce::Rectangle<float> (diameter, diameter).withCentre (bounds.getCentre());

            g.setColour (! button.isEnabled() ? juce::Colours::white
                         : down                ? accentLight.darker (0.08f)
                         : highlighted         ? accentLight
                                               : juce::Colours::white);
            g.fillEllipse (circle);

            g.setColour (button.isEnabled() ? accent.withAlpha (0.55f) : juce::Colours::black.withAlpha (0.12f));
            g.drawEllipse (circle, 1.2f);
        }

        juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override
        {
            return juce::FontOptions ((float) buttonHeight * 0.7f);
        }
    };

    /** A combo box that says whenever an item's chosen from its menu, even the one that's already
        chosen, which onChange doesn't.
    */
    struct ReselectableComboBox final : public juce::ComboBox
    {
        /** Called with the item chosen from the menu. */
        std::function<void (int itemId)> onItemChosen;

        void showPopup() override
        {
            if (! isEnabled())
                return;

            // The menu as the combo box shows it, with the chosen item ticked
            auto menu = *getRootMenu();

            for (juce::PopupMenu::MenuItemIterator iterator (menu, true); iterator.next();)
                if (auto& item = iterator.getItem(); item.itemID != 0)
                    item.isTicked = item.itemID == getSelectedId();

            menu.setLookAndFeel (&getLookAndFeel());
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this)
                                                          .withItemThatMustBeVisible (getSelectedId())
                                                          .withInitiallySelectedItem (getSelectedId())
                                                          .withMinimumWidth (getWidth())
                                                          .withMaximumNumColumns (1)
                                                          .withStandardItemHeight (getHeight()),
                                [safeThis = juce::Component::SafePointer (this)] (int result)
                                {
                                    if (safeThis == nullptr)
                                        return;

                                    safeThis->hidePopup();

                                    if (result == 0)
                                        return;

                                    safeThis->setSelectedId (result, juce::dontSendNotification);

                                    if (safeThis->onItemChosen != nullptr)
                                        safeThis->onItemChosen (result);
                                });
        }
    };

    /** A toolbar button showing a transport symbol instead of its text: a green triangle to play,
        a black square to stop, or a red circle to record. Its text names it, for accessibility.
    */
    struct TransportButton final : public juce::TextButton
    {
        enum class Symbol { play, stop, record };

        explicit TransportButton (Symbol initialSymbol) : symbol (initialSymbol) {}

        void setSymbol (Symbol newSymbol)
        {
            if (std::exchange (symbol, newSymbol) != newSymbol)
                repaint();
        }

        Symbol getSymbol() const noexcept { return symbol; }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            getLookAndFeel().drawButtonBackground (g, *this, findColour (getToggleState() ? buttonOnColourId : buttonColourId),
                                                   highlighted, down);

            const auto size = (float) getHeight() * 0.42f;
            const auto centre = getLocalBounds().toFloat().getCentre();

            if (symbol == Symbol::play)
            {
                // Pointing right, its middle a little right of the centre, so it looks centred
                juce::Path triangle;
                const auto height = size * 1.05f, width = height * 0.88f;
                const auto left = centre.x - width * 0.42f;
                triangle.addTriangle (left, centre.y - height / 2.0f, left, centre.y + height / 2.0f, left + width, centre.y);
                g.setColour (juce::Colour (0xff2e9e4f));
                g.fillPath (triangle);
                g.strokePath (triangle, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }
            else if (symbol == Symbol::stop)
            {
                g.setColour (juce::Colour (0xff1b1b1b));
                g.fillRoundedRectangle (juce::Rectangle<float> (size * 0.85f, size * 0.85f).withCentre (centre), 1.5f);
            }
            else
            {
                g.setColour (juce::Colour (0xffd8413a));
                g.fillEllipse (juce::Rectangle<float> (size, size).withCentre (centre));
            }
        }

    private:
        Symbol symbol;
    };

    /** A small button showing a waste bin, for taking something out: grey, and red under the mouse. */
    struct BinButton final : public juce::Button
    {
        BinButton() : juce::Button ("Delete") { setWantsKeyboardFocus (false); }

        void paintButton (juce::Graphics& g, bool highlighted, bool down) override
        {
            const auto area = getLocalBounds().toFloat().withSizeKeepingCentre (14.0f, 16.0f);
            const auto colour = ! isEnabled() ? juce::Colours::black.withAlpha (0.15f)
                              : down          ? juce::Colour (0xffb02a24)
                              : highlighted   ? juce::Colour (0xffd8413a)
                                              : secondaryText;

            // A lid with a handle, over a bin a little narrower at the bottom, with two grooves
            const auto lidY = area.getY() + 3.0f;
            juce::Path bin;
            bin.startNewSubPath (area.getX() + 1.5f, lidY + 2.5f);
            bin.lineTo (area.getX() + 2.8f, area.getBottom());
            bin.lineTo (area.getRight() - 2.8f, area.getBottom());
            bin.lineTo (area.getRight() - 1.5f, lidY + 2.5f);

            g.setColour (colour);
            g.strokePath (bin, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            g.drawLine (area.getX(), lidY, area.getRight(), lidY, 1.5f);
            g.drawRoundedRectangle (area.withTrimmedLeft (4.5f).withTrimmedRight (4.5f).withHeight (3.5f), 1.0f, 1.2f);

            for (auto x : { area.getCentreX() - 2.2f, area.getCentreX() + 2.2f })
                g.drawLine (x, lidY + 5.0f, x, area.getBottom() - 2.5f, 1.1f);
        }
    };
}
