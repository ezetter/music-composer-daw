#include "MainComponent.h"

class AnthropoceneMusicApplication final : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return JUCE_APPLICATION_NAME_STRING; }
    const juce::String getApplicationVersion() override { return JUCE_APPLICATION_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String&) override
    {
        // JUCE's usual Lucida Grande has no flat or sharp signs, and the font it falls back on
        // spaces them out, so chord names like B♭maj7 would come out as "B ♭ maj7".
        lookAndFeel.setDefaultSansSerifTypefaceName ("Helvetica Neue");
        juce::LookAndFeel::setDefaultLookAndFeel (&lookAndFeel);

        // Kept in ~/Library/Application Support/Anthropocene Music
        juce::PropertiesFile::Options options;
        options.applicationName = getApplicationName();
        options.folderName = getApplicationName();
        options.osxLibrarySubFolder = "Application Support";
        options.filenameSuffix = "settings";
        properties.setStorageParameters (options);

        mainWindow = std::make_unique<MainWindow> (getApplicationName(), *properties.getUserSettings());
    }

    void shutdown() override
    {
        mainWindow = nullptr;
        properties.closeFiles();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override
    {
        quit();
    }

private:
    class MainWindow final : public juce::DocumentWindow
    {
    public:
        MainWindow (const juce::String& name, juce::PropertiesFile& settings)
            : DocumentWindow (name,
                              juce::Desktop::getInstance().getDefaultLookAndFeel()
                                                          .findColour (juce::ResizableWindow::backgroundColourId),
                              DocumentWindow::allButtons)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent (settings), true);
            setResizable (true, false);
            setResizeLimits (MainComponent::minimumWidth, MainComponent::minimumHeight, 10000, 10000);
            centreWithSize (getWidth(), getHeight());
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            JUCEApplication::getInstance()->systemRequestedQuit();
        }

    private:
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    juce::LookAndFeel_V4 lookAndFeel { juce::LookAndFeel_V4::getLightColourScheme() };
    juce::ApplicationProperties properties;
    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (AnthropoceneMusicApplication)
