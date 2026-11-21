#pragma once

#include <JuceHeader.h>

namespace te = tracktion;

/*  Tracktion hands long jobs (render/export, freeze) to UIBehaviour::runTaskWithProgressBar,
    whose default does nothing, so without this a render silently produces no file. */
namespace tonkori
{
    /** Runs a job on the calling thread until finished or cancelled. */
    inline void runJobToCompletion (te::ThreadPoolJobWithProgress& job, std::function<bool (float)> keepGoing = {})
    {
        while (job.runJob() == juce::ThreadPoolJob::jobNeedsRunningAgain)
        {
            if (keepGoing && ! keepGoing (job.getCurrentTaskProgress()))
            {
                job.signalJobShouldExit();
                job.runJob();   // let it observe the exit flag and clean up
                return;
            }
        }
    }

    /** Runs a job behind a modal progress window (app). */
    inline void runJobWithProgressWindow (te::ThreadPoolJobWithProgress& job)
    {
        struct Window : public juce::ThreadWithProgressWindow
        {
            Window (te::ThreadPoolJobWithProgress& j)
                : ThreadWithProgressWindow (j.getJobName(), true, j.canCancel()), job (j) {}

            void run() override
            {
                runJobToCompletion (job, [this] (float p)
                {
                    setProgress (p);
                    return ! threadShouldExit();
                });
            }

            te::ThreadPoolJobWithProgress& job;
        };

        Window w (job);
        w.runThread();
    }
}
