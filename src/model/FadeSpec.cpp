#include "FadeSpec.h"

namespace tonkori
{
namespace
{
    const char* shapeKeys[] = { "linear", "equalPower", "sCurve", "exponential", "dBLinear", "points" };
    const char* interpKeys[] = { "spline", "damped", "linear" };

    double dbToGain (double db)  { return db <= -144.0 ? 0.0 : std::pow (10.0, db / 20.0); }
    double gainToDb (double g)   { return g <= 0.0 ? -144.0 : 20.0 * std::log10 (g); }

    // Fritsch–Carlson monotone cubic Hermite slopes ("damped spline": no overshoot)
    std::vector<double> monotoneSlopes (const std::vector<double>& x, const std::vector<double>& y)
    {
        const size_t n = x.size();
        std::vector<double> d (n - 1), m (n, 0.0);

        for (size_t i = 0; i + 1 < n; ++i)
            d[i] = (y[i + 1] - y[i]) / std::max (1e-12, x[i + 1] - x[i]);

        m[0] = d[0];
        m[n - 1] = d[n - 2];

        for (size_t i = 1; i + 1 < n; ++i)
            m[i] = (d[i - 1] * d[i] <= 0.0) ? 0.0 : (d[i - 1] + d[i]) * 0.5;

        for (size_t i = 0; i + 1 < n; ++i)
        {
            if (std::abs (d[i]) < 1e-12) { m[i] = m[i + 1] = 0.0; continue; }
            const double a = m[i] / d[i], b = m[i + 1] / d[i];
            const double s = a * a + b * b;
            if (s > 9.0)
            {
                const double tau = 3.0 / std::sqrt (s);
                m[i] = tau * a * d[i];
                m[i + 1] = tau * b * d[i];
            }
        }
        return m;
    }

    // Catmull-Rom style slopes (finite differences) for the plain spline
    std::vector<double> catmullSlopes (const std::vector<double>& x, const std::vector<double>& y)
    {
        const size_t n = x.size();
        std::vector<double> m (n);
        for (size_t i = 0; i < n; ++i)
        {
            const size_t a = i == 0 ? 0 : i - 1, b = i + 1 < n ? i + 1 : n - 1;
            m[i] = (y[b] - y[a]) / std::max (1e-12, x[b] - x[a]);
        }
        return m;
    }

    double hermite (double t, double x0, double x1, double y0, double y1, double m0, double m1)
    {
        const double h = x1 - x0;
        const double s = (t - x0) / h;
        const double s2 = s * s, s3 = s2 * s;
        return (2 * s3 - 3 * s2 + 1) * y0 + (s3 - 2 * s2 + s) * h * m0
             + (-2 * s3 + 3 * s2) * y1 + (s3 - s2) * h * m1;
    }
}

//==============================================================================
juce::StringArray FadeSpec::getShapeNames()
{
    return { "Linear (equal gain)", "Equal power", "S-curve", "Exponential", "dB-linear", "Points" };
}

juce::String FadeSpec::getParamName (Shape s)
{
    switch (s)
    {
        case Shape::sCurve:      return "Tension";
        case Shape::exponential: return "Curvature";
        case Shape::dBLinear:    return "Floor dB";
        default:                 return {};
    }
}

juce::Range<double> FadeSpec::getParamRange (Shape s)
{
    switch (s)
    {
        case Shape::sCurve:      return { 0.0, 1.0 };
        case Shape::exponential: return { -1.0, 1.0 };
        case Shape::dBLinear:    return { -120.0, -6.0 };
        default:                 return { 0.0, 1.0 };
    }
}

double FadeSpec::getDefaultParam (Shape s)
{
    switch (s)
    {
        case Shape::sCurve:      return 0.25;
        case Shape::exponential: return 0.5;
        case Shape::dBLinear:    return -60.0;
        default:                 return 0.0;
    }
}

//==============================================================================
FadeSpec FadeSpec::fromString (const juce::String& text)
{
    FadeSpec f;
    auto tokens = juce::StringArray::fromTokens (text.trim(), " ", "");
    tokens.removeEmptyStrings();
    if (tokens.isEmpty()) return f;

    for (int i = 0; i < 6; ++i)
        if (tokens[0].equalsIgnoreCase (shapeKeys[i]))
        {
            f.shape = (Shape) i;
            f.param = getDefaultParam (f.shape);
        }

    int next = 1;

    if (f.shape == Shape::points)
    {
        for (int i = 0; i < 3; ++i)
            if (tokens[1].equalsIgnoreCase (interpKeys[i]))
            {
                f.interp = (Interp) i;
                next = 2;
            }

        for (int i = next; i < tokens.size(); ++i)
        {
            const auto pct = tokens[i].upToFirstOccurrenceOf (":", false, false).trim();
            const auto db  = tokens[i].fromFirstOccurrenceOf (":", false, false).trim();
            if (pct.isEmpty() || db.isEmpty()) continue;

            const double t = juce::jlimit (0.0, 100.0, pct.getDoubleValue()) / 100.0;
            const double g = db.startsWithIgnoreCase ("-inf") ? 0.0 : juce::jlimit (0.0, 1.0, dbToGain (db.getDoubleValue()));
            if (t > 0.0 && t < 1.0)
                f.points.push_back ({ t, g });
        }

        std::sort (f.points.begin(), f.points.end(), [] (auto& a, auto& b) { return a.t < b.t; });
    }
    else if (tokens.size() > 1 && getParamName (f.shape).isNotEmpty())
    {
        f.param = getParamRange (f.shape).clipValue (tokens[1].getDoubleValue());
    }

    return f;
}

juce::String FadeSpec::toString() const
{
    juce::String s (shapeKeys[(int) shape]);

    if (shape == Shape::points)
    {
        s << " " << interpKeys[(int) interp];
        for (auto& p : points)
        {
            const double db = gainToDb (p.gain);
            s << " " << juce::String (p.t * 100.0, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd (".")
              << ":" << (db <= -144.0 ? juce::String ("-inf") : juce::String (db, 2).trimCharactersAtEnd ("0").trimCharactersAtEnd ("."));
        }
    }
    else if (getParamName (shape).isNotEmpty())
    {
        s << " " << juce::String (param, 4).trimCharactersAtEnd ("0").trimCharactersAtEnd (".");
    }

    return s;
}

//==============================================================================
double FadeSpec::gainAt (double t) const
{
    t = juce::jlimit (0.0, 1.0, t);
    const double pi = juce::MathConstants<double>::pi;

    switch (shape)
    {
        case Shape::linear:
            return t;

        case Shape::equalPower:
            return std::sin (t * pi * 0.5);

        case Shape::sCurve:
        {
            const double a = 1.0 + 4.0 * juce::jlimit (0.0, 1.0, param);
            if (t <= 0.0) return 0.0;
            if (t >= 1.0) return 1.0;
            const double ta = std::pow (t, a), ua = std::pow (1.0 - t, a);
            return ta / (ta + ua);
        }

        case Shape::exponential:
        {
            const double k = 8.0 * juce::jlimit (-1.0, 1.0, param);
            if (std::abs (k) < 1e-6) return t;
            return std::expm1 (k * t) / std::expm1 (k);
        }

        case Shape::dBLinear:
        {
            const double floorDb = juce::jlimit (-144.0, -1.0, param);
            const double g0 = dbToGain (floorDb);
            return juce::jlimit (0.0, 1.0, (dbToGain (floorDb * (1.0 - t)) - g0) / (1.0 - g0));
        }

        case Shape::points:
        {
            std::vector<double> x { 0.0 }, y { 0.0 };
            for (auto& p : points) { x.push_back (p.t); y.push_back (p.gain); }
            x.push_back (1.0); y.push_back (1.0);

            size_t i = 0;
            while (i + 2 < x.size() && t > x[i + 1]) ++i;

            if (interp == Interp::linear || x.size() == 2)
            {
                const double s = (t - x[i]) / std::max (1e-12, x[i + 1] - x[i]);
                return y[i] + (y[i + 1] - y[i]) * s;
            }

            const auto m = interp == Interp::damped ? monotoneSlopes (x, y) : catmullSlopes (x, y);
            return juce::jlimit (0.0, 1.0, hermite (t, x[i], x[i + 1], y[i], y[i + 1], m[i], m[i + 1]));
        }
    }

    return t;
}

std::vector<float> FadeSpec::makeTable (int n) const
{
    n = juce::jmax (2, n);
    std::vector<float> table ((size_t) n);
    for (int i = 0; i < n; ++i)
        table[(size_t) i] = (float) gainAt ((double) i / (double) (n - 1));
    table.front() = 0.0f;
    table.back() = 1.0f;
    return table;
}
}
