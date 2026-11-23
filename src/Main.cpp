#include <JuceHeader.h>
// Single translation unit: the vendored Tracktion example UI defines
// non-inline functions in headers, so everything is compiled here once.
#include "MainComponent.cpp"

//==============================================================================
class TonkoriApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return ProjectInfo::projectName; }
    const juce::String getApplicationVersion() override { return ProjectInfo::versionString; }
    bool moreThanOneInstanceAllowed() override          { return false; }

    void initialise (const juce::String& commandLine) override
    {
        mainWindow = std::make_unique<MainWindow> (*this);
        importFromCommandLine (commandLine);
    }

    void anotherInstanceStarted (const juce::String& commandLine) override { importFromCommandLine (commandLine); }

    void importFromCommandLine (const juce::String& commandLine)
    {
        juce::StringArray files;
        for (auto& a : juce::StringArray::fromTokens (commandLine, true))
        {
            juce::File f (a.unquoted());
            if (a.isNotEmpty() && ! a.startsWith ("-") && f.existsAsFile()) files.add (f.getFullPathName());
        }
        if (files.isEmpty() || mainWindow == nullptr) return;
        if (auto* mc = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
            mc->importFiles (files, te::TimePosition(), te::getAudioTracks (*mc->getEditForTesting()).getFirst());
    }

    void shutdown() override { mainWindow = nullptr; }

    void systemRequestedQuit() override
    {
        if (mainWindow != nullptr)
            if (auto* mc = dynamic_cast<MainComponent*> (mainWindow->getContentComponent()))
                mc->saveCurrentEdit();

        quit();
    }

private:
    class MainWindow : public juce::DocumentWindow
    {
    public:
        explicit MainWindow (juce::JUCEApplication& a)
            : DocumentWindow ("Tonkori DAW", juce::Colour (0xff1c1f23), DocumentWindow::allButtons),
              app (a)
        {
            setUsingNativeTitleBar (true);
            setContentOwned (new MainComponent(), true);
            setResizable (true, false);
            setResizeLimits (900, 500, 10000, 10000);
            centreWithSize (1400, 850);
            setVisible (true);
        }

        void closeButtonPressed() override { app.systemRequestedQuit(); }

    private:
        juce::JUCEApplication& app;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainWindow)
    };

    std::unique_ptr<MainWindow> mainWindow;
};

START_JUCE_APPLICATION (TonkoriApplication)
