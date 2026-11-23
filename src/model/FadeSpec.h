#pragma once

#include <JuceHeader.h>
#include <vector>

/*  A fade curve that can be programmed numerically.

    Convention: every spec describes a RISING curve g(t), t ∈ [0,1] (fraction of the fade's
    duration), g(0) = 0, g(1) = 1. A fade-in plays g(t); a fade-out plays g(1 − t), so the
    same spec on both sides of a crossfade is symmetric (equal power stays equal power).

    Shapes (parameter in brackets):
      linear        g = t                                   (equal gain)
      equalPower    g = sin(πt/2)                           (constant energy in a crossfade)
      sCurve        g = tᵃ / (tᵃ + (1−t)ᵃ), a = 1 + 4·tension     [tension 0…1]
      exponential   g = (e^{kt} − 1)/(e^k − 1), k = 8·curvature   [curvature −1…+1; + = slow start]
      dBLinear      straight line in dB from floor to 0 dB, offset so g(0) = 0   [floor dB, e.g. −60]
      points        user breakpoints (t, gain), endpoints fixed at (0,0) and (1,1),
                    interpolated by spline (Catmull-Rom), damped spline (monotone cubic,
                    never overshoots) or linear.

    Serialised as text so it can live in the session and be typed directly, e.g.
      "equalPower"
      "sCurve 0.35"
      "exponential -0.5"
      "dBLinear -48"
      "points damped 25:-12 50:-4 75:-1"      (time % : gain dB, endpoints implied)
*/
namespace tonkori
{
    struct FadeSpec
    {
        enum class Shape  { linear, equalPower, sCurve, exponential, dBLinear, points };
        enum class Interp { spline, damped, linear };

        struct Point { double t = 0, gain = 0; };   // t ∈ (0,1), linear gain ∈ [0,1]

        Shape shape = Shape::equalPower;
        double param = 0.0;
        Interp interp = Interp::damped;
        std::vector<Point> points;                  // interior points only, sorted by t

        //==============================================================================
        static FadeSpec fromString (const juce::String&);
        juce::String toString() const;

        double gainAt (double t) const;             // rising curve, t clamped to [0,1]

        /** Gain table sampled at n points over t ∈ [0,1] (rising), as the engine expects. */
        std::vector<float> makeTable (int n = 4097) const;

        static juce::StringArray getShapeNames();   // UI order matches Shape enum
        static juce::String getParamName (Shape);   // empty if the shape has no parameter
        static juce::Range<double> getParamRange (Shape);
        static double getDefaultParam (Shape);

        bool operator== (const FadeSpec& o) const   { return toString() == o.toString(); }
        bool operator!= (const FadeSpec& o) const   { return ! operator== (o); }
    };

    //==============================================================================
    /** Session properties Tonkori stores on clips / the edit. */
    namespace ids
    {
        static const juce::Identifier fadeInSpec  ("tkFadeIn");
        static const juce::Identifier fadeOutSpec ("tkFadeOut");
        static const juce::Identifier fadesOn     ("tkFadesOn");
        static const juce::Identifier autoFade    ("tkAutoFade");
        static const juce::Identifier autoFadeMs  ("tkAutoFadeMs");
        static const juce::Identifier autoFadeSpec("tkAutoFadeSpec");
    }
}
