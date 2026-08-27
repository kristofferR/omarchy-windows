#pragma once

#include <algorithm>
#include <optional>

namespace AeroSnap {

    struct Point {
        double x = 0;
        double y = 0;
    };

    struct Rect {
        double x = 0;
        double y = 0;
        double w = 0;
        double h = 0;
    };

    enum class Zone {
        None,
        Left,
        Right,
        TopLeft,
        TopRight,
        BottomLeft,
        BottomRight,
        Maximize,
    };

    inline Zone zoneAt(const Point cursor, const Rect monitor, const double edgeThreshold, const double cornerRatio) {
        if (monitor.w <= 0 || monitor.h <= 0 || edgeThreshold < 0)
            return Zone::None;

        const auto ratio          = std::clamp(cornerRatio, 0.0, 0.5);
        const bool nearLeft       = cursor.x <= monitor.x + edgeThreshold;
        const bool nearRight      = cursor.x >= monitor.x + monitor.w - edgeThreshold;
        const bool nearTop        = cursor.y <= monitor.y + edgeThreshold;
        const bool nearBottom     = cursor.y >= monitor.y + monitor.h - edgeThreshold;
        const bool inLeftRegion   = cursor.x <= monitor.x + monitor.w * ratio;
        const bool inRightRegion  = cursor.x >= monitor.x + monitor.w * (1.0 - ratio);
        const bool inTopRegion    = cursor.y <= monitor.y + monitor.h * ratio;
        const bool inBottomRegion = cursor.y >= monitor.y + monitor.h * (1.0 - ratio);

        if ((nearTop && inLeftRegion) || (nearLeft && inTopRegion))
            return Zone::TopLeft;
        if ((nearTop && inRightRegion) || (nearRight && inTopRegion))
            return Zone::TopRight;
        if ((nearBottom && inLeftRegion) || (nearLeft && inBottomRegion))
            return Zone::BottomLeft;
        if ((nearBottom && inRightRegion) || (nearRight && inBottomRegion))
            return Zone::BottomRight;
        if (nearTop)
            return Zone::Maximize;
        if (nearLeft)
            return Zone::Left;
        if (nearRight)
            return Zone::Right;

        return Zone::None;
    }

    inline std::optional<Rect> rectForZone(const Rect workArea, const Zone zone, const double horizontalGap, const double verticalGap) {
        if (workArea.w <= 0 || workArea.h <= 0 || horizontalGap < 0 || verticalGap < 0)
            return std::nullopt;

        if (zone == Zone::Maximize)
            return workArea;

        const double columnWidth = (workArea.w - horizontalGap) / 2.0;
        const double rowHeight   = (workArea.h - verticalGap) / 2.0;
        if (columnWidth <= 0 || rowHeight <= 0)
            return std::nullopt;

        switch (zone) {
            case Zone::Left: return Rect{workArea.x, workArea.y, columnWidth, workArea.h};
            case Zone::Right: return Rect{workArea.x + columnWidth + horizontalGap, workArea.y, columnWidth, workArea.h};
            case Zone::TopLeft: return Rect{workArea.x, workArea.y, columnWidth, rowHeight};
            case Zone::TopRight: return Rect{workArea.x + columnWidth + horizontalGap, workArea.y, columnWidth, rowHeight};
            case Zone::BottomLeft: return Rect{workArea.x, workArea.y + rowHeight + verticalGap, columnWidth, rowHeight};
            case Zone::BottomRight: return Rect{workArea.x + columnWidth + horizontalGap, workArea.y + rowHeight + verticalGap, columnWidth, rowHeight};
            case Zone::None:
            case Zone::Maximize: return std::nullopt;
        }

        return std::nullopt;
    }

} // namespace AeroSnap
