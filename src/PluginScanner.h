#pragma once

#include <JuceHeader.h>

/*  Out-of-process plug-in scanning (as Logic, Live and Reaper do).

    Each AU/VST3 is opened by a separate copy of Tonkori started with
    "--tonkori-scan <format> <identifier> <result.xml>". A plug-in that crashes or hangs
    while being scanned only takes that helper down; it is skipped (blacklisted) and the
    scan carries on, so one bad plug-in can never crash the session. */

namespace tonkori::scan
{
    inline const char* flag = "--tonkori-scan";

    /** Child side: scans one plug-in and writes its descriptions. Returns the exit code. */
    inline int runChildScan (const juce::StringArray& args)
    {
        const int i = args.indexOf (flag);
        if (i < 0 || i + 3 >= args.size()) return 2;
        const auto formatName = args[i + 1], identifier = args[i + 2];
        const juce::File out (args[i + 3]);

        juce::AudioPluginFormatManager fm;
        juce::addDefaultFormatsToManager (fm);

        juce::OwnedArray<juce::PluginDescription> found;
        for (auto* f : fm.getFormats())
            if (f->getName() == formatName)
                f->findAllTypesForFile (found, identifier);

        juce::XmlElement xml ("PLUGINS");
        for (auto* d : found)
            xml.addChildElement (d->createXml().release());
        return xml.writeTo (out) ? 0 : 3;
    }

    class OutOfProcessScanner : public juce::KnownPluginList::CustomScanner
    {
    public:
        bool findPluginTypesFor (juce::AudioPluginFormat& format, juce::OwnedArray<juce::PluginDescription>& result,
                                 const juce::String& fileOrIdentifier) override
        {
            const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
            const auto out = juce::File::getSpecialLocation (juce::File::tempDirectory)
                                .getNonexistentChildFile ("tonkori-scan", ".xml", false);

            juce::ChildProcess child;
            if (! child.start (juce::StringArray { exe.getFullPathName(), flag, format.getName(), fileOrIdentifier, out.getFullPathName() }, 0))
                return false;

            // Up to 2 minutes per plug-in (some AUs authorise or build caches on first open).
            for (int waited = 0; child.isRunning() && waited < 120000; waited += 100)
            {
                if (shouldExit()) { child.kill(); out.deleteFile(); return true; }
                juce::Thread::sleep (100);
            }

            if (child.isRunning()) { child.kill(); out.deleteFile(); return false; }   // hung: skip it
            const bool clean = child.getExitCode() == 0;

            if (auto xml = juce::parseXML (out))
                for (auto* e : xml->getChildIterator())
                {
                    auto d = std::make_unique<juce::PluginDescription>();
                    if (d->loadFromXml (*e)) result.add (d.release());
                }
            out.deleteFile();
            return clean;
        }
    };
}
