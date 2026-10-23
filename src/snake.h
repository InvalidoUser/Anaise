#pragma once

#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <string>
#include <vector>

namespace anaise {

// Session-only arcade state. The same clock and input paths serve live play
// and offscreen checks; targets never depend on the machine's random seed.
class Snake {
public:
    static constexpr int cols = 26, rows = 18;
    static constexpr ULONGLONG cinematicMs = 3600, contactMs = 500, revealMs = 2900;
    Snake() { reset(); }

    void reset() {
        body_ = {{5, 8}, {4, 8}, {3, 8}, {2, 8}};
        prevBody_ = body_;
        dir_ = 1; turns_.clear(); trail_.clear();
        score_ = 0; points_ = 0; person_ = 0; seed_ = 17; sinceSpawn_ = 0;
        started_ = paused_ = dead_ = won_ = false;
        frac_ = 1; deathAge_ = 0; gain_ = 0; gainAge_ = 99999; jam_ = false; drips_ = 0; bonus_ = false;
        clock_ = debt_ = impact_ = cinemaAge_ = 0;
        cinematic_ = sounded_ = false; pendingGroan_ = -1;
        target_ = {14, 8}; oldTarget_ = target_;
    }

    void key(int key) {
        if (key == 'R') { reset(); return; }
        if (key == VK_RETURN || key == VK_SPACE || key == 'P') {
            if (dead_ || won_) { reset(); started_ = true; }
            else if (!started_) started_ = true;
            else paused_ = !paused_;
            debt_ = 0;
            return;
        }
        if (cinematic_ || dead_ || won_) return;
        int next = key == VK_UP || key == 'W' ? 0 :
                   key == VK_RIGHT || key == 'D' ? 1 :
                   key == VK_DOWN || key == 'S' ? 2 :
                   key == VK_LEFT || key == 'A' ? 3 : -1;
        // Two turns can be buffered inside one tick, so quick corners land.
        const int last = turns_.empty() ? dir_ : turns_.back();
        if (next >= 0 && turns_.size() < 2 && next != last && (next + 2) % 4 != last) turns_.push_back(next);
    }

    void update(ULONGLONG now, bool focused) {
        const ULONGLONG elapsed = clock_ && now >= clock_ ? std::min<ULONGLONG>(now - clock_, 250) : 0;
        clock_ = now;
        if (!focused) { if (started_) paused_ = true; debt_ = 0; return; }
        gainAge_ = std::min<ULONGLONG>(99999, gainAge_ + elapsed);
        if (dead_) deathAge_ = std::min<ULONGLONG>(99999, deathAge_ + elapsed);
        if (!started_ || paused_ || dead_ || won_) { debt_ = 0; frac_ = 1; return; }
        if (cinematic_) {
            cinemaAge_ = std::min(cinematicMs, cinemaAge_ + elapsed);
            if (cinemaAge_ >= contactMs && !sounded_) {
                pendingGroan_ = person_; sounded_ = true;
            }
            if (cinemaAge_ >= cinematicMs) {
                cinematic_ = false; impact_ = 0; person_ = (person_ + 1) % 3;
                turns_.clear(); debt_ = 0; prevBody_ = body_; frac_ = 1; spawn();
            }
            return;
        }
        debt_ += elapsed;
        const ULONGLONG interval = tick();
        while (debt_ >= interval && !dead_ && !won_ && !cinematic_) { debt_ -= interval; step(); }
        frac_ = cinematic_ || dead_ ? 1.0 : std::min(1.0, double(debt_) / interval);
    }

    // True once when the drill jams, so the host can play the crunch.
    bool takeJam() { const bool j = jam_; jam_ = false; return j; }
    int best() const { return best_; }

    bool cinematic() const { return cinematic_; }
    bool paused() const { return paused_; }
    int takeGroan() { const int voice = pendingGroan_; pendingGroan_ = -1; return voice; }

    void draw(HDC dc, int width, int height, ULONGLONG now) const {
        const int saved = SaveDC(dc);
        SelectObject(dc, GetStockObject(DC_PEN));
        SelectObject(dc, GetStockObject(BLACK_BRUSH));
        SetDCPenColor(dc, RGB(255,255,255));
        SetTextColor(dc, RGB(255,255,255)); SetBkMode(dc, TRANSPARENT);
        if (cinematic_) {
            drawCinematic(dc, width, height);
            RestoreDC(dc, saved);
            return;
        }
        const int cell = std::max(1, std::min((width - (width >= 650 ? 180 : 24)) / cols, (height - 108) / rows));
        // A jammed drill shakes the whole board for a moment.
        const bool jamming = dead_ && deathAge_ < 900;
        const int shake = jamming ? int(((deathAge_ / 40) % 3) - 1) * std::max(2, cell / 6) : 0;
        const int left = (width - (width >= 650 ? 160 : 0) - cols * cell) / 2 + shake, top = 65;
        HFONT font = CreateFontW(std::clamp(cell / 2 + 4, 10, 18), 0, 0, 0, FW_NORMAL,
            FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
            NONANTIALIASED_QUALITY, FIXED_PITCH, L"Consolas");
        SelectObject(dc, font);
        auto text = [&](int x, int y, const std::wstring& value) { TextOutW(dc, x, y, value.c_str(), int(value.size())); };
        auto fill = [&](int x0, int y0, int x1, int y1) {
            RECT r{std::min(x0, x1), std::min(y0, y1), std::max(x0, x1), std::max(y0, y1)};
            FillRect(dc, &r, static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        };
        text(left, 10, L"S N A K E  /  DRILL HEAD");
        text(left, 34, L"LEVEL " + std::to_wstring(score_ + 1) + L"   SCORE " + std::to_wstring(points_) +
             L"   BEST " + std::to_wstring(best_) + L"   LENGTH " + std::to_wstring(body_.size()));
        // Speed gauge: fills as the tick shortens towards its 65 ms floor.
        {
            const int gx = left + cols * cell - 110, gy = 36, gw = 108;
            Rectangle(dc, gx, gy, gx + gw, gy + 12);
            fill(gx + 2, gy + 2, gx + 2 + (gw - 4) * (170 - int(tick())) / 105, gy + 10);
        }
        Rectangle(dc, left - 2, top - 2, left + cols * cell + 2, top + rows * cell + 2);
        for (int y = 1; y < rows; ++y) for (int x = 1; x < cols; ++x)
            SetPixelV(dc, left + x * cell, top + y * cell, RGB(255,255,255));
        auto line = [&](int x1, int y1, int x2, int y2) { MoveToEx(dc, x1, y1, nullptr); LineTo(dc, x2, y2); };
        // Drips left behind after each bore fade over a dozen steps.
        for (const auto& d : trail_) {
            const int x = left + d.p.x * cell, y = top + d.p.y * cell, r = std::max(1, cell * d.life / 60);
            const int ox = (d.p.x * 7 + d.p.y * 3) % std::max(1, cell - 4), oy = (d.p.x * 5 + d.p.y * 11) % std::max(1, cell - 4);
            Ellipse(dc, x + 2 + ox - r, y + 2 + oy - r, x + 3 + ox + r, y + 3 + oy + r);
            if (d.life > 6) SetPixelV(dc, x + cell / 2, y + cell / 2, RGB(255,255,255));
        }
        const POINT headCell = body_.front();
        const int closeness = std::abs(headCell.x - target_.x) + std::abs(headCell.y - target_.y);
        auto face = [&](POINT p, int person, bool drilled) {
            int x = left + p.x * cell + cell / 2;
            const int y = top + p.y * cell + cell / 2;
            const int w = cell * 3, h = cell * 7 / 2;
            // Targets tremble as the drill comes close.
            if (!drilled && closeness <= 4 && started_ && !paused_) x += int((now / 50) % 3) - 1;
            drawPortrait(dc, person % 3, x-w/2, y-cell/2, w, h, drilled, impact_);
            if (!drilled) {
                const int radius = std::max(3, cell/3 + int((now/180)%2));
                Ellipse(dc,x-radius,y-radius,x+radius+1,y+radius+1);
                line(x-cell/2,y,x+cell/2,y); line(x,y-cell/2,x,y+cell/2);
            } else {
                SelectObject(dc, GetStockObject(WHITE_BRUSH));
                Ellipse(dc,x-cell/3,y-cell/3,x+cell/3+1,y+cell/3+1);
                SelectObject(dc, GetStockObject(BLACK_BRUSH));
                for(int i=0;i<8;++i) {
                    const int dx[] = {1,1,0,-1,-1,-1,0,1}, dy[] = {0,1,1,1,0,-1,-1,-1};
                    line(x+dx[i]*cell/2,y+dy[i]*cell/2,x+dx[i]*cell,y+dy[i]*cell);
                }
            }
        };
        if (impact_) face(oldTarget_, (person_ + 2) % 3, true);
        if (!won_) face(target_, person_, false);
        // Segments slide between cells; the tube is drawn centre to centre.
        const double f = frac_;
        auto centre = [&](std::size_t i) {
            const POINT c = body_[i], p = i < prevBody_.size() ? prevBody_[i] : c;
            return std::pair<int, int>{left + int((p.x + (c.x - p.x) * f) * cell) + cell / 2,
                                       top + int((p.y + (c.y - p.y) * f) * cell) + cell / 2};
        };
        const bool blink = jamming && (deathAge_ / 110) % 2;
        if (!blink) {
            for (std::size_t i = body_.size(); i-- > 0;) {
                const auto [x, y] = centre(i);
                const int n = int(body_.size());
                // The tail tapers over its last three segments.
                const int half = std::max(2, (cell - 3) / 2 - std::max(0, int(i) - (n - 4)) * std::max(1, cell / 10));
                fill(x - half, y - half, x + half + 1, y + half + 1);
                if (i + 1 < body_.size()) {
                    const auto [px, py] = centre(i + 1);
                    const int nh = std::max(2, half - 1);
                    fill(std::min(x, px) - nh, std::min(y, py) - nh, std::max(x, px) + nh + 1, std::max(y, py) + nh + 1);
                }
            }
            SetDCPenColor(dc, RGB(0,0,0));
            for (std::size_t i = 1; i < body_.size(); ++i) {
                const auto [x, y] = centre(i);
                const POINT a = body_[i - 1], b = body_[i];
                if (a.y == b.y) line(x, y - cell / 2 + 3, x, y + cell / 2 - 2);
                else line(x - cell / 2 + 3, y, x + cell / 2 - 2, y);
            }
            const auto [hx, hy] = centre(0);
            // Spiral teeth on the drill head rotate in two crisp phases.
            for (int j = 3; j < cell - 2; j += 5) {
                const int offset = int((now / 90) % 2) * 2, x = hx - cell / 2, y = hy - cell / 2;
                if (dir_ % 2) line(x + j, y + 2 + offset, x + j - 3, y + cell - 2);
                else line(x + 2 + offset, y + j, x + cell - 2, y + j - 3);
            }
            SetDCPenColor(dc, RGB(255,255,255));
        }
        {
            const int dx[] = {0,1,0,-1}, dy[] = {-1,0,1,0};
            const auto [cx, cy] = centre(0);
            const int reach = cell + (dead_ ? 0 : int((now / 60) % 2));
            POINT bit[] = {{cx+dx[dir_]*reach,cy+dy[dir_]*reach},
                           {cx-dy[dir_]*cell/3,cy+dx[dir_]*cell/3},
                           {cx+dy[dir_]*cell/3,cy-dx[dir_]*cell/3}};
            SelectObject(dc, GetStockObject(WHITE_BRUSH)); Polygon(dc,bit,3);
            SelectObject(dc, GetStockObject(BLACK_BRUSH));
            if (jamming) {
                // Sparks fly from the jammed bit.
                const int tx = cx + dx[dir_] * cell, ty = cy + dy[dir_] * cell;
                for (int k = 0; k < 9; ++k) {
                    const double ang = k * 0.698 + deathAge_ * 0.013;
                    const int r0 = cell / 3, r1 = cell / 3 + int(cell * (0.6 + 0.5 * ((k * 37 + deathAge_ / 30) % 7) / 7.0));
                    line(tx + int(std::cos(ang) * r0), ty + int(std::sin(ang) * r0), tx + int(std::cos(ang) * r1), ty + int(std::sin(ang) * r1));
                }
            }
        }
        if (gain_ > 0 && gainAge_ < 1200 && !won_) {
            const int gx = left + oldTarget_.x * cell, gy = top + oldTarget_.y * cell - cell - 14 - int(gainAge_ / 40);
            text(gx, gy, L"+" + std::to_wstring(gain_) + (bonus_ ? L" QUICK" : L""));
        }
        if(width>=650) {
            const int sx=width-151;
            text(sx,top,impact_ ? L"LAST HIT" : L"NEXT TARGET");
            if(impact_) {
                drawPortrait(dc,(person_+2)%3,sx,top+30,128,160,true,impact_);
                text(sx,top+204,L"+1 / BORED");
            } else {
                drawPortrait(dc,person_%3,sx,top+30,128,160,false,0);
                text(sx,top+204,closeness <= 4 && started_ ? L"CLOSER..." : L"AIM: FOREHEAD");
            }
            // The quick-bore bonus drains while the target waits.
            text(sx,top+228,L"BONUS " + std::to_wstring(quickBonus()));
            text(sx,top+258,L"CC0 / THE MET");
        }
        text(left, top+rows*cell+12, L"ARROWS/WASD steer   SPACE pause   R reset");
        if ((!started_ || paused_ || dead_ || won_) && !jamming) {
            RECT panel{left+cols*cell/8,top+rows*cell/2-46,left+cols*cell*7/8,top+rows*cell/2+48};
            Rectangle(dc,panel.left,panel.top,panel.right,panel.bottom);
            RECT title=panel; title.top+=12;
            const wchar_t* label = dead_ ? L"DRILL JAMMED" : won_ ? L"ALL BORED!" : paused_ ? L"PAUSED" : L"A NEW KIND OF SNAKE";
            DrawTextW(dc,label,-1,&title,DT_CENTER|DT_TOP|DT_SINGLELINE);
            title.top+=24;
            const std::wstring sub = dead_ || won_
                ? L"SCORE " + std::to_wstring(points_) + (points_ > 0 && points_ >= best_ ? L"  NEW BEST!" : L"  BEST " + std::to_wstring(best_))
                : started_ ? L"SPACE to resume" : L"Bore fast for a bonus";
            DrawTextW(dc,sub.c_str(),-1,&title,DT_CENTER|DT_TOP|DT_SINGLELINE);
            title.top+=24;
            DrawTextW(dc,dead_ || won_ ? L"ENTER to retry" : started_ ? L"" : L"ENTER / aim for the forehead",-1,&title,DT_CENTER|DT_TOP|DT_SINGLELINE);
        }
        RestoreDC(dc,saved); DeleteObject(font);
    }

    static bool selfTest() {
        Snake s;
        s.key(VK_RETURN);
        for(int i=0;i<9;++i) s.step();
        if(s.score_!=1 || s.body_.size()!=5 || !s.cinematic_ || s.person_!=0) return false;
        const auto frozen=s.body_; const POINT old=s.target_;
        s.clock_=100; s.update(350,true);
        if(s.takeGroan()!=-1 || s.cinemaAge_!=250) return false;
        s.update(600,true);
        if(s.takeGroan()!=0 || s.takeGroan()!=-1 || s.person_!=0) return false;
        s.key(VK_UP); s.step();
        if(s.dir_!=1 || !same(frozen[0],s.body_[0])) return false;
        s.key(VK_SPACE); s.update(850,true);
        if(s.cinemaAge_!=500) return false;
        s.key(VK_SPACE); s.update(1100,false);
        if(!s.paused_ || s.cinemaAge_!=500) return false;
        s.key(VK_SPACE);
        ULONGLONG now=1100;
        while(s.cinematic_) { now+=100; s.update(now,true); }
        if(s.person_!=1 || s.score_!=1 || s.takeGroan()!=-1 || same(s.target_,old) || !same(s.body_[0],frozen[0])) return false;
        for(const auto& p:s.body_) if(same(p,s.target_)) return false;
        // Every subsequent person gets a fresh sequence and exactly one voice.
        for(int person=1;person<=2;++person) {
            s.body_={{13,8},{12,8},{11,8},{10,8}}; s.target_={14,8};
            s.dir_=1; s.turns_.clear(); s.step();
            if(!s.cinematic_ || s.person_!=person) return false;
            int cues=0;
            while(s.cinematic_) {
                now+=100; s.update(now,true);
                int cue=s.takeGroan(); if(cue>=0) { if(cue!=person) return false; ++cues; }
            }
            if(cues!=1 || s.person_!=(person+1)%3) return false;
        }
        s.reset(); s.key(VK_RETURN);
        s.key(VK_LEFT); s.step(); if(s.dir_!=1) return false;
        s.key(VK_UP); s.key(VK_LEFT); s.step(); if(s.dir_!=0) return false;
        s.key(VK_SPACE); const POINT p=s.body_[0];
        s.update(1000,true); s.update(1200,true); if(!same(p,s.body_[0])) return false;
        s.reset(); s.target_={5,3}; s.started_=true;
        for(int i=0;i<30;++i) s.step(); if(!s.dead_) return false;
        s.reset(); s.body_={{4,4},{4,5},{3,5},{3,4},{3,3},{4,3},{5,3},{5,4}};
        s.dir_=3; s.step(); if(!s.dead_ || !s.takeJam() || s.takeJam()) return false;
        s.reset(); s.body_={{4,4},{4,5},{3,5},{3,4}};
        s.dir_=3; s.step(); if(s.dead_) return false;
        // Two quick presses inside one tick both land, in order.
        s.reset(); s.key(VK_RETURN); s.key(VK_UP); s.key(VK_LEFT);
        s.step(); if(s.dir_!=0) return false;
        s.step(); if(s.dir_!=3) return false;
        // Hits score points with a quick-bore bonus; best survives a reset.
        s.reset(); s.key(VK_RETURN); for(int i=0;i<9;++i) s.step();
        if(s.points_ <= 100 || s.best_ < s.points_ || !s.bonus_) return false;
        { const int kept=s.best_; s.reset(); if(s.best_!=kept || s.points_!=0) return false; }
        s.preview(800); s.key('R');
        return !s.started_ && !s.cinematic_ && s.takeGroan()==-1 && s.score_==0 && s.body_.size()==4;
    }

    void preview(ULONGLONG age) {
        reset(); started_ = true;
        for(int i=0;i<9;++i) step();
        cinemaAge_ = std::min(age, cinematicMs - 1);
        sounded_ = cinemaAge_ >= contactMs;
        if(age>=cinematicMs) {
            cinematic_=false;impact_=0;person_=1;debt_=0;spawn();
        }
    }
    bool loadFaces() {
        for(int i=0;i<3;++i) {
            if(!faces_[i].empty()) continue;
            HBITMAP bitmap=static_cast<HBITMAP>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(501+i),IMAGE_BITMAP,0,0,LR_CREATEDIBSECTION));
            if(!bitmap) return false;
            BITMAPINFO info{};
            info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=128; info.bmiHeader.biHeight=-128;
            info.bmiHeader.biPlanes=1; info.bmiHeader.biBitCount=32; info.bmiHeader.biCompression=BI_RGB;
            faces_[i].resize(128*128);
            HDC dc=CreateCompatibleDC(nullptr);
            int lines=GetDIBits(dc,bitmap,0,128,faces_[i].data(),&info,DIB_RGB_COLORS);
            DeleteDC(dc); DeleteObject(bitmap);
            if(lines!=128) { faces_[i].clear(); return false; }
        }
        return true;
    }
private:
    void drawCinematic(HDC dc, int width, int height) const {
        const int age = int(cinemaAge_);
        const bool next = cinemaAge_ >= revealMs;
        const int person = next ? (person_ + 1) % 3 : person_;
        const int margin = std::max(28, height / 12);
        const double zoom = next ? .76 : .66 + .32 * std::min(1.0, age / double(contactMs));
        const int size = std::max(16, int(std::min(width - 40, height - margin * 2) * zoom));
        const int shake = !next && age >= 500 && age < 1700 ? ((age/45)%3-1)*2 : 0;
        const int x = (width-size)/2+shake, y = (height-size)/2;
        const ULONGLONG reaction = age > int(contactMs) ? age-contactMs : 0;
        drawPortrait(dc,person,x,y,size,size,!next && age>=int(contactMs),2200-std::min<ULONGLONG>(2200,reaction));
        if (!next) {
            // A spinning drill enters from the left and reaches the crosshair
            // at contactMs. Its teeth animate while the reaction plays.
            const int tipX = x + size/2 - int(size*.7*(1.0-std::min(1.0,age/double(contactMs))));
            const int tipY = y+size/7, thick=std::max(4,size/30);
            const int shaft = std::max(0,tipX-size/9);
            SelectObject(dc,GetStockObject(WHITE_BRUSH));
            Rectangle(dc,0,tipY-thick,shaft,tipY+thick);
            POINT tip[]={{tipX,tipY},{shaft,tipY-thick},{shaft,tipY+thick}};
            Polygon(dc,tip,3);
            SetDCPenColor(dc,RGB(0,0,0));
            for(int i=int((age/35)%12);i<shaft;i+=12) {
                MoveToEx(dc,i,tipY-thick,nullptr);LineTo(dc,i-7,tipY+thick);
            }
            SetDCPenColor(dc,RGB(255,255,255));
            SelectObject(dc,GetStockObject(BLACK_BRUSH));
        }
        HFONT title=CreateFontW(std::clamp(height/24,14,26),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,
            DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,NONANTIALIASED_QUALITY,FIXED_PITCH,L"Consolas");
        HGDIOBJ old=SelectObject(dc,title);
        RECT heading{8,8,width-8,margin};
        const std::wstring caption=next ? L"LEVEL "+std::to_wstring(score_+1)+L" / NEXT PERSON"
            : L"LEVEL "+std::to_wstring(score_)+L" / DRILL CAM";
        DrawTextW(dc,caption.c_str(),-1,&heading,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
        RECT footer{8,height-margin,width-8,height-12};
        DrawTextW(dc,paused_ ? L"PAUSED / SPACE TO RESUME" : next ? L"GET READY" : age<500 ? L"CONTACT..." : L"AUGHHH...",-1,&footer,DT_CENTER|DT_SINGLELINE|DT_VCENTER);
        RECT track{width/5,height-8,width*4/5,height-5};
        Rectangle(dc,track.left,track.top,track.right,track.bottom);
        track.right=track.left+int((track.right-track.left)*cinemaAge_/cinematicMs);
        FillRect(dc,&track,static_cast<HBRUSH>(GetStockObject(WHITE_BRUSH)));
        SelectObject(dc,old);DeleteObject(title);
    }
    void drawPortrait(HDC dc,int person,int x,int y,int w,int h,bool hit,ULONGLONG remaining) const {
        const auto& pixels=faces_[person];
        constexpr int bayer[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
        if(!pixels.empty() && w>0 && h>0) {
            std::vector<unsigned> rendered(static_cast<std::size_t>(w)*h);
            for(int py=0;py<h;++py) for(int px=0;px<w;++px) {
                unsigned v=pixels[(py*128/h)*128+px*128/w]&255;
                rendered[static_cast<std::size_t>(py)*w+px]=v>unsigned(bayer[py%4][px%4]*16+8)?0xffffffu:0;
            }
            BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
            info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;
            info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
            SetDIBitsToDevice(dc,x,y,w,h,0,0,0,h,rendered.data(),&info,DIB_RGB_COLORS);
        }
        if(!hit) return;
        // Landmark coordinates refer to the checked-in crops, not the full paintings.
        constexpr double eyes[3][4]={{.22,.17,.56,.17},{.17,.29,.46,.27},{.43,.42,.68,.41}};
        constexpr double mouths[3][2]={{.39,.52},{.34,.55},{.58,.68}};
        const int age=int(2200-std::min<ULONGLONG>(2200,remaining));
        const int rx=std::max(3,w/13),ry=std::max(2,h/24);
        SelectObject(dc,GetStockObject(WHITE_BRUSH));
        for(int i=0;i<2;++i) {
            int ex=x+int(eyes[person][i*2]*w),ey=y+int(eyes[person][i*2+1]*h);
            Ellipse(dc,ex-rx,ey-ry,ex+rx+1,ey+ry+1);
            SelectObject(dc,GetStockObject(BLACK_BRUSH));
            // Pupils travel upwards, disappearing beneath the upper lids.
            int pupil=ey-std::min(ry,age*ry/350);
            Ellipse(dc,ex-1,pupil,ex+2,pupil+std::max(2,ry/2));
            SelectObject(dc,GetStockObject(WHITE_BRUSH));
        }
        int mx=x+int(mouths[person][0]*w),my=y+int(mouths[person][1]*h);
        SelectObject(dc,GetStockObject(BLACK_BRUSH));
        Ellipse(dc,mx-w/12,my-h/28,mx+w/12,my+h/18);
        int drip=std::min(h/4,age*h/2400);
        MoveToEx(dc,mx+w/18,my+h/32,nullptr);LineTo(dc,mx+w/18,my+drip);
        SelectObject(dc,GetStockObject(WHITE_BRUSH));
        int dropY=my+drip+int((age/35)%std::max(1,h/10));
        Ellipse(dc,mx+w/18-2,dropY,mx+w/18+2,dropY+4);
        SelectObject(dc,GetStockObject(BLACK_BRUSH));
    }
    std::array<std::vector<unsigned>,3> faces_;
    static bool same(POINT a, POINT b) { return a.x==b.x && a.y==b.y; }
    struct Drip { POINT p; int life; };
    ULONGLONG tick() const { return std::max(65, 170 - score_ * 5); }
    int quickBonus() const { return std::max(0, 300 - 12 * sinceSpawn_); }
    void jamDrill() { dead_=true; jam_=true; deathAge_=0; prevBody_=body_; frac_=1; best_=std::max(best_,points_); }
    void spawn() {
        sinceSpawn_=0;
        // Find a clear face-sized area; scan all candidates before declaring victory.
        seed_ = seed_ * 1664525u + 1013904223u;
        constexpr int count=(cols-4)*(rows-5);
        for(int i=0;i<count;++i) {
            const int n=int(seed_%count+i)%count;
            POINT p{2+n%(cols-4),2+n/(cols-4)};
            bool clear=true;
            for(const auto& b:body_) if(std::abs(b.x-p.x)<=2 && b.y>=p.y-1 && b.y<=p.y+3) { clear=false; break; }
            if(clear) { target_=p; return; }
        }
        won_=true;
    }
    void step() {
        if(dead_ || won_ || cinematic_) return;
        if(!turns_.empty()) { dir_=turns_.front(); turns_.erase(turns_.begin()); }
        prevBody_=body_;
        ++sinceSpawn_;
        for(auto& d:trail_) --d.life;
        trail_.erase(std::remove_if(trail_.begin(),trail_.end(),[](const Drip& d){return d.life<=0;}),trail_.end());
        if(drips_>0) { trail_.push_back({body_.back(),14}); --drips_; }
        const int dx[]={0,1,0,-1}, dy[]={-1,0,1,0};
        POINT next{body_[0].x+dx[dir_],body_[0].y+dy[dir_]};
        const bool eat=same(next,target_);
        if(next.x<0 || next.y<0 || next.x>=cols || next.y>=rows) { jamDrill(); return; }
        for(std::size_t i=0;i<body_.size()-(eat?0:1);++i)
            if(same(next,body_[i])) { jamDrill(); return; }
        body_.insert(body_.begin(),next);
        if(eat) {
            ++score_; oldTarget_=target_; impact_=2200; cinematic_=true;
            const int quick=quickBonus();
            bonus_=quick>0; gain_=100*score_+quick; points_+=gain_; best_=std::max(best_,points_);
            gainAge_=0; drips_=12;
            cinemaAge_=0; sounded_=false; pendingGroan_=-1; debt_=0;
        }
        else body_.pop_back();
    }
    std::vector<POINT> body_;
    POINT target_{}, oldTarget_{};
    std::vector<POINT> prevBody_;
    std::vector<int> turns_;
    std::vector<Drip> trail_;
    int dir_=1, score_=0, person_=0, points_=0, best_=0, sinceSpawn_=0, gain_=0, drips_=0;
    bool bonus_=false, jam_=false;
    double frac_=1;
    ULONGLONG deathAge_=0, gainAge_=99999;
    unsigned seed_=17;
    bool cinematic_=false, sounded_=false;
    int pendingGroan_=-1;
    ULONGLONG cinemaAge_=0;
    bool started_=false, paused_=false, dead_=false, won_=false;
    ULONGLONG clock_=0, debt_=0, impact_=0;
};
} // namespace anaise
