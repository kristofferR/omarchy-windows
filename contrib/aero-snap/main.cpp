#define WLR_USE_UNSTABLE

#include "geometry.hpp"

#include <hyprland/src/Compositor.hpp>
#include <hyprland/src/config/ConfigManager.hpp>
#include <hyprland/src/config/shared/complex/ComplexDataTypes.hpp>
#include <hyprland/src/config/values/types/BoolValue.hpp>
#include <hyprland/src/config/values/types/ColorValue.hpp>
#include <hyprland/src/config/values/types/FloatValue.hpp>
#include <hyprland/src/config/values/types/IntValue.hpp>
#include <hyprland/src/desktop/state/FocusState.hpp>
#include <hyprland/src/desktop/view/Window.hpp>
#include <hyprland/src/event/EventBus.hpp>
#include <hyprland/src/layout/LayoutManager.hpp>
#include <hyprland/src/layout/space/Space.hpp>
#include <hyprland/src/managers/eventLoop/EventLoopManager.hpp>
#include <hyprland/src/managers/fullscreen/FullscreenController.hpp>
#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/render/Renderer.hpp>
#include <hyprland/src/render/pass/RectPassElement.hpp>
#include <hyprland/src/state/MonitorState.hpp>

#include <linux/input-event-codes.h>
#include <unistd.h>

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern "C" {
#include <lua.h>
#include <lauxlib.h>
}

namespace {

    using AeroSnap::Zone;

    HANDLE pluginHandle = nullptr;

    struct SConfigValues {
        SP<Config::Values::CBoolValue>  enabled;
        SP<Config::Values::CBoolValue>  floatingModeOnly;
        SP<Config::Values::CIntValue>   edgeThreshold;
        SP<Config::Values::CFloatValue> cornerRatio;
        SP<Config::Values::CColorValue> previewColor;
        SP<Config::Values::CColorValue> previewBorderColor;
        SP<Config::Values::CIntValue>   previewBorderSize;
        SP<Config::Values::CIntValue>   previewRounding;
    };

    struct SPlacement {
        SP<Layout::ITarget> target;
        PHLMONITOR          monitor;
        Zone                zone = Zone::None;
        CBox                previewBox;
        CBox                clientBox;
    };

    struct SPreview {
        PHLMONITORREF monitor;
        CBox          box;
    };

    struct SSnapRecord {
        PHLWINDOWREF window;
        CBox         restoreBox;
    };

    struct SDragState {
        SP<Layout::ITarget>       target;
        CBox                      startBox;
        std::optional<SPlacement> candidate;
    };

    struct SPluginState {
        SConfigValues             config;
        std::string               enabledMarker;
        std::optional<SPreview>   preview;
        std::optional<SDragState> drag;
        std::vector<SSnapRecord>  records;
        UP<SEventLoopDoLaterLock> pendingApply;
    };

    UP<SPluginState> state;

    AeroSnap::Rect   toRect(const CBox& box) {
        return {box.x, box.y, box.w, box.h};
    }

    CBox toBox(const AeroSnap::Rect rect) {
        CBox box{rect.x, rect.y, rect.w, rect.h};
        box.round();
        return box;
    }

    bool sameBox(const CBox& lhs, const CBox& rhs) {
        return lhs.x == rhs.x && lhs.y == rhs.y && lhs.w == rhs.w && lhs.h == rhs.h;
    }

    void damagePreviewBox(const CBox& box) {
        constexpr double padding = 4;
        g_pHyprRenderer->damageBox({box.x - padding, box.y - padding, box.w + padding * 2, box.h + padding * 2});
    }

    void clearPreview() {
        if (!state || !state->preview)
            return;

        damagePreviewBox(state->preview->box);
        state->preview.reset();
    }

    void setPreview(const PHLMONITOR& monitor, const CBox& box) {
        if (state->preview && state->preview->monitor.lock() == monitor && sameBox(state->preview->box, box))
            return;

        clearPreview();
        state->preview = SPreview{monitor, box};
        damagePreviewBox(box);
    }

    void pruneRecords() {
        std::erase_if(state->records, [](const auto& record) { return !record.window.valid() || record.window.expired(); });
    }

    SSnapRecord* recordFor(const PHLWINDOW& window) {
        pruneRecords();
        const auto found = std::ranges::find_if(state->records, [&](const auto& record) { return record.window.lock() == window; });
        return found == state->records.end() ? nullptr : &*found;
    }

    void rememberRestoreBox(const PHLWINDOW& window, const CBox& box) {
        if (recordFor(window))
            return;

        state->records.push_back({window, box});
    }

    void forgetRestoreBox(const PHLWINDOW& window) {
        std::erase_if(state->records, [&](const auto& record) { return record.window.lock() == window; });
    }

    bool floatingModeActive() {
        if (!state->config.enabled->value())
            return false;
        if (!state->config.floatingModeOnly->value())
            return true;

        return access(state->enabledMarker.c_str(), F_OK) == 0;
    }

    PHLWORKSPACE activeWorkspace(const PHLMONITOR& monitor) {
        return monitor->m_activeSpecialWorkspace ? monitor->m_activeSpecialWorkspace : monitor->m_activeWorkspace;
    }

    std::optional<SPlacement> placementFor(const SP<Layout::ITarget>& target, const PHLMONITOR& monitor, const Zone zone) {
        if (!target || !target->floating() || !Desktop::View::validMapped(target->window()) || !monitor || zone == Zone::None)
            return std::nullopt;

        const auto workspace = activeWorkspace(monitor);
        if (!workspace || !workspace->m_space)
            return std::nullopt;

        const CBox   workArea = workspace->m_space->workArea(zone == Zone::Maximize);

        static auto  gapsInValue   = CConfigValue<Config::IComplexConfigValue>("general:gaps_in");
        const auto*  gapsIn        = sc<Config::CCssGapData*>(gapsInValue.ptr());
        const double horizontalGap = gapsIn->m_left + gapsIn->m_right;
        const double verticalGap   = gapsIn->m_top + gapsIn->m_bottom;
        const auto   zoneRect      = AeroSnap::rectForZone(toRect(workArea), zone, horizontalGap, verticalGap);
        if (!zoneRect)
            return std::nullopt;

        const CBox requestedOuter = toBox(*zoneRect);
        if (zone == Zone::Maximize)
            return SPlacement{target, monitor, zone, requestedOuter, {}};

        const auto     window  = target->window();
        const auto     extents = window->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS);
        const Vector2D availableSize{
            requestedOuter.w - extents.topLeft.x - extents.bottomRight.x,
            requestedOuter.h - extents.topLeft.y - extents.bottomRight.y,
        };
        const Vector2D minSize         = target->minSize().value_or(Vector2D{1, 1});
        const Vector2D reportedMaxSize = target->maxSize().value_or(Math::VECTOR2D_MAX);
        const Vector2D maxSize{
            std::max(minSize.x, reportedMaxSize.x),
            std::max(minSize.y, reportedMaxSize.y),
        };

        if (availableSize.x < minSize.x || availableSize.y < minSize.y)
            return std::nullopt;

        const Vector2D clientSize{
            std::clamp(availableSize.x, minSize.x, maxSize.x),
            std::clamp(availableSize.y, minSize.y, maxSize.y),
        };
        const Vector2D actualOuterSize{
            clientSize.x + extents.topLeft.x + extents.bottomRight.x,
            clientSize.y + extents.topLeft.y + extents.bottomRight.y,
        };
        CBox actualOuter{
            requestedOuter.x + (requestedOuter.w - actualOuterSize.x) / 2.0,
            requestedOuter.y + (requestedOuter.h - actualOuterSize.y) / 2.0,
            actualOuterSize.x,
            actualOuterSize.y,
        };
        CBox clientBox{
            actualOuter.x + extents.topLeft.x,
            actualOuter.y + extents.topLeft.y,
            clientSize.x,
            clientSize.y,
        };
        actualOuter.round();
        clientBox.round();

        return SPlacement{target, monitor, zone, actualOuter, clientBox};
    }

    void applyPlacement(const SPlacement& placement, const CBox& restoreBox) {
        const auto target = placement.target;
        if (!target || !target->floating() || !Desktop::View::validMapped(target->window()))
            return;

        const auto window = target->window();
        rememberRestoreBox(window, restoreBox);

        if (placement.zone == Zone::Maximize) {
            Fullscreen::controller()->setFullscreenMode(window, Fullscreen::FSMODE_MAXIMIZED);
            return;
        }

        if (Fullscreen::controller()->isFullscreen(window))
            Fullscreen::controller()->setFullscreenMode(window, Fullscreen::FSMODE_NONE);

        g_layoutManager->setTargetGeom(placement.clientBox, target);
        target->warpPositionSize();
        target->damageEntire();
    }

    bool restoreSnappedDrag(const SP<Layout::ITarget>& target, const Vector2D cursor, Event::SCallbackInfo& info) {
        const auto window = target->window();
        const auto record = recordFor(window);
        if (!record)
            return false;

        const CBox restoreBox = record->restoreBox;
        const auto extents    = window->getWindowExtentsUnified(Desktop::View::RESERVED_EXTENTS | Desktop::View::INPUT_EXTENTS);
        const CBox current    = target->position();
        const CBox currentOuter{
            current.x - extents.topLeft.x,
            current.y - extents.topLeft.y,
            current.w + extents.topLeft.x + extents.bottomRight.x,
            current.h + extents.topLeft.y + extents.bottomRight.y,
        };
        const double   anchorX = std::clamp((cursor.x - currentOuter.x) / std::max(1.0, currentOuter.w), 0.08, 0.92);
        const double   anchorY = std::clamp(cursor.y - currentOuter.y, 0.0, std::max(0.0, currentOuter.h * 0.25));
        const Vector2D restoredOuterSize{
            restoreBox.w + extents.topLeft.x + extents.bottomRight.x,
            restoreBox.h + extents.topLeft.y + extents.bottomRight.y,
        };
        CBox anchoredRestore{
            cursor.x - restoredOuterSize.x * anchorX + extents.topLeft.x,
            cursor.y - anchorY + extents.topLeft.y,
            restoreBox.w,
            restoreBox.h,
        };
        anchoredRestore.round();

        forgetRestoreBox(window);
        clearPreview();
        g_layoutManager->endDragTarget();
        g_layoutManager->setTargetGeom(anchoredRestore, target);
        target->warpPositionSize();
        g_layoutManager->beginDragTarget(target, MBIND_MOVE);
        state->drag    = SDragState{target, anchoredRestore, std::nullopt};
        info.cancelled = true;
        return true;
    }

    void resetDrag() {
        clearPreview();
        state->drag.reset();
    }

    void onMouseMove(const Vector2D cursor, Event::SCallbackInfo& info) {
        const auto& controller = g_layoutManager->dragController();
        const auto  target     = controller->target();
        if (!target || controller->mode() != MBIND_MOVE || controller->draggingTiled() || !target->floating() || !Desktop::View::validMapped(target->window())) {
            resetDrag();
            return;
        }
        if (!floatingModeActive()) {
            resetDrag();
            return;
        }

        if (!state->drag || state->drag->target != target)
            state->drag = SDragState{target, target->position(), std::nullopt};

        if (controller->dragThresholdReached() && restoreSnappedDrag(target, cursor, info))
            return;

        if (!controller->dragThresholdReached()) {
            clearPreview();
            state->drag->candidate.reset();
            return;
        }

        const auto monitor = State::monitorState()->query().vec(cursor).run();
        if (!monitor) {
            clearPreview();
            state->drag->candidate.reset();
            return;
        }

        const auto monitorBox = monitor->logicalBox();
        const auto zone       = AeroSnap::zoneAt({cursor.x, cursor.y}, toRect(monitorBox), state->config.edgeThreshold->value(), state->config.cornerRatio->value());
        const auto placement  = placementFor(target, monitor, zone);
        if (!placement) {
            clearPreview();
            state->drag->candidate.reset();
            return;
        }

        state->drag->candidate = placement;
        setPreview(monitor, placement->previewBox);
    }

    void onMouseButton(const IPointer::SButtonEvent event) {
        if (event.state != WL_POINTER_BUTTON_STATE_RELEASED || event.button != BTN_LEFT || !state->drag)
            return;

        const auto drag = *state->drag;
        resetDrag();
        if (!drag.candidate)
            return;

        const auto placement = *drag.candidate;
        state->pendingApply  = g_pEventLoopManager->doLaterLock([placement, restoreBox = drag.startBox] {
            if (!state || !floatingModeActive() || !placement.target || !Desktop::View::validMapped(placement.target->window()))
                return;

            if (g_layoutManager->dragController()->target() == placement.target)
                g_layoutManager->endDragTarget();
            applyPlacement(placement, restoreBox);
        });
    }

    std::optional<Zone> parseZone(std::string_view value) {
        while (!value.empty() && value.front() == ' ')
            value.remove_prefix(1);
        while (!value.empty() && value.back() == ' ')
            value.remove_suffix(1);

        if (value == "left")
            return Zone::Left;
        if (value == "right")
            return Zone::Right;
        if (value == "top-left")
            return Zone::TopLeft;
        if (value == "top-right")
            return Zone::TopRight;
        if (value == "bottom-left")
            return Zone::BottomLeft;
        if (value == "bottom-right")
            return Zone::BottomRight;
        if (value == "maximize" || value == "top")
            return Zone::Maximize;
        return std::nullopt;
    }

    SDispatchResult snapDispatcher(const std::string argument) {
        if (!floatingModeActive())
            return {.success = false, .error = "omarchy-snap is available only while Floating Mode is active"};

        const auto window = Desktop::focusState()->window();
        if (!Desktop::View::validMapped(window) || !window->m_target || !window->m_target->floating())
            return {.success = false, .error = "omarchy-snap requires a focused floating window"};

        if (argument == "restore") {
            const auto record = recordFor(window);
            if (!record)
                return {.success = false, .error = "focused window has no saved pre-snap geometry"};

            const auto restoreBox = record->restoreBox;
            forgetRestoreBox(window);
            if (Fullscreen::controller()->isFullscreen(window))
                Fullscreen::controller()->setFullscreenMode(window, Fullscreen::FSMODE_NONE);
            g_layoutManager->setTargetGeom(restoreBox, window->m_target);
            window->m_target->warpPositionSize();
            return {};
        }

        const auto zone = parseZone(argument);
        if (!zone)
            return {.success = false, .error = "expected left, right, top-left, top-right, bottom-left, bottom-right, maximize, or restore"};

        const auto monitor   = window->m_monitor.lock();
        const auto placement = placementFor(window->m_target, monitor, *zone);
        if (!placement)
            return {.success = false, .error = "snap zone cannot satisfy this window's size constraints"};

        applyPlacement(*placement, window->m_target->position());
        return {};
    }

    int runLuaSnapDispatcher(lua_State* luaState) {
        const auto* argument = lua_tostring(luaState, lua_upvalueindex(1));
        const auto  result   = snapDispatcher(argument ? argument : "");

        lua_createtable(luaState, 0, result.success ? 1 : 2);
        lua_pushboolean(luaState, result.success);
        lua_setfield(luaState, -2, "ok");
        if (!result.success) {
            lua_pushlstring(luaState, result.error.data(), result.error.size());
            lua_setfield(luaState, -2, "error");
        }
        return 1;
    }

    int createLuaSnapDispatcher(lua_State* luaState) {
        if (!lua_isstring(luaState, 1))
            return luaL_error(luaState, "hl.plugin.omarchy_windows_snap.snap: expected a zone string");

        lua_pushvalue(luaState, 1);
        lua_pushcclosure(luaState, runLuaSnapDispatcher, 1);
        return 1;
    }

    void onRenderStage(const eRenderStage stageValue) {
        if (stageValue != RENDER_POST_WINDOWS || !state->preview)
            return;

        const auto monitor = g_pHyprRenderer->renderData().pMonitor.lock();
        if (!monitor || state->preview->monitor.lock() != monitor)
            return;

        CBox outer = state->preview->box;
        outer.translate(-monitor->m_position).scale(monitor->m_scale).round();

        const int border   = std::max(0, static_cast<int>(std::round(state->config.previewBorderSize->value() * monitor->m_scale)));
        const int rounding = std::max(0, static_cast<int>(std::round(state->config.previewRounding->value() * monitor->m_scale)));
        if (border > 0) {
            CRectPassElement::SRectData borderData;
            borderData.box   = outer;
            borderData.color = CHyprColor{static_cast<uint64_t>(state->config.previewBorderColor->value())};
            borderData.round = rounding;
            g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(borderData));
        }

        CBox inner{outer.x + border, outer.y + border, outer.w - border * 2, outer.h - border * 2};
        if (inner.w <= 0 || inner.h <= 0)
            return;

        CRectPassElement::SRectData fillData;
        fillData.box   = inner;
        fillData.color = CHyprColor{static_cast<uint64_t>(state->config.previewColor->value())};
        fillData.round = std::max(0, rounding - border);
        g_pHyprRenderer->m_renderPass.add(makeUnique<CRectPassElement>(fillData));
    }

} // namespace

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    pluginHandle = handle;

    const std::string runningHash = __hyprland_api_get_hash();
    const std::string headersHash = __hyprland_api_get_client_hash();
    if (runningHash != headersHash) {
        HyprlandAPI::addNotification(pluginHandle, "[omarchy-windows-snap] Hyprland version mismatch", CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("omarchy-windows-snap was built for a different Hyprland version");
    }

    state                        = makeUnique<SPluginState>();
    const auto* runtimeDirectory = std::getenv("XDG_RUNTIME_DIR");
    state->enabledMarker         = runtimeDirectory && *runtimeDirectory ? std::string{runtimeDirectory} + "/omarchy-floating-mode/enabled" :
                                                                           "/run/user/" + std::to_string(getuid()) + "/omarchy-floating-mode/enabled";

    state->config.enabled = makeShared<Config::Values::CBoolValue>("plugin:omarchy_windows_snap:enabled", "Enable Aero-style drag snap zones", true);
    state->config.floatingModeOnly =
        makeShared<Config::Values::CBoolValue>("plugin:omarchy_windows_snap:floating_mode_only", "Activate only while Omarchy Floating Mode is on", true);
    state->config.edgeThreshold = makeShared<Config::Values::CIntValue>("plugin:omarchy_windows_snap:edge_threshold", "Logical pixels from a monitor edge that activate a zone", 12,
                                                                        Config::Values::SIntValueOptions{.min = 1, .max = 128});
    state->config.cornerRatio   = makeShared<Config::Values::CFloatValue>("plugin:omarchy_windows_snap:corner_ratio", "Fraction of each monitor edge reserved for corner zones",
                                                                          0.25F, Config::Values::SFloatValueOptions{.min = 0.05F, .max = 0.5F});
    state->config.previewColor  = makeShared<Config::Values::CColorValue>("plugin:omarchy_windows_snap:preview_color", "Snap preview fill color", 0x383b82f6);
    state->config.previewBorderColor = makeShared<Config::Values::CColorValue>("plugin:omarchy_windows_snap:preview_border_color", "Snap preview border color", 0xcc60a5fa);
    state->config.previewBorderSize  = makeShared<Config::Values::CIntValue>("plugin:omarchy_windows_snap:preview_border_size", "Snap preview border size", 2,
                                                                             Config::Values::SIntValueOptions{.min = 0, .max = 16});
    state->config.previewRounding    = makeShared<Config::Values::CIntValue>("plugin:omarchy_windows_snap:preview_rounding", "Snap preview corner radius", 8,
                                                                             Config::Values::SIntValueOptions{.min = 0, .max = 64});

    HyprlandAPI::addConfigValueV2(pluginHandle, state->config.enabled);
    HyprlandAPI::addConfigValueV2(pluginHandle, state->config.floatingModeOnly);
    HyprlandAPI::addConfigValueV2(pluginHandle, state->config.edgeThreshold);
    HyprlandAPI::addConfigValueV2(pluginHandle, state->config.cornerRatio);
    HyprlandAPI::addConfigValueV2(pluginHandle, state->config.previewColor);
    HyprlandAPI::addConfigValueV2(pluginHandle, state->config.previewBorderColor);
    HyprlandAPI::addConfigValueV2(pluginHandle, state->config.previewBorderSize);
    HyprlandAPI::addConfigValueV2(pluginHandle, state->config.previewRounding);

    static auto mouseMoveListener     = Event::bus()->m_events.input.mouse.move.listen([](Vector2D cursor, Event::SCallbackInfo& info) { onMouseMove(cursor, info); });
    static auto mouseButtonListener   = Event::bus()->m_events.input.mouse.button.listen([](IPointer::SButtonEvent event, Event::SCallbackInfo&) { onMouseButton(event); });
    static auto renderListener        = Event::bus()->m_events.render.stage.listen([](eRenderStage stageValue) { onRenderStage(stageValue); });
    static auto windowDestroyListener = Event::bus()->m_events.window.destroy.listen([](PHLWINDOWREF window) {
        if (!state)
            return;
        std::erase_if(state->records, [&](const auto& record) { return record.window == window; });
        if (state->drag && state->drag->target && state->drag->target->window() == window.lock())
            resetDrag();
    });

    if (!HyprlandAPI::addDispatcherV2(pluginHandle, "omarchy-snap", snapDispatcher))
        throw std::runtime_error("failed to register the omarchy-snap dispatcher");
    if (!HyprlandAPI::addLuaFunction(pluginHandle, "omarchy_windows_snap", "snap", createLuaSnapDispatcher))
        throw std::runtime_error("failed to register the omarchy_windows_snap Lua dispatcher");

    HyprlandAPI::reloadConfig();
    return {"omarchy-windows-snap", "Aero-style drag snap zones for Omarchy Floating Mode", "Norbert Winter and contributors", "1.0"};
}

APICALL EXPORT void PLUGIN_EXIT() {
    if (!state)
        return;

    clearPreview();
    state->pendingApply.reset();
    HyprlandAPI::removeLuaFunction(pluginHandle, "omarchy_windows_snap", "snap");
    HyprlandAPI::removeDispatcher(pluginHandle, "omarchy-snap");
    state.reset();
}
