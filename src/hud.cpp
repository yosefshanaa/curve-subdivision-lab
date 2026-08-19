// hud.cpp — the on-screen readout.
//
// Drawn onto the resolved canvas at native resolution (never supersampled), so
// text stays sharp regardless of the 3D anti-aliasing factor.
#include <algorithm>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#include "app.h"

namespace sl {
namespace {

std::string fmt(const char* f, ...) __attribute__((format(printf, 1, 2)));

std::string fmt(const char* f, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, f);
    std::vsnprintf(buf, sizeof buf, f, ap);
    va_end(ap);
    return buf;
}

std::string withThousands(long long v) {
    std::string s = std::to_string(v);
    std::string out;
    int c = 0;
    for (int i = int(s.size()) - 1; i >= 0; i--) {
        out += s[i];
        if (++c % 3 == 0 && i > 0 && s[i - 1] != '-') out += ' ';
    }
    std::reverse(out.begin(), out.end());
    return out;
}

struct Layout {
    Canvas& cv;
    int x, w;
    int y;
    int limit = 1 << 30;   // hard bottom: nothing is drawn past this

    bool room(int need) const { return y + need <= limit; }
    void gap(int px) { y += px; }

    void sectionLabel(const std::string& s) {
        if (!room(20)) return;
        // Small caps-ish section header with a hairline to its right.
        drawTextTop(cv, Face::UIBold, x, y, s, theme::textFaint);
        int tw = textWidth(Face::UIBold, s);
        cv.rect(x + tw + 10, y + 8, w - tw - 10, 1, theme::panelEdge, 0.9);
        y += 20;
    }

    void title(const std::string& s, Vec3 c) {
        if (!room(textLineHeight(Face::UIBold))) return;
        drawTextTop(cv, Face::UIBold, x, y, s, c);
        y += textLineHeight(Face::UIBold) + 1;
    }

    void body(const std::string& s, Vec3 c) {
        if (!room(textLineHeight(Face::UI))) return;
        drawTextTop(cv, Face::UI, x, y, s, c);
        y += textLineHeight(Face::UI);
    }

    // label on the left, mono value flushed right — the "readout" look.
    void stat(const std::string& label, const std::string& value, Vec3 vc) {
        if (!room(textLineHeight(Face::UI))) return;
        drawTextTop(cv, Face::UI, x, y, label, theme::textDim);
        int vw = textWidth(Face::MonoBold, value);
        drawTextTop(cv, Face::MonoBold, x + w - vw, y, value, vc);
        y += textLineHeight(Face::UI) + 2;
    }

    // Several small chips flowed onto as few lines as possible.
    void chips(const std::vector<std::pair<std::string, std::pair<Vec3, bool>>>& items) {
        int cx = x;
        const int lh = textLineHeight(Face::UI);
        for (const auto& it : items) {
            int tw = textWidth(Face::UI, it.first) + 16;
            if (cx > x && cx + tw > x + w) { cx = x; y += lh + 2; }
            if (!room(lh)) return;
            const Vec3& c = it.second.first;
            bool on = it.second.second;
            cv.roundRect(cx, y + 4, 7, 7, 2, c, on ? 1.0 : 0.22);
            drawTextTop(cv, Face::UI, cx + 12, y, it.first, on ? theme::text : theme::textFaint);
            cx += tw;
        }
        y += lh + 2;
    }

};

// A level slider with tick marks: filled ticks up to the current level.
void drawLevelTicks(Canvas& cv, int x, int y, int w, int level, int maxLevel, Vec3 c) {
    const int n = maxLevel + 1;
    const int gap = 4;
    const int tw = (w - gap * (n - 1)) / n;
    for (int i = 0; i < n; i++) {
        bool on = i <= level;
        cv.roundRect(x + i * (tw + gap), y, tw, 6, 3, on ? c : theme::panelEdge, on ? 1.0 : 0.85);
    }
}

// Log-scaled bars: subdivision multiplies the face count by ~4 per level, so a
// linear chart would show one visible bar and five slivers.
void drawGrowthChart(Canvas& cv, int x, int y, int w, int h, const std::vector<int>& counts,
                     Vec3 c) {
    if (counts.size() < 2) return;
    double maxLog = 0;
    for (int v : counts) maxLog = std::max(maxLog, std::log2(std::max(1, v) + 1.0));
    if (maxLog <= 0) return;

    const int n = int(counts.size());
    const int gap = 3;
    const int bw = std::max(4, (w - gap * (n - 1)) / n);
    for (int i = 0; i < n; i++) {
        double t = std::log2(std::max(1, counts[i]) + 1.0) / maxLog;
        int bh = std::max(2, int(t * h));
        int bx = x + i * (bw + gap);
        cv.roundRect(bx, y + h - bh, bw, bh, 2, c, i == n - 1 ? 1.0 : 0.45);
        drawTextTop(cv, Face::Mono, bx + bw / 2 - textWidth(Face::Mono, std::to_string(i)) / 2,
                    y + h + 3, std::to_string(i), theme::textFaint);
    }
}

void drawBadge(Canvas& cv, int x, int y, const std::string& s, Vec3 fg, Vec3 bg) {
    int tw = textWidth(Face::UIBold, s);
    cv.roundRect(x, y, tw + 18, 20, 10, bg, 1.0);
    drawTextTop(cv, Face::UIBold, x + 9, y + 3, s, fg);
}

}  // namespace

void drawHud(Canvas& cv, const AppState& st, const FrameInfo& fi) {
    const int PX = 22, PY = 20, PW = 306;
    const int PH = cv.h - PY * 2;
    // Optional blocks are dropped on short windows so the panel never overflows.
    const bool compact = PH < 720;
    const bool veryCompact = PH < 560;

    cv.roundRect(PX, PY, PW, PH, 12, theme::panel, 0.93);
    cv.roundRectOutline(PX, PY, PW, PH, 12, theme::panelEdge, 0.9);

    Layout L{cv, PX + 20, PW - 40, PY + 20, PY + PH - 32};

    // ---- header
    drawTextTop(cv, Face::Title, L.x, L.y, "Subdivision Lab", theme::text);
    L.y += textLineHeight(Face::Title) - 2;
    drawTextTop(cv, Face::UI, L.x, L.y, "surface subdivision · software renderer",
                theme::textFaint);
    L.y += textLineHeight(Face::UI) + 14;

    // ---- mode tabs
    {
        const char* names[5] = {"Surface", "Curve", "Terrain", "Compare", "Levels"};
        int bx = L.x;
        for (int i = 0; i < 5; i++) {
            bool on = int(st.mode) == i;
            int tw = textWidth(Face::UIBold, names[i]) + 16;
            if (bx + tw > L.x + L.w) { bx = L.x; L.y += 27; }
            cv.roundRect(bx, L.y, tw, 22, 6, on ? theme::accent : theme::panelEdge, on ? 0.9 : 0.55);
            drawTextTop(cv, Face::UIBold, bx + 8, L.y + 4, names[i],
                        on ? rgb(0x0E1A16) : theme::textDim);
            bx += tw + 5;
        }
        L.y += 34;
    }

    if (st.mode != Mode::Curve && st.mode != Mode::Terrain) {
        // ---- scheme card
        L.sectionLabel(st.mode == Mode::Compare ? "SCHEMES" : "SCHEME");
        // Levels mode shows one scheme, like Surface does.
        if (st.mode == Mode::Compare) {
            for (int i = 0; i < 4; i++) {
                SurfScheme s = AppState::kCompareSchemes[i];
                cv.roundRect(L.x, L.y + 3, 9, 9, 2, schemeColor(s), 1.0);
                drawTextTop(cv, Face::UIBold, L.x + 17, L.y, schemeName(s), theme::text);
                int nw = textWidth(Face::UIBold, schemeName(s));
                drawTextTop(cv, Face::UI, L.x + 17 + nw + 8, L.y, schemeKind(s),
                            s == SurfScheme::Butterfly ? theme::warn : theme::textFaint);
                L.y += textLineHeight(Face::UI) + 3;
                drawTextTop(cv, Face::Mono, L.x + 17, L.y,
                            fmt("%s  V %s  F %s", schemeFaceKind(s),
                                withThousands(st.cmpStats_[i].verts).c_str(),
                                withThousands(st.cmpStats_[i].faces).c_str()),
                            theme::textFaint);
                L.y += textLineHeight(Face::Mono) + 8;
            }
        } else {
            Vec3 sc = schemeColor(st.scheme);
            cv.roundRect(L.x, L.y + 5, 10, 10, 3, sc, 1.0);
            drawTextTop(cv, Face::Title, L.x + 20, L.y - 3, schemeName(st.scheme), theme::text);
            L.y += textLineHeight(Face::Title) - 2;
            drawBadge(cv, L.x, L.y, schemeKind(st.scheme),
                      st.scheme == SurfScheme::Butterfly ? rgb(0x2B2416) : rgb(0x11221C),
                      st.scheme == SurfScheme::Butterfly ? theme::warn : theme::accent);
            drawTextTop(cv, Face::UI, L.x + textWidth(Face::UIBold, schemeKind(st.scheme)) + 28,
                        L.y + 3, schemeFaceKind(st.scheme), theme::textFaint);
            L.y += 28;
        }
        L.gap(4);

        // ---- cage + level
        L.sectionLabel("CONTROL CAGE");
        L.stat(baseMeshName(st.base),
               fmt("%d V · %d F", st.cageStats_.verts, st.cageStats_.faces), theme::cage);
        L.gap(8);

        L.sectionLabel("LEVEL");
        drawLevelTicks(cv, L.x, L.y + 4, L.w, st.level, st.maxLevel(),
                       st.mode == Mode::Compare ? theme::accent : schemeColor(st.scheme));
        L.y += 16;
        L.stat(fmt("%d of %d", st.level, st.maxLevel()),
               st.mode == Mode::Surface ? fmt("%.1f ms", st.surf_.milliseconds) : "",
               theme::textDim);
        L.gap(6);

        if (st.mode == Mode::Levels) {
            L.sectionLabel("EACH TILE, ONE LEVEL");
            for (int i = 0; i < 4; i++)
                L.stat(fmt("level %d", i),
                       fmt("%s V · %s F", withThousands(st.lvlStats_[i].verts).c_str(),
                           withThousands(st.lvlStats_[i].faces).c_str()),
                       i == 3 ? schemeColor(st.scheme) : theme::textDim);
            L.gap(10);
            L.sectionLabel("FACES PER LEVEL  (log)");
            {
                std::vector<int> hist;
                for (int i = 0; i < 4; i++) hist.push_back(st.lvlStats_[i].faces);
                drawGrowthChart(cv, L.x, L.y, L.w, 42, hist, schemeColor(st.scheme));
                L.y += 42 + 20;
            }
            L.body("Every level multiplies the face", theme::textDim);
            L.body("count by four. Five levels is", theme::textDim);
            L.body("1024x the cage — which is why", theme::textDim);
            L.body("~5 steps already look smooth.", theme::warn);
        }

        if (st.mode == Mode::Compare) {
            L.sectionLabel("WHAT TO LOOK FOR");
            L.body("All four refine the same cage.", theme::textDim);
            L.body("Butterfly's surface touches every", theme::textDim);
            L.body("cage corner — it interpolates.", theme::warn);
            L.body("The other three pull inward, so", theme::textDim);
            L.body("the cage is only a hull for them.", theme::textDim);
            L.gap(10);
            L.sectionLabel("PRIMAL VS DUAL");
            L.body("Catmull-Clark splits faces;", theme::textDim);
            L.body("Doo-Sabin builds the dual, so its", theme::textDim);
            L.body("V and F counts are swapped.", theme::textDim);
            L.gap(10);
            L.sectionLabel("EULER CHARACTERISTIC");
            for (int i = 0; i < 4; i++)
                L.stat(schemeShortName(AppState::kCompareSchemes[i]),
                       std::to_string(st.cmpStats_[i].euler),
                       st.cmpStats_[i].euler == st.cageStats_.euler ? theme::accent : theme::warn);
        }

        if (st.mode == Mode::Surface) {
            const MeshStats& s = st.surfStats_;
            L.sectionLabel("REFINED MESH");
            L.stat("vertices", withThousands(s.verts), theme::text);
            L.stat("edges", withThousands(s.edges), theme::text);
            L.stat("faces", withThousands(s.faces), theme::text);
            L.stat("V \u2212 E + F", std::to_string(s.euler),
                   s.euler == st.cageStats_.euler ? theme::accent : theme::warn);
            L.stat("extraordinary", std::to_string(s.extraordinary),
                   s.extraordinary ? theme::hot : theme::textDim);
            L.stat("longest edge", fmt("%.2f px", fi.maxEdgePx),
                   fi.maxEdgePx < 2.0 ? theme::accent : theme::textDim);
            L.gap(6);

            if (!compact && L.room(80) && st.surf_.faceHistory.size() > 1) {
                L.sectionLabel("FACES PER LEVEL  (log)");
                drawGrowthChart(cv, L.x, L.y, L.w, 42, st.surf_.faceHistory,
                                schemeColor(st.scheme));
                L.y += 42 + 18;
                double ratio = st.surf_.faceHistory.size() > 1
                                   ? double(st.surf_.faceHistory.back()) /
                                         std::max(1, st.surf_.faceHistory[st.surf_.faceHistory.size() - 2])
                                   : 0;
                L.stat("growth per level", fmt("×%.0f", ratio), theme::textDim);
            }

            L.gap(2);
            if (!compact) {
            L.sectionLabel("REFINEMENT RULE");
            for (const std::string& line : schemeRule(st.scheme)) {
                if (!L.room(textLineHeight(Face::Mono))) break;
                drawTextTop(cv, Face::Mono, L.x, L.y, line, theme::textDim);
                L.y += textLineHeight(Face::Mono) + 1;
            }
            L.gap(8);
            }
            if (st.surf_.triangulatedFirst)
                L.body("cage triangulated first (scheme needs triangles)", theme::warn);
            if (st.surf_.cappedByBudget)
                L.body("stopped early: face budget reached", theme::warn);
            if (st.scheme == SurfScheme::Butterfly)
                L.body("interpolating: cage vertices stay put", theme::warn);
            if (fi.maxEdgePx > 0 && fi.maxEdgePx < 2.0)
                L.body("≈ limit surface (edges < 2 px)", theme::accent);
        }
    } else if (st.mode == Mode::Curve) {
        L.sectionLabel("SCHEME");
        Vec3 cc = (st.curveScheme == CurveScheme::Chaikin)   ? theme::accent
                : (st.curveScheme == CurveScheme::FourPoint) ? theme::accent2
                                                             : theme::violet;
        cv.roundRect(L.x, L.y + 5, 10, 10, 3, cc, 1.0);
        drawTextTop(cv, Face::Title, L.x + 20, L.y - 3, curveSchemeName(st.curveScheme),
                    theme::text);
        L.y += textLineHeight(Face::Title) - 2;
        drawBadge(cv, L.x, L.y, curveSchemeKind(st.curveScheme), rgb(0x11221C), cc);
        L.y += 30;

        L.sectionLabel("CONTROL POLYGON");
        L.stat(presetCurveName(st.curvePreset),
               fmt("%d pts", int(st.control_.size())), theme::cage);
        L.gap(8);

        L.sectionLabel("LEVEL");
        drawLevelTicks(cv, L.x, L.y + 4, L.w, st.curveLevel, st.maxLevel(), cc);
        L.y += 18;
        L.stat(fmt("%d of %d", st.curveLevel, st.maxLevel()), "", theme::textDim);
        L.stat(curveParamName(st.curveScheme), fmt("%.4f", st.curveParam), cc);
        L.gap(6);

        L.sectionLabel("CURVE");
        L.stat("vertices", withThousands(int(st.curve_.curve.size())), theme::text);
        L.stat("longest edge", fmt("%.2f px", fi.maxEdgePx),
               fi.maxEdgePx < 1.0 ? theme::accent : theme::textDim);
        L.stat("total length", fmt("%.3f", st.curve_.totalLength), theme::textDim);
        L.gap(6);

        if (!compact) {
        L.sectionLabel("REFINEMENT RULE");
        {
            std::vector<std::string> rule;
            if (st.curveScheme == CurveScheme::Chaikin)
                rule = {"each edge (A,B) becomes two points", "  (1-t)A + tB   and   tA + (1-t)B"};
            else if (st.curveScheme == CurveScheme::FourPoint)
                rule = {"keep P, insert per edge:", "  (1/2+w)(Pi+Pi+1) - w(Pi-1+Pi+2)"};
            else
                rule = {"keep P, insert the midpoint", "  displaced by +/- r*|edge|/2"};
            for (const std::string& line : rule) {
                if (!L.room(textLineHeight(Face::Mono))) break;
                drawTextTop(cv, Face::Mono, L.x, L.y, line, theme::textDim);
                L.y += textLineHeight(Face::Mono) + 1;
            }
        }
        L.gap(8);
        }

        if (st.curveScheme == CurveScheme::FourPoint && st.curveParam > 0.125)
            L.body("w far from 1/16 → fractal, not smooth", theme::warn);
        if (fi.maxEdgePx > 0 && fi.maxEdgePx < 1.0)
            L.body("≈ limit curve (edges < 1 px)", theme::accent);
        L.gap(6);
        L.sectionLabel("SURFACE COUNTERPART");
        L.body(st.curveScheme == CurveScheme::FourPoint ? "Four-Point → Butterfly"
             : st.curveScheme == CurveScheme::Chaikin   ? "Chaikin → Doo-Sabin"
                                                        : "midpoint → diamond-square",
               theme::textDim);
    } else {  // Terrain
        L.sectionLabel("SCHEME");
        drawTextTop(cv, Face::Title, L.x, L.y - 3, "Diamond-square", theme::text);
        L.y += textLineHeight(Face::Title) - 2;
        drawBadge(cv, L.x, L.y, "random / fractal", rgb(0x2B2416), theme::warn);
        L.y += 30;
        L.body("midpoint displacement, one dimension up", theme::textFaint);
        L.gap(10);

        L.sectionLabel("LEVEL");
        drawLevelTicks(cv, L.x, L.y + 4, L.w, st.terrain.levels, st.maxLevel(), theme::accent);
        L.y += 18;
        L.stat(fmt("%d of %d", st.terrain.levels, st.maxLevel()),
               fmt("%d²", st.terr_.gridSize), theme::textDim);
        L.stat("roughness", fmt("%.2f", st.terrain.roughness), theme::warn);
        L.stat("seed", fmt("%u", st.terrain.seed), theme::textFaint);
        L.gap(8);

        L.sectionLabel("MESH");
        L.stat("vertices", withThousands(st.terr_.mesh.numVerts()), theme::text);
        L.stat("quads", withThousands(st.terr_.mesh.numFaces()), theme::text);
        L.stat("water", fmt("%.0f %%", st.terr_.waterFraction * 100), theme::accent2);
        L.stat("build time", fmt("%.1f ms", st.terr_.milliseconds), theme::textDim);
        L.gap(10);

        if (!compact && L.room(80) && st.terr_.vertHistory.size() > 1) {
            L.sectionLabel("VERTICES PER LEVEL  (log)");
            drawGrowthChart(cv, L.x, L.y, L.w, 42, st.terr_.vertHistory, theme::accent);
            L.y += 42 + 20;
        }
        if (!compact) {
        L.sectionLabel("REFINEMENT RULE");
        {
            const char* rule[] = {"diamond: square centre =",
                                  "  mean of 4 corners + random",
                                  "square:  edge midpoint =",
                                  "  mean of 4 neighbours + random"};
            for (const char* line : rule) {
                if (!L.room(textLineHeight(Face::Mono))) break;
                drawTextTop(cv, Face::Mono, L.x, L.y, line, theme::textDim);
                L.y += textLineHeight(Face::Mono) + 1;
            }
        }
        L.gap(8);
        }
        L.body("displacement range shrinks each level", theme::textFaint);
    }

    // ---- view section, pinned near the bottom when there is room for it
    const bool curveMode = st.mode == Mode::Curve;
    const int viewBlock = 24 + 2 * (textLineHeight(Face::UI) + 2);   // label + 2 chip rows
    const int viewY = PY + PH - 34 - viewBlock;
    // Pin it to the bottom when the panel has slack, otherwise let it follow the
    // content. Only one chip row has to fit to be worth drawing — chips() drops
    // whatever runs past the limit.
    if (!veryCompact && L.y + 24 + textLineHeight(Face::UI) <= L.limit) {
        L.y = std::max(L.y, viewY);
        L.sectionLabel("VIEW");
        std::vector<std::pair<std::string, std::pair<Vec3, bool>>> items;
        if (!curveMode) {
            items.push_back({shadingName(st.shading), {theme::accent2, true}});
            items.push_back({"wire", {theme::accent, st.showWire}});
        }
        items.push_back({curveMode ? "polygon" : "cage", {theme::cage, st.showCage}});
        items.push_back({"grid", {theme::textDim, st.showGrid}});
        if (st.mode == Mode::Surface)
            items.push_back({"extraordinary", {theme::hot, st.showExtraordinary}});
        L.chips(items);
    }

    // ---- footer
    {
        std::string s = fmt("%s tris · %.0f ms · %d×%d", withThousands(fi.triangles).c_str(),
                            fi.renderMs, cv.w, cv.h);
        drawTextTop(cv, Face::Mono, L.x, PY + PH - 26, s, theme::textFaint);
    }

    // ---- key hints along the bottom of the viewport
    {
        const char* full = "1-5 mode · S scheme · M cage · [ ] level · F shading · "
                           "W wire · C cage · X extraordinary · A spin · P png · Q quit";
        const char* brief = "1-5 mode · S scheme · [ ] level · A spin · Q quit";
        int bx = PX + PW + 24, by = cv.h - 34;
        int avail = cv.w - bx - 22;
        const char* hints = (textWidth(Face::UI, full) + 24 <= avail) ? full : brief;
        int tw = textWidth(Face::UI, hints);
        if (tw + 24 <= avail) {
            cv.roundRect(bx, by, tw + 24, 24, 12, theme::panel, 0.8);
            drawTextTop(cv, Face::UI, bx + 12, by + 4, hints, theme::textFaint);
        }
    }

    // ---- tile captions
    if (st.isTiled()) {
        const int gutter = 348, pad = 6, hintStrip = 40;
        const int vw = (cv.w - gutter - pad * 3) / 2, vh = (cv.h - pad * 3 - hintStrip) / 2;
        for (int i = 0; i < 4; i++) {
            bool cmpMode = st.mode == Mode::Compare;
            SurfScheme s = cmpMode ? AppState::kCompareSchemes[i] : st.scheme;
            int vx = gutter + pad + (i % 2) * (vw + pad);
            int vy = pad + (i / 2) * (vh + pad);
            cv.roundRectOutline(vx, vy, vw, vh, 8, theme::panelEdge, 0.5);
            std::string label =
                cmpMode ? fmt("%s · %s", schemeName(s), schemeKind(s))
                        : fmt("level %d · %s faces", i,
                              withThousands(st.lvlStats_[i].faces).c_str());
            int tw = textWidth(Face::UIBold, label);
            cv.roundRect(vx + 12, vy + 12, tw + 42, 26, 13, theme::panel, 0.88);
            cv.roundRect(vx + 24, vy + 21, 8, 8, 2, schemeColor(s), 1.0);
            drawTextTop(cv, Face::UIBold, vx + 40, vy + 17, label, theme::text);
        }
    }
}

}  // namespace sl
