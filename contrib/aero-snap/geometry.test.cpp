#include "geometry.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string_view>

namespace {

    using AeroSnap::Point;
    using AeroSnap::Rect;
    using AeroSnap::Zone;

    void expect(const bool condition, const std::string_view message) {
        if (condition)
            return;

        std::cerr << "geometry test failed: " << message << '\n';
        std::exit(1);
    }

    bool closeTo(const double actual, const double expected) {
        return std::abs(actual - expected) < 0.001;
    }

    void expectRect(const Rect actual, const Rect expected, const std::string_view message) {
        expect(closeTo(actual.x, expected.x) && closeTo(actual.y, expected.y) && closeTo(actual.w, expected.w) && closeTo(actual.h, expected.h), message);
    }

} // namespace

int main() {
    const Rect ultrawide{0, 0, 4096, 1152};
    const Rect widescreen{0, 0, 1920, 1080};

    expect(AeroSnap::columnsForMonitor(ultrawide, "auto") == 3, "auto selects thirds above 16:9");
    expect(AeroSnap::columnsForMonitor(widescreen, "auto") == 2, "auto keeps exact 16:9 in halves");
    expect(AeroSnap::columnsForMonitor(ultrawide, "2") == 2, "two columns can be forced on an ultrawide monitor");
    expect(AeroSnap::columnsForMonitor(widescreen, "3") == 3, "three columns can be forced on a 16:9 monitor");

    expect(AeroSnap::zoneAt(Point{0, 576}, ultrawide, 12, 0.25, 3) == Zone::Left, "left edge selects the outer column");
    expect(AeroSnap::zoneAt(Point{4095, 100}, ultrawide, 12, 0.25, 3) == Zone::TopRight, "side-edge corner bands are generous");
    expect(AeroSnap::zoneAt(Point{2048, 0}, ultrawide, 12, 0.25, 3) == Zone::Maximize, "top center maximizes");
    expect(AeroSnap::zoneAt(Point{2048, 1151}, ultrawide, 12, 0.25, 3) == Zone::Center, "bottom center selects the middle column");
    expect(AeroSnap::zoneAt(Point{2048, 1151}, ultrawide, 12, 0.25, 2) == Zone::None, "two-column mode leaves bottom center untouched");
    expect(AeroSnap::zoneAt(Point{500, 500}, ultrawide, 12, 0.25, 3) == Zone::None, "interior movement is untouched");

    const Rect workArea{10, 36, 4076, 1106};
    expectRect(*AeroSnap::rectForZone(workArea, Zone::Left, 10, 10, 3), Rect{10, 36, 1352, 1106}, "left third preserves the horizontal gaps");
    expectRect(*AeroSnap::rectForZone(workArea, Zone::Center, 10, 10, 3), Rect{1372, 36, 1352, 1106}, "middle third preserves the horizontal gaps");
    expectRect(*AeroSnap::rectForZone(workArea, Zone::Right, 10, 10, 3), Rect{2734, 36, 1352, 1106}, "right third reaches the work area edge");
    expectRect(*AeroSnap::rectForZone(workArea, Zone::Left, 10, 10, 2), Rect{10, 36, 2033, 1106}, "forced halves preserve the horizontal gap");
    expectRect(*AeroSnap::rectForZone(workArea, Zone::BottomRight, 10, 10, 3), Rect{2053, 594, 2033, 548}, "quarters remain halves of the work area");
    expectRect(*AeroSnap::rectForZone(workArea, Zone::Maximize, 10, 10, 3), workArea, "maximize uses the complete work area");
    expect(!AeroSnap::rectForZone(workArea, Zone::Center, 10, 10, 2), "center is unavailable in two-column mode");
    expect(!AeroSnap::rectForZone(Rect{0, 0, 8, 8}, Zone::Left, 10, 10, 3), "impossible geometry is rejected");

    return 0;
}
