#include "TouchGrab.hpp"
#include "globals.hpp"
#include <src/debug/log/Logger.hpp>
#include <src/desktop/state/ViewState.hpp>
#include <src/desktop/state/WindowState.hpp>
#include <src/desktop/view/window/Window.hpp>
#include <src/devices/ITouch.hpp>
#include <src/managers/SeatManager.hpp>
#include <src/output/Monitor.hpp>
#include <src/pointer/PointerController.hpp>
#include <src/pointer/PointerManager.hpp>
#include <src/protocols/core/Compositor.hpp>
#include <string>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace TouchGrab {
    class CGrabTestTouch : public ITouch {
      public:
        bool isVirtual() const override {
            return true;
        }

        SP<Aquamarine::ITouch> aq() override {
            return nullptr;
        }
    };

    static SP<CSeatGrab> g_grab;
    static SP<ITouch>    g_touch;

    static PHLWINDOW     windowByClass(const std::string& cls) {
        for (const auto& window : Desktop::windowState()->windows()) {
            if (window->metadata().appID() == cls)
                return window;
        }

        return nullptr;
    }

    // class of the window owning a surface, "" for none
    static std::string classOf(SP<CWLSurfaceResource> surface) {
        if (!surface)
            return "";

        const auto WINDOW = Desktop::viewState()->query().type(Desktop::View::VIEW_TYPE_WINDOW).surface(surface).runWindow();
        return WINDOW ? WINDOW->metadata().appID() : "(non-window)";
    }

    static void reset() {
        if (g_touch) {
            g_touch->m_events.destroy.emit();
            Pointer::mgr()->detachTouch(g_touch);
            g_touch.reset();
        }

        if (g_grab && g_pSeatManager->m_seatGrab == g_grab)
            g_pSeatManager->setGrab(nullptr);

        g_grab.reset();
    }

    // touch_grab_setup(grabbedClass): cursor onto the window, then a pointer + keyboard seat grab
    // that only accepts that window, like the grab an xdg popup opened with the mouse creates.
    static int luaSetup(lua_State* L) {
        const std::string CLS     = luaL_checkstring(L, 1);
        const auto        WINDOW  = windowByClass(CLS);
        const auto        MONITOR = WINDOW ? WINDOW->m_monitor.lock() : nullptr;
        if (!WINDOW || !MONITOR || !WINDOW->wlSurface() || !WINDOW->wlSurface()->resource())
            return luaL_error(L, "touch_grab_setup: no mapped window with class '%s'", CLS.c_str());
        if (g_grab || g_touch || g_pSeatManager->m_seatGrab)
            return luaL_error(L, "touch_grab_setup: a grab or touch fixture is already active");

        Pointer::pointerController()->warpTo(WINDOW->getWindowMainSurfaceBox().middle(), true);

        g_grab             = makeShared<CSeatGrab>();
        g_grab->m_pointer  = true;
        g_grab->m_keyboard = true;
        g_grab->add(WINDOW->wlSurface()->resource());
        g_pSeatManager->setGrab(g_grab);

        g_touch                = makeShared<CGrabTestTouch>();
        g_touch->m_self        = g_touch;
        g_touch->m_hlName      = "hyprtester-grab-touch";
        g_touch->m_boundOutput = MONITOR->m_name;
        Pointer::mgr()->attachTouch(g_touch);
        return 0;
    }

    // touch_grab_tap(targetClass): touch down in the middle of that window, report the seat state
    // while the finger is down, then lift it.
    static int luaTap(lua_State* L) {
        const std::string CLS     = luaL_checkstring(L, 1);
        const auto        WINDOW  = windowByClass(CLS);
        const auto        MONITOR = WINDOW ? WINDOW->m_monitor.lock() : nullptr;
        if (!WINDOW || !MONITOR || !g_touch)
            return luaL_error(L, "touch_grab_tap: needs touch_grab_setup and a window with class '%s'", CLS.c_str());

        const auto POS = (WINDOW->getWindowMainSurfaceBox().middle() - MONITOR->m_position) / MONITOR->m_size;

        g_touch->m_touchEvents.down.emit(ITouch::SDownEvent{.touchID = 0, .pos = POS, .device = g_touch});

        lua_newtable(L);
        lua_pushstring(L, classOf(g_pSeatManager->m_state.touchFocus.lock()).c_str());
        lua_setfield(L, -2, "touch_focus");
        lua_pushstring(L, classOf(g_pSeatManager->m_state.pointerFocus.lock()).c_str());
        lua_setfield(L, -2, "pointer_focus");
        lua_pushstring(L, classOf(g_pSeatManager->m_state.keyboardFocus.lock()).c_str());
        lua_setfield(L, -2, "keyboard_focus");
        lua_pushboolean(L, g_grab && g_pSeatManager->m_seatGrab == g_grab);
        lua_setfield(L, -2, "grab_active");

        g_touch->m_touchEvents.up.emit(ITouch::SUpEvent{.touchID = 0});
        return 1;
    }

    static int luaReset(lua_State* L) {
        reset();
        return 0;
    }

    void registerFunctions() {
        const std::pair<const char*, PLUGIN_LUA_FN> FUNCTIONS[] = {
            {"touch_grab_setup", luaSetup},
            {"touch_grab_tap", luaTap},
            {"touch_grab_reset", luaReset},
        };
        for (const auto& [name, fn] : FUNCTIONS) {
            if (!HyprlandAPI::addLuaFunction(PHANDLE, "test", name, fn))
                LOG(Log::ERR, "hyprtester plugin: failed to register hl.plugin.test.{}", name);
        }
    }
}
