#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

// A config shared with upstream Hyprland uses hl.refocus to detect the fork before using
// fork-only features (upstream has no such field).
TEST_CASE(refocusMarker) {
    OK(getFromSocket("/eval assert(type(hl.refocus) == 'table' and hl.refocus.api == 1, 'hl.refocus.api should be 1')"));
}
