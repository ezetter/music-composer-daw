#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

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
}
