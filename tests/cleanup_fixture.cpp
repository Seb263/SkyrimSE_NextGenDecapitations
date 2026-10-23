// The runner inserts the real MaintainDecapitations cleanup loop below.
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>

namespace RE { using FormID = std::uint32_t; }
struct DecapEntry { RE::FormID headId; float gameTime; };
struct MiscUtils {
    static bool IsFormIDValid(RE::FormID id) { return id > 0 && id < 0xFFFFFFFF; }
};
#define TRACE(...) ((void)0)
std::unordered_map<RE::FormID, DecapEntry> actorToHeadMap;
std::unordered_map<RE::FormID, RE::FormID> headToActorMap;

void Cleanup(const std::vector<RE::FormID>& toRemove)
{
    // INSERT_PRODUCTION_CLEANUP_LOOP
}

void require(bool condition, const char* message)
{
    if (!condition) { std::cerr << message << '\n'; std::exit(2); }
}
void reset() { actorToHeadMap.clear(); headToActorMap.clear(); }
void add(RE::FormID actor, RE::FormID head)
{
    actorToHeadMap.emplace(actor, DecapEntry{head, 10.0f});
    headToActorMap.emplace(head, actor);
}
void empty() { require(actorToHeadMap.empty() && headToActorMap.empty(), "stale entries remain"); }
int main()
{
    Cleanup({}); empty();
    Cleanup({0xFF000100}); empty();

    add(0x12345, 0xFF000100);
    Cleanup({0xFF000100}); empty();

    add(0x12345, 0xFF000100); add(0x22345, 0xFF000200); add(0x32345, 0xFF000300);
    Cleanup({0xFF000100, 0xFF000300});
    require(actorToHeadMap.size() == 1 && headToActorMap.size() == 1, "wrong removal count");
    require(actorToHeadMap.at(0x22345).headId == 0xFF000200 && headToActorMap.at(0xFF000200) == 0x22345,
            "unrelated mapping changed");
    Cleanup({0xFF000200, 0xFF000200}); empty();

    // Missing reverse entries and invalid origin IDs must remain harmless.
    headToActorMap.emplace(0xFF000100, 0x12345);
    Cleanup({0xFF000100}); empty();
    headToActorMap.emplace(0xFF000100, 0);
    headToActorMap.emplace(0xFF000200, 0xFFFFFFFF);
    Cleanup({0xFF000100, 0xFF000200}); empty();

    std::vector<RE::FormID> removed;
    for (RE::FormID i = 1; i <= 4096; ++i) {
        add(i, 0xFF000000 + i);
        if (i % 2 == 0) removed.push_back(0xFF000000 + i);
    }
    Cleanup(removed);
    require(actorToHeadMap.size() == 2048 && headToActorMap.size() == 2048, "batch count wrong");
    for (RE::FormID i = 1; i <= 4096; ++i) {
        require(actorToHeadMap.contains(i) == (i % 2 != 0), "batch removed wrong actor");
        require(headToActorMap.contains(0xFF000000 + i) == (i % 2 != 0), "batch removed wrong head");
    }
    reset();
    for (RE::FormID cycle = 0; cycle < 1000; ++cycle) {
        add(0x12345, 0xFF000100);
        Cleanup({0xFF000100}); empty();
    }
    std::cout << "PASS: empty, missing, single, selective, duplicate, absent reverse, invalid origin, "
                 "4096-entry batch, and 1000 repeated cleanup cycles\n";
}
