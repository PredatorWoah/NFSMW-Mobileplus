// SpeedBreaker One. GPL-3.0-or-later. game/yellow_filter.h's shape test and blue.
#include <game/yellow_filter.h>

#include <cstdio>

namespace
{
    int s_failures = 0;
#define CHECK(cond, ...)                                              \
    do                                                                \
    {                                                                 \
        if (!(cond))                                                  \
        {                                                             \
            fprintf(stderr, "  FAIL %s:%d: %s: ", __FILE__, __LINE__, #cond); \
            fprintf(stderr, __VA_ARGS__);                             \
            fprintf(stderr, "\n");                                    \
            s_failures++;                                             \
        }                                                             \
    } while (0)
}

int main()
{
    using namespace game::visual_filter;
    // The PC's default tint and the community's yellow presets (the NFSMW Extra Options gist): all yellow.
    CHECK(TintShape(0.88f, 0.80f, 0.44f), "the PC default");
    CHECK(TintShape(0.68f, 0.58f, 0.03f), "X360-like");
    CHECK(TintShape(0.90f, 1.00f, 0.46f), "Diamond's X360-like");
    // Not yellow, and so not touched: the community's less-yellow fix, a neutral grey, ordinary colours.
    CHECK(!TintShape(0.88f, 0.80f, 0.95f), "less yellow");
    CHECK(!TintShape(0.5f, 0.5f, 0.5f), "grey");
    CHECK(!TintShape(1.75f, 0.5f, 0.9f), "bloom numbers");
    CHECK(!TintShape(NAN, 0.8f, 0.4f), "NaN");
    // The blue: a little over the red/green mean; capped at 1; and the result is itself no longer yellow.
    CHECK(NeutralBlue(0.88f, 0.80f) > 0.89f && NeutralBlue(0.88f, 0.80f) < 0.91f, "0.88 0.80 -> %.3f", NeutralBlue(0.88f, 0.80f));
    CHECK(NeutralBlue(1.0f, 1.0f) == 1.0f, "cap");
    CHECK(!TintShape(0.88f, 0.80f, NeutralBlue(0.88f, 0.80f)), "after the fix it is not yellow");
    CHECK(!TintShape(0.68f, 0.58f, NeutralBlue(0.68f, 0.58f)), "X360-like after the fix");
    if (s_failures)
        return 1;
    printf("yellow_filter_test: all passed\n");
    return 0;
}
