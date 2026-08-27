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

    expect(AeroSnap::zoneAt(Point{0, 576}, ultrawide, 12, 0.25) == Zone::Left, "left edge selects a half");
    expect(AeroSnap::zoneAt(Point{4095, 100}, ultrawide, 12, 0.25) == Zone::TopRight, "side-edge corner bands are generous");
    expect(AeroSnap::zoneAt(Point{2048, 0}, ultrawide, 12, 0.25) == Zone::Maximize, "top center maximizes");
    expect(AeroSnap::zoneAt(Point{2048, 1151}, ultrawide, 12, 0.25) == Zone::None, "bottom center has no accidental zone");
    expect(AeroSnap::zoneAt(Point{500, 500}, ultrawide, 12, 0.25) == Zone::None, "interior movement is untouched");

    const Rect workArea{10, 36, 4076, 1106};
    expectRect(*AeroSnap::rectForZone(workArea, Zone::Left, 10, 10), Rect{10, 36, 2033, 1106}, "halves preserve the horizontal gap");
    expectRect(*AeroSnap::rectForZone(workArea, Zone::BottomRight, 10, 10), Rect{2053, 594, 2033, 548}, "quarters preserve both gaps");
    expectRect(*AeroSnap::rectForZone(workArea, Zone::Maximize, 10, 10), workArea, "maximize uses the complete work area");
    expect(!AeroSnap::rectForZone(Rect{0, 0, 8, 8}, Zone::Left, 10, 10), "impossible geometry is rejected");

    return 0;
}
