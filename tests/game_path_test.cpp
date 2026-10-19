#include "../src/game.h"

#include <cstdio>
#include <cwchar>
#include <filesystem>
#include <string>

namespace {

int failures = 0;

void expect(bool condition, const char* label) {
    if (!condition) {
        std::printf("FAIL: %s\n", label);
        ++failures;
    } else {
        std::printf("ok: %s\n", label);
    }
}

}  // namespace

int main() {
    std::filesystem::path temp =
        std::filesystem::temp_directory_path() / L"hollowspire-game-tests";
    std::filesystem::create_directories(temp);
    _wputenv_s(L"APPDATA", temp.wstring().c_str());

    std::wstring output;
    auto collect = [&](const Block& block) {
        for (const auto& span : block.spans) output += span.text;
        output += L"\n";
    };

    Game game(collect);
    auto run = [&](const wchar_t* command) {
        output.clear();
        Action action = game.submit(command);
        return action;
    };
    auto has = [&](const wchar_t* needle) {
        return std::wcsstr(output.c_str(), needle) != nullptr;
    };

    game.newGame();
    expect(game.locationId() == L"gate", "new game starts at the gate");

    run(L"take lantern");
    expect(game.carried() == 1, "lantern can be taken");
    run(L"north");
    run(L"west");
    expect(game.locationId() == L"gatehouse", "gatehouse is west of courtyard");
    run(L"take crowbar");
    run(L"take oil");
    run(L"take journal");
    run(L"read journal");
    expect(has(L"silver bell"), "journal reveals the bell clue");

    run(L"east");
    run(L"north");
    expect(game.locationId() == L"hall", "hall is north of courtyard");
    run(L"open hatch");
    run(L"down");
    expect(game.locationId() == L"cellar", "hatch leads to cellar");
    expect(has(L"Blackness"), "cellar is dark before the lantern is lit");
    run(L"light lantern");
    expect(has(L"The lantern pushes the dark back"), "lantern lights the cellar");
    run(L"take bell");
    run(L"ring bell");
    expect(has(L"brass key"), "ringing the bell releases the key");
    run(L"take key");

    run(L"up");
    run(L"up");
    expect(game.locationId() == L"library", "stairs lead to the scriptorium");
    run(L"open cabinet");
    run(L"take lens");
    run(L"read chart");
    expect(has(L"Aster"), "star chart reveals the keeper's name");
    run(L"up");
    expect(game.locationId() == L"observatory", "stair leads to observatory");

    output.clear();
    Action saved = game.submit(L"save");
    (void)saved;
    expect(has(L"saved"), "game can be saved");

    run(L"use lens on telescope");
    expect(has(L"ASTER"), "telescope reveals the name");
    run(L"say aster");
    expect(has(L"stair of cold air"), "speaking the name opens the beacon stair");
    run(L"up");
    expect(game.locationId() == L"beacon", "beacon is reachable");
    run(L"light beacon");
    expect(game.gameOver(), "the good ending closes the chronicle");
    expect(has(L"THE BEACON BURNS"), "good ending text is printed");
    expect(game.score() >= 90, "good ending awards a keeper's score");

    // A separate run checks that loading restores the room and inventory.
    Game second(collect);
    auto runSecond = [&](const wchar_t* command) {
        output.clear();
        return second.submit(command);
    };
    second.newGame();
    runSecond(L"take lantern");
    runSecond(L"save");
    runSecond(L"north");
    expect(second.locationId() == L"courtyard", "moved away before loading");
    runSecond(L"load");
    expect(second.locationId() == L"gate", "load returns to the saved room");
    expect(second.carried() == 1, "load restores inventory");

    std::printf("\n%s (%d failure%s)\n", failures == 0 ? "PASS" : "FAIL",
                failures, failures == 1 ? "" : "s");
    return failures == 0 ? 0 : 1;
}
