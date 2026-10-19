#include "game.h"

#include <algorithm>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace {

const std::wstring kSaveVersion = L"HOLLOWSPIRE 1";

std::wstring lower(std::wstring value) {
    for (wchar_t& ch : value) {
        if (ch >= L'A' && ch <= L'Z') {
            ch = static_cast<wchar_t>(ch - L'A' + L'a');
        }
    }
    return value;
}

std::wstring trim(const std::wstring& value) {
    size_t first = 0;
    while (first < value.size() &&
           (value[first] == L' ' || value[first] == L'\t' ||
            value[first] == L'\r' || value[first] == L'\n')) {
        ++first;
    }
    size_t last = value.size();
    while (last > first &&
           (value[last - 1] == L' ' || value[last - 1] == L'\t' ||
            value[last - 1] == L'\r' || value[last - 1] == L'\n')) {
        --last;
    }
    return value.substr(first, last - first);
}

bool isArticle(const std::wstring& word) {
    return word == L"the" || word == L"a" || word == L"an";
}

std::wstring itemDisplay(const std::wstring& id) {
    if (id == L"lantern") return L"a rusty lantern";
    if (id == L"oil") return L"a flask of lamp oil";
    if (id == L"crowbar") return L"a bent iron crowbar";
    if (id == L"journal") return L"the keeper's journal";
    if (id == L"bell") return L"a silver hand-bell";
    if (id == L"key") return L"a small brass key";
    if (id == L"lens") return L"a ground glass lens";
    if (id == L"coin") return L"a green copper coin";
    if (id == L"ember") return L"the ember";
    return id;
}

bool isItemId(const std::wstring& id) {
    return id == L"lantern" || id == L"oil" || id == L"crowbar" ||
           id == L"journal" || id == L"bell" || id == L"key" ||
           id == L"lens" || id == L"coin" || id == L"ember";
}

std::wstring joinNarrow(const std::vector<std::wstring>& values,
                        const std::wstring& separator) {
    std::wstring result;
    for (size_t i = 0; i < values.size(); ++i) {
        if (i) result += separator;
        result += values[i];
    }
    return result;
}

std::vector<std::wstring> splitOn(const std::wstring& value, wchar_t separator) {
    std::vector<std::wstring> result;
    size_t start = 0;
    while (start <= value.size()) {
        size_t next = value.find(separator, start);
        if (next == std::wstring::npos) {
            result.push_back(value.substr(start));
            break;
        }
        result.push_back(value.substr(start, next - start));
        start = next + 1;
    }
    return result;
}

std::string narrow(const std::wstring& value) {
    std::string result;
    result.reserve(value.size());
    for (wchar_t ch : value) {
        result.push_back(static_cast<char>(ch & 0x7f));
    }
    return result;
}

std::wstring widen(const std::string& value) {
    std::wstring result;
    result.reserve(value.size());
    for (char ch : value) {
        result.push_back(static_cast<wchar_t>(static_cast<unsigned char>(ch)));
    }
    return result;
}

const std::vector<std::pair<std::wstring, std::vector<std::wstring>>>& objectAliases() {
    static const std::vector<std::pair<std::wstring, std::vector<std::wstring>>> aliases = {
        {L"lantern", {L"lantern", L"rusty lantern", L"lamp", L"light", L"keepers lantern"}},
        {L"oil", {L"oil", L"flask", L"oil flask", L"lamp oil"}},
        {L"crowbar", {L"crowbar", L"crow bar", L"iron bar", L"prybar", L"pry bar", L"bar"}},
        {L"journal", {L"journal", L"diary", L"logbook", L"keepers journal", L"keeper journal", L"book"}},
        {L"bell", {L"bell", L"silver bell", L"handbell", L"hand bell"}},
        {L"key", {L"key", L"brass key"}},
        {L"lens", {L"lens", L"glass lens", L"crystal", L"prism", L"disc"}},
        {L"coin", {L"coin", L"copper coin", L"penny"}},
        {L"chart", {L"chart", L"star chart", L"starchart", L"star map", L"constellations"}},
        {L"hatch", {L"hatch", L"trapdoor", L"trap door", L"floor door"}},
        {L"cabinet", {L"cabinet", L"cupboard", L"locker", L"case"}},
        {L"telescope", {L"telescope", L"spyglass", L"scope"}},
        {L"pedestal", {L"pedestal", L"plinth", L"stone mouth", L"mouth"}},
        {L"altar", {L"altar"}},
        {L"ghost", {L"ghost", L"spirit", L"apparition", L"woman", L"keeper"}},
        {L"skeleton", {L"skeleton", L"bones", L"corpse", L"body"}},
        {L"statue", {L"statue", L"astronomer"}},
        {L"fountain", {L"fountain", L"basin"}},
        {L"hearth", {L"hearth", L"fireplace", L"fire", L"ashes"}},
        {L"sign", {L"sign", L"plaque"}},
        {L"door", {L"door", L"doors", L"gate", L"gates", L"great doors"}},
        {L"brazier", {L"brazier", L"cage", L"iron cage"}},
        {L"beacon", {L"beacon", L"spire", L"tower"}},
        {L"stair", {L"stair", L"stairs", L"staircase", L"steps"}},
        {L"shelves", {L"shelves", L"shelf", L"books", L"tomes", L"bookcase"}},
        {L"portrait", {L"portrait", L"painting"}},
        {L"window", {L"window", L"windows"}},
        {L"sky", {L"sky", L"stars", L"dome"}},
        {L"ember", {L"ember", L"heart", L"flame", L"star"}}
    };
    return aliases;
}

std::wstring cleanPhrase(const std::wstring& phrase) {
    std::wstring normalized;
    std::wstring word;
    auto flush = [&]() {
        if (!word.empty()) {
            if (!isArticle(word)) {
                if (!normalized.empty()) normalized.push_back(L' ');
                normalized += word;
            }
            word.clear();
        }
    };
    for (wchar_t ch : lower(phrase)) {
        bool wordChar = (ch >= L'a' && ch <= L'z') ||
                        (ch >= L'0' && ch <= L'9');
        if (wordChar) {
            word.push_back(ch);
        } else {
            flush();
        }
    }
    flush();
    return normalized;
}

}  // namespace

Game::Game(Sink sink) : sink_(std::move(sink)) {}

// ---------------------------------------------------------------------------
// Output helpers
// ---------------------------------------------------------------------------

void Game::emit(const std::wstring& text, Ink ink) {
    Block block;
    block.spans.push_back({text, ink});
    sink_(block);
}

void Game::emitSpans(std::vector<Span> spans) {
    Block block;
    block.spans = std::move(spans);
    sink_(block);
}

void Game::blank() {
    sink_(Block{});
}

void Game::emitArt(const std::wstring& art) {
    std::wstringstream stream(art);
    std::wstring line;
    while (std::getline(stream, line)) {
        if (!line.empty() && line.back() == L'\r') line.pop_back();
        Block block;
        block.art = true;
        if (!line.empty()) block.spans.push_back({line, Ink::Art});
        sink_(block);
    }
}

// ---------------------------------------------------------------------------
// World data
// ---------------------------------------------------------------------------

std::wstring Game::roomName(const std::wstring& id) const {
    if (id == L"gate") return L"The Weirwood Gate";
    if (id == L"courtyard") return L"The Courtyard";
    if (id == L"gatehouse") return L"The Gatehouse";
    if (id == L"hall") return L"The Great Hall";
    if (id == L"cellar") return L"The Cellar";
    if (id == L"library") return L"The Scriptorium";
    if (id == L"observatory") return L"The Observatory";
    if (id == L"beacon") return L"The Beacon";
    return L"The Hollow Spire";
}

std::wstring Game::roomArt(const std::wstring& id) const {
    if (id == L"gate") {
        return LR"(              /\
             /  \
            / /\ \
           / /  \ \
          /_/____\_\
  _______/__________\_______
 |  []   []   []   []   []  |
 |___________________________|
    ||                   ||
 ___||___________________||___
/_____________________________\)";
    }
    if (id == L"courtyard") {
        return LR"(                        /\
                       /  \
                      / /\ \
                     / /  \ \
                    /_/____\_\
            .--------.  ||  .--------.
           /  ~ ~ ~ ~ ~ || ~ ~ ~ ~ ~ \
          |  ~ ~ ~ (o)  ||  ~ ~ ~ ~ ~ |
           \  ~ ~ ~ ~ ~ || ~ ~ ~ ~ ~ /
            '--------'  ||  '--------')";
    }
    if (id == L"gatehouse") {
        return LR"(        ______________________________
       /  ____________________________/|
      /  /  []     []     []          / |
     /  /_____________________________/  |
    |  |  ___       ___       ___     |  |
    |  | |   |     |   |     |   |    |  |
    |__|_|___|_____|___|_____|___|____|__/
         ||                     ||)";
    }
    if (id == L"hall") {
        return LR"(      ____________________________________
     |  []   []   []   []   []   []   []  |
     |____________________________________|
             |                    |
             |    THE GREAT HALL  |
        _____|____________________|_____
       |  [ hearth ]      [ stairs ]    |
       |________________________________|)";
    }
    if (id == L"cellar") {
        return LR"(        ____________________________________
       |  |    |    |    |    |    |    |  |
       |__|____|____|____|____|____|____|__|
          |    |    |    |    |
          |    |    |    |    |
       ___|____|____|____|____|___
      /_____________________________\
           \   o    o    o   /
            \________________/)";
    }
    if (id == L"library") {
        return LR"(      __________________________________________
     |  [] [] [] [] [] [] [] [] [] [] [] []    |
     |  [] [] [] [] [] [] [] [] [] [] [] []    |
     |  [] [] [] []   __________________  []    |
     |  [] [] [] []  |   STAR  CHART    | []    |
     |_______________|__________________|_______|
              \  reading desk  /
               \______________/)";
    }
    if (id == L"observatory") {
        return LR"(                  _______________________
                 /   *     *     *     *  \
                /       ____________        \
               |       /            \        |
               |      |     (o)      |       |
               |       \____________/        |
                \           |  |            /
                 \__________|__|___________/
                            |  |)";
    }
    if (id == L"beacon") {
        return LR"(                    .    *    .
              *         |         *
                        |
                  .-----+-----.
                 /  THE EMBER  \
                |    ~  ~  ~    |
                 \    ~  ~     /
                  '-----------'
                      |   |
                  ____|___|____)";
    }
    return L"";
}

std::wstring Game::roomDescription(const std::wstring& id) {
    if (id == L"gate") {
        return L"A ring of broken wall gathers around the foot of the spire. The "
               L"gate stands open, its iron long since rusted away. The pilgrim "
               L"road behind you vanishes into mist.";
    }
    if (id == L"courtyard") {
        return L"Briars crowd the flagstones. A dry fountain squats at the "
               L"centre, and the spire rises above it, black against the grey "
               L"sky. The great doors to the hall stand ajar to the north; a low "
               L"gatehouse slumps to the west.";
    }
    if (id == L"gatehouse") {
        return L"The gatehouse smells of wet stone and old smoke. A keeper's "
               L"skeleton sits against the wall, an open journal across its "
               L"knees. A workbench and a shelf of tins line the far wall.";
    }
    if (id == L"hall") {
        std::wstring text =
            L"The great hall is a ribcage of old timber. A cold hearth yawns on "
            L"the west wall, and a stair climbs into shadow at the north end. ";
        if (flag(L"hatchOpen")) {
            text += L"In the centre of the floor, the iron hatch stands open, "
                    L"breathing cold air.";
        } else {
            text += L"In the centre of the floor, an iron hatch lies furred "
                    L"with rust.";
        }
        return text;
    }
    if (id == L"cellar") {
        if (!lanternLightHere()) {
            return L"Blackness. Water drips somewhere ahead, and the air tastes "
                   L"of iron. Without a light, the cellar is only a rumour.";
        }
        std::wstring text =
            L"The lantern pushes the dark back. Barrel staves rot along the "
            L"walls, and a stone altar waits at the far end. ";
        if (!flag(L"bellRung")) {
            text += L"A silver bell sits on a crate nearby. Between you and the "
                    L"altar stands the keeper's ghost, pale as breath.";
        } else if (!taken_.count(L"key")) {
            text += L"The ghost is gone. On the altar, a small brass key catches "
                    L"the light.";
        } else {
            text += L"The ghost is gone, and the altar is bare. The dark has "
                    L"gone quiet.";
        }
        return text;
    }
    if (id == L"library") {
        std::wstring text =
            L"Shelves lean like old teeth. Most of the books have gone to mould, "
            L"but a star chart hangs above the reading desk, and a narrow stair "
            L"winds up into the observatory. ";
        if (flag(L"cabinetOpen")) {
            text += L"The cabinet beside the window stands open.";
        } else {
            text += L"The cabinet beside the window is still locked.";
        }
        return text;
    }
    if (id == L"observatory") {
        std::wstring text =
            L"The observatory is open to the sky. A great telescope crouches "
            L"beneath the broken dome. At the room's centre, a stone pedestal "
            L"waits, its carved mouth shut. ";
        if (flag(L"lensInstalled")) {
            text += L"The telescope's glass eye is full of stars. ";
        } else {
            text += L"The telescope's lens socket is empty. ";
        }
        if (flag(L"beaconOpen")) {
            text += L"The pedestal's mouth hangs open; a stair of cold air "
                    L"climbs to the beacon.";
        } else {
            text += L"Words have worn the pedestal smooth.";
        }
        return text;
    }
    if (id == L"beacon") {
        return L"The beacon room is a cage of glass at the top of the world. In "
               L"its heart, an iron brazier holds a small ember of white-gold "
               L"light. The air tastes of thunder and salt. The ember leans "
               L"toward you, as if listening.";
    }
    return L"";
}

std::vector<std::wstring> Game::visibleItems() const {
    std::vector<std::wstring> items;
    auto add = [&](const std::wstring& id) {
        if (std::find(items.begin(), items.end(), id) == items.end()) {
            items.push_back(id);
        }
    };

    auto dropped = dropped_.find(room_);
    if (dropped != dropped_.end()) {
        for (const auto& id : dropped->second) add(id);
    }

    if (room_ == L"gate" && !taken_.count(L"lantern")) add(L"lantern");
    if (room_ == L"courtyard" && !taken_.count(L"coin")) add(L"coin");
    if (room_ == L"gatehouse") {
        if (!taken_.count(L"journal")) add(L"journal");
        if (!taken_.count(L"crowbar")) add(L"crowbar");
        if (!taken_.count(L"oil")) add(L"oil");
    }
    if (room_ == L"cellar" && lanternLightHere()) {
        if (!taken_.count(L"bell")) add(L"bell");
        if (flag(L"bellRung") && !taken_.count(L"key")) add(L"key");
    }
    if (room_ == L"library" && flag(L"cabinetOpen") &&
        !taken_.count(L"lens")) {
        add(L"lens");
    }
    return items;
}

bool Game::itemVisibleHere(const std::wstring& id) const {
    auto items = visibleItems();
    return std::find(items.begin(), items.end(), id) != items.end();
}

bool Game::itemAvailableHere(const std::wstring& id) const {
    return itemVisibleHere(id);
}

bool Game::hasItem(const std::wstring& id) const {
    return std::find(inventory_.begin(), inventory_.end(), id) != inventory_.end();
}

void Game::addItem(const std::wstring& id) {
    if (!hasItem(id)) inventory_.push_back(id);
}

void Game::removeItem(const std::wstring& id) {
    inventory_.erase(std::remove(inventory_.begin(), inventory_.end(), id),
                     inventory_.end());
}

bool Game::flag(const std::wstring& name) const {
    auto it = flags_.find(name);
    return it != flags_.end() && it->second;
}

void Game::setFlag(const std::wstring& name, bool value) {
    flags_[name] = value;
}

bool Game::lanternLightHere() const {
    if (!flag(L"lanternLit")) return false;
    if (hasItem(L"lantern")) return true;
    auto it = dropped_.find(room_);
    return it != dropped_.end() &&
           std::find(it->second.begin(), it->second.end(), L"lantern") !=
               it->second.end();
}

std::vector<Span> Game::itemLine() {
    std::vector<Span> spans;
    auto items = visibleItems();
    if (items.empty()) return spans;
    spans.push_back({L"You see ", Ink::Dim});
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) spans.push_back({L", ", Ink::Text});
        spans.push_back({itemDisplay(items[i]), Ink::Item});
    }
    spans.push_back({L".", Ink::Text});
    return spans;
}

void Game::emitItemsHere() {
    auto spans = itemLine();
    if (!spans.empty()) emitSpans(std::move(spans));
}

void Game::emitExits() {
    std::vector<Span> spans;
    spans.push_back({L"Exits: ", Ink::Dim});

    auto add = [&](const std::wstring& direction, const std::wstring& destination) {
        if (spans.size() > 1) spans.push_back({L", ", Ink::Dim});
        spans.push_back({direction, Ink::Accent});
        spans.push_back({L" (" + destination + L")", Ink::Dim});
    };

    if (room_ == L"gate") {
        add(L"north", roomName(L"courtyard"));
        add(L"south", L"the pilgrim road");
    } else if (room_ == L"courtyard") {
        add(L"north", roomName(L"hall"));
        add(L"west", roomName(L"gatehouse"));
        add(L"south", roomName(L"gate"));
    } else if (room_ == L"gatehouse") {
        add(L"east", roomName(L"courtyard"));
    } else if (room_ == L"hall") {
        add(L"south", roomName(L"courtyard"));
        add(L"up", roomName(L"library"));
        add(L"down", flag(L"hatchOpen") ? roomName(L"cellar")
                                        : L"a rusted hatch");
    } else if (room_ == L"cellar") {
        add(L"up", roomName(L"hall"));
    } else if (room_ == L"library") {
        add(L"down", roomName(L"hall"));
        add(L"up", roomName(L"observatory"));
    } else if (room_ == L"observatory") {
        add(L"down", roomName(L"library"));
        add(L"up", flag(L"beaconOpen") ? roomName(L"beacon") : L"a sealed stair");
    } else if (room_ == L"beacon") {
        add(L"down", roomName(L"observatory"));
    }

    if (spans.size() > 1) {
        spans.push_back({L".", Ink::Dim});
        emitSpans(std::move(spans));
    }
}

void Game::emitRoom(bool withArt) {
    blank();

    if (room_ == L"cellar" && !lanternLightHere()) {
        withArt = false;
    }

    if (withArt) {
        auto art = roomArt(room_);
        if (!art.empty()) {
            emitArt(art);
            blank();
        }
    }

    emit(L"== " + roomName(room_) + L" ==", Ink::Room);
    emit(roomDescription(room_), Ink::Text);

    if (room_ == L"cellar" && !lanternLightHere()) {
        if (!hasItem(L"lantern") && !flag(L"lanternLit")) {
            emit(L"It is far too dark to search. You need a light.", Ink::Bad);
        } else if (!flag(L"lanternLit")) {
            emit(L"Your lantern is unlit. LIGHT LANTERN would change that.", Ink::Dim);
        } else {
            emit(L"You have light, but it is not with you.", Ink::Dim);
        }
    } else {
        emitItemsHere();
    }

    emitExits();
    visited_.insert(room_);
}

// ---------------------------------------------------------------------------
// Parsing helpers
// ---------------------------------------------------------------------------

std::vector<std::wstring> Game::tokenize(const std::wstring& text) {
    std::vector<std::wstring> words;
    std::wstring word;
    for (wchar_t ch : lower(text)) {
        bool wordChar = (ch >= L'a' && ch <= L'z') ||
                        (ch >= L'0' && ch <= L'9');
        if (wordChar) {
            word.push_back(ch);
        } else if (!word.empty()) {
            words.push_back(word);
            word.clear();
        }
    }
    if (!word.empty()) words.push_back(word);
    return words;
}

std::wstring Game::join(const std::vector<std::wstring>& words, size_t from) {
    std::wstring result;
    for (size_t i = from; i < words.size(); ++i) {
        if (!result.empty()) result.push_back(L' ');
        result += words[i];
    }
    return result;
}

std::wstring Game::canonicalObject(const std::wstring& phrase) {
    std::wstring clean = cleanPhrase(phrase);
    if (clean.empty()) return L"";
    std::wstring best;
    size_t bestLength = 0;
    for (const auto& entry : objectAliases()) {
        for (const auto& alias : entry.second) {
            if (clean == alias && alias.size() >= bestLength) {
                best = entry.first;
                bestLength = alias.size();
            }
        }
    }
    return best;
}

std::wstring Game::canonicalDirection(const std::wstring& word) {
    std::wstring clean = cleanPhrase(word);
    if (clean == L"north" || clean == L"n") return L"north";
    if (clean == L"south" || clean == L"s") return L"south";
    if (clean == L"east" || clean == L"e") return L"east";
    if (clean == L"west" || clean == L"w") return L"west";
    if (clean == L"up" || clean == L"u" || clean == L"upstairs" ||
        clean == L"above") {
        return L"up";
    }
    if (clean == L"down" || clean == L"d" || clean == L"downstairs" ||
        clean == L"below") {
        return L"down";
    }
    return L"";
}

// ---------------------------------------------------------------------------
// Movement
// ---------------------------------------------------------------------------

bool Game::move(const std::wstring& direction) {
    if (gameOver_) {
        emit(L"The story has ended. Type NEW to begin again.", Ink::Dim);
        return false;
    }

    std::wstring dest;
    if (room_ == L"gate") {
        if (direction == L"north") dest = L"courtyard";
        if (direction == L"south") {
            emit(L"The road waits behind you, swallowed by mist. If you truly "
                 L"mean to abandon the vigil, type LEAVE.",
                 Ink::Dim);
            return false;
        }
    } else if (room_ == L"courtyard") {
        if (direction == L"north") dest = L"hall";
        if (direction == L"west") dest = L"gatehouse";
        if (direction == L"south") dest = L"gate";
    } else if (room_ == L"gatehouse") {
        if (direction == L"east") dest = L"courtyard";
    } else if (room_ == L"hall") {
        if (direction == L"south") dest = L"courtyard";
        if (direction == L"up") dest = L"library";
        if (direction == L"down") {
            if (!flag(L"hatchOpen")) {
                emit(L"You kneel beside the hatch, but it is rusted into its "
                     L"frame. You need something to lever it open.",
                     Ink::Bad);
                return false;
            }
            dest = L"cellar";
        }
    } else if (room_ == L"cellar") {
        if (direction == L"up") dest = L"hall";
    } else if (room_ == L"library") {
        if (direction == L"down") dest = L"hall";
        if (direction == L"up") dest = L"observatory";
    } else if (room_ == L"observatory") {
        if (direction == L"down") dest = L"library";
        if (direction == L"up") {
            if (!flag(L"beaconOpen")) {
                emit(L"A slab of black stone seals the stair. The pedestal in "
                     L"the centre of the room waits, its carved mouth shut.",
                     Ink::Bad);
                return false;
            }
            dest = L"beacon";
        }
    } else if (room_ == L"beacon") {
        if (direction == L"down") dest = L"observatory";
    }

    if (dest.empty()) {
        emit(L"You can't go " + direction + L" from here.", Ink::Bad);
        emitExits();
        return false;
    }

    bool firstVisit = visited_.find(dest) == visited_.end();
    room_ = dest;
    ++moves_;
    emitRoom(firstVisit);
    return true;
}

void Game::cmdGo(const std::wstring& phrase) {
    if (phrase.empty()) {
        emit(L"Go where? Use a compass direction, UP, or DOWN.", Ink::Dim);
        emitExits();
        return;
    }
    auto words = tokenize(phrase);
    std::wstring direction = canonicalDirection(join(words, 0));
    if (!direction.empty()) {
        move(direction);
        return;
    }

    // Let people name an adjoining room, too.
    std::wstring clean = cleanPhrase(phrase);
    std::wstring dest;
    if (clean.find(L"courtyard") != std::wstring::npos) dest = L"courtyard";
    else if (clean.find(L"gatehouse") != std::wstring::npos) dest = L"gatehouse";
    else if (clean.find(L"cellar") != std::wstring::npos) dest = L"cellar";
    else if (clean.find(L"scriptorium") != std::wstring::npos ||
             clean.find(L"library") != std::wstring::npos) dest = L"library";
    else if (clean.find(L"observatory") != std::wstring::npos) dest = L"observatory";
    else if (clean.find(L"beacon") != std::wstring::npos) dest = L"beacon";
    else if (clean == L"gate" || clean.find(L"weirwood") != std::wstring::npos) dest = L"gate";
    else if (clean.find(L"hall") != std::wstring::npos) dest = L"hall";

    if (!dest.empty()) {
        std::wstring directionTo;
        // Translate the destination into one of the available exits.
        if (room_ == L"gate" && dest == L"courtyard") directionTo = L"north";
        if (room_ == L"courtyard") {
            if (dest == L"hall") directionTo = L"north";
            if (dest == L"gatehouse") directionTo = L"west";
            if (dest == L"gate") directionTo = L"south";
        }
        if (room_ == L"gatehouse" && dest == L"courtyard") directionTo = L"east";
        if (room_ == L"hall") {
            if (dest == L"courtyard") directionTo = L"south";
            if (dest == L"library") directionTo = L"up";
            if (dest == L"cellar") directionTo = L"down";
        }
        if (room_ == L"cellar" && dest == L"hall") directionTo = L"up";
        if (room_ == L"library") {
            if (dest == L"hall") directionTo = L"down";
            if (dest == L"observatory") directionTo = L"up";
        }
        if (room_ == L"observatory") {
            if (dest == L"library") directionTo = L"down";
            if (dest == L"beacon") directionTo = L"up";
        }
        if (room_ == L"beacon" && dest == L"observatory") directionTo = L"down";

        if (!directionTo.empty()) {
            move(directionTo);
            return;
        }
        emit(L"You can see it in your mind, but there is no straight path from "
             L"here.",
             Ink::Dim);
        return;
    }

    emit(L"You can't go there from here.", Ink::Bad);
    emitExits();
}

// ---------------------------------------------------------------------------
// Looking and examining
// ---------------------------------------------------------------------------

void Game::cmdLook(const std::vector<std::wstring>& args) {
    if (args.empty()) {
        emitRoom(true);
        return;
    }
    size_t start = 0;
    if (args[0] == L"at" || args[0] == L"to") start = 1;
    cmdExamine(join(args, start));
}

void Game::cmdExamine(const std::wstring& phrase) {
    std::wstring id = canonicalObject(phrase);
    if (id.empty()) {
        emit(L"You don't see anything like that here.", Ink::Bad);
        return;
    }

    bool dark = room_ == L"cellar" && !lanternLightHere();
    if (dark && !(isItemId(id) && hasItem(id))) {
        emit(L"It is too dark to see that.", Ink::Bad);
        return;
    }
    describeObject(id);
}

void Game::describeObject(const std::wstring& id) {
    bool dark = room_ == L"cellar" && !lanternLightHere();

    if (isItemId(id) && id != L"ember") {
        bool present = hasItem(id) || itemVisibleHere(id);
        if (id == L"lantern" && room_ == L"beacon") {
            present = false;  // In the beacon, "lamp" means the great lamp.
        }
        if (!present) {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        if (id == L"lantern") {
            std::wstring text = L"A keeper's lantern of blackened brass. Its "
                                L"glass is soot-stained. ";
            text += flag(L"lanternLit") ? L"It is lit." : L"It is unlit.";
            emit(text, Ink::Text);
        } else if (id == L"oil") {
            emit(L"A clay flask, still half full of lamp oil.", Ink::Text);
        } else if (id == L"crowbar") {
            emit(L"A bent iron crowbar, cold and heavy. It has opened more than "
                 L"crates.",
                 Ink::Text);
        } else if (id == L"journal") {
            emit(L"The keeper's journal. Its pages are warped with damp. You "
                 L"could READ it.",
                 Ink::Text);
        } else if (id == L"bell") {
            emit(L"A silver hand-bell. Its clapper is wrapped in old cloth to "
                 L"keep it quiet.",
                 Ink::Text);
        } else if (id == L"key") {
            emit(L"A small brass key, green at the teeth. It smells of cellar "
                 L"stone.",
                 Ink::Text);
        } else if (id == L"lens") {
            emit(L"A disc of ground crystal. When you tilt it, the room bends "
                 L"at the edges.",
                 Ink::Text);
        } else if (id == L"coin") {
            emit(L"A copper coin, worn smooth. The face is a tower with one "
                 L"window lit.",
                 Ink::Text);
        }
        return;
    }

    if (id == L"ember") {
        if (room_ != L"beacon") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"A seed of white-gold light, no larger than a plum. It turns "
             L"slowly inside the iron cage. When you move, it turns to follow "
             L"you.",
             Ink::Text);
        return;
    }

    if (id == L"hatch") {
        if (room_ != L"hall") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        if (flag(L"hatchOpen")) {
            emit(L"The hatch stands open. Cold air climbs out of the cellar, "
                 L"smelling of iron and old water.",
                 Ink::Text);
        } else {
            emit(L"An iron hatch, rusted into its frame. It needs a lever.",
                 Ink::Text);
        }
    } else if (id == L"cabinet") {
        if (room_ != L"library") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        if (flag(L"cabinetOpen")) {
            emit(L"The cabinet door hangs open. It smells of cedar and dust.",
                 Ink::Text);
        } else {
            emit(L"Locked. The brass keyhole is shaped like a tiny tower.",
                 Ink::Text);
        }
    } else if (id == L"telescope") {
        if (room_ != L"observatory") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        if (flag(L"lensInstalled")) {
            emit(L"The lens sits true. When you look, the constellation of the "
                 L"Lantern burns in the eyepiece, and silver letters spell "
                 L"ASTER beneath it.",
                 Ink::Text);
        } else {
            emit(L"A brass giant, blind in one eye. The lens socket waits.",
                 Ink::Text);
        }
    } else if (id == L"pedestal") {
        if (room_ != L"observatory") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        if (flag(L"beaconOpen")) {
            emit(L"The stone mouth hangs open. A stair of cold air climbs to "
                 L"the beacon.",
                 Ink::Text);
        } else {
            emit(L"The pedestal's mouth is closed. Words have worn the stone "
                 L"smooth around it. It is waiting for a name.",
                 Ink::Text);
        }
    } else if (id == L"altar") {
        if (room_ != L"cellar" || dark) {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        if (flag(L"bellRung")) {
            emit(L"The altar is black stone, worn hollow in the middle. It is "
                 L"bare now.",
                 Ink::Text);
        } else {
            emit(L"The altar is black stone, worn hollow in the middle. The "
                 L"ghost stands between you and it.",
                 Ink::Text);
        }
    } else if (id == L"ghost") {
        if (room_ != L"cellar" || dark || flag(L"bellRung")) {
            emit(L"There is no ghost here now.", Ink::Bad);
            return;
        }
        emit(L"She is pale as breath, her face turned toward the silver bell. "
             L"Her hands are folded. She does not seem angry. She seems "
             L"tired.",
             Ink::Text);
    } else if (id == L"skeleton") {
        if (room_ != L"gatehouse") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"Her cloak has rotted to threads. A brass badge at her breast "
             L"reads: ASTER, KEEPER OF THE SPIRE.",
             Ink::Text);
        nameKnown_ = true;
    } else if (id == L"statue") {
        if (room_ != L"courtyard") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"A weathered astronomer in a hooded coat, one hand raised to the "
             L"sky. Rain has worn away her face.",
             Ink::Text);
    } else if (id == L"fountain") {
        if (room_ != L"courtyard") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        if (!taken_.count(L"coin")) {
            emit(L"The fountain is dry. A green coin glints beneath the silt.",
                 Ink::Text);
        } else {
            emit(L"The fountain is dry and empty. The basin has not held water "
                 L"in years.",
                 Ink::Text);
        }
    } else if (id == L"hearth") {
        if (room_ != L"hall") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"Cold ashes and a smell of old smoke. The chimney above is full "
             L"of wind.",
             Ink::Text);
    } else if (id == L"sign") {
        if (room_ != L"gate") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"A weathered plaque: KEEP THE LIGHT. KEEP THE NAME.", Ink::Text);
        emit(L"Below it, in newer scratches: The sky remembers her, even if the "
             L"stone does not.",
             Ink::Dim);
    } else if (id == L"door") {
        if (room_ == L"gate" || room_ == L"courtyard") {
            emit(L"The great doors stand open. Nothing has closed them in a "
                 L"long time.",
                 Ink::Text);
        } else if (room_ == L"observatory") {
            emit(L"The way up is sealed by stone, not by any door you can "
                 L"touch.",
                 Ink::Text);
        } else {
            emit(L"There is no door here.", Ink::Bad);
        }
    } else if (id == L"brazier" || id == L"beacon") {
        if (room_ == L"beacon") {
            emit(L"An iron cage, built to hold a star. The latch is not locked. "
                 L"The ember waits inside, leaning toward you.",
                 Ink::Text);
        } else {
            emit(L"The beacon is a dark finger of stone above the tower. It has "
                 L"not burned in thirty years.",
                 Ink::Text);
        }
    } else if (id == L"stair") {
        if (room_ == L"hall") {
            emit(L"The stair climbs north into shadow, toward the scriptorium.",
                 Ink::Text);
        } else if (room_ == L"library") {
            emit(L"A narrow stair winds up into the observatory.", Ink::Text);
        } else if (room_ == L"observatory") {
            emit(flag(L"beaconOpen")
                     ? L"A stair of cold air climbs to the beacon."
                     : L"A slab of black stone seals the stair.",
                 Ink::Text);
        } else {
            emit(L"You don't see that here.", Ink::Bad);
        }
    } else if (id == L"shelves") {
        if (room_ != L"library") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"Rotting books, their spines gone soft. One is titled A BESTIARY "
             L"OF QUIET THINGS. It is empty.",
             Ink::Text);
    } else if (id == L"portrait") {
        if (room_ != L"hall") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"A row of keepers in oil paint, their faces mostly lost to damp. "
             L"The last frame is empty.",
             Ink::Text);
    } else if (id == L"window") {
        if (room_ != L"library" && room_ != L"observatory") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"Grey rain crawls down the glass. Beyond it, the mist has no "
             L"bottom.",
             Ink::Text);
    } else if (id == L"sky") {
        if (room_ != L"observatory" && room_ != L"beacon") {
            emit(L"You don't see that here.", Ink::Bad);
            return;
        }
        emit(L"Stars, hard and bright, scattered like salt. One constellation "
             L"is ringed in your memory: the Lantern.",
             Ink::Text);
    } else {
        emit(L"You study it, but learn nothing new.", Ink::Dim);
    }
}

// ---------------------------------------------------------------------------
// Inventory and objects
// ---------------------------------------------------------------------------

void Game::cmdTake(const std::wstring& phrase) {
    if (room_ == L"cellar" && !lanternLightHere()) {
        emit(L"You grope in the dark and find only cold air.", Ink::Bad);
        return;
    }

    std::wstring id = canonicalObject(phrase);
    if (id.empty()) {
        emit(L"Take what?", Ink::Dim);
        return;
    }

    if (id == L"ember") {
        if (room_ == L"beacon") {
            takeEmber();
        } else {
            emit(L"You don't see that here.", Ink::Bad);
        }
        return;
    }

    if (!isItemId(id)) {
        emit(L"That is part of the tower. It will not come away.", Ink::Bad);
        return;
    }
    if (hasItem(id)) {
        emit(L"You already have " + itemDisplay(id) + L".", Ink::Dim);
        return;
    }
    if (!itemAvailableHere(id)) {
        if (id == L"lens" && room_ == L"library" && !flag(L"cabinetOpen")) {
            emit(L"The cabinet is locked.", Ink::Bad);
        } else if (id == L"key" && room_ == L"cellar" && !flag(L"bellRung")) {
            emit(L"The ghost stands between you and the altar.", Ink::Bad);
        } else {
            emit(L"You don't see that here.", Ink::Bad);
        }
        return;
    }

    bool fromDropped = false;
    auto dropped = dropped_.find(room_);
    if (dropped != dropped_.end()) {
        auto it = std::find(dropped->second.begin(), dropped->second.end(), id);
        if (it != dropped->second.end()) {
            dropped->second.erase(it);
            fromDropped = true;
        }
    }

    addItem(id);
    taken_.insert(id);

    if (id == L"coin") {
        addScore(5);
        emit(L"You fish the coin out of the fountain. It is green and unspent. "
             L"(+5)",
             Ink::Good);
    } else if (id == L"bell") {
        emit(L"You lift the silver bell from the crate. It is colder than the "
             L"cellar.",
             Ink::Text);
    } else if (id == L"key") {
        emit(L"You take the brass key from the altar. The ghost's absence is a "
             L"pressure in the air.",
             Ink::Text);
    } else if (id == L"lens") {
        emit(L"You lift the lens from a bed of rotted velvet.", Ink::Text);
    } else if (id == L"lantern") {
        emit(L"You take the lantern from its hook. It is heavier than it looks.",
             Ink::Text);
    } else {
        emit(L"You take " + itemDisplay(id) + L".", Ink::Text);
    }
    (void)fromDropped;
}

void Game::cmdDrop(const std::wstring& phrase) {
    std::wstring id = canonicalObject(phrase);
    if (id.empty()) {
        emit(L"Drop what?", Ink::Dim);
        return;
    }
    if (!isItemId(id) || !hasItem(id)) {
        emit(L"You aren't carrying that.", Ink::Bad);
        return;
    }
    if (room_ == L"cellar" && id == L"lantern" && !lanternLightHere()) {
        emit(L"You would lose it in the dark. Better to keep hold of it.",
             Ink::Bad);
        return;
    }
    removeItem(id);
    dropped_[room_].push_back(id);
    emit(L"You set down " + itemDisplay(id) + L".", Ink::Text);
}

void Game::cmdInventory() {
    if (inventory_.empty()) {
        emit(L"You are carrying nothing but dust and stubbornness.", Ink::Dim);
        return;
    }
    emit(L"You are carrying:", Ink::Dim);
    for (const auto& id : inventory_) {
        emitSpans({ {L"  - ", Ink::Dim}, {itemDisplay(id), Ink::Item} });
    }
}

// ---------------------------------------------------------------------------
// Interaction commands
// ---------------------------------------------------------------------------

void Game::cmdUse(const std::vector<std::wstring>& args) {
    if (args.empty()) {
        emit(L"Use what? Try USE <item> ON <thing>.", Ink::Dim);
        return;
    }

    size_t onPos = args.size();
    for (size_t i = 0; i < args.size(); ++i) {
        if (args[i] == L"on") {
            onPos = i;
            break;
        }
    }

    std::wstring itemPhrase;
    std::wstring targetPhrase;
    for (size_t i = 0; i < args.size(); ++i) {
        if (i < onPos) {
            if (!itemPhrase.empty()) itemPhrase.push_back(L' ');
            itemPhrase += args[i];
        } else if (i > onPos) {
            if (!targetPhrase.empty()) targetPhrase.push_back(L' ');
            targetPhrase += args[i];
        }
    }
    if (onPos == args.size()) itemPhrase = join(args, 0);

    std::wstring item = canonicalObject(itemPhrase);
    std::wstring target = canonicalObject(targetPhrase);

    if (item.empty()) {
        emit(L"You don't have anything like that.", Ink::Bad);
        return;
    }

    if (item == L"oil") {
        if (!hasItem(L"oil")) {
            emit(L"You have no oil.", Ink::Bad);
        } else if (target.empty() || target == L"lantern") {
            if (flag(L"lanternLit")) {
                emit(L"The lantern is already burning.", Ink::Dim);
            } else if (!hasItem(L"lantern") && !itemVisibleHere(L"lantern")) {
                emit(L"You have no lantern to fill.", Ink::Bad);
            } else {
                removeItem(L"oil");
                setFlag(L"oiled");
                emit(L"You fill the lantern's reservoir with oil. The wick is "
                     L"ready.",
                     Ink::Text);
            }
        } else {
            emit(L"The oil would only make a mess there.", Ink::Dim);
        }
        return;
    }

    if (item == L"lantern") {
        if (target.empty() || target == L"lantern" || target == L"oil") {
            lightLantern();
        } else {
            emit(L"The lantern casts its light, but nothing happens.", Ink::Dim);
        }
        return;
    }

    if (item == L"crowbar") {
        if (target == L"hatch" || target.empty()) {
            if (room_ != L"hall") {
                emit(L"There is no hatch here.", Ink::Bad);
            } else {
                openHatch();
            }
        } else {
            emit(L"The crowbar is not much use on that.", Ink::Dim);
        }
        return;
    }

    if (item == L"key") {
        if (target == L"cabinet" || target.empty()) {
            if (room_ != L"library") {
                emit(L"There is no cabinet here.", Ink::Bad);
            } else {
                openCabinet();
            }
        } else {
            emit(L"The key does not fit that.", Ink::Dim);
        }
        return;
    }

    if (item == L"lens") {
        if (target == L"telescope" || target.empty()) {
            if (room_ != L"observatory") {
                emit(L"There is no telescope here.", Ink::Bad);
            } else {
                installLens();
            }
        } else {
            emit(L"The lens is meant for the telescope.", Ink::Dim);
        }
        return;
    }

    if (item == L"bell") {
        ringBell();
        return;
    }

    if (item == L"ember") {
        if (room_ == L"beacon") {
            lightBeacon();
        } else {
            emit(L"The ember is not here.", Ink::Bad);
        }
        return;
    }

    if (item == L"journal") {
        emit(L"You could READ the journal.", Ink::Dim);
        return;
    }
    if (item == L"coin") {
        emit(L"The coin is not a tool. It is warm in your hand, and that is "
             L"enough.",
             Ink::Dim);
        return;
    }

    if (!hasItem(item)) {
        emit(L"You don't have that.", Ink::Bad);
        return;
    }
    emit(L"Nothing happens.", Ink::Dim);
}

void Game::openHatch() {
    if (room_ != L"hall") {
        emit(L"There is no hatch here.", Ink::Bad);
        return;
    }
    if (flag(L"hatchOpen")) {
        emit(L"The hatch is already open.", Ink::Dim);
        return;
    }
    if (!hasItem(L"crowbar")) {
        emit(L"The hatch is rusted shut. You need a lever.", Ink::Bad);
        return;
    }
    setFlag(L"hatchOpen");
    addScore(5);
    emit(L"You wedge the crowbar beneath the rim and lean. Rust screams; the "
         L"hatch bangs open. Cold air climbs out of the cellar.",
         Ink::Good);
}

void Game::openCabinet() {
    if (room_ != L"library") {
        emit(L"There is no cabinet here.", Ink::Bad);
        return;
    }
    if (flag(L"cabinetOpen")) {
        emit(L"The cabinet is already open.", Ink::Dim);
        return;
    }
    if (!hasItem(L"key")) {
        emit(L"The cabinet is locked. The brass keyhole is shaped like a tiny "
             L"tower.",
             Ink::Bad);
        return;
    }
    setFlag(L"cabinetOpen");
    addScore(5);
    emit(L"The brass key turns. Inside, on a bed of rotted velvet, lies a "
         L"ground glass lens.",
         Ink::Good);
}

void Game::cmdOpen(const std::wstring& phrase) {
    std::wstring id = canonicalObject(phrase);
    if (id.empty()) {
        emit(L"Open what?", Ink::Dim);
        return;
    }
    if (id == L"hatch") {
        openHatch();
    } else if (id == L"cabinet") {
        openCabinet();
    } else if (id == L"door" || id == L"gate") {
        if (room_ == L"gate" || room_ == L"courtyard") {
            emit(L"It already stands open.", Ink::Dim);
        } else if (room_ == L"observatory") {
            emit(L"The stair is sealed by stone, not by any door you can touch.",
                 Ink::Dim);
        } else {
            emit(L"There is no door here.", Ink::Bad);
        }
    } else if (id == L"brazier" || id == L"beacon") {
        if (room_ == L"beacon") {
            emit(L"The latch is not locked. The ember brightens, waiting. Type "
                 L"FREE EMBER to release it, or LIGHT BEACON to set it in the "
                 L"lamp.",
                 Ink::Text);
        } else {
            emit(L"The beacon is not a door.", Ink::Dim);
        }
    } else {
        emit(L"You can't open that.", Ink::Dim);
    }
}

void Game::lightLantern() {
    if (!hasItem(L"lantern")) {
        if (itemVisibleHere(L"lantern")) {
            emit(L"You would have to take the lantern first.", Ink::Dim);
        } else {
            emit(L"You have no lantern to light.", Ink::Bad);
        }
        return;
    }
    if (flag(L"lanternLit")) {
        emit(L"The lantern is already lit.", Ink::Dim);
        return;
    }
    if (!flag(L"oiled")) {
        if (!hasItem(L"oil")) {
            emit(L"The wick is dry. You need oil.", Ink::Bad);
            return;
        }
        removeItem(L"oil");
        setFlag(L"oiled");
    }

    setFlag(L"lanternLit");
    if (!flag(L"lanternScore")) {
        setFlag(L"lanternScore");
        addScore(5);
    }
    emit(L"The wick catches. A small yellow light pushes at the dark.",
         Ink::Good);

    if (room_ == L"cellar") {
        bool showArt = !flag(L"cellarLitSeen");
        setFlag(L"cellarLitSeen");
        emitRoom(showArt);
    }
}

void Game::cmdLight(const std::wstring& phrase) {
    std::wstring id = canonicalObject(phrase);
    std::wstring clean = cleanPhrase(phrase);
    bool explicitlyLantern = clean.find(L"lantern") != std::wstring::npos;
    if (room_ == L"beacon" && !explicitlyLantern &&
        (phrase.empty() || id == L"beacon" || id == L"brazier" ||
         id == L"ember" || clean == L"lamp")) {
        lightBeacon();
        return;
    }
    if (id == L"lantern" || phrase.empty()) {
        lightLantern();
        return;
    }
    if (id == L"brazier" || id == L"beacon") {
        emit(L"The beacon is not here.", Ink::Bad);
        return;
    }
    emit(L"There is nothing here to light.", Ink::Dim);
}

void Game::ringBell() {
    if (!hasItem(L"bell")) {
        emit(L"You have no bell.", Ink::Bad);
        return;
    }
    if (room_ != L"cellar") {
        emit(L"The bell gives a thin, lonely note. Nothing answers.", Ink::Dim);
        return;
    }
    if (flag(L"bellRung")) {
        emit(L"The bell hums, but the dead have already gone.", Ink::Dim);
        return;
    }
    if (!lanternLightHere()) {
        emit(L"You ring the bell in the dark. Something listens, but you cannot "
             L"see it.",
             Ink::Dim);
        return;
    }

    setFlag(L"bellRung");
    addScore(10);
    emit(L"You ring the silver bell. The note is small and clear.", Ink::Good);
    emit(L"The ghost turns toward the sound. For a moment her face is only a "
         L"tired woman's, and then she is gone. The cold lifts.",
         Ink::Text);
    emit(L"On the altar lies a small brass key.", Ink::Item);
}

void Game::installLens() {
    if (room_ != L"observatory") {
        emit(L"There is no telescope here.", Ink::Bad);
        return;
    }
    if (flag(L"lensInstalled")) {
        emit(L"The lens is already set.", Ink::Dim);
        return;
    }
    if (!hasItem(L"lens")) {
        emit(L"You have no lens.", Ink::Bad);
        return;
    }
    removeItem(L"lens");
    setFlag(L"lensInstalled");
    nameKnown_ = true;
    addScore(10);
    emit(L"The lens settles into the brass socket with a click. You put your "
         L"eye to the telescope.",
         Ink::Text);
    emit(L"The sky rushes close: a river of stars, and there, burning at its "
         L"heart, the constellation of the Lantern. Beneath it, in silver "
         L"letters: ASTER.",
         Ink::Good);
    emit(L"The pedestal below waits for a name.", Ink::Dim);
}

void Game::cmdRead(const std::wstring& phrase) {
    std::wstring id = canonicalObject(phrase);
    if (id.empty()) {
        emit(L"Read what?", Ink::Dim);
        return;
    }

    if (room_ == L"cellar" && !lanternLightHere()) {
        emit(L"It is too dark to read.", Ink::Bad);
        return;
    }

    if (id == L"journal") {
        if (!hasItem(L"journal") && !itemVisibleHere(L"journal")) {
            emit(L"You don't have the journal.", Ink::Bad);
            return;
        }
        if (!flag(L"journalRead")) {
            setFlag(L"journalRead");
            addScore(5);
        }
        emit(L"The last entry is written in a cramped, careful hand:", Ink::Dim);
        emit(L"\"The dark is not empty. It listens. I rang the silver bell until "
             L"my hands were sore, and it went quiet. If you follow me down, take "
             L"the bell. Below the hall, behind the ghost, the key. The sky "
             L"remembers my name, even if the stone does not.\"",
             Ink::Text);
        emit(L"                    -- A.", Ink::Dim);
    } else if (id == L"chart") {
        if (room_ != L"library") {
            emit(L"The chart is not here.", Ink::Bad);
            return;
        }
        if (!flag(L"chartRead")) {
            setFlag(L"chartRead");
            addScore(5);
        }
        nameKnown_ = true;
        emit(L"The chart is drawn in silver ink. One constellation is circled: "
             L"the Lantern. Beneath it, in a cramped hand: \"Aster, who kept "
             L"us.\"",
             Ink::Good);
    } else if (id == L"sign") {
        if (room_ != L"gate") {
            emit(L"The sign is not here.", Ink::Bad);
            return;
        }
        describeObject(L"sign");
    } else if (id == L"shelves") {
        if (room_ != L"library") {
            emit(L"There is nothing to read here.", Ink::Bad);
            return;
        }
        emit(L"Most of the titles have gone to mould. One spine reads: A "
             L"BESTIARY OF QUIET THINGS. The pages are blank.",
             Ink::Text);
    } else {
        emit(L"There is nothing written there that you can read.", Ink::Dim);
    }
}

void Game::cmdSay(const std::vector<std::wstring>& args) {
    if (args.empty()) {
        emit(L"Say what?", Ink::Dim);
        return;
    }
    std::wstring phrase = join(args, 0);
    std::wstring clean = cleanPhrase(phrase);
    if (clean.find(L"aster") != std::wstring::npos) {
        speakName();
        return;
    }
    if (room_ == L"cellar" && !flag(L"bellRung") && lanternLightHere()) {
        emit(L"The ghost does not answer. Her eyes stay fixed on the bell.",
             Ink::Dim);
    } else {
        emit(L"Your voice dies against the stone. Nothing answers.", Ink::Dim);
    }
}

void Game::speakName() {
    if (room_ != L"observatory") {
        if (nameKnown_) {
            emit(L"The name \"Aster\" hangs in the air. The tower is listening, "
                 L"but this is not the room that answers.",
                 Ink::Dim);
        } else {
            emit(L"You speak into the dark. Somewhere, something turns over in "
                 L"its sleep.",
                 Ink::Dim);
        }
        return;
    }
    if (flag(L"beaconOpen")) {
        emit(L"The pedestal's mouth is already open. The stair waits.", Ink::Dim);
        return;
    }
    if (!nameKnown_) {
        emit(L"You speak a name, but the stone does not know it. You do not yet "
             L"know the name the tower remembers.",
             Ink::Bad);
        return;
    }
    setFlag(L"beaconOpen");
    addScore(5);
    emit(L"You speak the name: Aster.", Ink::Accent);
    emit(L"The pedestal's carved mouth opens. Stone grinds against stone, and "
         L"a stair of cold air uncoils toward the beacon.",
         Ink::Good);
}

// ---------------------------------------------------------------------------
// Misc commands
// ---------------------------------------------------------------------------

void Game::cmdMap() {
    emitArt(LR"(                   [ BEACON ]
                       |
                [ OBSERVATORY ]
                       |
                [ SCRIPTORIUM ]
                       |
   [ GATEHOUSE ]----[ GREAT HALL ]
                       |
                  [ COURTYARD ]
                       |
                    [ GATE ]

        (the cellar lies below the great hall))");
    blank();
    emit(L"You are in " + roomName(room_) + L".", Ink::Dim);
}

void Game::cmdScore() {
    emitSpans({
        {L"Score: ", Ink::Dim}, {std::to_wstring(score_), Ink::Accent},
        {L"    Steps: ", Ink::Dim}, {std::to_wstring(moves_), Ink::Text},
        {L"    Carrying: ", Ink::Dim}, {std::to_wstring(carried()), Ink::Text},
        {L"    Rooms seen: ", Ink::Dim},
        {std::to_wstring(visited_.size()) + L" of 8", Ink::Text},
    });
    if (gameOver_) {
        emit(L"The chronicle is closed.", Ink::Dim);
    }
}

void Game::cmdWait() {
    ++moves_;
    if (room_ == L"cellar" && lanternLightHere() && !flag(L"bellRung")) {
        emit(L"You wait. The ghost does not move. The silence is worse than a "
             L"scream.",
             Ink::Dim);
        return;
    }
    static const wchar_t* lines[] = {
        L"You wait. Dust settles in the light.",
        L"You wait. The tower creaks like a ship at anchor.",
        L"You wait. Somewhere above, a loose shutter knocks once.",
        L"You wait. Nothing comes. That is not the same as nothing watching.",
    };
    emit(lines[moves_ % 4], Ink::Dim);
}

void Game::cmdListen() {
    if (room_ == L"gate") {
        emit(L"Wind moves through the ruined wall. Somewhere above, a loose "
             L"shutter knocks.",
             Ink::Text);
    } else if (room_ == L"courtyard") {
        emit(L"Water does not run in the fountain. The tower creaks like a ship.",
             Ink::Text);
    } else if (room_ == L"gatehouse") {
        emit(L"Nothing but the rain's old memory in the stones.", Ink::Text);
    } else if (room_ == L"hall") {
        emit(L"The hearth is silent. The stair complains at the wind.", Ink::Text);
    } else if (room_ == L"cellar") {
        if (lanternLightHere() && !flag(L"bellRung")) {
            emit(L"Dripping water. And, if you are honest, breathing.", Ink::Text);
        } else {
            emit(L"Only water, counting the seconds in the dark.", Ink::Text);
        }
    } else if (room_ == L"library") {
        emit(L"Paper shifts. The tower settles around you.", Ink::Text);
    } else if (room_ == L"observatory") {
        emit(L"The stars are very loud tonight.", Ink::Text);
    } else if (room_ == L"beacon") {
        emit(L"The ember hums, one low note, like a held breath.", Ink::Text);
    }
}

void Game::cmdAbout() {
    emit(L"THE HOLLOW SPIRE", Ink::Accent);
    emit(L"An ASCII text adventure in a native Windows window.", Ink::Text);
    emit(L"Version 1.0. Story, code, and art in one small executable.", Ink::Dim);
    blank();
    emit(L"Every word is rendered with GDI. There is no game engine and no "
         L"network connection. The window is a real Win32 window, with F11 "
         L"fullscreen, resizing, menus, and Alt+F4.",
         Ink::Text);
    blank();
    emit(L"Press F1 for commands, or type SCORE to measure your progress.",
         Ink::Dim);
}

void Game::showHelp() {
    emit(L"COMMANDS", Ink::Accent);
    emit(L"  LOOK / L             describe this place", Ink::Text);
    emit(L"  EXAMINE <thing> / X  study something closely", Ink::Text);
    emit(L"  TAKE <thing>         pick it up", Ink::Text);
    emit(L"  DROP <thing>         put it down", Ink::Text);
    emit(L"  INVENTORY / I        what you carry", Ink::Text);
    emit(L"  GO <direction>       north, south, east, west, up, down", Ink::Text);
    emit(L"  USE <item> ON <thing>  try an object on something", Ink::Text);
    emit(L"  OPEN / LIGHT / RING / READ / SAY <word>", Ink::Text);
    emit(L"  MAP                  the tower at a glance", Ink::Text);
    emit(L"  WAIT / LISTEN        pause and pay attention", Ink::Text);
    emit(L"  SAVE / LOAD          keep or resume a chronicle", Ink::Text);
    emit(L"  NEW / RESTART        begin again", Ink::Text);
    emit(L"  SCORE                how you are doing", Ink::Text);
    emit(L"  QUIT                 close the window", Ink::Text);
    blank();
    emit(L"WINDOW", Ink::Accent);
    emit(L"  F11 or Alt+Enter     fullscreen", Ink::Text);
    emit(L"  Esc                  leave fullscreen / clear the line", Ink::Text);
    emit(L"  F1 help   F2 new   F5 save   F9 load", Ink::Text);
    emit(L"  Alt+F4               close", Ink::Text);
    emit(L"  Ctrl+C               copy the transcript", Ink::Text);
    emit(L"  PageUp / PageDown    scroll the story", Ink::Text);
    blank();
    emit(L"Useful words are usually enough on their own: typing LANTERN or "
         L"TELESCOPE examines it.",
         Ink::Dim);
}

// ---------------------------------------------------------------------------
// Endings
// ---------------------------------------------------------------------------

void Game::emitEnding(const std::wstring& art, const std::wstring& title,
                      const std::wstring& body, const std::wstring& rank,
                      int bonus) {
    if (gameOver_) return;
    gameOver_ = true;
    setFlag(L"ended");
    addScore(bonus);
    blank();
    emitArt(art);
    blank();
    emit(title, Ink::Room);
    emit(body, Ink::Text);
    blank();
    emit(L"THE END", Ink::Accent);
    emit(L"Rank: " + rank, Ink::Good);
    emitSpans({{L"Score: ", Ink::Dim}, {std::to_wstring(score_), Ink::Accent},
               {L"    Steps: ", Ink::Dim}, {std::to_wstring(moves_), Ink::Text}});
    blank();
    emit(L"Type NEW to begin again, or QUIT to close the window.", Ink::Dim);
}

void Game::takeEmber() {
    if (room_ != L"beacon") {
        emit(L"The ember is not here.", Ink::Bad);
        return;
    }
    emitEnding(
        LR"(          ________
         |  .--.  |
         | | ** | |
         |  '--'  |
          \______/
            \__/)",
        L"THE COLD COIN",
        L"You close your hand around the ember. It is warm. It is yours. You "
        L"climb down through the dark hall and out through the gate, and the "
        L"tower does not stop you. By morning, the spire is only a black tooth "
        L"against the sky. In your pocket, the ember has gone cold. You are rich "
        L"in light, and poor in everything else.",
        L"Thief of the Spire", 15);
}

void Game::freeEmber() {
    if (room_ != L"beacon") {
        emit(L"There is nothing here to release.", Ink::Bad);
        return;
    }
    emitEnding(
        LR"(        *       *       *
           *   .     .   *
              / \   / \
             /   \ /   \
            *     *     *
                *   *)",
        L"A SKY FULL OF LANTERNS",
        L"You open the cage. The ember rises, brightens, and breaks into a "
        L"thousand drifting sparks. The tower groans as the old light leaves "
        L"it, and stones fall from the dome. But the night sky is suddenly "
        L"full--every star sharp as a nail, and one of them, the brightest, is "
        L"new. Some lights are not meant to be kept.",
        L"The One Who Let Go", 40);
}

void Game::lightBeacon() {
    if (room_ != L"beacon") {
        emit(L"The beacon is not here.", Ink::Bad);
        return;
    }
    emitEnding(
        LR"(          \   |   /
           \  |  /
        .---\ | /---.
       /     \|/     \
      |   THE LAMP   |
      |    IS LIT    |
       \____________/
            ||||)",
        L"THE BEACON BURNS",
        L"You lift the ember from its cage. It does not burn you. You set it "
        L"into the great lamp, and the tower draws a breath. Light climbs the "
        L"spire like ivy; the mist peels back from the pilgrim road; the stars "
        L"answer. The beacon burns again, and the lost will find their way "
        L"home.",
        L"Keeper of the Spire", 50);
}

void Game::leaveTower() {
    if (room_ != L"gate") {
        emit(L"There is no road out of here--not yet.", Ink::Dim);
        return;
    }
    emitEnding(
        LR"(        ______________
       |              |
       |  THE ROAD    |
       |  GOES HOME   |
       |______________|
            ||  ||)",
        L"THE ROAD HOME",
        L"You turn your back on the spire. The mist closes over the road. "
        L"Behind you, the beacon stays dark--but somewhere in the tower, a bell "
        L"rings once, as if in farewell. Some stories are left unfinished.",
        L"The One Who Walked Away", 0);
}

// ---------------------------------------------------------------------------
// Save / load
// ---------------------------------------------------------------------------

std::wstring Game::savePath() const {
    const wchar_t* appData = _wgetenv(L"APPDATA");
    std::filesystem::path base;
    if (appData && *appData) {
        base = appData;
    } else {
        base = std::filesystem::current_path();
    }
    base /= L"HollowSpire";
    base /= L"chronicle.save";
    return base.wstring();
}

bool Game::save() {
    if (gameOver_) {
        emit(L"The chronicle has ended. There is nothing left to save.",
             Ink::Dim);
        return false;
    }
    std::filesystem::path path(savePath());
    std::error_code ec;
    std::filesystem::create_directories(path.parent_path(), ec);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        emit(L"The chronicle could not be saved.", Ink::Bad);
        return false;
    }

    std::vector<std::wstring> visited(visited_.begin(), visited_.end());
    std::vector<std::wstring> taken(taken_.begin(), taken_.end());
    std::vector<std::wstring> flags;
    for (const auto& entry : flags_) {
        flags.push_back(entry.first + L"=" + (entry.second ? L"1" : L"0"));
    }
    std::vector<std::wstring> dropped;
    for (const auto& entry : dropped_) {
        if (entry.second.empty()) continue;
        dropped.push_back(entry.first + L":" + joinNarrow(entry.second, L","));
    }

    file << narrow(kSaveVersion) << "\n";
    file << "room " << narrow(room_) << "\n";
    file << "moves " << moves_ << "\n";
    file << "score " << score_ << "\n";
    file << "nameKnown " << (nameKnown_ ? 1 : 0) << "\n";
    file << "visited " << narrow(joinNarrow(visited, L",")) << "\n";
    file << "inventory " << narrow(joinNarrow(inventory_, L",")) << "\n";
    file << "taken " << narrow(joinNarrow(taken, L",")) << "\n";
    file << "flags " << narrow(joinNarrow(flags, L",")) << "\n";

    std::wstring droppedLine;
    for (size_t i = 0; i < dropped.size(); ++i) {
        if (i) droppedLine += L";";
        droppedLine += dropped[i];
    }
    file << "dropped " << narrow(droppedLine) << "\n";

    if (!file) {
        emit(L"The chronicle could not be written.", Ink::Bad);
        return false;
    }
    emit(L"The chronicle is saved.", Ink::Good);
    return true;
}

bool Game::load() {
    std::ifstream file(std::filesystem::path(savePath()), std::ios::binary);
    if (!file) {
        emit(L"No saved chronicle was found.", Ink::Bad);
        return false;
    }

    std::wstring header;
    std::string narrowLine;
    if (!std::getline(file, narrowLine)) {
        emit(L"The saved chronicle is empty.", Ink::Bad);
        return false;
    }
    header = trim(widen(narrowLine));
    if (header != kSaveVersion) {
        emit(L"That file is not a chronicle this tower understands.", Ink::Bad);
        return false;
    }

    std::wstring room = L"gate";
    int moves = 0;
    int score = 0;
    bool nameKnown = false;
    std::vector<std::wstring> inventory;
    std::set<std::wstring> visited;
    std::set<std::wstring> taken;
    std::map<std::wstring, bool> flags;
    std::map<std::wstring, std::vector<std::wstring>> dropped;

    while (std::getline(file, narrowLine)) {
        std::wstring wide = trim(widen(narrowLine));
        if (wide.empty()) continue;
        size_t space = wide.find(L' ');
        std::wstring key = space == std::wstring::npos ? wide : wide.substr(0, space);
        std::wstring value = space == std::wstring::npos ? L"" : wide.substr(space + 1);
        if (key == L"room") {
            room = value;
        } else if (key == L"moves") {
            moves = std::max(0, _wtoi(value.c_str()));
        } else if (key == L"score") {
            score = std::max(0, _wtoi(value.c_str()));
        } else if (key == L"nameKnown") {
            nameKnown = value == L"1";
        } else if (key == L"visited" || key == L"inventory" ||
                   key == L"taken") {
            std::vector<std::wstring> parts;
            if (value != L"-") parts = splitOn(value, L',');
            for (auto& part : parts) part = trim(part);
            parts.erase(std::remove(parts.begin(), parts.end(), L""), parts.end());
            if (key == L"visited") visited.insert(parts.begin(), parts.end());
            else if (key == L"inventory") inventory = parts;
            else taken.insert(parts.begin(), parts.end());
        } else if (key == L"flags") {
            for (const auto& part : splitOn(value, L',')) {
                if (part.empty()) continue;
                size_t equals = part.find(L'=');
                if (equals == std::wstring::npos) {
                    flags[part] = true;
                } else {
                    flags[part.substr(0, equals)] = part.substr(equals + 1) == L"1";
                }
            }
        } else if (key == L"dropped") {
            for (const auto& roomEntry : splitOn(value, L';')) {
                if (roomEntry.empty()) continue;
                size_t colon = roomEntry.find(L':');
                if (colon == std::wstring::npos) continue;
                std::wstring dropRoom = roomEntry.substr(0, colon);
                std::wstring list = roomEntry.substr(colon + 1);
                std::vector<std::wstring> parts;
                if (list != L"-") parts = splitOn(list, L',');
                for (auto& part : parts) part = trim(part);
                parts.erase(std::remove(parts.begin(), parts.end(), L""), parts.end());
                if (!parts.empty()) dropped[dropRoom] = parts;
            }
        }
    }

    const std::set<std::wstring> rooms = {
        L"gate", L"courtyard", L"gatehouse", L"hall",
        L"cellar", L"library", L"observatory", L"beacon"};
    if (!rooms.count(room)) {
        emit(L"The saved chronicle points at a room that does not exist.", Ink::Bad);
        return false;
    }

    room_ = room;
    moves_ = moves;
    score_ = score;
    nameKnown_ = nameKnown;
    inventory_ = inventory;
    visited_ = visited;
    taken_ = taken;
    flags_ = flags;
    dropped_ = dropped;
    gameOver_ = false;
    visited_.insert(room_);

    emit(L"The chronicle resumes...", Ink::System);
    emitRoom(true);
    return true;
}

// ---------------------------------------------------------------------------
// Command entry point
// ---------------------------------------------------------------------------

Action Game::submit(const std::wstring& input) {
    std::wstring trimmed = trim(input);
    if (trimmed.empty()) return Action::None;

    auto words = tokenize(trimmed);
    if (words.empty()) return Action::None;

    std::wstring verb = words[0];
    std::wstring rest = join(words, 1);

    if (verb == L"quit" || verb == L"exit" || verb == L"q") {
        return Action::Quit;
    }
    if (verb == L"new" || verb == L"restart") {
        return Action::NewGame;
    }
    if (verb == L"clear" || verb == L"cls") {
        return Action::Clear;
    }
    if (verb == L"help" || verb == L"commands" || verb == L"man") {
        showHelp();
        return Action::None;
    }
    if (verb == L"about" || verb == L"version") {
        cmdAbout();
        return Action::None;
    }
    if (verb == L"save") {
        save();
        return Action::None;
    }
    if (verb == L"load" || verb == L"restore") {
        load();
        return Action::None;
    }
    if (verb == L"score") {
        cmdScore();
        return Action::None;
    }

    if (gameOver_) {
        emit(L"The story has ended. Type NEW to begin again, or QUIT to close "
             L"the window.",
             Ink::Dim);
        return Action::None;
    }

    std::wstring loneDirection = canonicalDirection(verb);
    if (words.size() == 1 && !loneDirection.empty()) {
        move(loneDirection);
        return Action::None;
    }

    if (verb == L"look" || verb == L"l" || verb == L"see") {
        std::vector<std::wstring> args(words.begin() + 1, words.end());
        cmdLook(args);
    } else if (verb == L"examine" || verb == L"x" || verb == L"inspect" ||
               verb == L"check") {
        cmdExamine(rest);
    } else if (verb == L"take" || verb == L"get" || verb == L"grab" ||
               verb == L"collect") {
        cmdTake(rest);
    } else if (verb == L"drop" || verb == L"put") {
        cmdDrop(rest);
    } else if (verb == L"inventory" || verb == L"inv" || verb == L"i" ||
               verb == L"items") {
        cmdInventory();
    } else if (verb == L"go" || verb == L"walk" || verb == L"move" ||
               verb == L"head" || verb == L"travel") {
        cmdGo(rest);
    } else if (verb == L"use" || verb == L"apply") {
        std::vector<std::wstring> args(words.begin() + 1, words.end());
        cmdUse(args);
    } else if (verb == L"open" || verb == L"unlock" || verb == L"pry") {
        cmdOpen(rest);
    } else if (verb == L"light" || verb == L"ignite" || verb == L"kindle") {
        cmdLight(rest);
    } else if (verb == L"ring") {
        cmdRing(rest);
    } else if (verb == L"read") {
        cmdRead(rest);
    } else if (verb == L"say" || verb == L"speak" || verb == L"whisper") {
        std::vector<std::wstring> args(words.begin() + 1, words.end());
        cmdSay(args);
    } else if (verb == L"map" || verb == L"atlas") {
        cmdMap();
    } else if (verb == L"wait" || verb == L"z" || verb == L"rest") {
        cmdWait();
    } else if (verb == L"listen" || verb == L"hear") {
        cmdListen();
    } else if (verb == L"leave" || verb == L"abandon") {
        leaveTower();
    } else if (verb == L"free" || verb == L"release") {
        freeEmber();
    } else if (verb == L"fill") {
        std::vector<std::wstring> useArgs = {L"oil", L"on", L"lantern"};
        cmdUse(useArgs);
    } else if (verb == L"install" || verb == L"fit") {
        std::vector<std::wstring> useArgs = {L"lens", L"on", L"telescope"};
        cmdUse(useArgs);
    } else {
        // Typing a single object name is treated as EXAMINE.
        std::wstring object = canonicalObject(trimmed);
        if (!object.empty()) {
            cmdExamine(trimmed);
        } else {
            emit(L"I don't understand \"" + trimmed + L"\". Type HELP for a list "
                 L"of commands.",
                 Ink::Bad);
        }
    }
    return Action::None;
}

void Game::cmdRing(const std::wstring& phrase) {
    (void)phrase;
    ringBell();
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

void Game::newGame() {
    room_ = L"gate";
    moves_ = 0;
    score_ = 0;
    gameOver_ = false;
    nameKnown_ = false;
    inventory_.clear();
    taken_.clear();
    visited_.clear();
    flags_.clear();
    dropped_.clear();

    emit(L"The beacon of the Hollow Spire has been dark for thirty years.",
         Ink::Text);
    emit(L"You are a lamplighter of the old order, sent up the pilgrim road to "
         L"learn why. The mist is cold, the gate is open, and somewhere above "
         L"you a bell is waiting.",
         Ink::Text);
    blank();

    emitRoom(true);
}

std::wstring Game::location() const {
    return roomName(room_);
}

std::wstring Game::statusLine() const {
    return L"steps " + std::to_wstring(moves_) + L"  |  score " +
           std::to_wstring(score_) + L"  |  carrying " +
           std::to_wstring(inventory_.size());
}

std::vector<std::wstring> Game::completerWords() const {
    static const std::vector<std::wstring> words = {
        L"look", L"examine", L"take", L"drop", L"inventory", L"go", L"use",
        L"open", L"light", L"ring", L"read", L"say", L"map", L"wait",
        L"listen", L"save", L"load", L"new", L"score", L"help", L"clear",
        L"quit", L"leave", L"free", L"release", L"install", L"fill",
        L"north", L"south", L"east", L"west", L"up", L"down",
        L"lantern", L"lamp", L"oil", L"crowbar", L"journal", L"bell",
        L"key", L"lens", L"coin", L"chart", L"hatch", L"cabinet",
        L"telescope", L"pedestal", L"altar", L"ghost", L"skeleton",
        L"statue", L"fountain", L"hearth", L"sign", L"door", L"brazier",
        L"beacon", L"stair", L"shelves", L"portrait", L"window", L"sky",
        L"ember", L"cage", L"aster", L"about", L"listen", L"score"
    };
    return words;
}
