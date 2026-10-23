// The New Gods: offline ritual, software 3D duels and an original tower chapter.
// No network, runtime assets, wall clock or filesystem access in this module.
#pragma once
#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

namespace anaise {
class NewGods {
public:
    enum class Stage { Dormant, Rupture, Revelation, Arena, Worthy, Tower, Ending, Dead };
    struct Input { double forward = 0, strafe = 0, turn = 0; bool guard = false; };
    bool active() const { return stage_ != Stage::Dormant; }
    bool paused() const { return paused_; }
    Stage stage() const { return stage_; }
    unsigned takeSounds() { const unsigned pending=sounds_;sounds_=0;return pending; }
    double hymnProgress() const { return std::clamp(age_ / 16.0, 0.0, 1.0); }

    void clickTitle(ULONGLONG now, int x, int y, int width=640, int height=400) {
        sounds_ |= 1u;
        clicks_.erase(std::remove_if(clicks_.begin(), clicks_.end(),
            [now](ULONGLONG t) { return now - t > 10000; }), clicks_.end());
        clicks_.push_back(now);
        // Each click sheds a small, deterministic constellation.
        for (int i = 0; i < 14; ++i) sparks_.push_back({x, y, now, i + int(clicks_.size()) * 17});
        if (sparks_.size() > 240) sparks_.erase(sparks_.begin(), sparks_.end() - 240);
        if (clicks_.size() >= 15) {
            stage_ = Stage::Rupture; age_ = stageAge_ = 0; last_ = now;
            originX_ = MulDiv(x,640,std::max(1,width)); originY_ = MulDiv(y,400,std::max(1,height)); clicks_.clear();
            sounds_ |= 32u;
        }
    }

    void quit() { *this = NewGods{}; }
    void pause() { if (active()) paused_ = true; }
    void key(int key) {
        if (key == VK_ESCAPE) { paused_ = !paused_; return; }
        if (paused_) {
            if (key == VK_RETURN) paused_ = false;
            else if (key == 'Q') quit();
            return;
        }
        if (stage_ == Stage::Dead && key == 'R') {
            hp_ = 100; stamina_ = 100; stage_ = deathStage_; stageAge_ = 0;
            x_ = 8; z_ = 12; angle_ = -pi / 2; swing_ = dodge_ = 0;
            if (stage_ == Stage::Arena) spawnBoss();
            else { floorTime_ = 0; spawnBoss(3); }
            message_ = L"The tower remembers. You may try again.";
            return;
        }
        if (stage_ == Stage::Worthy && (key == 'E' || key == VK_RETURN)) {
            stage_ = Stage::Tower; floor_ = 0; x_ = 8; z_ = 12; angle_ = -pi / 2;
            stageAge_ = floorTime_ = 0; hp_ = stamina_ = 100;
            message_ = L"Find the three climbers. E speaks; 1 swears an oath; 2 refuses.";
        } else if (stage_ == Stage::Arena || stage_ == Stage::Tower) {
            if (finalChoice_) {
                if (key == '1' || key == '2') {
                    abandoned_ = key == '2'; finalChoice_ = false;
                    stage_ = Stage::Ending; stageAge_ = 0;
                }
                return;
            }
            if (talking_ >= 0) {
                if (key == '1' || key == '2') {
                    if (key == '1') { allies_[talking_] = true; message_ = oathReply(talking_); }
                    else message_ = L"They turn away. The offer remains open.";
                    talking_ = -1;
                } else if (key == VK_BACK) talking_ = -1;
                return;
            }
            if (key == VK_SPACE || key == 'J') attack();
            else if (key == VK_SHIFT) evade();
            else if (key == 'E' || key == VK_RETURN) interact();
        } else if (stage_ == Stage::Ending && key == VK_RETURN) quit();
    }

    void update(ULONGLONG now, const Input& input) {
        if (!active()) return;
        double elapsed = std::min(0.12, double(now - last_) / 1000.0);
        last_ = now;
        if (paused_) return;
        // Bounded substeps keep telegraphs, collision and stamina stable.
        while (elapsed > 0.000001) {
            const double dt = std::min(0.01, elapsed);
            step(dt, input); elapsed -= dt;
        }
    }

    // Deterministic entry points for screenshots; live progression never uses them.
    void preview(Stage stage, ULONGLONG now, int view=0) {
        quit(); stage_ = stage; last_ = now; age_ = double(now) / 1000;
        stageAge_ = age_; originX_ = 530; originY_ = 55;
        if (stage == Stage::Arena) spawnBoss();
        if (stage == Stage::Tower) { allies_[0] = true; message_ = L"Mara has sworn to climb with you. Find Ione and Thales."; }
        switch(view){
            case 1:stage_=Stage::Rupture;break;
            case 2:stage_=Stage::Arena;bossKind_=1;spawnBoss();boss_.phase=1;break;
            case 3:stage_=Stage::Arena;bossKind_=2;spawnBoss();boss_.phase=1;boss_.markX=x_;boss_.markZ=z_;break;
            case 4:stage_=Stage::Worthy;bossKind_=3;break;
            case 5:stage_=Stage::Tower;floor_=1;allies_={true,true,true};message_=L"Remember: ASH, TIDE, STAR. E invokes a nearby seal.";break;
            case 6:stage_=Stage::Tower;floor_=2;allies_={true,true,true};spawnBoss(3);break;
            case 7:stage_=Stage::Tower;floor_=3;allies_={true,true,true};finalChoice_=true;break;
            case 8:stage_=Stage::Ending;floor_=3;allies_={true,true,true};break;
            case 9:stage_=Stage::Dead;hp_=0;break;
            case 10:stage_=Stage::Tower;talking_=1;break;
        }
    }

    void drawSparks(HDC dc, ULONGLONG now) const {
        const int old = SetROP2(dc, R2_NOT);
        for (const auto& p : sparks_) {
            const double t = double(now - p.born) / 1000;
            if (t < 0 || t > 0.8) continue;
            const double a = p.seed * 2.39996;
            const int x = p.x + int(std::cos(a) * (22 + p.seed % 43) * t);
            const int y = p.y + int(std::sin(a) * 65 * t + 55 * t * t);
            line(dc, x - 2, y, x + 3, y); line(dc, x, y - 2, x, y + 3);
        }
        SetROP2(dc, old);
    }

    void draw(HDC dc, int width, int height) const {
        // Work at a fixed scene resolution; nearest-neighbour scaling preserves 1 bit.
        constexpr int w = 640, h = 400;
        HDC frame = CreateCompatibleDC(dc);
        HBITMAP bitmap = CreateCompatibleBitmap(dc, w, h);
        auto old = SelectObject(frame, bitmap);
        rect(frame, 0, 0, w, h, false);
        SetBkMode(frame, TRANSPARENT);
        HFONT small = font(12, L"Consolas"), heading = font(30, L"Georgia"), large = font(52, L"Georgia");
        if (stage_ == Stage::Rupture) {
            const double t = stageAge_;
            for (int i = 0; i < 190; ++i) {
                const double a = i * 2.39996, speed = 15 + hash(i, 3) % 220;
                const int x = w / 2 + int(std::cos(a) * speed * t);
                const int y = h / 2 + int(std::sin(a) * speed * t + 20 * t * t);
                line(frame, x, y, x + int(std::cos(a) * (5 + t * 12)), y + int(std::sin(a) * (5 + t * 12)));
            }
            const std::wstring title = L"The New Gods";
            for (int i = 0; i < int(title.size()); ++i) {
                const double a = i * 2.39;
                text(frame, heading, title.substr(i, 1),
                    originX_ - 90 + i * 15 + int(std::cos(a) * t * 90),
                    originY_ - 12 + int(std::sin(a) * t * 130 + t * t * 12));
            }
            text(frame, small, L"FIFTEEN PRAYERS. ONE ANSWER.", 320, 352, true);
        } else if (stage_ == Stage::Revelation) {
            const double rise = std::clamp(stageAge_ / 3.5, 0.0, 1.0);
            cthulhu(frame, 320 + int(std::sin(stageAge_ * 0.8) * 4), 230 + int((1 - rise) * 270), 285, age_, 0, 0);
            rect(frame, 0, 0, 640, 42, false); rect(frame, 0, 325, 640, 400, false);
            // Silence, then an oversized impact that contracts into the caption.
            // The reveal contains exactly this one line of text.
            const double impact=stageAge_-3.6;
            if(impact>=0){
                const double settle=std::exp(-impact*9);
                HFONT title=font(30+int(22*settle),L"Georgia");
                const int shake=impact<.25?int(std::sin(impact*95)*5*(1-impact/.25)):0;
                text(frame,title,L"THE GODS WILL KNEEL.",320+shake,340-int(9*settle),true);
                DeleteObject(title);
            }
        } else {
            world(frame);
            if (stage_ == Stage::Arena || (stage_ == Stage::Tower && floor_ == 2)) drawBoss(frame);
            if (stage_ == Stage::Tower) drawTowerObjects(frame, small);
            sword(frame);
            hud(frame, small, heading);
            if (stage_ == Stage::Worthy) {
                panel(frame, 55, 240, 585, 370);
                text(frame, small, L"CTHULHU", 320, 253, true);
                text(frame, heading, L"You are worthy", 320, 272, true);
                text(frame, small, L"to enter the tower.", 320, 311, true);
                text(frame, small, L"E / ENTER  -  CROSS THE THRESHOLD", 320, 346, true);
            } else if (stage_ == Stage::Dead) {
                panel(frame, 95, 125, 545, 275);
                text(frame, large, L"UNWORTHY", 320, 145, true);
                text(frame, small, L"R  RETRY THIS TRIAL     ESC  PAUSE", 320, 238, true);
            } else if (stage_ == Stage::Ending) {
                panel(frame, 35, 85, 605, 310);
                text(frame, small, L"CHAPTER I / THE UNWRITTEN OATH", 320, 104, true);
                text(frame, heading, abandoned_ ? L"A throne for one." : L"No one climbs alone.", 320, 137, true);
                text(frame, small, abandoned_ ? L"You traded their names for a place above the gods." : L"You broke the contract. Your companions cross beside you.", 320, 190, true);
                text(frame, small, L"Behind the gate, Cthulhu closes one ancient eye.", 320, 215, true);
                text(frame, small, L"THE FIRST ASCENT IS COMPLETE", 320, 256, true);
                text(frame, small, L"ENTER  RETURN TO THE DESKTOP", 320, 285, true);
            }
            if (talking_ >= 0) dialogue(frame, small, heading);
        }
        if (paused_) {
            panel(frame, 120, 132, 520, 266);
            text(frame, heading, L"THE WITNESS WAITS", 320, 152, true);
            text(frame, small, L"ENTER / ESC  RESUME     Q  DESKTOP", 320, 211, true);
            text(frame, small, L"M  MUTE", 320, 237, true);
        }
        DeleteObject(small); DeleteObject(heading); DeleteObject(large);
        SetStretchBltMode(dc, COLORONCOLOR);
        StretchBlt(dc, 0, 0, width, height, frame, 0, 0, w, h, SRCCOPY);
        SelectObject(frame, old); DeleteObject(bitmap); DeleteDC(frame);
    }

    static bool selfTest();

private:
    static constexpr double pi = 3.14159265358979323846;
    struct Spark { int x, y; ULONGLONG born; int seed; };
    struct Boss { double x = 8, z = 7, hp = 100, clock = 0; int phase = 0; double markX = 8, markZ = 8; } boss_;
    struct Person { const wchar_t* name; double x, z; const wchar_t* role; };
    static constexpr Person people_[3] = {
        {L"MARA", 5, 9, L"THE SHIELD / +20 vitality"},
        {L"IONE", 10, 6, L"THE SEER / reveals the seal order"},
        {L"THALES", 5, 3, L"THE EXILE / faster stamina recovery"}
    };
    static const wchar_t* oathReply(int i) {
        return i == 0 ? L"Mara: Then I will stand between you and the blade." :
            i == 1 ? L"Ione: Remember the seals: ASH, TIDE, STAR. In that order." :
                     L"Thales: The last gate will ask for a name. Do not give it ours.";
    }
    static unsigned hash(int a, int b) { unsigned v = unsigned(a) * 374761393u + unsigned(b) * 668265263u; v = (v ^ (v >> 13)) * 1274126177u; return v ^ (v >> 16); }
    static HFONT font(int size, const wchar_t* face) {
        return CreateFontW(-size, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET,
            OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, NONANTIALIASED_QUALITY, DEFAULT_PITCH, face);
    }
    static void rect(HDC dc, int x, int y, int r, int b, bool white = true) {
        RECT box{x, y, r, b}; FillRect(dc, &box, static_cast<HBRUSH>(GetStockObject(white ? WHITE_BRUSH : BLACK_BRUSH)));
    }
    static void line(HDC dc, int x, int y, int x2, int y2, int thick = 1, bool white = true) {
        HPEN pen = CreatePen(PS_SOLID, thick, white ? RGB(255,255,255) : RGB(0,0,0));
        auto old = SelectObject(dc, pen); MoveToEx(dc, x, y, nullptr); LineTo(dc, x2, y2);
        SelectObject(dc, old); DeleteObject(pen);
    }
    static void ellipse(HDC dc, int x, int y, int rx, int ry, bool white = true) {
        auto brush = SelectObject(dc, GetStockObject(white ? WHITE_BRUSH : BLACK_BRUSH));
        auto pen = SelectObject(dc, GetStockObject(NULL_PEN));
        Ellipse(dc, x-rx, y-ry, x+rx, y+ry); SelectObject(dc, brush); SelectObject(dc, pen);
    }
    static void poly(HDC dc, std::initializer_list<POINT> points, bool white = true) {
        auto brush = SelectObject(dc, GetStockObject(white ? WHITE_BRUSH : BLACK_BRUSH));
        auto pen = SelectObject(dc, GetStockObject(white ? WHITE_PEN : BLACK_PEN));
        Polygon(dc, points.begin(), int(points.size())); SelectObject(dc, brush); SelectObject(dc, pen);
    }
    static void text(HDC dc, HFONT f, const std::wstring& value, int x, int y, bool center = false) {
        auto old = SelectObject(dc, f); SetTextColor(dc, RGB(255,255,255));
        SetTextAlign(dc, (center ? TA_CENTER : TA_LEFT) | TA_TOP);
        TextOutW(dc, x, y, value.c_str(), int(value.size())); SelectObject(dc, old);
    }
    static void panel(HDC dc, int l, int t, int r, int b) {
        rect(dc,l,t,r,b); rect(dc,l+1,t+1,r-1,b-1,false);
    }
    static bool dither(int x, int y, double light) {
        static constexpr int b[4][4]={{0,8,2,10},{12,4,14,6},{3,11,1,9},{15,7,13,5}};
        return light * 16 > b[y&3][x&3] + 0.5;
    }

    void spawnBoss(int kind = -1) {
        if (kind >= 0) bossKind_ = kind;
        boss_ = Boss{}; boss_.hp = bossKind_ == 2 ? 120 : 100;
        boss_.clock = -1.5; // Breathing room on arrival.
        x_ = 8; z_ = 11; angle_ = -pi / 2;
        message_ = bossKind_ == 0 ? L"ARES: Your prayers have edges. Show me." :
            bossKind_ == 1 ? L"ATHENA: A wild blade never finds an opening." :
            bossKind_ == 2 ? L"ZEUS: Even the sky must answer to the witness." :
                            L"THE HOLLOW WARDEN: Endure, then strike.";
    }
    bool solid(double x, double z) const {
        if (x < 1 || z < 1 || x > 15 || z > 15) return true;
        for (int a : {3, 12}) for (int b : {3, 12})
            if (x >= a && x <= a + 1 && z >= b && z <= b + 1) return true;
        return false;
    }
    bool free(double x, double z) const {
        constexpr double r = .22;
        return !solid(x-r,z-r) && !solid(x+r,z-r) && !solid(x-r,z+r) && !solid(x+r,z+r);
    }
    bool visible(double x, double z) const {
        const double d = std::hypot(x-x_,z-z_);
        for (double t = .15; t < d; t += .12)
            if (solid(x_+(x-x_)*t/d,z_+(z-z_)*t/d)) return false;
        return true;
    }
    bool fighting() const { return stage_ == Stage::Arena || (stage_ == Stage::Tower && floor_ == 2 && !floorClear_); }
    double facingError() const { return std::remainder(std::atan2(boss_.z-z_,boss_.x-x_) - angle_, 2*pi); }
    void damage(double amount) {
        if (dodge_ > 0.13) { message_ = L"EVADED"; return; }
        if (guard_ && stamina_ >= 18 && std::abs(facingError()) < 1.25) {
            stamina_ -= 18;
            if (guardAge_ < .24) { boss_.phase = 2; boss_.clock = -.45; sounds_ |= 8u; message_ = L"PARRY / the god is exposed"; }
            else { hp_ -= amount * .18; message_ = L"BLOCK / stamina -18"; }
        } else { hp_ -= amount; hurt_ = .35; sounds_ |= 4u; message_ = L"WOUNDED / read the wind-up. Dodge or face the blade and guard."; }
        if (hp_ <= 0) { deathStage_ = stage_; stage_ = Stage::Dead; hp_ = 0; }
    }
    void attack() {
        if (!fighting() || swing_ > 0 || stamina_ < 22) return;
        stamina_ -= 22; swing_ = .46;
        sounds_ |= 2u;
        if (std::hypot(boss_.x-x_,boss_.z-z_) > 2.5 || std::abs(facingError()) > .72 || !visible(boss_.x,boss_.z)) {
            message_ = L"AIR / close the distance and face your opponent."; return;
        }
        const bool shield = bossKind_ == 1 && boss_.phase != 2;
        boss_.hp -= shield ? 4 : boss_.phase == 2 ? 32 : 18;
        hit_ = .18;
        sounds_ |= shield ? 8u : 4u;
        message_ = shield ? L"DEFLECTED / Athena's shield opens after her thrust." : L"STEEL FINDS A GOD";
        if (boss_.hp <= 0) {
            if (stage_ == Stage::Tower) { floorClear_ = true; message_ = L"The Warden falls. The north gate is unsealed."; }
            else if (++bossKind_ < 3) { hp_ = std::min(maxHp(), hp_+28); stamina_ = 100; spawnBoss(); }
            else { stage_ = Stage::Worthy; stageAge_ = 0; }
        }
    }
    void evade() {
        if (!fighting() || stamina_ < 30 || dodge_ > 0) return;
        stamina_ -= 30; dodge_ = .52;
        // Dodge backward, with collision resolved in substeps by step().
    }
    double maxHp() const { return allies_[0] ? 120 : 100; }
    void step(double dt, const Input& input) {
        const double previousAge=stageAge_;
        age_ += dt; stageAge_ += dt;
        if(stage_==Stage::Revelation&&previousAge<3.6&&stageAge_>=3.6)sounds_|=32u;
        if (stage_ == Stage::Rupture && stageAge_ >= 2.8) { stage_ = Stage::Revelation; stageAge_ = 0; sounds_|=16u; }
        if (stage_ == Stage::Revelation && stageAge_ >= 7.5) { stage_ = Stage::Arena; stageAge_ = 0; spawnBoss(); }
        if (stage_ != Stage::Arena && stage_ != Stage::Tower) return;
        if (talking_ >= 0 || finalChoice_) return;
        if (stage_ == Stage::Tower) floorTime_ += dt;
        swing_ = std::max(0.0,swing_-dt); hit_ = std::max(0.0,hit_-dt); hurt_ = std::max(0.0,hurt_-dt);
        dodge_ = std::max(0.0,dodge_-dt);
        guard_ = input.guard;
        guardAge_ = guard_ ? guardAge_+dt : 0;
        stamina_ = std::min(100.0,stamina_+dt*(allies_[2]?32:23)*(guard_?.25:1));
        angle_ = std::remainder(angle_+input.turn*dt*1.8,2*pi);
        const double length = std::max(1.0,std::hypot(input.forward,input.strafe));
        const double speed = (guard_?1.4:3.2)*dt;
        const double f = dodge_ > .13 ? -dt*8 : input.forward/length*speed;
        const double s = input.strafe/length*speed;
        const double nx = x_+std::cos(angle_)*f-std::sin(angle_)*s;
        const double nz = z_+std::sin(angle_)*f+std::cos(angle_)*s;
        if (free(nx,z_)) x_=nx;
        if (free(x_,nz)) z_=nz;
        if (!fighting()) return;
        const double dx=x_-boss_.x, dz=z_-boss_.z, d=std::hypot(dx,dz);
        boss_.clock += dt;
        if (boss_.phase == 0) {
            if (d > 1.85) {
                const double move=dt*(bossKind_==0?2.0:bossKind_==1?1.6:1.3)/d;
                if (free(boss_.x+dx*move,boss_.z)) boss_.x+=dx*move;
                if (free(boss_.x,boss_.z+dz*move)) boss_.z+=dz*move;
            }
            if (boss_.clock > (bossKind_==0?.8:1.3) && (d<2.9 || bossKind_==2)) {
                boss_.phase=1; boss_.clock=0; boss_.markX=x_; boss_.markZ=z_;
            }
        } else if (boss_.phase == 1) {
            const double windup = bossKind_==0?.72:bossKind_==1?.95:1.35;
            if (boss_.clock >= windup) {
                const bool lightning = bossKind_==2;
                if (lightning ? std::hypot(x_-boss_.markX,z_-boss_.markZ)<1.45 : d<2.8 && visible(boss_.x,boss_.z)) damage(lightning?32:bossKind_==0?24:28);
                boss_.phase=2; boss_.clock=0;
            }
        } else if (boss_.clock > (bossKind_==1?1.2:.95)) { boss_.phase=0; boss_.clock=0; }
    }

    void nextFloor() {
        ++floor_; floorTime_=0; floorClear_=false; seal_=0; x_=8; z_=12; angle_=-pi/2;
        hp_=std::min(maxHp(),hp_+30); stamina_=100;
        if (floor_==1) message_=L"THE TEST OF MEMORY / Three seals. Ash, tide, star. E invokes a nearby seal.";
        if (floor_==2) { spawnBoss(3); message_=L"THE TEST OF ENDURANCE / Defeat the Hollow Warden with your borrowed strength."; }
        if (floor_==3) message_=L"THE PRICE OF ASCENT / Approach the north gate. E reads the contract.";
    }
    void interact() {
        if (stage_!=Stage::Tower) return;
        if (floor_==0) {
            for (int i=0;i<3;++i) if (std::hypot(x_-people_[i].x,z_-people_[i].z)<2 && visible(people_[i].x,people_[i].z)) { talking_=i; return; }
        }
        if (floor_==1 && !floorClear_) {
            static constexpr double sx[3]={5,11,8}, sz[3]={6,6,3};
            for (int i=0;i<3;++i) if (std::hypot(x_-sx[i],z_-sz[i])<1.9) {
                if (i==seal_) { ++seal_; message_=seal_==3?L"The remembered names open the north gate.":L"The seal answers. Find the next."; }
                else { seal_=0; hp_=std::max(1.0,hp_-10); message_=L"The tower rejects the order. Begin again: ASH, TIDE, STAR."; }
                floorClear_=seal_==3; return;
            }
        }
        if (std::hypot(x_-8,z_-1.8)<2.2) {
            if (floor_==0 && int(allies_[0])+int(allies_[1])+int(allies_[2])>=2) nextFloor();
            else if ((floor_==1||floor_==2)&&floorClear_) nextFloor();
            else if (floor_==3) finalChoice_=true;
            else message_=floor_==0?L"The gate demands at least two sworn companions.":L"The gate is sealed. Complete the trial.";
        } else message_=L"E: speak to a climber, invoke a seal, or open the north gate.";
    }

    // Original silhouette: wings, plated brow, tentacles and pupils that follow
    // the player in world space. It is drawn behind the arena's masonry.
    static void cthulhu(HDC dc,int cx,int cy,int size,double t,double gazeX,double gazeY) {
        auto X=[&](double x){return cx+int(x*size);};
        auto Y=[&](double y){return cy+int(y*size);};
        for (int side : {-1,1}) {
            poly(dc,{{X(side*.15),Y(.1)},{X(side*.92),Y(-.54)},{X(side*.79),Y(-.02)},
                {X(side*.95),Y(.22)},{X(side*.57),Y(.16)},{X(side*.46),Y(.55)},{X(side*.18),Y(.3)}});
            for(int i=0;i<8;++i) line(dc,X(side*.18),Y(.17),X(side*(.46+i*.063)),Y(-.44+i*.07),1,false);
        }
        ellipse(dc,cx,Y(.3),int(size*.31),int(size*.4));
        ellipse(dc,cx,Y(-.20),int(size*.27),int(size*.33));
        // Model the skull with binary stipple rather than a flat white mask.
        for(int py=Y(-.52);py<Y(.08);++py) for(int px=X(-.265);px<X(.265);++px){
            const double nx=double(px-cx)/(size*.27),ny=(double(py-cy)/size+.20)/.33;
            const double sphere=1-nx*nx-ny*ny;
            if(sphere<0)continue;
            const double ridges=std::sin(nx*32+std::sin(ny*8)*2)*.06;
            const double light=std::clamp(.14+.56*std::sqrt(sphere)-.20*nx+ridges,.04,.8);
            SetPixelV(dc,px,py,dither(px,py,light)?RGB(255,255,255):RGB(0,0,0));
        }
        // Horned crown and deeply cut facial plates.
        for(int side:{-1,1}){
            poly(dc,{{X(side*.13),Y(-.43)},{X(side*.28),Y(-.65)},{X(side*.23),Y(-.29)}},false);
            line(dc,X(side*.16),Y(-.43),X(side*.27),Y(-.60));
            for(int i=0;i<6;++i)line(dc,X(side*(.04+i*.028)),Y(-.42+i*.012),X(side*(.02+i*.025)),Y(-.27+i*.014),1,false);
        }
        for(int i=0;i<11;++i) {
            const double root=(i-5)*.039;
            int lx=X(root), ly=Y(-.04);
            for(int j=1;j<=28;++j) {
                const double a=j/28.0;
                const double xx=root+std::sin(a*5+t*.65+i*.8)*a*.16;
                const double yy=-.04+a*(.49+.08*std::sin(i));
                int xxp=X(xx), yyp=Y(yy);
                line(dc,lx,ly,xxp,yyp,std::max(2,int(size*.025*(1-a))),true);
                line(dc,lx+2,ly,xxp+2,yyp,1,false);
                lx=xxp;ly=yyp;
            }
        }
        for(int side:{-1,1}) {
            const int ex=X(side*.115), ey=Y(-.19);
            ellipse(dc,ex,ey,int(size*.079),int(size*.039),false);
            poly(dc,{{ex-int(size*.065),ey-int(size*.025)},{ex+int(size*.06),ey-int(size*.012)},
                {ex+int(size*.04),ey+int(size*.02)},{ex-int(size*.045),ey+int(size*.01)}});
            ellipse(dc,ex+int(std::clamp(gazeX,-1.0,1.0)*size*.028),ey+int(std::clamp(gazeY,-1.0,1.0)*size*.01),std::max(2,int(size*.009)),std::max(3,int(size*.024)),false);
            line(dc,X(side*.04),Y(-.25),X(side*.20),Y(-.31),std::max(1,size/90),false);
        }
        // Claws embrace the arena rim.
        for(int side:{-1,1}) for(int i=0;i<4;++i) {
            const int x=X(side*(.31+i*.052));
            ellipse(dc,x,Y(.45),int(size*.034),int(size*.12));
            line(dc,x,Y(.40),x+side*3,Y(.56),1,false);
        }
    }

    void world(HDC dc) const {
        constexpr int w=640,h=400, horizon=180;
        const bool tower=stage_==Stage::Tower||stage_==Stage::Ending||(stage_==Stage::Dead&&deathStage_==Stage::Tower);
        // A physical bearing keeps the witness in the north sky as you turn.
        const double bearing=std::remainder(-pi/2-angle_,2*pi);
        std::vector<std::uint32_t> pixels(w*h,0);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
        info.bmiHeader.biWidth=w;info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;
        void* skyBits=nullptr;
        HBITMAP sky=CreateDIBSection(dc,&info,DIB_RGB_COLORS,&skyBits,nullptr,0);
        if(sky&&skyBits){
            HDC skyDc=CreateCompatibleDC(dc);auto old=SelectObject(skyDc,sky);
            std::memset(skyBits,0,w*h*4);
            if(!tower&&std::abs(bearing)<1.65)
                cthulhu(skyDc,320+int(std::tan(std::clamp(bearing,-1.1,1.1))*300),185,180,age_,(x_-8)/7,(z_-8)/7);
            GdiFlush();std::memcpy(pixels.data(),skyBits,w*h*4);
            SelectObject(skyDc,old);DeleteDC(skyDc);DeleteObject(sky);
        }else if(sky)DeleteObject(sky);
        std::array<double,w> depth{};
        // Floor-cast each pixel through the camera plane into a tiled arena.
        for(int y=horizon+1;y<h;++y) {
            const double dist=200.0/(y-horizon);
            for(int x=0;x<w;++x) {
                const double plane=(x-320.0)/300;
                const double wx=x_+dist*(std::cos(angle_)-std::sin(angle_)*plane);
                const double wz=z_+dist*(std::sin(angle_)+std::cos(angle_)*plane);
                const double fx=wx-std::floor(wx),fz=wz-std::floor(wz);
                const bool grout=fx<.035||fz<.035;
                const bool ring=std::abs(std::hypot(wx-8,wz-8)-5)<.05;
                const double light=ring?.8:grout?.05:((int(std::floor(wx))+int(std::floor(wz)))&1)?.24:.37;
                pixels[y*w+x]=dither(x,y,light*std::min(1.0,7.0/dist))?0xffffff:0;
            }
        }
        for(int x=0;x<w;++x) {
            const double plane=(x-320.0)/300;
            const double dx=std::cos(angle_)-std::sin(angle_)*plane;
            const double dz=std::sin(angle_)+std::cos(angle_)*plane;
            // DDA: exact tile boundary intersections, without ray marching gaps.
            int tx=int(x_),tz=int(z_); const int sx=dx>0?1:-1,sz=dz>0?1:-1;
            const double ddx=1/std::max(.00001,std::abs(dx)),ddz=1/std::max(.00001,std::abs(dz));
            double ax=(dx>0?tx+1-x_:x_-tx)*ddx,az=(dz>0?tz+1-z_:z_-tz)*ddz;
            bool side=false; double dist=1;
            for(int i=0;i<48;++i) {
                if(ax<az){dist=ax;ax+=ddx;tx+=sx;side=false;}else{dist=az;az+=ddz;tz+=sz;side=true;}
                if(solid(tx+.5,tz+.5))break;
            }
            depth[x]=dist;
            const bool boundary=tx<=0||tz<=0||tx>=15||tz>=15;
            const double wallH=boundary?(tower?3.5:.85):3.7;
            const int top=std::max(0,int(horizon-(wallH-1)*200/std::max(.15,dist)));
            const int bottom=std::min(h,int(horizon+200/std::max(.15,dist)));
            const double u=(side?x_+dx*dist:z_+dz*dist); const double frac=u-std::floor(u);
            for(int y=top;y<bottom;++y) {
                const int v=int((y-horizon)*dist/20);
                const bool mortar=(std::abs(v)%12==0)||std::fmod(frac+((v/12)&1)*.5,1.0)<.025;
                const double light=(mortar?.035:side?.29:.52)*std::min(1.0,8/dist);
                pixels[y*w+x]=dither(x,y,light)?0xffffff:0;
            }
        }
        SetDIBitsToDevice(dc,0,0,w,h,0,0,0,h,pixels.data(),&info,DIB_RGB_COLORS);
        if(tower) {
            // North doorway projected into the same camera as the walls.
            int sx,sy,scale;
            if(project(8,1.12,2,sx,sy,scale)) {
                const int saved=SaveDC(dc); clipSprite(dc,8,1.12,scale);
                panel(dc,sx-scale/2,sy-scale*2,sx+scale/2,sy+scale/2);
                for(int i=-2;i<=2;++i)line(dc,sx+i*scale/7,sy-scale*2,sx+i*scale/7,sy+scale/2);
                RestoreDC(dc,saved);
            }
        }
        // Depth is recomputed locally for sprites via their line of sight.
    }
    bool project(double wx,double wz,double altitude,int& sx,int& sy,int& scale) const {
        const double dx=wx-x_,dz=wz-z_;
        const double depth=dx*std::cos(angle_)+dz*std::sin(angle_);
        if(depth<.25)return false;
        sx=320+int((-dx*std::sin(angle_)+dz*std::cos(angle_))*300/depth);
        sy=180+int((1-altitude)*200/depth);scale=int(200/depth);
        return sx>-300&&sx<940;
    }
    void clipSprite(HDC dc,double wx,double wz,int) const {
        // Occlusion per screen column, evaluated against walls and pillars.
        int sx,sy,sc; if(!project(wx,wz,0,sx,sy,sc))return;
        for(int x=std::max(0,sx-sc);x<std::min(640,sx+sc);++x) {
            const double dx=std::cos(angle_)-std::sin(angle_)*(x-320.0)/300;
            const double dz=std::sin(angle_)+std::cos(angle_)*(x-320.0)/300;
            const double depth=(wx-x_)*std::cos(angle_)+(wz-z_)*std::sin(angle_);
            for(double d=.12;d<depth-.2;d+=.12)if(solid(x_+dx*d,z_+dz*d)){ExcludeClipRect(dc,x,0,x+1,400);break;}
        }
    }
    void drawBoss(HDC dc) const {
        if(stage_==Stage::Tower&&floorClear_)return;
        int x,y,s; if(!project(boss_.x,boss_.z,0,x,y,s))return;
        s=std::min(s,260); const int saved=SaveDC(dc);clipSprite(dc,boss_.x,boss_.z,s);
        const bool wind=boss_.phase==1;
        const int headY=y-int(s*1.75), shoulder=y-int(s*1.4), hip=y-int(s*.65);
        // Armoured silhouette, segmented cuirass, skirt, greaves and crest.
        ellipse(dc,x,headY,int(s*.21),int(s*.27));
        rect(dc,x-int(s*.19),headY-2,x+int(s*.19),headY+4,false);
        poly(dc,{{x-int(s*.34),shoulder},{x+int(s*.34),shoulder},{x+int(s*.22),hip},{x-int(s*.22),hip}});
        for(int i=0;i<5;++i)line(dc,x-int(s*.23),shoulder+i*s/8,x+int(s*.23),shoulder+i*s/8,1,false);
        poly(dc,{{x-int(s*.22),hip},{x+int(s*.22),hip},{x+int(s*.34),hip+s/3},{x-int(s*.34),hip+s/3}});
        line(dc,x-s/7,hip+s/4,x-s/5,y,std::max(2,s/8));
        line(dc,x+s/7,hip+s/4,x+s/5,y,std::max(2,s/8));
        if(bossKind_==0||bossKind_==1) {
            poly(dc,{{x-s/5,headY-s/5},{x-s/6,headY-s/2},{x+s/4,headY-s/2},{x+s/5,headY-s/5}});
            for(int i=0;i<5;++i)line(dc,x-s/6+i*s/12,headY-s/2,x-s/6+i*s/12,headY-s/3,1,false);
        }
        const int handX=x+s/2, handY=wind?headY-s/2:hip;
        line(dc,x+s/4,shoulder,handX,handY,std::max(2,s/9));
        if(bossKind_==2) {
            for(int i=0;i<5;++i) line(dc,handX+(i%2)*s/4,handY-i*s/4,handX+((i+1)%2)*s/4,handY-(i+1)*s/4,2);
            for(int i=0;i<4;++i)line(dc,x-s/6+i*s/10,headY+s/6,x+(i-2)*s/12,shoulder+s/7,2);
        } else {
            line(dc,handX,handY+s/3,handX+(wind?-s/3:s/5),handY-s,std::max(2,s/25));
            line(dc,handX-s/6,handY,handX+s/6,handY,std::max(2,s/20));
        }
        if(bossKind_==1) {
            ellipse(dc,x-s/3,shoulder+s/3,s/3,s/2);
            ellipse(dc,x-s/3,shoulder+s/3,s/4,int(s*.4),false);
            ellipse(dc,x-s/3,shoulder+s/3,s/12,s/10);
        } else line(dc,x-s/4,shoulder,x-s/2,hip,std::max(2,s/10));
        if(hit_>0)for(int i=0;i<9;++i)line(dc,x,y-s,x+int(std::cos(i*2.4)*s),y-s+int(std::sin(i*2.4)*s));
        RestoreDC(dc,saved);
        if(bossKind_==2&&wind) {
            textSmall(dc,L"LIGHTNING / LEAVE THE MARKED GROUND",320,308);
            int a,b,c;if(project(boss_.markX,boss_.markZ,0,a,b,c)){
                for(int i=0;i<24;++i)line(dc,a+int(std::cos(i*pi/12)*c),b+int(std::sin(i*pi/12)*c*.25),a+int(std::cos((i+1)*pi/12)*c),b+int(std::sin((i+1)*pi/12)*c*.25));
            }
        }
    }
    static void textSmall(HDC dc,const wchar_t* v,int x,int y){HFONT f=font(12,L"Consolas");text(dc,f,v,x,y,true);DeleteObject(f);}
    void sword(HDC dc) const {
        if(stage_==Stage::Worthy||stage_==Stage::Ending)return;
        const double arc=swing_>0?std::sin((.46-swing_)/.46*pi):0;
        const int dx=int(-arc*220),dy=int(-arc*50)+(guard_?-80:0);
        poly(dc,{{490+dx,400+dy},{467+dx,351+dy},{489+dx,334+dy},{530+dx,400+dy}});
        poly(dc,{{482+dx,343+dy},{468+dx,326+dy},{424+dx,175+dy},{440+dx,184+dy},{500+dx,328+dy}});
        line(dc,434+dx,191+dy,483+dx,328+dy,2,false);
        line(dc,453+dx,346+dy,512+dx,320+dy,7);
        if(guard_){panel(dc,160,277,272,400);line(dc,173,290,260,385,3);}
        line(dc,314,199,326,199);line(dc,320,193,320,205);
        if(hurt_>0){for(int i=0;i<12;++i){line(dc,i*55,0,i*55+25,18,2);line(dc,i*55,399,i*55+25,380,2);}}
    }
    void drawTowerObjects(HDC dc,HFONT f) const {
        if(floor_==0) {
            std::array<int,3> order{0,1,2};
            std::sort(order.begin(),order.end(),[this](int a,int b){return std::hypot(x_-people_[a].x,z_-people_[a].z)>std::hypot(x_-people_[b].x,z_-people_[b].z);});
            for(int i:order){int x,y,s;const auto& p=people_[i];if(!project(p.x,p.z,0,x,y,s))continue;
                const int saved=SaveDC(dc);clipSprite(dc,p.x,p.z,s);
                ellipse(dc,x,y-s*3/2,s/7,s/5);poly(dc,{{x-s/5,y-s*5/4},{x+s/5,y-s*5/4},{x+s/3,y},{x-s/3,y}});
                line(dc,x,y-s,x,y-4,2,false);line(dc,x-s/5,y-s,x-s/2,y-s/3,3);
                text(dc,f,std::wstring(p.name)+(allies_[i]?L" / SWORN":L" / E"),x,y-s*2,true);RestoreDC(dc,saved);
            }
        } else if(floor_==1) {
            static const wchar_t* names[3]={L"ASH",L"TIDE",L"STAR"};
            const double xx[3]={5,11,8},zz[3]={6,6,3};
            for(int i=0;i<3;++i){int x,y,s;if(!project(xx[i],zz[i],0,x,y,s))continue;
                const int saved=SaveDC(dc);clipSprite(dc,xx[i],zz[i],s);
                panel(dc,x-s/4,y-s,x+s/4,y);text(dc,f,names[i],x,y-s-20,true);
                if(i<seal_)ellipse(dc,x,y-s/2,s/8,s/8);
                else {line(dc,x-s/7,y-s/2,x+s/7,y-s/2);line(dc,x,y-s*3/4,x,y-s/4);}
                RestoreDC(dc,saved);
            }
        }
    }
    void hud(HDC dc,HFONT small,HFONT heading) const {
        rect(dc,0,0,640,74,false);
        const bool arena=stage_==Stage::Arena||stage_==Stage::Worthy||(stage_==Stage::Dead&&deathStage_==Stage::Arena);
        const wchar_t* names[4]={L"ARES / THE RED OATH",L"ATHENA / THE UNBROKEN AEGIS",L"ZEUS / THE LAST THUNDER",L"THE HOLLOW WARDEN"};
        text(dc,small,arena?L"THE WITNESS' ARENA / TRIAL "+std::to_wstring(std::min(3,bossKind_+1))+L" OF 3":L"THE UNWRITTEN TOWER / FLOOR "+std::to_wstring(floor_+1)+L" OF 4",15,10);
        text(dc,small,L"M MUTE   ESC PAUSE",490,10);
        text(dc,small,L"VITALITY "+std::to_wstring(int(hp_)),15,32);
        rect(dc,15,51,165,56,false);rect(dc,15,51,15+int(150*std::max(0.0,hp_)/maxHp()),56);
        text(dc,small,L"RESOLVE "+std::to_wstring(int(stamina_)),195,32);
        rect(dc,195,51,195+int(stamina_*1.5),56);
        if(fighting()) {
            rect(dc,10,77,255,116,false);
            text(dc,small,names[std::min(3,bossKind_)],16,80);
            panel(dc,16,101,245,108);rect(dc,18,103,18+int(225*std::max(0.0,boss_.hp)/(bossKind_==2?120:100)),106);
            if(boss_.phase==1&&bossKind_!=2){panel(dc,175,306,465,328);text(dc,small,L"WIND-UP / PARRY, GUARD OR DODGE",320,311,true);}
            if(boss_.phase==2){panel(dc,214,306,426,328);text(dc,small,L"OPENING / STRIKE NOW",320,311,true);}
        } else if(stage_==Stage::Tower) {
            const wchar_t* titles[4]={L"I / THE OATHKEEPERS",L"II / THE NAMES BELOW",L"III / BORROWED STRENGTH",L"IV / THE PRICE OF ASCENT"};
            text(dc,small,titles[floor_],320,80,true);
            std::wstring objective=floor_==0?L"Recruit 2 climbers. Find the north gate (E).":floor_==1?L"Invoke ASH, TIDE, STAR. Then the north gate (E).":floor_==2?L"Defeat the Warden. Then the north gate (E).":L"Reach the north gate (E). Choose who will ascend.";
            text(dc,small,objective,320,99,true);
            text(dc,small,L"SWORN: "+std::to_wstring(int(allies_[0])+int(allies_[1])+int(allies_[2]))+L" / 3",15,65);
        }
        rect(dc,0,348,640,400,false);line(dc,12,348,628,348);
        // Wrap notices explicitly, avoiding clipped dialogue at the scene edge.
        RECT msg{16,354,624,384};auto old=SelectObject(dc,small);SetTextAlign(dc,TA_LEFT|TA_TOP);
        DrawTextW(dc,message_.c_str(),-1,&msg,DT_WORDBREAK|DT_CENTER|DT_NOPREFIX);SelectObject(dc,old);
        text(dc,small,L"WASD MOVE / ARROWS TURN / J or CLICK SWORD / K GUARD / SHIFT DODGE / E INTERACT",320,386,true);
        // North marker: direction is readable without mouse capture.
        const double n=std::remainder(-pi/2-angle_,2*pi);
        text(dc,small,L"N",610,42,true);line(dc,610,65,610+int(std::sin(n)*10),65-int(std::cos(n)*10));
        if(finalChoice_){panel(dc,55,167,585,310);text(dc,heading,L"ONE NAME MAY PASS",320,180,true);
            text(dc,small,L"The contract asks you to surrender your companions.",320,227,true);
            text(dc,small,L"1  BREAK THE CONTRACT / ASCEND TOGETHER",320,259,true);
            text(dc,small,L"2  ACCEPT THE THRONE / LEAVE THEM BELOW",320,281,true);}
    }
    void dialogue(HDC dc,HFONT small,HFONT heading) const {
        panel(dc,35,170,605,340);text(dc,heading,people_[talking_].name,55,183);
        const wchar_t* lines[3]={L"My last party left me below. Swear that no one climbs alone.",L"I remember every name the tower erases. Will you carry mine?",L"I once served the gate. It eats the names of those you abandon."};
        text(dc,small,people_[talking_].role,55,225);text(dc,small,lines[talking_],55,250);
        text(dc,small,L"1  SWEAR THE OATH     2  WALK AWAY     BACKSPACE  CLOSE",55,300);
    }

    Stage stage_=Stage::Dormant, deathStage_=Stage::Arena;
    std::vector<ULONGLONG> clicks_;
    std::vector<Spark> sparks_;
    ULONGLONG last_=0;
    double age_=0,stageAge_=0,x_=8,z_=11,angle_=-pi/2;
    double hp_=100,stamina_=100,swing_=0,dodge_=0,hit_=0,hurt_=0,guardAge_=0,floorTime_=0;
    int bossKind_=0,floor_=0,seal_=0,talking_=-1,originX_=0,originY_=0;
    bool paused_=false,guard_=false,floorClear_=false,finalChoice_=false,abandoned_=false;
    std::array<bool,3> allies_{};
    std::wstring message_;
    unsigned sounds_=0;
};
inline bool NewGods::selfTest() {
    NewGods g;
    for(int i=0;i<14;++i)g.clickTitle(i*500,500,70);
    if(g.active())return false;
    g.clickTitle(10001,500,70); // First click is now outside the rolling window.
    if(g.active())return false;
    g.clickTitle(10002,500,70);
    if(g.stage_!=Stage::Rupture)return false;
    for(ULONGLONG t=10042;t<21000;t+=40)g.update(t,{});
    if(g.stage_!=Stage::Arena)return false;
    // Simulation freezes on focus loss/pause, without catching up on resume.
    const double x=g.x_,clock=g.boss_.clock,age=g.age_;
    g.pause();g.update(22000,{1,0,1,true});
    if(g.x_!=x||g.boss_.clock!=clock||g.age_!=age)return false;
    g.key(VK_RETURN);g.update(22040,{1,0,0,false});
    if(g.x_!=x||g.z_>=11)return false; // Facing north; W moves along negative Z.
    // Collision, facing, range, stamina and recovery all affect real attacks.
    g.x_=8;g.z_=9;g.boss_.x=8;g.boss_.z=7;
    g.angle_=pi/2;g.key('J');if(g.boss_.hp!=100)return false;
    g.swing_=0;g.stamina_=100;g.angle_=-pi/2;g.boss_.phase=2;
    g.key('J');if(g.boss_.hp!=68||g.stamina_!=78)return false;
    g.key('J');if(g.boss_.hp!=68)return false;
    g.swing_=0;g.stamina_=0;g.key('J');if(g.boss_.hp!=68)return false;
    g.bossKind_=1;g.boss_.hp=100;g.boss_.phase=0;g.stamina_=100;g.key('J');
    if(g.boss_.hp!=96)return false;
    // Timed guard parries; late guard chips; dodge grants an evasion window.
    g.guard_=true;g.guardAge_=.1;g.stamina_=100;g.hp_=100;g.damage(24);
    if(g.hp_!=100||g.boss_.phase!=2||g.stamina_!=82)return false;
    g.guardAge_=.5;g.damage(24);if(g.hp_>=100)return false;
    g.hp_=100;g.dodge_=.4;g.guard_=false;g.damage(32);if(g.hp_!=100)return false;
    g.dodge_=0;g.damage(101);if(g.stage_!=Stage::Dead)return false;
    g.key('R');if(g.stage_!=Stage::Arena||g.hp_!=100||g.bossKind_!=1)return false;
    // Win each actual duel through attacks in a valid opening.
    g.bossKind_=0;g.spawnBoss();
    for(int god=0;god<3;++god){
        for(int hit=0;hit<4;++hit){
            g.x_=8;g.z_=9;g.angle_=-pi/2;g.boss_.x=8;g.boss_.z=7;
            g.boss_.phase=2;g.stamina_=100;g.swing_=0;g.key('J');
        }
        if(god<2&&g.bossKind_!=god+1)return false;
    }
    if(g.stage_!=Stage::Worthy)return false;
    g.key('E');if(g.stage_!=Stage::Tower||g.floor_!=0)return false;
    // Gate remains locked until two independent climbers accept your oath.
    g.x_=8;g.z_=2;g.key('E');if(g.floor_!=0)return false;
    for(int i=0;i<3;++i){g.x_=people_[i].x;g.z_=people_[i].z+.5;g.key('E');
        if(g.talking_!=i)return false;g.key('1');if(!g.allies_[i])return false;}
    g.x_=8;g.z_=2;g.key('E');if(g.floor_!=1)return false;
    g.x_=11;g.z_=6;g.key('E');if(g.seal_!=0)return false;
    const double xx[3]={5,11,8},zz[3]={6,6,3};
    for(int i=0;i<3;++i){g.x_=xx[i];g.z_=zz[i]+.5;g.key('E');if(g.seal_!=i+1)return false;}
    g.x_=8;g.z_=2;g.key('E');if(g.floor_!=2||!g.fighting())return false;
    // Retry preserves chapter, companions and the current encounter.
    g.dodge_=0;g.hp_=1;g.damage(100);g.key('R');
    if(g.floor_!=2||g.stage_!=Stage::Tower||!g.allies_[1]||g.boss_.hp!=100)return false;
    for(int i=0;i<4;++i){g.x_=8;g.z_=9;g.angle_=-pi/2;g.boss_.x=8;g.boss_.z=7;
        g.boss_.phase=2;g.stamina_=100;g.swing_=0;g.key('J');}
    if(!g.floorClear_)return false;
    g.x_=8;g.z_=2;g.key('E');if(g.floor_!=3)return false;
    g.x_=8;g.z_=2;g.key('E');if(!g.finalChoice_)return false;
    NewGods other=g;g.key('1');other.key('2');
    if(g.stage_!=Stage::Ending||g.abandoned_||other.stage_!=Stage::Ending||!other.abandoned_)return false;
    g.key(VK_RETURN);if(g.active())return false;
    // A long movement run cannot escape through the boundary or pillars.
    g.preview(Stage::Arena,0);g.x_=1.25;g.z_=8;g.angle_=pi;
    for(int i=1;i<80;++i)g.update(i*40,{1,0,0,false});
    return g.x_>=1.22&&g.x_<=1.25;
}
} // namespace anaise
