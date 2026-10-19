#pragma once

#include <functional>
#include <map>
#include <set>
#include <string>
#include <vector>

// The colours the game asks the window to use. The actual RGB values live in
// main.cpp so the story code stays independent of the renderer.
enum class Ink {
    Text,
    Accent,
    Room,
    Art,
    Dim,
    Item,
    Good,
    Bad,
    System,
    Command
};

struct Span {
    std::wstring text;
    Ink ink = Ink::Text;
};

// A logical paragraph. The window wraps it to whatever width is available.
struct Block {
    std::vector<Span> spans;
    bool art = false;
};

enum class Action {
    None,
    Quit,
    NewGame,
    Clear
};

class Game {
public:
    using Sink = std::function<void(const Block&)>;

    explicit Game(Sink sink);

    void newGame();
    Action submit(const std::wstring& input);
    void showHelp();

    bool save();
    bool load();

    std::wstring location() const;
    std::wstring locationId() const { return room_; }
    std::wstring statusLine() const;
    std::vector<std::wstring> completerWords() const;

    int score() const { return score_; }
    int moves() const { return moves_; }
    int carried() const { return static_cast<int>(inventory_.size()); }
    bool gameOver() const { return gameOver_; }

private:
    // ---- output -----------------------------------------------------------
    void emit(const std::wstring& text, Ink ink = Ink::Text);
    void emitSpans(std::vector<Span> spans);
    void blank();
    void emitArt(const std::wstring& art);
    void emitRoom(bool withArt);
    void emitItemsHere();
    void emitExits();
    void emitEnding(const std::wstring& art, const std::wstring& title,
                    const std::wstring& body, const std::wstring& rank,
                    int bonus);

    // ---- world ------------------------------------------------------------
    std::wstring roomName(const std::wstring& id) const;
    std::wstring roomDescription(const std::wstring& id);
    std::wstring roomArt(const std::wstring& id) const;
    std::vector<Span> itemLine();
    std::vector<std::wstring> visibleItems() const;
    bool itemAvailableHere(const std::wstring& id) const;
    bool itemVisibleHere(const std::wstring& id) const;
    bool hasItem(const std::wstring& id) const;
    void addItem(const std::wstring& id);
    void removeItem(const std::wstring& id);
    bool flag(const std::wstring& name) const;
    void setFlag(const std::wstring& name, bool value = true);
    bool lanternLightHere() const;
    void addScore(int amount) { score_ += amount; }

    // ---- parsing ----------------------------------------------------------
    static std::vector<std::wstring> tokenize(const std::wstring& text);
    static std::wstring join(const std::vector<std::wstring>& words, size_t from);
    static std::wstring canonicalObject(const std::wstring& phrase);
    static std::wstring canonicalDirection(const std::wstring& word);

    // ---- commands ---------------------------------------------------------
    void cmdLook(const std::vector<std::wstring>& args);
    void cmdExamine(const std::wstring& phrase);
    void cmdTake(const std::wstring& phrase);
    void cmdDrop(const std::wstring& phrase);
    void cmdInventory();
    void cmdGo(const std::wstring& phrase);
    bool move(const std::wstring& direction);
    void cmdUse(const std::vector<std::wstring>& args);
    void cmdOpen(const std::wstring& phrase);
    void cmdLight(const std::wstring& phrase);
    void cmdRing(const std::wstring& phrase);
    void cmdRead(const std::wstring& phrase);
    void cmdSay(const std::vector<std::wstring>& args);
    void cmdMap();
    void cmdScore();
    void cmdWait();
    void cmdListen();
    void cmdAbout();
    void describeObject(const std::wstring& id);
    void openHatch();
    void openCabinet();
    void lightLantern();
    void ringBell();
    void installLens();
    void speakName();
    void takeEmber();
    void freeEmber();
    void lightBeacon();
    void leaveTower();

    std::wstring savePath() const;

    // ---- state ------------------------------------------------------------
    Sink sink_;
    std::wstring room_ = L"gate";
    int moves_ = 0;
    int score_ = 0;
    bool gameOver_ = false;
    bool nameKnown_ = false;

    std::vector<std::wstring> inventory_;
    std::set<std::wstring> taken_;
    std::set<std::wstring> visited_;
    std::map<std::wstring, bool> flags_;
    std::map<std::wstring, std::vector<std::wstring>> dropped_;
};
