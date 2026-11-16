#include "tests.hpp"
#include "../../shared.hpp"
#include "../../hyprctlCompat.hpp"
#include "../shared.hpp"

#include <hyprutils/utils/ScopeGuard.hpp>
#include <string>

// Touch is routed on its own: a seat grab made with the mouse (an xdg popup, a focus grab)
// must neither swallow touches elsewhere nor end because of them. Before the fork's change,
// a touch outside the grab ended it and was delivered to the surface under the mouse cursor.
TEST_CASE(touchIgnoresMouseGrab) {
    Hyprutils::Utils::CScopeGuard cleanup([] {
        getFromSocket("/eval hl.plugin.test.touch_grab_reset()");
        Tests::killAllWindows();
    });

    SPAWN_KITTY("touch_grab_a");
    SPAWN_KITTY("touch_grab_b");
    EXPECT(Tests::windowCount(), 2);

    // tap outside the grab: goes to the window under the finger, grab and focus stay put
    OK(getFromSocket(R"(/eval
        local p = hl.plugin.test
        p.touch_grab_setup('touch_grab_a')
        local ok, s = pcall(p.touch_grab_tap, 'touch_grab_b')
        p.touch_grab_reset()
        assert(ok, s)
        assert(s.touch_focus == 'touch_grab_b', 'touch went to "' .. s.touch_focus .. '", expected the window under the finger')
        assert(s.grab_active, 'the touch ended the mouse grab')
        assert(s.pointer_focus == 'touch_grab_a', 'pointer focus moved to "' .. s.pointer_focus .. '"')
        assert(s.keyboard_focus == 'touch_grab_a', 'keyboard focus left the grab for "' .. s.keyboard_focus .. '"')
    )"));

    // no grab: the tap focuses the touched window for the keyboard, the pointer stays where the cursor is
    OK(getFromSocket(R"(/eval
        local p = hl.plugin.test
        p.touch_grab_setup('touch_grab_a', false)
        local ok, s = pcall(p.touch_grab_tap, 'touch_grab_b')
        p.touch_grab_reset()
        assert(ok, s)
        assert(s.touch_focus == 'touch_grab_b', 'touch went to "' .. s.touch_focus .. '"')
        assert(s.keyboard_focus == 'touch_grab_b', 'keyboard focus is on "' .. s.keyboard_focus .. '", expected the touched window')
        assert(s.pointer_focus == 'touch_grab_a', 'pointer focus moved to "' .. s.pointer_focus .. '"')
        assert(not s.grab_active, 'no grab was set up')
    )"));

    // tap inside the grab: delivered normally, grab stays
    OK(getFromSocket(R"(/eval
        local p = hl.plugin.test
        p.touch_grab_setup('touch_grab_a')
        local ok, s = pcall(p.touch_grab_tap, 'touch_grab_a')
        p.touch_grab_reset()
        assert(ok, s)
        assert(s.touch_focus == 'touch_grab_a', 'touch went to "' .. s.touch_focus .. '"')
        assert(s.grab_active, 'the touch ended the mouse grab')
    )"));
}
