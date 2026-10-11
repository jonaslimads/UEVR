#pragma once
#include <cstdint>
namespace uevr::swgr_auto {
// Evidence comes from scene textures before the separately rendered UI.
struct Decision {
    bool mono{};
    unsigned bad{}, good{}, dark{};
    uint64_t last_sample_ms{};
    void reset() { *this = {}; }
    bool update(unsigned left_lit, unsigned right_lit, uint64_t now_ms) {
        if (last_sample_ms && now_ms - last_sample_ms > 500) reset();
        last_sample_ms = now_ms;
        if (left_lit >= 4 && right_lit <= 1) {
            good = dark = 0;
            if (++bad >= 2) { mono = true; bad = 2; }
        } else if (right_lit >= 4) {
            bad = dark = 0;
            if (++good >= 2) { mono = false; good = 2; }
        } else if (left_lit <= 1 && right_lit <= 1) {
            bad = good = 0;
            if (++dark >= 3) { mono = false; dark = 3; }
        } else {
            bad = good = dark = 0;
        }
        return mono;
    }
    bool active(uint64_t now_ms) const {
        return mono && last_sample_ms && now_ms - last_sample_ms <= 500;
    }
};
}
