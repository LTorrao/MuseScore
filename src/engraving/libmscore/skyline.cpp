/*
 * SPDX-License-Identifier: GPL-3.0-only
 * MuseScore-CLA-applies
 *
 * MuseScore
 * Music Composition & Notation
 *
 * Copyright (C) 2021 MuseScore BVBA and others
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 3 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include "skyline.h"
#include "arpeggio.h"
#include "draw/painter.h"

#include "shape.h"

#include "realfn.h"

using namespace mu;
using namespace mu::draw;

namespace mu::engraving {
static const double MAXIMUM_Y = 1000000.0;
static const double MINIMUM_Y = -1000000.0;

// #define SKL_DEBUG

#ifdef SKL_DEBUG
#define DP(...)   printf(__VA_ARGS__)
#else
#define DP(...)
#endif

int findSpan(const ShapeElement& r)
{
    int span = 0;
    // add more conditions for other cross-staff skyline items
    if (r.toItem && r.toItem->isArpeggio()) {
        span = toArpeggio(r.toItem)->span() - 1; // for some reason, Arpeggio::_span is 1-indexed
    }
    return span;
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void Skyline::add(const ShapeElement& r)
{
    int span = findSpan(r);
    _north.add(r.x(), r.top(), r.width(), span);
    _south.add(r.x(), r.bottom(), r.width(), span);
}

//---------------------------------------------------------
//   insert
//---------------------------------------------------------

SkylineLine::SegIter SkylineLine::insert(SegIter i, double x, double y, double w, int span)
{
    const double xr = x + w;
    // Only x coordinate change is handled here as width change gets handled
    // in SkylineLine::add().
    if (i != seg.end() && xr > i->x) {
        i->x = xr;
    }
    return seg.emplace(i, x, y, w, span);
}

//---------------------------------------------------------
//   append
//---------------------------------------------------------

void SkylineLine::append(double x, double y, double w, int span)
{
    seg.emplace_back(x, y, w, span);
}

//---------------------------------------------------------
//   getApproxPosition
//---------------------------------------------------------

SkylineLine::SegIter SkylineLine::find(double x)
{
    auto it = std::upper_bound(seg.begin(), seg.end(), x, [](double x, const SkylineSegment& s) { return x < s.x; });
    if (it == seg.begin()) {
        return it;
    }
    return --it;
}

SkylineLine::SegConstIter SkylineLine::find(double x) const
{
    return const_cast<SkylineLine*>(this)->find(x);
}

//---------------------------------------------------------
//   add
//---------------------------------------------------------

void SkylineLine::add(const Shape& s)
{
    for (const auto& r : s) {
        add(r);
    }
}

void SkylineLine::add(const ShapeElement& r)
{
    int span = findSpan(r);
    if (north) {
        add(r.x(), r.top(), r.width(), span);
    } else {
        add(r.x(), r.bottom(), r.width(), span);
    }
}

void Skyline::add(const Shape& s)
{
    for (const auto& r : s) {
        add(r);
    }
}

void SkylineLine::add(double x, double y, double w, int span)
{
//      assert(w >= 0.0);
    if (x < 0.0) {
        w -= -x;
        x = 0.0;
        if (w <= 0.0) {
            return;
        }
    }

    DP("===add  %f %f %f\n", x, y, w);

    SegIter i = find(x);
    double cx = seg.empty() ? 0.0 : i->x;
    for (; i != seg.end(); ++i) {
        double cy = i->y;
        if ((x + w) <= cx) {                                            // A
            return;       // break;
        }
        if (x > (cx + i->w)) {                                          // B
            cx += i->w;
            continue;
        }
        if ((north && (cy <= y)) || (!north && (cy >= y))) {
            cx += i->w;
            continue;
        }
        if ((x >= cx) && ((x + w) < (cx + i->w))) {                     // (E) insert segment
            DP("    insert at %f %f   x:%f w:%f\n", cx, i->w, x, w);
            double w1 = x - cx;
            double w2 = w;
            double w3 = i->w - (w1 + w2);
            if (w1 > 0.0000001) {
                i->w = w1;
                ++i;
                i = insert(i, x, y, w2, span);
                DP("       A w1 %f w2 %f\n", w1, w2);
            } else {
                i->w = w2;
                i->y = y;
                DP("       B w2 %f\n", w2);
            }
            if (w3 > 0.0000001) {
                ++i;
                DP("       C w3 %f\n", w3);
                insert(i, x + w2, cy, w3, span);
            }
            return;
        } else if ((x <= cx) && ((x + w) >= (cx + i->w))) {                 // F
            DP("    change(F) cx %f y %f\n", cx, y);
            i->y = y;
        } else if (x < cx) {                                            // C
            double w1 = x + w - cx;
            i->w    -= w1;
            DP("    add(C) cx %f y %f w %f w1 %f\n", cx, y, w1, i->w);
            insert(i, cx, y, w1, span);
            return;
        } else {                                                        // D
            double w1 = x - cx;
            double w2 = i->w - w1;
            if (w2 > 0.0000001) {
                i->w = w1;
                cx  += w1;
                DP("    add(D) %f %f\n", y, w2);
                ++i;
                i = insert(i, cx, y, w2, span);
            }
        }
        cx += i->w;
    }
    if (x >= cx) {
        if (x > cx) {
            double cy = north ? MAXIMUM_Y : MINIMUM_Y;
            DP("    append1 %f %f\n", cy, x - cx);
            append(cx, cy, x - cx, span);
        }
        DP("    append2 %f %f\n", y, w);
        append(x, y, w, span);
    } else if (x + w > cx) {
        append(cx, y, x + w - cx, span);
    }
}

//---------------------------------------------------------
//   clear
//---------------------------------------------------------

void Skyline::clear()
{
    _north.clear();
    _south.clear();
}

//-------------------------------------------------------------------
//   minDistance
//    a is located below this skyline.
//    Calculates the minimum distance between two skylines
//-------------------------------------------------------------------

double Skyline::minDistance(const Skyline& s) const
{
    return south().minDistance(s.north());
}

double SkylineLine::minDistance(const SkylineLine& sl) const
{
    double dist = MINIMUM_Y;

    double x1 = 0.0;
    double x2 = 0.0;
    auto k   = sl.begin();
    for (auto i = begin(); i != end(); ++i) {
        if (i->staffSpan > 0) {
            continue; // don't add this to the distance because it crosses to the next staff
        }
        while (k != sl.end() && (x2 + k->w) < x1) {
            x2 += k->w;
            ++k;
        }
        if (k == sl.end()) {
            break;
        }
        for (;;) {
            if ((x1 + i->w > x2) && (x1 < x2 + k->w) && k->staffSpan >= 0) {
                // (staffSpan: don't add lower north skyline object if it crosses into our staff)
                dist = std::max(dist, i->y - k->y);
            }
            if (x2 + k->w < x1 + i->w) {
                x2 += k->w;
                ++k;
                if (k == sl.end()) {
                    break;
                }
            } else {
                break;
            }
        }
        if (k == sl.end()) {
            break;
        }
        x1 += i->w;
    }
    return dist;
}

void Skyline::paint(Painter& painter, double lineWidth) const
{
    painter.save();

    painter.setBrush(BrushStyle::NoBrush);
    painter.setPen(Pen(Color(144, 238, 144), lineWidth));
    _north.paint(painter);
    painter.setPen(Pen(Color(144, 144, 238), lineWidth));
    _south.paint(painter);

    painter.restore();
}

void SkylineLine::paint(Painter& painter) const
{
    double x1 = 0.0;
    double x2;
    double y = 0.0;

    bool pvalid = false;
    for (const SkylineSegment& s : *this) {
        x2 = x1 + s.w;
        if (valid(s)) {
            if (pvalid && !RealIsEqual(y, s.y)) {
                painter.drawLine(LineF(x1, y, x1, s.y));
            }
            y = s.y;
            if (!RealIsEqual(x1, x2)) {
                painter.drawLine(LineF(x1, y, x2, y));
            }
            pvalid = true;
        } else {
            pvalid = false;
        }
        x1 = x2;
    }
}

bool SkylineLine::valid() const
{
    return !seg.empty();
}

bool SkylineLine::valid(const SkylineSegment& s) const
{
    return north ? (s.y != MAXIMUM_Y) : (s.y != MINIMUM_Y);
}

//---------------------------------------------------------
//   dump
//---------------------------------------------------------

void Skyline::dump(const char* p, bool n) const
{
    printf("Skyline dump: %p %s\n", this, p);
    if (n) {
        _north.dump();
    } else {
        _south.dump();
    }
}

void SkylineLine::dump() const
{
    double x = 0.0;
    for (const SkylineSegment& s : *this) {
        printf("   x %f y %f w %f\n", x, s.y, s.w);
        x += s.w;
    }
}

//---------------------------------------------------------
//   max
//---------------------------------------------------------

double SkylineLine::max() const
{
    double val;
    if (north) {
        val = MAXIMUM_Y;
        for (const SkylineSegment& s : *this) {
            val = std::min(val, s.y);
        }
    } else {
        val = MINIMUM_Y;
        for (const SkylineSegment& s : *this) {
            val = std::max(val, s.y);
        }
    }
    return val;
}
} // namespace mu::engraving
