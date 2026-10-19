// WreckingBall: FH6-style destruction for Forza Horizon 2 on PC (Azimuth), as a single
// fh2-mods DLL on the FH2 Mod Platform. Nothing in the game's files is changed; it all
// happens through hooks on the 24 Nov 2015 build.
//
//  - Trees and bushes: every procedural tree gets a smashable group (an apple or olive
//    tree definition) and its solid trunk collision is filtered out, so the game's own
//    smashable code breaks it. Big trees count as log piles (Lumberjack).
//  - Logs: a stretched trunk piece is thrown out of each broken tree.
//  - Fences, railings and roadside clutter: procedural barriers and placed objects that
//    have a breakable version somewhere in the game break like it; event and crowd
//    barriers, and ones in front of a drop, stay solid.
//  - Telephone poles: placed wooden poles fall over when hit; the big concrete pylons,
//    which have no collision in the game, are made solid.
//  - Debris clean-up, and model fallbacks where the apple tree isn't streamed in.
//
// Notes on the game code each part relies on are next to it.
#include "fh2mod.h"

#include <windows.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cwchar>
#include <cstring>
#include <mutex>
#include <set>
#include <unordered_map>
#include <vector>
#include <string>
#include <utility>

namespace {

const FH2ModApi* g_api = nullptr;
FH2Mod* g_mod          = nullptr;
uintptr_t g_base       = 0;

// ---- settings ---------------------------------------------------------------
int g_inject      = 1;
int g_treeDef     = 97;
int g_loadedLooks = 1;   // fall back on loaded models for tree pieces, logs and poles (LoadedModelFallback)
int g_bigTreeDef  = 98;
int g_logLibs     = 1;
int g_maxLogLines = 4000;
char g_prefix[256] = "*";
char g_sizeRules[512] = "Bush_:0.5,Crop_:0.7,VINEYARD_:0.5,Skinny:0.8,Palm:0.9,Pine:1.3,Tall:1.3,Trees_:1.0";
// Bushes and crops: models whose name contains one of these break as BushDef (the game's own
// flattening bush: a small foliage puff, no pieces, no log, no Lumberjack). "Bush_" doesn't match
// the olive trees ("Trees_Olive_A_Bushy"). The vineyard rows (VINEYARD_*) are vines on little
// wooden stakes: they flatten like bushes rather than throwing a log.
char g_bushModels[256] = "Bush_,Crop_,VINEYARD_";
char g_skipModels[256] = "";   // models that stay solid, as without the mod
int g_bushDef = 110;           // GEN_OBJ_Bush_Flatten; 0 = bushes break like trees (1.1 behaviour)
// The game's own breakable definitions that are plants, not trees: they break as the game made
// them, but like a bush (no log, the bush slowdown). 266 is VEG_Crops_vineyardB_Low_SegB, the
// vineyard row the game itself made breakable (the other vineyard rows are BushModels).
char g_bushDefs[64] = "266";
int nd(uint8_t* table, int def);
bool bush_like_def(uint8_t* defTable, int lo) {
    for (const char* r = g_bushDefs; *r;) {
        while (*r == ' ' || *r == ',') ++r;
        if (!*r) break;
        if (lo == nd(defTable, std::atoi(r))) return true;
        while (*r && *r != ',') ++r;
    }
    return false;
}
float g_bushSpeedLoss = 0.04f; // fixed share of speed a bush takes; below 0 = by size, like trees
char g_bushEffects[256] = "";  // extra effects on top of the bush's own puff
int g_sizeTiers = 0;
int g_treeObjectType = 5;
float g_lumberjackMinSize = 1.6f;
float g_massMax = 25.0f;
float g_massScale = 4.0f, g_massExponent = 2.0f, g_breakSpeedScale = 1.0f, g_leanFactor = 0.35f;
int g_filterCollision = 2;
// ---- breakable barriers (fences, railings) ----
int g_barriers = 1, g_barrierSkipEvents = 1;
char g_barrierLibs[256]    = "BARRIERTEMPLATE";
char g_barrierRules[512]   = "Wood:113,Wicker:116,Tarp:115,Wire:96,Chain:96,Plastic:129,Bollard:14,Fenc:114,Rail:114,Barrier:114";
char g_barrierExclude[512] = "Crowd,FEST_,Carmeet,Banner,Festival,Concrete,Blast,Wall,Bridge,Gate,Hydraulic,Tyre,Tire";
float g_barrierSpeedLoss = 0.04f, g_barrierPointRadius = 0.9f;
float g_trunkRadius   = 1.6f;

// ---- game addresses (RVAs, 24 Nov 2015 build) -------------------------------
constexpr uintptr_t RVA_DESERIALIZE   = 0x1871B90; // ProceduralModelsData::Serialize(data, archive)
constexpr uintptr_t RVA_GROUP_RESIZE  = 0x1881EC0; // vector<Group>::resize(vec, n)
constexpr uintptr_t RVA_CELL_ADD      = 0x036A330; // CWorldCollObjMan::AddCell(mgr, msg)
constexpr uintptr_t RVA_CELL_SPAWN    = 0x0368D90; // CWorldCollObjMan::SpawnCell(mgr, cell)
constexpr uintptr_t RVA_SWAP          = 0x0368C60; // CWorldCollObjMan::MakeDynamic(mgr, obj)
constexpr uintptr_t RVA_MODELS_VTABLE = 0x2F5A300; // Procedural::ProceduralModels vftable
constexpr uintptr_t RVA_COLLOBJ_VTABLE = 0x2C6A208; // CWorldCollObj vftable (slot 0 = OnCollision)
constexpr uintptr_t RVA_TEMPLATE_QUERY = 0x12CC1A0; // collision template grid query (T, box, max, outTris, outVerts, flag)
constexpr uintptr_t RVA_HOLDER_LOAD    = 0x12B2500; // make a collision instance's template available (holder)
constexpr uintptr_t RVA_TEMPLATE_RES_VT = 0x2EB43C0; // StreamedPhysics::CollisionTemplateResource vftable

// ---- logging ----------------------------------------------------------------
std::mutex g_logLock;
FILE* g_log = nullptr;
std::atomic<int> g_lines{0};

void logf(const char* fmt, ...) {
    if (g_lines.fetch_add(1) >= g_maxLogLines) return;
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    std::lock_guard<std::mutex> l(g_logLock);
    if (!g_log) return;
    SYSTEMTIME t;
    GetLocalTime(&t);
    fprintf(g_log, "%02d:%02d:%02d.%03d %s\n", t.wHour, t.wMinute, t.wSecond, t.wMilliseconds, buf);
    fflush(g_log);
}
int g_debug = 0;   // Debug=1: detailed log, F8 snapshot, definitions dump
#define dlogf(...) do { if (g_debug) logf(__VA_ARGS__); } while (0)

// ---- MSVC 2012 std::string / std::vector layouts -----------------------------
struct MsvcString {
    union { char buf[16]; char* ptr; };
    size_t size;
    size_t cap;
    const char* c_str() const { return cap >= 16 ? ptr : buf; }
};
static_assert(sizeof(MsvcString) == 0x20);

struct MsvcVector {
    uint8_t* first;
    uint8_t* last;
    uint8_t* end;
    size_t count(size_t elem) const { return first ? (size_t)(last - first) / elem : 0; }
};

struct Group { int32_t def, model, start, count, bbBatch, bbStart, bbCount; };
static_assert(sizeof(Group) == 0x1C);

// ProceduralModels data object (0x100 bytes)
constexpr size_t D_LIB = 0x00, D_GROUPS = 0x60, D_BILLBOARDS = 0x88, D_MODELS = 0xA0;
// Batch (0x58 bytes)
constexpr size_t BATCH_SIZE = 0x58, B_INSTANCES = 0x00, B_NAME = 0x30;
constexpr size_t INSTANCE_SIZE = 0x20;

const char* str_at(const uint8_t* p) { return reinterpret_cast<const MsvcString*>(p)->c_str(); }
MsvcVector* vec_at(uint8_t* p) { return reinterpret_cast<MsvcVector*>(p); }

bool starts_with_ci(const char* s, const char* prefix) {
    for (; *prefix; ++s, ++prefix)
        if (!*s || (*s | 0x20) != (*prefix | 0x20)) return false;
    return true;
}

// ---- stats ------------------------------------------------------------------
std::atomic<int> g_procsInjected{0}, g_treesInjected{0}, g_treeCells{0}, g_treeSpawns{0}, g_swaps{0};
std::mutex g_seenLock;
std::set<std::pair<int, std::string>> g_seenSmash; // (def, model) already logged
std::set<std::string> g_seenLibs;
std::set<std::string> g_seenModels;  // tree-procedural models already logged (Debug)

void mark_tree_proc(int i);

// ---- trunk points: where breakable trees stand ------------------------------
// A spatial hash of tree positions whose static trunk collision is removed.
// Writers take a lock; readers walk the lists without one (nodes never change
// after they're published).
struct TrunkNode { float x, y, z, size; int32_t next; };
constexpr int TRUNK_POOL = 1 << 19, TRUNK_BUCKETS = 1 << 16;
constexpr float TRUNK_CELL = 4.0f;
TrunkNode* g_trunkPool = nullptr;
std::atomic<int32_t> g_trunkHead[TRUNK_BUCKETS];
std::atomic<int> g_trunkCount{0};
std::mutex g_trunkLock;

uint32_t trunk_bucket(int cx, int cz) { return ((uint32_t)cx * 73856093u ^ (uint32_t)cz * 19349663u) & (TRUNK_BUCKETS - 1); }
int trunk_cell(float v) { return (int)std::floor(v / TRUNK_CELL); }

// Per barrier point: the fence's direction, and whether solid ground has been seen on
// each side of it (bit 0 / bit 1). A barrier only gives way once both sides are known to
// be ground; one with nothing behind it (a drop, an edge of the drivable world) stays solid.
struct BarInfo { float dx, dz; std::atomic<uint8_t> flags; };
BarInfo* g_barInfo = nullptr;
float g_barrierProbe = 3.0f;
int g_barrierNeedGround = 1;
inline bool bar_ok(const TrunkNode* t) {
    if (!g_barrierNeedGround || !g_barInfo) return true;
    return (g_barInfo[t - g_trunkPool].flags.load(std::memory_order_relaxed) & 3) == 3;
}

int trunk_add(float x, float y, float z, float size = 1.0f) {
    if (!g_trunkPool) return -1;
    std::lock_guard<std::mutex> l(g_trunkLock);
    int cx = trunk_cell(x), cz = trunk_cell(z);
    uint32_t b = trunk_bucket(cx, cz);
    for (int32_t i = g_trunkHead[b].load(std::memory_order_relaxed); i >= 0; i = g_trunkPool[i].next) {
        const TrunkNode& t = g_trunkPool[i];
        if (std::fabs(t.x - x) < 0.05f && std::fabs(t.z - z) < 0.05f && std::fabs(t.y - y) < 1.0f) return i; // already known
    }
    int n = g_trunkCount.load(std::memory_order_relaxed);
    if (n >= TRUNK_POOL) return -1;
    g_trunkPool[n] = TrunkNode{x, y, z, size, g_trunkHead[b].load(std::memory_order_relaxed)};
    g_trunkCount.store(n + 1, std::memory_order_relaxed);
    g_trunkHead[b].store(n, std::memory_order_release);
    return n;
}

// Collect trunk points within the (x,z) rectangle, up to max.
int trunk_find(float x0, float z0, float x1, float z1, const TrunkNode** out, int max) {
    if (!g_trunkPool) return 0;
    int n = 0;
    int cx0 = trunk_cell(x0), cx1 = trunk_cell(x1), cz0 = trunk_cell(z0), cz1 = trunk_cell(z1);
    if (cx1 - cx0 > 16 || cz1 - cz0 > 16) return 0; // huge query box: not a car touching a trunk
    for (int cx = cx0; cx <= cx1; ++cx)
        for (int cz = cz0; cz <= cz1; ++cz)
            for (int32_t i = g_trunkHead[trunk_bucket(cx, cz)].load(std::memory_order_acquire); i >= 0;
                 i = g_trunkPool[i].next) {
                const TrunkNode& t = g_trunkPool[i];
                if (t.x < x0 || t.x > x1 || t.z < z0 || t.z > z1) continue;
                if (n < max) out[n++] = &t;
            }
    return n;
}

using DecodeInstanceFn = float* (*)(uint8_t* data, float* outMatrix, const uint8_t* inst, float* outScale);
constexpr uintptr_t RVA_DECODE_INSTANCE = 0x1874600;


// ---- size tiers: each tree gets a copy of the tree definition with its own mass
constexpr int NUM_TIERS = 24, TIER_ZERO = 8;   // size = 2^((tier - 8) / 4): 0.25 .. ~11
constexpr int CLONE_BASE = 0xF000;             // definition IDs for the copies
constexpr int BUSH_TIER = 0xFF;                // a bush's entry in g_childTier (it uses BushDef, not a tier)
int nd(uint8_t* table, int def);              // a mainland definition number on this map (below)
int tree_def(uint8_t* table, bool big);       // this map's tree definition
constexpr size_t DEF_SIZE = 0xF0;              // CCollObjPhysicsDefinition
float tier_size(int t) { return std::pow(2.0f, (t - TIER_ZERO) / 4.0f); }
int size_tier(float size) {
    if (!(size > 0)) size = 1;
    int t = (int)std::lround(std::log2(size) * 4.0f) + TIER_ZERO;
    return t < 0 ? 0 : t >= NUM_TIERS ? NUM_TIERS - 1 : t;
}
bool contains_ci(const char* s, const char* needle);
// first rule whose text appears in the model name wins: "Bush_:0.5,Pine:1.3"
float model_size_factor(const char* name) {
    const char* r = g_sizeRules;
    while (*r) {
        char key[64]; int k = 0;
        while (*r && *r != ':' && *r != ',' && k < 63) key[k++] = *r++;
        key[k] = 0;
        float f = 1.0f;
        if (*r == ':') { f = (float)std::atof(++r); while (*r && *r != ',') ++r; }
        if (*r == ',') ++r;
        while (k && key[k - 1] == ' ') key[--k] = 0;
        const char* kk = key; while (*kk == ' ') ++kk;
        if (*kk && contains_ci(name, kk)) return f;
    }
    return 1.0f;
}
bool name_selected(const char* name) {
    if (starts_with_ci(name, "FEST_")) return false;
    const char* r = g_prefix;
    while (*r == ' ') ++r;
    if (r[0] == '*' ) return true;
    while (*r) {
        char key[64]; int k = 0;
        while (*r && *r != ',' && k < 63) key[k++] = *r++;
        key[k] = 0;
        if (*r == ',') ++r;
        if (k && starts_with_ci(name, key)) return true;
    }
    return false;
}

using DefLookupFn = uint8_t* (*)(uint8_t* table, uint32_t id);
using DefModelFn  = int (*)(uint8_t* table, uint32_t id);
DefLookupFn o_defLookup = nullptr;
DefModelFn o_defModel   = nullptr;
std::atomic<uint8_t*> g_clone[NUM_TIERS];
std::atomic<uint8_t*> g_cloneSource{nullptr};
std::mutex g_cloneLock;

uint8_t* get_clone(uint8_t* table, int tier) {
    uint8_t* src = o_defLookup(table, 0xFFFF0000u | (uint32_t)(tree_def(table, false) & 0xFFFF));
    if (src != g_cloneSource.load(std::memory_order_acquire)) {
        std::lock_guard<std::mutex> l(g_cloneLock);
        if (src != g_cloneSource.load()) {
            for (auto& c : g_clone) c.store(nullptr);
            g_cloneSource.store(src, std::memory_order_release);
            dlogf("tree definition %d: mass=%.1f radius=%.2f breakSpeed=%.1f mph lowSpeedBehaviour=%d type=%d/%d",
                 g_treeDef, *reinterpret_cast<float*>(src + 0x9C), *reinterpret_cast<float*>(src + 0x98),
                 *reinterpret_cast<float*>(src + 0xB0), *reinterpret_cast<int32_t*>(src + 0xCC), src[0xC8], src[0xC9]);
        }
    }
    uint8_t* c = g_clone[tier].load(std::memory_order_acquire);
    if (c) return c;
    std::lock_guard<std::mutex> l(g_cloneLock);
    if ((c = g_clone[tier].load())) return c;
    // copies live for the whole session (a few KB); never freed, so a reload can't leave them dangling
    c = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!c) return src;
    std::memcpy(c, src, DEF_SIZE);
    float size = tier_size(tier);
    float mass = *reinterpret_cast<float*>(src + 0x9C) * g_massScale * std::pow(size, g_massExponent);
    if (mass > g_massMax * *reinterpret_cast<float*>(src + 0x9C)) mass = g_massMax * *reinterpret_cast<float*>(src + 0x9C);
    *reinterpret_cast<float*>(c + 0x9C) = mass;
    *reinterpret_cast<float*>(c + 0xB0) *= g_breakSpeedScale;
    // ObjectTypeIndex: SmashableObjectTypes.xml order (0 Default, 1 Fence, 2 Mailbox, 3 RoadClosedSign,
    // 4 TrashCan, 5 LogPiles, 6 FruitCart, 7 FireHydrant, 8 TargetTypeA). LogPiles awards Lumberjack.
    // Only big trees count as log piles (Lumberjack); smaller ones stay as TreeDef's type.
    if (g_treeObjectType >= 0 && g_treeObjectType < 256 && size >= g_lumberjackMinSize - 0.01f) c[0xC8] = (uint8_t)g_treeObjectType;
    g_clone[tier].store(c, std::memory_order_release);
    dlogf("made tree definition for size %.2f: mass %.1f, break speed %.1f mph, object type %d", size, mass,
         *reinterpret_cast<float*>(c + 0xB0), c[0xC8]);
    return c;
}


// ---- one-off dump of every collidable definition the game has loaded -----------
const char* const k_smashNames[] = {
#include "smashable_names.inc"
};
std::atomic<int> g_defsDumped{0};
int g_dumpDefs = 0;
using NameHashFn = uint32_t (*)(const char* s, bool caseless, uint32_t seed);
const char* hash_name(uint32_t h) {
    static std::unordered_map<uint32_t, const char*> map;
    if (map.empty()) {
        auto hf = reinterpret_cast<NameHashFn>(g_base + 0x126E020);
        for (const char* n : k_smashNames) map[hf(n, true, 0x1505)] = n;
    }
    auto it = map.find(h);
    return it == map.end() ? "?" : it->second;
}
// true if [p, p+n) is committed, readable memory
bool readable(const void* p, size_t n) {
    uintptr_t a = reinterpret_cast<uintptr_t>(p);
    if (a < 0x10000 || (a & 7)) return false;
    MEMORY_BASIC_INFORMATION mbi;
    if (!VirtualQuery(p, &mbi, sizeof mbi) || mbi.State != MEM_COMMIT) return false;
    if (mbi.Protect & (PAGE_NOACCESS | PAGE_GUARD)) return false;
    if (!(mbi.Protect & (PAGE_READONLY | PAGE_READWRITE | PAGE_WRITECOPY | PAGE_EXECUTE_READ | PAGE_EXECUTE_READWRITE | PAGE_EXECUTE_WRITECOPY))) return false;
    return a + n <= reinterpret_cast<uintptr_t>(mbi.BaseAddress) + mbi.RegionSize || readable(reinterpret_cast<const uint8_t*>(p) + n - 8, 8);
}
void dump_defs(uint8_t* table) {
    if (!g_dumpDefs || g_defsDumped.exchange(1)) return;
    __try {
        uint8_t** list = *reinterpret_cast<uint8_t***>(table + 8);
        FILE* f = _wfopen((std::wstring(g_api->azimuth_dir) + L"\\fh2-mods\\WreckingBall-defs.txt").c_str(), L"w");
        if (!f) return;
        fprintf(f, "index | name (from the audio list) | mass | radius | breakSpeedMph | model | type/effect | pieces\n");
        int found = 0, misses = 0;
        // the table has no count we know of: walk until 64 entries in a row aren't definitions.
        // Every pointer is checked with VirtualQuery before it's read (no nested __try).
        for (size_t i = 0; i < 4096 && misses < 64; ++i) {
            if (!readable(list + i, 8)) break;
            uint8_t* d = list[i];
            if (!readable(d, DEF_SIZE) || *reinterpret_cast<uintptr_t*>(d) != g_base + 0x2EB5BD8) { ++misses; continue; }
            misses = 0;
            ++found;
            uint32_t h  = *reinterpret_cast<uint32_t*>(d + 0xC4);
            uint32_t np = *reinterpret_cast<uint32_t*>(d + 0x94);
            fprintf(f, "%zu | %s (0x%08X) | %.2f | %.2f | %.1f | %d | %d/%d | %u\n", i, hash_name(h), h,
                    *reinterpret_cast<float*>(d + 0x9C), *reinterpret_cast<float*>(d + 0x98), *reinterpret_cast<float*>(d + 0xB0),
                    *reinterpret_cast<int32_t*>(d + 0xA8), d[0xC8], d[0xC9], np);
            uint8_t** kids = *reinterpret_cast<uint8_t***>(d + 0x88);
            for (uint32_t k = 0; k < np && k < 64 && readable(kids + k, 8); ++k) {
                uint8_t* c = kids[k];
                if (!readable(c, DEF_SIZE)) continue;
                uint32_t ch = *reinterpret_cast<uint32_t*>(c + 0xC4);
                fprintf(f, "      piece %u: %s (0x%08X) mass %.2f radius %.2f model %d\n", k, hash_name(ch), ch,
                        *reinterpret_cast<float*>(c + 0x9C), *reinterpret_cast<float*>(c + 0x98), *reinterpret_cast<int32_t*>(c + 0xA8));
            }
        }
        logf("definition dump: %d definitions", found);
        fclose(f);
        logf("definition dump written to WreckingBall-defs.txt");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logf("definition dump failed");
    }
}

std::atomic<uint8_t*> g_bigDefPatched{nullptr};
void patch_big_def(uint8_t* table) {
    if (g_sizeTiers || g_treeObjectType < 0 || g_treeObjectType > 255) return;
    __try {
        uint8_t* d = o_defLookup(table, 0xFFFF0000u | (uint32_t)(tree_def(table, true) & 0xFFFF));
        if (!d || d == g_bigDefPatched.load(std::memory_order_relaxed)) return;
        uint8_t old = d[0xC8];
        d[0xC8]     = (uint8_t)g_treeObjectType;
        g_bigDefPatched.store(d);
        dlogf("big tree definition %d: object type %d -> %d (mass %.1f, break speed %.1f mph)", g_bigTreeDef, old, d[0xC8],
             *reinterpret_cast<float*>(d + 0x9C), *reinterpret_cast<float*>(d + 0xB0));
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// render-only definition for logs drawn with another model (see spawn_log)
constexpr uint32_t LOG_RENDER_DEF = 0xF0FF, POLE_RENDER_DEF = 0xF0FE;
std::atomic<uint8_t*> g_logRenderDef{nullptr}, g_poleRenderDef{nullptr};

uint8_t* d_defLookup(uint8_t* table, uint32_t id) {
    uint32_t lo = id & 0xFFFF;
    if (lo == POLE_RENDER_DEF) {
        if (uint8_t* d = g_poleRenderDef.load(std::memory_order_acquire)) return d;
        return o_defLookup(table, (id & 0xFFFF0000u) | (uint32_t)(nd(table, 28) < 0 ? 28 : nd(table, 28)));
    }
    if (lo == LOG_RENDER_DEF) {
        if (uint8_t* d = g_logRenderDef.load(std::memory_order_acquire)) return d;
        return o_defLookup(table, (id & 0xFFFF0000u) | (uint32_t)(nd(table, 28) < 0 ? 28 : nd(table, 28)));
    }
    if (!g_sizeTiers && (id >> 16) == 0xFFFF && lo == (uint32_t)(tree_def(table, true) & 0xFFFF)) patch_big_def(table);
    if (!g_defsDumped.load(std::memory_order_relaxed)) dump_defs(table);
    if (lo >= (uint32_t)CLONE_BASE && lo < (uint32_t)(CLONE_BASE + NUM_TIERS)) {
        // A broken-off piece (high 16 bits = piece index): the tree definition's own piece.
        if ((id >> 16) != 0xFFFF) return o_defLookup(table, (id & 0xFFFF0000u) | (uint32_t)(tree_def(table, false) & 0xFFFF));
        __try {
            return get_clone(table, (int)(lo - CLONE_BASE));
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            return o_defLookup(table, 0xFFFF0000u | (uint32_t)(tree_def(table, false) & 0xFFFF));
        }
    }
    return o_defLookup(table, id);
}

int d_defModel(uint8_t* table, uint32_t id) {
    uint32_t lo = id & 0xFFFF;
    if (lo >= (uint32_t)CLONE_BASE && lo < (uint32_t)(CLONE_BASE + NUM_TIERS))
        return o_defModel(table, (id & 0xFFFF0000u) | (uint32_t)(tree_def(table, false) & 0xFFFF));
    return o_defModel(table, id);
}

// ---- per-tree sizes, by procedural data and child index -----------------------
std::mutex g_sizeLock;
std::unordered_map<uint8_t*, std::vector<uint8_t>> g_childTier; // data -> tier of each child
float child_size(uint8_t* data, int child) {
    std::lock_guard<std::mutex> l(g_sizeLock);
    auto it = g_childTier.find(data);
    if (it == g_childTier.end() || child < 0 || child >= (int)it->second.size()) return -1.0f;
    if (it->second[child] == BUSH_TIER) return 0.5f;
    return tier_size(it->second[child]);
}

// ---- hooks ------------------------------------------------------------------
using DeserializeFn = void (*)(uint8_t* data, void* archive);
using ResizeFn      = void (*)(MsvcVector* vec, size_t n);
using CellAddFn     = void (*)(uint8_t* mgr, uint8_t* msg);
using CellSpawnFn   = void (*)(uint8_t* mgr, uint8_t* cell);
using SwapFn        = void (*)(uint8_t* mgr, uint8_t* obj);

DeserializeFn o_deserialize = nullptr;
CellAddFn o_cellAdd         = nullptr;
CellSpawnFn o_cellSpawn     = nullptr;
SwapFn o_swap               = nullptr;

// "key:value,key:value": the value of the first key found in name (case-insensitive), or -1
int rule_value(const char* rules, const char* name) {
    const char* r = rules;
    while (*r) {
        char key[64]; int k = 0;
        while (*r && *r != ':' && *r != ',' && k < 63) key[k++] = *r++;
        key[k] = 0;
        int v = -1;
        if (*r == ':') { v = std::atoi(++r); while (*r && *r != ',') ++r; }
        if (*r == ',') ++r;
        while (k && key[k - 1] == ' ') key[--k] = 0;
        const char* kk = key; while (*kk == ' ') ++kk;
        if (*kk && (std::strcmp(kk, "*") == 0 || contains_ci(name, kk))) return v;
    }
    return -1;
}
// any comma-separated item of list found in name
const char* list_match(const char* list, const char* name) {
    static thread_local char hit[64];
    const char* r = list;
    while (*r) {
        int k = 0;
        while (*r == ' ' || *r == ',') ++r;
        while (*r && *r != ',' && k < 63) hit[k++] = *r++;
        while (k && hit[k - 1] == ' ') --k;
        hit[k] = 0;
        if (k && contains_ci(name, hit)) return hit;
    }
    return nullptr;
}

std::mutex g_barrierLock;
std::set<uint8_t*> g_barrierData;
std::set<std::string> g_barrierNamesLogged;
std::atomic<int> g_barrierProcs{0}, g_barrierInstances{0};
void mark_barrier_proc(int i);
bool is_barrier_proc(int i);

// The game's breakable objects, by name (WreckingBall-defs.txt for this build): a placed
// model with the same name as one of these breaks exactly like the game's own copy.
struct DefName { const char* name; int def; float radius; };
const DefName k_defNames[] = {
#include "def_names.inc"
};
const DefName* exact_def(const char* model) {
    char base[128];
    size_t n = std::strlen(model);
    if (n >= sizeof base) return nullptr;
    std::memcpy(base, model, n + 1);
    for (const char* suf : {"_3D", "_Template", "_LOD0", "_Low"}) {
        size_t sl = std::strlen(suf);
        if (n > sl && _stricmp(base + n - sl, suf) == 0) { base[n - sl] = 0; n -= sl; break; }
    }
    for (const DefName& d : k_defNames)
        if (_stricmp(d.name, base) == 0) return &d;
    return nullptr;
}
// ---- definition numbers per map -------------------------------------------------------
// Each map has its own list of breakable definitions, numbered in its own order: on Storm Island
// 97 (the mainland's apple tree) is a porta-loo and 110 (the flattening bush) an electronic
// arrow sign. The mod's numbers, and the ini's, are the mainland's; nd() turns one into the
// number on the map that `table` belongs to, by the definition's name (hashed at def+0xC4), or
// -1 if that map hasn't got it. Numbers the mod has no name for are passed through.
struct DefMap { uint8_t* table; uint8_t* list; uint8_t* end; bool moved; int16_t nat[0x1000]; };
std::mutex g_defMapLock;
std::vector<DefMap*> g_defMaps;                 // never freed: a few KB per map
std::atomic<DefMap*> g_lastDefMap{nullptr};

// table+8 and +0x10 are the list's begin and end: a list still being filled gets looked at again
DefMap* def_map(uint8_t* table, uint8_t* list, uint8_t* end) {
    std::lock_guard<std::mutex> l(g_defMapLock);
    for (DefMap* m : g_defMaps)
        if (m->table == table && m->list == list && m->end == end) { g_lastDefMap.store(m, std::memory_order_release); return m; }
    auto* m = new DefMap{table, list, end, false, {}};
    for (auto& v : m->nat) v = -2;              // -2: not one we know by name; passed through
    std::unordered_map<uint32_t, int> byHash;
    uint8_t** defs = reinterpret_cast<uint8_t**>(list);
    int misses = 0, found = 0;
    size_t count = end ? (size_t)(end - list) / 8 : 0x1000;
    for (size_t i = 0; i < count && misses < 64; ++i) {
        if (!readable(defs + i, 8)) break;
        uint8_t* d = defs[i];
        if (!readable(d, DEF_SIZE) || *reinterpret_cast<uintptr_t*>(d) != g_base + 0x2EB5BD8) { ++misses; continue; }
        misses = 0;
        ++found;
        byHash.emplace(*reinterpret_cast<uint32_t*>(d + 0xC4), (int)i);
    }
    auto hf = reinterpret_cast<NameHashFn>(g_base + 0x126E020);
    int moved = 0, missing = 0;
    for (const DefName& dn : k_defNames) {
        if (dn.def < 0 || dn.def >= 0x1000) continue;
        auto it = byHash.find(hf(dn.name, true, 0x1505));
        if (it == byHash.end()) { m->nat[dn.def] = -1; ++missing; continue; }
        m->nat[dn.def] = (int16_t)it->second;
        if (it->second != dn.def) ++moved;
    }
    m->moved = moved || missing;
    g_defMaps.push_back(m);
    g_lastDefMap.store(m, std::memory_order_release);
    if (m->moved)
        logf("breakable definitions: %d on this map, numbered differently from the mainland (%d moved, %d not here); using this map's numbers",
             found, moved, missing);
    else
        dlogf("breakable definitions: %d on this map, numbered as on the mainland", found);
    return m;
}

int nd(uint8_t* table, int def) {
    if (!table || def < 0 || def >= 0x1000) return def;
    uint8_t* list = *reinterpret_cast<uint8_t**>(table + 8);
    uint8_t* end  = *reinterpret_cast<uint8_t**>(table + 0x10);
    // only when it looks like the list's end (a vector); otherwise the list is walked to its last definition
    if (!(end > list && (size_t)(end - list) % 8 == 0 && (size_t)(end - list) / 8 <= 0x1000)) end = nullptr;
    DefMap* m     = g_lastDefMap.load(std::memory_order_acquire);
    if (!m || m->table != table || m->list != list || m->end != end) m = def_map(table, list, end);
    int v = m->nat[def];
    return v == -2 ? def : v;
}

int g_treeAlts[8] = {97, 262, 98, 263};      // trees to break as when one isn't loaded (TreeAlts)
int g_nTreeAlts = 4;
int tree_def(uint8_t* table, bool big) {
    int v = nd(table, big ? g_bigTreeDef : g_treeDef);
    for (int k = 0; v < 0 && k < g_nTreeAlts; ++k) v = nd(table, g_treeAlts[k]);
    return v >= 0 ? v : big ? g_bigTreeDef : g_treeDef;
}

// ---- the groups we add, in this map's numbers ---------------------------------------------
// Groups are written with mainland numbers when a procedural loads; when its area registers with
// the world (d_cellAdd), the map is known and they're put into its numbers. A group whose object
// this map hasn't got gets a stand-in (map_group_def), or, failing that, no children.
struct OurGroups { size_t first; std::vector<int> defs, counts, models, starts; uint8_t* table; uint8_t* list; };
std::mutex g_ourLock;
std::unordered_map<uint8_t*, OurGroups> g_ourGroups;
void note_our_groups(uint8_t* data, size_t first, const Group* g, int n) {
    OurGroups o{first, {}, {}, {}, {}, nullptr, nullptr};
    for (int k = 0; k < n; ++k) {
        o.defs.push_back(g[k].def);
        o.counts.push_back(g[k].count);
        o.models.push_back(g[k].model);
        o.starts.push_back(g[k].start);
    }
    std::lock_guard<std::mutex> l(g_ourLock);
    g_ourGroups[data] = std::move(o);
}
// A stand-in for an object this map hasn't got: another tree for a tree, else the flattening
// bush. Its collision is already filtered out, so it has to break somehow.
int map_group_def(uint8_t* table, int d) {
    int v = nd(table, d);
    if (v >= 0) return v;
    bool treeish = d == g_treeDef || d == g_bigTreeDef || d == g_bushDef;
    for (int k = 0; k < g_nTreeAlts; ++k) treeish = treeish || d == g_treeAlts[k];
    for (int k = 0; treeish && k < g_nTreeAlts; ++k) if ((v = nd(table, g_treeAlts[k])) >= 0) return v;
    if ((v = nd(table, g_bushDef > 0 ? g_bushDef : 110)) >= 0) return v;
    for (int k = 0; k < g_nTreeAlts; ++k) if ((v = nd(table, g_treeAlts[k])) >= 0) return v;
    return -1;
}
void translate_groups(uint8_t* mgr, uint8_t* data) {
    uint8_t* table = *reinterpret_cast<uint8_t**>(mgr + 0x4C8);
    if (!table || !readable(table + 8, 8)) return;
    uint8_t* list = *reinterpret_cast<uint8_t**>(table + 8);
    std::lock_guard<std::mutex> l(g_ourLock);
    auto it = g_ourGroups.find(data);
    if (it == g_ourGroups.end()) return;
    OurGroups& o = it->second;
    if (o.table == table && o.list == list) return;
    MsvcVector* grp = vec_at(data + D_GROUPS);
    if (grp->count(sizeof(Group)) != o.first + o.defs.size()) { g_ourGroups.erase(it); return; }
    auto* g = reinterpret_cast<Group*>(grp->first) + o.first;
    // still the groups we wrote (same batches and runs)? The note is keyed by the data's address,
    // which the game reuses for other procedurals as areas stream in and out
    for (size_t k = 0; k < o.defs.size(); ++k)
        if (g[k].model != o.models[k] || g[k].start != o.starts[k]) { g_ourGroups.erase(it); return; }
    int changed = 0, dropped = 0;
    for (size_t k = 0; k < o.defs.size(); ++k) {
        int d = o.defs[k];
        if (d < 0 || d >= 0xF000) continue;   // our own copies (SizeTiers) look themselves up
        int v = map_group_def(table, d);
        if (v >= 0 && nd(table, d) < 0) {
            static std::atomic<int> ns{0};
            if (++ns <= 20) logf("definition %d isn't on this map; its objects break as %d instead", d, v);
        }
        int count = v < 0 ? 0 : o.counts[k];
        if (v < 0) { v = d; ++dropped; }
        if (g[k].def != v || g[k].count != count) { g[k].def = v; g[k].count = count; ++changed; }
    }
    o.table = table;
    o.list  = list;
    static std::atomic<int> nl{0};
    if (changed && ++nl <= 20)
        dlogf("procedural %p: %d of our groups put into this map's numbers (%d without children: nothing to break as)", data, changed, dropped);
}

int g_clutter = 1;
char g_clutterExcludeLibs[256] = "TREEANDBUSHTEMPLATE,CROWDTEMPLATE";
std::atomic<int> g_clutterInstances{0};

void inject_barriers(uint8_t* data, const char* lib) {
    MsvcVector* grp = vec_at(data + D_GROUPS);
    MsvcVector* mdl = vec_at(data + D_MODELS);
    size_t nModels  = mdl->count(BATCH_SIZE);
    const char* events = str_at(data + 0x20);   // event IDs: this set only appears for those events
    auto note = [&](const char* name, const char* what) {
        std::lock_guard<std::mutex> l(g_barrierLock);
        if (g_barrierNamesLogged.size() < 1500 && g_barrierNamesLogged.insert(name).second)
            dlogf("model %s (%s%s%s): %s", name, lib, *events ? ", events " : "", events, what);
    };
    // "1442;8004" = only during those events; "~1442;~8004" = everywhere except during them
    bool eventOnly = false;
    for (const char* e = events; *e;) {
        while (*e == ' ' || *e == ';') ++e;
        if (!*e) break;
        if (*e != '~') eventOnly = true;
        while (*e && *e != ';') ++e;
    }
    if (g_barrierSkipEvents && eventOnly) {
        for (size_t i = 0; i < nModels; ++i) note(str_at(mdl->first + i * BATCH_SIZE + B_NAME), "left alone (only there for an event)");
        return;
    }
    auto decode = reinterpret_cast<DecodeInstanceFn>(g_base + RVA_DECODE_INSTANCE);
    struct Run { int batch, start, count, def; };
    static thread_local Run runs[1024];
    int n = 0, total = 0;
    bool isBarrierLib = g_barriers && list_match(g_barrierLibs, lib);
    // batches the game already made breakable
    size_t nOld = grp->count(sizeof(Group));
    static thread_local uint8_t covered[4096];
    std::memset(covered, 0, nModels < 4096 ? nModels : 4096);
    for (size_t k = 0; k < nOld; ++k) {
        int mb = reinterpret_cast<Group*>(grp->first)[k].model;
        if (mb >= 0 && (size_t)mb < nModels && mb < 4096) covered[mb] = 1;
    }
    for (size_t i = 0; i < nModels && i < 4096 && n < 1024; ++i) {
        uint8_t* b       = mdl->first + i * BATCH_SIZE;
        const char* name = str_at(b + B_NAME);
        MsvcVector* iv   = vec_at(b + B_INSTANCES);
        int cnt          = (int)iv->count(INSTANCE_SIZE);
        if (!cnt || covered[i]) continue;
        if (const char* ex = list_match(g_barrierExclude, name)) {
            char why[96]; snprintf(why, sizeof why, "left alone (excluded: %s)", ex);
            note(name, why);
            continue;
        }
        const DefName* exact = g_clutter ? exact_def(name) : nullptr;
        int def = exact ? exact->def : isBarrierLib ? rule_value(g_barrierRules, name) : -1;
        if (def < 0 || def >= 0xF000) { note(name, isBarrierLib ? "left alone (no rule)" : "left alone (no matching breakable object)"); continue; }
        bool chain = isBarrierLib;   // fences and rails: a chain of points along the run, and the ground check
        char what[96];
        snprintf(what, sizeof what, "breakable as definition %d%s, %d here", def, exact ? " (its own breakable version)" : "", cnt);
        note(name, what);
        if (!chain) {
            // one point per object, the size of the breakable version; no ground check
            float r = exact ? exact->radius : 1.0f;
            if (r < 0.5f) r = 0.5f;
            if (r > 2.5f) r = 2.5f;
            if (g_filterCollision >= 2)
                for (int j = 0; j < cnt; ++j) {
                    alignas(16) float m[16];
                    float sc = 1.0f;
                    decode(data, m, iv->first + j * INSTANCE_SIZE, &sc);
                    int ix = trunk_add(m[12], m[13], m[14], -r);
                    if (ix >= 0 && g_barInfo) { g_barInfo[ix].dx = 1; g_barInfo[ix].dz = 0; g_barInfo[ix].flags.store(3); }
                }
            runs[n++] = Run{(int)i, 0, cnt, def};
            total += cnt;
            g_clutterInstances += cnt;
            continue;
        }
        // collision points along each segment: segments are laid end to end, so the
        // nearest neighbour gives the length and direction
        static thread_local float pos[4096][3];
        int np = cnt < 4096 ? cnt : 4096;
        static thread_local float axes[4096][3];
        for (int j = 0; j < np; ++j) {
            alignas(16) float m[16];
            float sc = 1.0f;
            decode(data, m, iv->first + j * INSTANCE_SIZE, &sc);
            pos[j][0] = m[12]; pos[j][1] = m[13]; pos[j][2] = m[14];
            float l = std::sqrt(m[0] * m[0] + m[2] * m[2]);
            axes[j][0] = l > 1e-4f ? m[0] / l : 1; axes[j][1] = 0; axes[j][2] = l > 1e-4f ? m[2] / l : 0;
        }
        if (g_filterCollision >= 2) {
            for (int j = 0; j < np; ++j) {
                float best = 1e9f, dx = axes[j][0], dz = axes[j][2];
                for (int k = 0; k < np; ++k) {
                    if (k == j) continue;
                    float ex = pos[k][0] - pos[j][0], ez = pos[k][2] - pos[j][2], ey = pos[k][1] - pos[j][1];
                    float d2 = ex * ex + ez * ez + ey * ey;
                    if (d2 < best && d2 > 0.01f) {
                        best = d2;
                        float l = std::sqrt(ex * ex + ez * ez);
                        if (l > 1e-3f) { dx = ex / l; dz = ez / l; }
                    }
                }
                float L = best < 1e8f ? std::sqrt(best) : 3.0f;
                if (L < 0.5f) L = 0.5f;
                if (L > 8.0f) L = 8.0f;
                // cover from half a segment behind the origin to a full segment ahead
                for (float t = -0.5f * L; t <= L + 0.01f; t += 0.8f)
                {
                    int ix = trunk_add(pos[j][0] + dx * t, pos[j][1], pos[j][2] + dz * t, -g_barrierPointRadius);
                    if (ix >= 0 && g_barInfo) { g_barInfo[ix].dx = dx; g_barInfo[ix].dz = dz; }
                }
            }
        }
        if (n < 1024) runs[n++] = Run{(int)i, 0, cnt, def};
        total += cnt;
    }
    if (!n) return;
    auto resize = reinterpret_cast<ResizeFn>(g_base + RVA_GROUP_RESIZE);
    resize(grp, nOld + (size_t)n);   // the game's own groups stay first, so their children keep their numbers
    if (grp->count(sizeof(Group)) != nOld + (size_t)n) {
        logf("ERROR: group resize failed (barriers)");
        return;
    }
    auto* g = reinterpret_cast<Group*>(grp->first) + nOld;
    for (int k = 0; k < n; ++k) g[k] = Group{runs[k].def, runs[k].batch, runs[k].start, runs[k].count, -1, 0, 0};
    note_our_groups(data, nOld, g, n);
    {
        std::lock_guard<std::mutex> l(g_barrierLock);
        g_barrierData.insert(data);
    }
    int procs = ++g_barrierProcs;
    g_barrierInstances += total;
    if (procs <= 30 || procs % 50 == 0)
        dlogf("made %d pieces breakable (%d groups, lib %s); totals: %d procedurals, %d pieces (%d clutter)", total, n, lib, procs, g_barrierInstances.load(), g_clutterInstances.load());
}

void inspect_and_inject(uint8_t* data) {
    const char* lib   = str_at(data + D_LIB);
    MsvcVector* grp   = vec_at(data + D_GROUPS);
    MsvcVector* mdl   = vec_at(data + D_MODELS);
    size_t nGroups    = grp->count(sizeof(Group));
    size_t nModels    = mdl->count(BATCH_SIZE);
    size_t nBillboard = vec_at(data + D_BILLBOARDS)->count(BATCH_SIZE);

    if (g_logLibs) {
        std::lock_guard<std::mutex> l(g_seenLock);
        if (g_seenLibs.insert(lib).second)
            dlogf("library seen: %s (groups=%zu models=%zu billboards=%zu)", lib, nGroups, nModels, nBillboard);
    }

    // Log every distinct (def, model) of existing groups, e.g. smashables.
    if (nGroups) {
        auto* g = reinterpret_cast<Group*>(grp->first);
        for (size_t i = 0; i < nGroups; ++i) {
            const char* name = "?";
            if (g[i].count > 0 && g[i].model >= 0 && (size_t)g[i].model < nModels)
                name = str_at(mdl->first + g[i].model * BATCH_SIZE + B_NAME);
            std::lock_guard<std::mutex> l(g_seenLock);
            if (g_seenSmash.insert({g[i].def, name}).second)
                dlogf("group: lib=%s def=%d model=%s count=%d billboards=%d", lib, g[i].def, name, g[i].count, g[i].bbCount);
        }
        // the rest of a procedural the game already made partly breakable
        if (g_clutter && nModels && !list_match(g_clutterExcludeLibs, lib)) inject_barriers(data, lib);
        return;
    }

    if (nModels && !list_match(g_clutterExcludeLibs, lib) && ((g_barriers && list_match(g_barrierLibs, lib)) || g_clutter)) {
        inject_barriers(data, lib);
        return;
    }
    if (!g_inject || std::strcmp(lib, "TREEANDBUSHTEMPLATE") != 0 || nModels == 0) return;

    // Groups: runs of consecutive instances of the same batch and size tier, in
    // batch order (deterministic, so saved smashes keep matching).
    auto decode = reinterpret_cast<DecodeInstanceFn>(g_base + RVA_DECODE_INSTANCE);
    struct Run { int batch, start, count, tier; };
    static thread_local Run runs[4096];
    int n = 0, trees = 0;
    for (size_t i = 0; i < nModels; ++i) {
        uint8_t* b       = mdl->first + i * BATCH_SIZE;
        const char* name = str_at(b + B_NAME);
        MsvcVector* iv   = vec_at(b + B_INSTANCES);
        int cnt          = (int)iv->count(INSTANCE_SIZE);
        if (cnt == 0 || !name_selected(name)) continue;
        bool skip    = *g_skipModels && list_match(g_skipModels, name);
        bool bush    = !skip && g_bushDef > 0 && *g_bushModels && list_match(g_bushModels, name);
        float factor = model_size_factor(name);
        if (g_debug) {
            std::lock_guard<std::mutex> l(g_seenLock);
            if (g_seenModels.insert(name).second)
                logf("tree model %s: %s, size %.2f (%d here)", name, skip ? "left solid" : bush ? "bush" : "tree", factor, cnt);
        }
        if (skip) continue;
        for (int j = 0; j < cnt; ++j) {
            alignas(16) float m[16];
            float scale = 1.0f;
            decode(data, m, iv->first + j * INSTANCE_SIZE, &scale);
            float size = factor * scale;
            int tier   = bush ? BUSH_TIER : size_tier(size);
            if (g_filterCollision >= 2) trunk_add(m[12], m[13], m[14], size);
            if (n > 0 && runs[n - 1].batch == (int)i && runs[n - 1].tier == tier) runs[n - 1].count++;
            else if (n < 4096) runs[n++] = Run{(int)i, j, 1, tier};
            ++trees;
        }
    }
    if (!n) return;

    auto resize = reinterpret_cast<ResizeFn>(g_base + RVA_GROUP_RESIZE);
    resize(grp, (size_t)n); // the game's own allocator
    if (grp->count(sizeof(Group)) != (size_t)n) {
        logf("ERROR: group resize failed");
        return;
    }
    auto* g = reinterpret_cast<Group*>(grp->first);
    for (int k = 0; k < n; ++k)
        g[k] = Group{runs[k].tier == BUSH_TIER ? g_bushDef
                     : g_sizeTiers ? CLONE_BASE + runs[k].tier
                                   : (tier_size(runs[k].tier) >= g_lumberjackMinSize - 0.01f ? g_bigTreeDef : g_treeDef),
                     runs[k].batch, runs[k].start, runs[k].count, -1, 0, 0};
    note_our_groups(data, 0, g, n);
    {
        std::vector<uint8_t> tiers;
        for (int k = 0; k < n; ++k) tiers.insert(tiers.end(), (size_t)runs[k].count, (uint8_t)runs[k].tier);
        std::lock_guard<std::mutex> l(g_sizeLock);
        g_childTier[data] = std::move(tiers);
    }
    int procs = ++g_procsInjected;
    g_treesInjected += trees;
    if (procs <= 40 || procs % 50 == 0)
        dlogf("injected %d groups (%d trees) into a tree procedural; totals: %d procedurals, %d trees, %d trunk points", n, trees, procs,
             g_treesInjected.load(), g_trunkCount.load());
}

// A procedural data object is loaded (again, or a new one at a freed one's address): what we
// noted about whatever was there before no longer applies.
void forget_data(uint8_t* data) {
    {
        std::lock_guard<std::mutex> l(g_ourLock);
        g_ourGroups.erase(data);
    }
    {
        std::lock_guard<std::mutex> l(g_sizeLock);
        g_childTier.erase(data);
    }
    std::lock_guard<std::mutex> l(g_barrierLock);
    g_barrierData.erase(data);
}

void d_deserialize(uint8_t* data, void* archive) {
    o_deserialize(data, archive);
    forget_data(data);
    __try {
        inspect_and_inject(data);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        logf("exception while inspecting procedural data %p", data);
    }
}

// Is this collidable-cell provider one of the tree procedurals we injected?
bool is_tree_provider(uint8_t* prov, const char** name) {
    if (!prov || *reinterpret_cast<uintptr_t*>(prov) != g_base + RVA_MODELS_VTABLE) return false;
    uint8_t* data = *reinterpret_cast<uint8_t**>(prov + 0xF8);
    if (!data || std::strcmp(str_at(data + D_LIB), "TREEANDBUSHTEMPLATE") != 0) return false;
    if (name) *name = str_at(prov + 0x18);
    return true;
}

// (functions with __try can't hold objects that need unwinding, so the locking is done in helpers)
bool is_barrier_data(uint8_t* data) {
    std::lock_guard<std::mutex> l(g_barrierLock);
    return data && g_barrierData.count(data);
}
void d_cellAdd(uint8_t* mgr, uint8_t* msg) {
    __try {
        uint8_t* prov    = *reinterpret_cast<uint8_t**>(msg + 0x60);
        const char* name = "";
        if (prov && *reinterpret_cast<uintptr_t*>(prov) == g_base + RVA_MODELS_VTABLE) {
            uint8_t* data = *reinterpret_cast<uint8_t**>(prov + 0xF8);
            translate_groups(mgr, data);   // into this map's definition numbers
            if (is_barrier_data(data)) mark_barrier_proc(*reinterpret_cast<int32_t*>(prov + 0x58));
        }
        if (is_tree_provider(prov, &name)) {
            mark_tree_proc(*reinterpret_cast<int32_t*>(prov + 0x58));
            int cells = ++g_treeCells;
            if (cells <= 20 || cells % 100 == 0) {
                unsigned count   = *reinterpret_cast<uint16_t*>(msg + 0x68);
                uint64_t free    = *reinterpret_cast<uint64_t*>(mgr + 0x510);
                auto* slots      = vec_at(mgr + 0x290);
                dlogf("tree cell registered: proc=%d count=%u freeSlots=%llu slotVector=%zu name=%s (tree cells so far %d)",
                     *reinterpret_cast<int32_t*>(prov + 0x58), count, (unsigned long long)free, slots->count(8), name, cells);
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    o_cellAdd(mgr, msg);
}

void d_cellSpawn(uint8_t* mgr, uint8_t* cell) {
    bool tree = false;
    __try {
        tree = is_tree_provider(*reinterpret_cast<uint8_t**>(cell + 0x20), nullptr);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    uint64_t freeBefore = *reinterpret_cast<uint64_t*>(mgr + 0x510);
    o_cellSpawn(mgr, cell);
    if (tree) {
        int s = ++g_treeSpawns;
        if (s <= 20 || s % 100 == 0)
            dlogf("tree cell spawned: cell=%d count=%u freeSlots %llu -> %llu (tree spawns so far %d)",
                 *reinterpret_cast<int32_t*>(cell + 0x28), *reinterpret_cast<uint16_t*>(cell + 0x30),
                 (unsigned long long)freeBefore, (unsigned long long)*reinterpret_cast<uint64_t*>(mgr + 0x510), s);
    }
}

void d_swap(uint8_t* mgr, uint8_t* obj) {
    __try {
        int def  = *reinterpret_cast<int32_t*>(obj + 0x124);
        int proc = *reinterpret_cast<int32_t*>(obj + 0xC0);
        int kid  = *reinterpret_cast<int32_t*>(obj + 0xC4);
        bool tree = false;
        // find the cell for this procedural index
        MsvcVector* cells = vec_at(mgr + 0x518);
        for (uint8_t* c = cells->first; c && c < cells->last; c += 0x40)
            if (*reinterpret_cast<int32_t*>(c + 0x28) == proc) {
                tree = is_tree_provider(*reinterpret_cast<uint8_t**>(c + 0x20), nullptr);
                break;
            }
        int n = ++g_swaps;
        if (n <= 300) dlogf("made dynamic: def=%d proc=%d child=%d %s", def, proc, kid, tree ? "<-- TREE" : "");
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    o_swap(mgr, obj);
}


// ---- broken trees and their collision ---------------------------------------
// Positions of trees that have broken this session. Append-only, read lock-free.
struct Broken { float x, y, z; };
constexpr int MAX_BROKEN = 8192;
Broken g_broken[MAX_BROKEN];
std::atomic<int> g_nBroken{0};
std::mutex g_brokenLock;

// Procedural indexes of tree cells (set when the cell registers).
constexpr int MAX_PROC = 1 << 20;
uint8_t g_treeProc[MAX_PROC / 8];
void mark_tree_proc(int i) { if (i >= 0 && i < MAX_PROC) g_treeProc[i >> 3] |= (uint8_t)(1 << (i & 7)); }
bool is_tree_proc(int i) { return i >= 0 && i < MAX_PROC && (g_treeProc[i >> 3] >> (i & 7)) & 1; }
uint8_t g_barrierProc[MAX_PROC / 8];
void mark_barrier_proc(int i) { if (i >= 0 && i < MAX_PROC) g_barrierProc[i >> 3] |= (uint8_t)(1 << (i & 7)); }
bool is_barrier_proc(int i) { return i >= 0 && i < MAX_PROC && (g_barrierProc[i >> 3] >> (i & 7)) & 1; }

// Which collision templates are procedural physics (".procphys") meshes.
struct TplEntry { std::atomic<uintptr_t> tpl; std::atomic<uintptr_t> res; std::atomic<int> isProc; };
constexpr int TPL_SLOTS = 1 << 15;
TplEntry g_tpl[TPL_SLOTS];
std::mutex g_tplLock;
std::set<std::string> g_tplNamesLogged;
std::atomic<int> g_procTemplates{0}, g_filtered{0};

size_t tpl_hash(uintptr_t p) { return (size_t)((p >> 4) * 0x9E3779B97F4A7C15ull >> 49) & (TPL_SLOTS - 1); }

int tpl_lookup(uintptr_t tpl) {
    size_t h = tpl_hash(tpl);
    for (int probe = 0; probe < 64; ++probe) {
        TplEntry& e = g_tpl[(h + probe) & (TPL_SLOTS - 1)];
        uintptr_t k = e.tpl.load(std::memory_order_acquire);
        if (k == tpl) return e.isProc.load(std::memory_order_relaxed);
        if (k == 0) return -1;
    }
    return -1;
}

void tpl_store(uintptr_t tpl, uintptr_t res, int isProc) {
    std::lock_guard<std::mutex> l(g_tplLock);
    size_t h = tpl_hash(tpl);
    for (int probe = 0; probe < 64; ++probe) {
        TplEntry& e = g_tpl[(h + probe) & (TPL_SLOTS - 1)];
        uintptr_t k = e.tpl.load();
        if (k == tpl || k == 0) {
            e.res.store(res);
            e.isProc.store(isProc);
            e.tpl.store(tpl, std::memory_order_release);
            return;
        }
    }
}

uintptr_t tpl_res(uintptr_t tpl) {
    size_t h = tpl_hash(tpl);
    for (int probe = 0; probe < 64; ++probe) {
        TplEntry& e = g_tpl[(h + probe) & (TPL_SLOTS - 1)];
        uintptr_t k = e.tpl.load(std::memory_order_acquire);
        if (k == tpl) return e.res.load();
        if (k == 0) return 0;
    }
    return 0;
}

bool contains_ci(const char* s, const char* needle) {
    size_t n = std::strlen(needle);
    for (; *s; ++s) {
        size_t i = 0;
        while (i < n && s[i] && (s[i] | 0x20) == (needle[i] | 0x20)) ++i;
        if (i == n) return true;
    }
    return false;
}

using HolderLoadFn = void (*)(uint8_t* holder);
HolderLoadFn o_holderLoad = nullptr;

void note_template(const char* name, int isProc) {
    std::lock_guard<std::mutex> l(g_seenLock);
    if (g_tplNamesLogged.size() < 80 && g_tplNamesLogged.insert(name).second)
        dlogf("collision template: %s%s", name, isProc ? "  <-- procedural physics" : "");
}
void d_holderLoad(uint8_t* holder) {
    o_holderLoad(holder);
    __try {
        uintptr_t tpl = *reinterpret_cast<uintptr_t*>(holder + 0x30);
        uintptr_t res = *reinterpret_cast<uintptr_t*>(holder + 0x28);
        if (!tpl || !res) return;
        if (tpl_res(tpl) == res) return; // already classified
        int isProc = 0;
        const char* name = "?";
        if (*reinterpret_cast<uintptr_t*>(res) == g_base + RVA_TEMPLATE_RES_VT) {
            name   = str_at(reinterpret_cast<uint8_t*>(res + 0xC0));
            isProc = contains_ci(name, "procphys") ? 1 : 0;
        }
        tpl_store(tpl, res, isProc);
        if (isProc) ++g_procTemplates;
        note_template(name, isProc);
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

using TemplateQueryFn = int (*)(uint8_t* T, const float* box, int max, uint8_t** outTris, uint8_t** outVerts, uint8_t flag);
TemplateQueryFn o_templateQuery = nullptr;

std::atomic<long long> g_barFiltered{0};
// ---- F8 "mark": what collision is around the car right now -----------------------
std::atomic<uint8_t*> g_worldMgr{nullptr};
struct RecentTri { float v[9]; uintptr_t T; uint8_t nearBar, removed; uint32_t frame; };
constexpr int NRECENT = 512;
RecentTri g_recent[NRECENT];
std::atomic<int> g_recentPos{0};
std::atomic<uint32_t> g_frame{0};
struct RecentQuery { float box[6]; uintptr_t T; int n, removed; uint32_t frame; };
constexpr int NRQ = 256;
RecentQuery g_rq[NRQ];
std::atomic<int> g_rqPos{0};
int g_markEnabled = 0;
void record_tris(uint8_t* T, uint8_t** tris, uint8_t** verts, int n, int removedCount, const uint8_t* flags) {
    uint32_t f = g_frame.load(std::memory_order_relaxed);
    for (int k = 0; k < n && k < 64; ++k) {
        const float* v = reinterpret_cast<const float*>(verts[k]);
        uint8_t* tri   = tris[k];
        uint32_t idx[3] = {*reinterpret_cast<uint32_t*>(tri + 4), *reinterpret_cast<uint32_t*>(tri + 8), *reinterpret_cast<uint32_t*>(tri + 12)};
        int pos = g_recentPos.fetch_add(1) & (NRECENT - 1);
        RecentTri& r = g_recent[pos];
        for (int c = 0; c < 3; ++c) { r.v[c * 3] = v[idx[c] * 4]; r.v[c * 3 + 1] = v[idx[c] * 4 + 1]; r.v[c * 3 + 2] = v[idx[c] * 4 + 2]; }
        r.T = reinterpret_cast<uintptr_t>(T); r.nearBar = flags ? flags[k] : 0; r.removed = 0; r.frame = f;
    }
    (void)removedCount;
}
bool near_player(const float* box, float pad) {
    uint8_t* m = g_worldMgr.load(std::memory_order_relaxed);
    if (!m) return false;
    const float* p = reinterpret_cast<const float*>(m + 0x4F0);
    return p[0] > box[0] - pad && p[0] < box[4] + pad && p[2] > box[2] - pad && p[2] < box[6] + pad;
}

int d_templateQuery(uint8_t* T, const float* box, int max, uint8_t** outTris, uint8_t** outVerts, uint8_t flag) {
    int n = o_templateQuery(T, box, max, outTris, outVerts, flag);
    if (n > 0 && g_markEnabled &&
        (near_player(box, 10.0f) || (std::fabs(box[0] + box[4]) < 400 && std::fabs(box[2] + box[6]) < 400))) {
        // queries near the player, and ones in a template's own small coordinates (instanced shapes)
        int q = g_rqPos.fetch_add(1) & (NRQ - 1);
        RecentQuery& r = g_rq[q];
        r.box[0] = box[0]; r.box[1] = box[1]; r.box[2] = box[2]; r.box[3] = box[4]; r.box[4] = box[5]; r.box[5] = box[6];
        r.T = reinterpret_cast<uintptr_t>(T); r.n = n; r.removed = 0; r.frame = g_frame.load(std::memory_order_relaxed);
    }
    if (n <= 0 || !g_filterCollision || g_trunkCount.load(std::memory_order_relaxed) == 0) {
        if (n > 0 && g_markEnabled && near_player(box, 6.0f)) record_tris(T, outTris, outVerts, n, 0, nullptr);
        return n;
    }
    __try {
        const float R = g_trunkRadius;
        const TrunkNode* cand[256];
        int nc = trunk_find(box[0] - 4 * R - 6, box[2] - 4 * R - 6, box[4] + 4 * R + 6, box[6] + 4 * R + 6, cand, 256);
        if (!nc) return n;
        // split into tree points and barrier points
        const TrunkNode* trees[256];
        const TrunkNode* bars[256];
        int nt = 0, nbp = 0;
        const TrunkNode* barsAll[256];
        const TrunkNode* poles[64];
        int nba = 0, npo = 0;
        for (int c = 0; c < nc; ++c) {
            if (cand[c]->size >= 0) { trees[nt++] = cand[c]; continue; }
            if (cand[c]->size <= -100.0f) { if (npo < 64) poles[npo++] = cand[c]; continue; }   // telephone pole
            barsAll[nba++] = cand[c];
        }
        // look for ground on each side of every barrier point not yet settled
        if (g_barrierNeedGround && g_barInfo)
            for (int c = 0; c < nba; ++c) {
                const TrunkNode* b = barsAll[c];
                BarInfo& bi = g_barInfo[b - g_trunkPool];
                uint8_t fl = bi.flags.load(std::memory_order_relaxed);
                if ((fl & 3) == 3) continue;
                float px = -bi.dz, pz = bi.dx;   // across the fence
                for (int side = 0; side < 2; ++side) {
                    if (fl & (1 << side)) continue;
                    float sx = b->x + px * g_barrierProbe * (side ? -1 : 1), sz = b->z + pz * g_barrierProbe * (side ? -1 : 1);
                    for (int k = 0; k < n; ++k) {
                        uint8_t* tri    = outTris[k];
                        const float* v  = reinterpret_cast<const float*>(outVerts[k]);
                        const float* a0 = v + *reinterpret_cast<uint32_t*>(tri + 4) * 4;
                        const float* a1 = v + *reinterpret_cast<uint32_t*>(tri + 8) * 4;
                        const float* a2 = v + *reinterpret_cast<uint32_t*>(tri + 12) * 4;
                        // upward-ish face whose outline covers the probe point, at a drivable height
                        float ux = a1[0] - a0[0], uy = a1[1] - a0[1], uz = a1[2] - a0[2];
                        float wx = a2[0] - a0[0], wy = a2[1] - a0[1], wz = a2[2] - a0[2];
                        float ny = uz * wx - ux * wz;
                        float nx = uy * wz - uz * wy, nz = ux * wy - uy * wx;
                        float l2 = nx * nx + ny * ny + nz * nz;
                        if (l2 < 1e-12f || ny * ny < 0.5f * l2) continue;
                        float d = ux * wz - uz * wx;
                        if (std::fabs(d) < 1e-9f) continue;
                        float qx = sx - a0[0], qz = sz - a0[2];
                        float s1 = (qx * wz - qz * wx) / d, s2 = (ux * qz - uz * qx) / d;
                        if (s1 < 0 || s2 < 0 || s1 + s2 > 1) continue;
                        float gy = a0[1] + s1 * uy + s2 * wy;
                        if (gy < b->y - 6.0f || gy > b->y + 3.0f) continue;
                        bi.flags.fetch_or((uint8_t)(1 << side), std::memory_order_relaxed);
                        break;
                    }
                }
            }
        for (int c = 0; c < nba; ++c) if (bar_ok(barsAll[c])) bars[nbp++] = barsAll[c];
        int cls = tpl_lookup(reinterpret_cast<uintptr_t>(T) - 0xD8);
        int w = 0, nNear = 0, barNear = 0, barDrop = 0, keptOut = 0, keptFlat = 0;
        char sample[3 * 32 + 1] = "";
        char barSample[200] = "";
        for (int k = 0; k < n; ++k) {
            uint8_t* tri    = outTris[k];
            const float* v  = reinterpret_cast<const float*>(outVerts[k]);
            uint32_t idx[3] = {*reinterpret_cast<uint32_t*>(tri + 4), *reinterpret_cast<uint32_t*>(tri + 8),
                               *reinterpret_cast<uint32_t*>(tri + 12)};
            const float* p0 = v + idx[0] * 4;
            const float* p1 = v + idx[1] * 4;
            const float* p2 = v + idx[2] * 4;
            float cx = (p0[0] + p1[0] + p2[0]) / 3.0f, cy = (p0[1] + p1[1] + p2[1]) / 3.0f, cz = (p0[2] + p1[2] + p2[2]) / 3.0f;
            // face normal: only near-vertical faces (trunk sides) are removed, never ground
            float ax = p1[0] - p0[0], ay = p1[1] - p0[1], az = p1[2] - p0[2];
            float bx = p2[0] - p0[0], by = p2[1] - p0[1], bz = p2[2] - p0[2];
            float nx = ay * bz - az * by, ny = az * bx - ax * bz, nz = ax * by - ay * bx;
            float len2 = nx * nx + ny * ny + nz * nz;
            bool vertical = len2 > 1e-12f && ny * ny < 0.6f * 0.6f * len2;
            bool drop = false;
            for (int c = 0; c < nt && !drop; ++c) {
                const TrunkNode& b = *trees[c];
                float dx = cx - b.x, dz = cz - b.z;
                float h  = cy - b.y;
                float rr = R * (b.size > 1.0f ? b.size : 1.0f) + g_leanFactor * (h > 0 ? h : 0);
                if (dx * dx + dz * dz >= rr * rr || cy < b.y - 4.0f || cy > b.y + 60.0f) continue;
                if (nNear++ == 0)
                    for (int i = 0; i < 32; ++i) snprintf(sample + i * 3, 4, "%02x ", tri[i]);
                bool inside = true;
                for (const float* q : {p0, p1, p2}) {
                    float ex = q[0] - b.x, ez = q[2] - b.z;
                    float qh = q[1] - b.y, qr = rr + 0.6f + g_leanFactor * (qh > h ? qh - h : 0);
                    if (ex * ex + ez * ez > qr * qr) inside = false;
                }
                drop = inside && vertical;
            }
            // Poles: thin and tall. A triangle goes if it all sits within a small radius of the
            // pole, from a little below its base to well above its top, and it stands up or is
            // clear of the ground (never the ground around it).
            for (int c = 0; c < npo && !drop; ++c) {
                const TrunkNode& b = *poles[c];
                float r = -b.size - 100.0f;
                float dx = cx - b.x, dz = cz - b.z;
                if (dx * dx + dz * dz >= r * r || cy < b.y - 3.0f || cy > b.y + 15.0f) continue;
                bool inside = true;
                float minY = 1e30f;
                for (const float* q : {p0, p1, p2}) {
                    float ex = q[0] - b.x, ez = q[2] - b.z, qr = r + 0.4f;
                    if (ex * ex + ez * ez > qr * qr || q[1] < b.y - 4.0f || q[1] > b.y + 16.0f) inside = false;
                    if (q[1] < minY) minY = q[1];
                }
                drop = inside && (vertical || minY >= b.y + 0.3f);
            }
            // Barriers: a fence is a chain of points; a triangle goes if its centre is near one
            // and each corner is near some point of the chain (long rails span several), and it
            // stands up or sits clear of the ground (rail tops and caps).
            if (!drop && nbp) {
                const TrunkNode* nb = nullptr;
                for (int c = 0; c < nbp; ++c) {
                    const TrunkNode& b = *bars[c];
                    float rr = -b.size, dx = cx - b.x, dz = cz - b.z;
                    if (dx * dx + dz * dz < rr * rr && cy >= b.y - 4.5f && cy <= b.y + 4.5f) { nb = &b; break; }
                }
                if (nb) {
                    ++barNear;
                    bool allIn = true;
                    float minY = 1e30f;
                    for (const float* q : {p0, p1, p2}) {
                        if (q[1] < minY) minY = q[1];
                        bool in = false;
                        for (int c = 0; c < nbp && !in; ++c) {
                            const TrunkNode& b = *bars[c];
                            float qr = -b.size + 0.6f, ex = q[0] - b.x, ez = q[2] - b.z;
                            in = ex * ex + ez * ez <= qr * qr && q[1] >= b.y - 8.0f && q[1] <= b.y + 6.0f;   // walls have skirts reaching well below ground
                        }
                        if (!in) { allIn = false; break; }
                    }
                    bool raised = minY >= nb->y + 0.25f;
                    if (allIn && (vertical || raised)) {
                        drop = true;
                        ++barDrop;
                    } else {
                        if (!allIn) ++keptOut; else ++keptFlat;
                        if (!barSample[0])
                            snprintf(barSample, sizeof barSample, "%s: (%.1f,%.1f,%.1f) (%.1f,%.1f,%.1f) (%.1f,%.1f,%.1f) from point",
                                     allIn ? "flat, low" : "corner too far", p0[0] - nb->x, p0[1] - nb->y, p0[2] - nb->z, p1[0] - nb->x,
                                     p1[1] - nb->y, p1[2] - nb->z, p2[0] - nb->x, p2[1] - nb->y, p2[2] - nb->z);
                    }
                }
            }
            if (drop) continue;
            outTris[w]  = outTris[k];
            outVerts[w] = outVerts[k];
            ++w;
        }
        if (nNear) {
            static std::atomic<int> logged{0};
            if (logged.fetch_add(1) < 15)
                dlogf("collision query near a broken tree: T=%p class=%d returned=%d near=%d removed=%d box=(%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f) tri: %s",
                     T, cls, n, nNear, n - w, box[0], box[1], box[2], box[4], box[5], box[6], sample);
        }
        if (barNear) {
            g_barFiltered.fetch_add(barDrop);
            static std::atomic<int> logged{0};
            if ((keptOut || keptFlat) && logged.fetch_add(1) < 60)
                dlogf("collision near a barrier at (%.1f, %.1f, %.1f): %d triangles, %d near, %d removed, kept %d (corner too far) %d (flat, low); e.g. %s",
                     (box[0] + box[4]) / 2, (box[1] + box[5]) / 2, (box[2] + box[6]) / 2, n, barNear, barDrop, keptOut, keptFlat, barSample);
        }
        if (w != n) g_filtered.fetch_add(n - w);
        if (g_markEnabled && near_player(box, 6.0f)) record_tris(T, outTris, outVerts, w, n - w, nullptr);
        if (g_markEnabled) {
            int q = (g_rqPos.load() - 1) & (NRQ - 1);
            if (g_rq[q].T == reinterpret_cast<uintptr_t>(T)) g_rq[q].removed = n - w;
        }
        return w;
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        return n;
    }
}

// CWorldCollObj::OnCollision (vtable slot 0). After it, a standing tree that
// stops being static (state +0x148 leaves 0) has broken.
// ---- Wreckage (Skills::Destruction) retrigger delay ---------------------------
// Skills.xml gives Destruction RetriggerDelay=2.5 s: after a Wreckage, smashes in
// the next 2.5 s don't award another one. Destruction's vtable slot 14 loads its
// settings; SkillBase keeps RetriggerDelay at +0x24 (float).
float g_wreckRetrigger = 1.0f;
std::atomic<uint8_t*> g_destruction{nullptr};
using SkillLoadFn = void (*)(uint8_t* skill, void* node);
SkillLoadFn o_destructionLoad = nullptr;
constexpr uintptr_t RVA_DESTRUCTION_VTABLE = 0x2D4BBF0;
void d_destructionLoad(uint8_t* skill, void* node) {
    o_destructionLoad(skill, node);
    __try {
        float* rt = reinterpret_cast<float*>(skill + 0x24);
        dlogf("Wreckage skill loaded: RetriggerDelay %.2f, MaxTimeBetweenCollisions %.2f", *rt, *reinterpret_cast<float*>(skill + 0x44));
        g_destruction.store(skill);
        if (g_wreckRetrigger >= 0 && *rt > 0 && *rt < 60) {
            *rt = g_wreckRetrigger;
            dlogf("Wreckage RetriggerDelay set to %.2f", *rt);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// Lumberjack and Sideswipe: same RetriggerDelay field, own vtables (slot 14 = load).
float g_lumberjackRetrigger = 2.5f, g_sideswipeRetrigger = 5.0f;
constexpr uintptr_t RVA_LUMBERJACK_VTABLE = 0x2D4BA28, RVA_SIDESWIPE_VTABLE = 0x2D4B270;
SkillLoadFn o_lumberjackLoad = nullptr, o_sideswipeLoad = nullptr;
void set_retrigger(uint8_t* skill, const char* name, float value) {
    __try {
        float* rt = reinterpret_cast<float*>(skill + 0x24);
        dlogf("%s skill loaded: RetriggerDelay %.2f", name, *rt);
        if (value >= 0 && *rt >= 0 && *rt < 60) {
            *rt = value;
            dlogf("%s RetriggerDelay set to %.2f", name, *rt);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}
void d_lumberjackLoad(uint8_t* skill, void* node) { o_lumberjackLoad(skill, node); set_retrigger(skill, "Lumberjack", g_lumberjackRetrigger); }
void d_sideswipeLoad(uint8_t* skill, void* node) { o_sideswipeLoad(skill, node); set_retrigger(skill, "Sideswipe", g_sideswipeRetrigger); }

// ---- speed loss: the car that broke a tree loses speed by the tree's size ----
// contact+0x08 is the other physics object; type 1 = a car. Its CCarDynamics
// interface is at car+0x4C08: vtable +0x288 GetVelocity(out), +0x290 SetVelocity(in).
float g_speedLoss = 0.12f, g_speedLossExponent = 1.5f, g_speedLossMax = 0.45f;
std::atomic<uint8_t*> g_lastCarDyn{nullptr};
void apply_speed_loss(void* contact, float size, float* lossOut, float* beforeMph, float* afterMph, float* velOut, float fixedLoss = -1) {
    __try {
        uint8_t* other = *reinterpret_cast<uint8_t**>(static_cast<uint8_t*>(contact) + 8);
        if (!other) return;
        void** vt = *reinterpret_cast<void***>(other);
        using IntFn = int (*)(void*);
        using PtrFn = uint8_t* (*)(void*);
        if (reinterpret_cast<IntFn>(vt[0x38 / 8])(other) != 1) return; // not a car
        uint8_t* car = reinterpret_cast<PtrFn>(vt[0x40 / 8])(other);
        if (!car) return;
        car -= 0x10;
        uint8_t* dyn = *reinterpret_cast<uint8_t**>(car + 0x4C08);
        if (!dyn) return;
        void** dvt = *reinterpret_cast<void***>(dyn);
        if (reinterpret_cast<uintptr_t>(dvt[0x290 / 8]) != g_base + 0x12E3CF0) return; // not CCarDynamics::SetVelocity
        g_lastCarDyn.store(dyn);
        using GetVelFn = float* (*)(void*, float*);
        using SetVelFn = void (*)(void*, const float*);
        alignas(16) float v[4];
        reinterpret_cast<GetVelFn>(dvt[0x288 / 8])(dyn, v);
        velOut[0] = v[0]; velOut[1] = v[1]; velOut[2] = v[2];
        float loss = fixedLoss >= 0 ? fixedLoss : g_speedLoss * std::pow(size, g_speedLossExponent);
        if (loss <= 0) return;
        if (loss > g_speedLossMax) loss = g_speedLossMax;
        if (loss < 0) loss = 0;
        float speed = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
        alignas(16) float nv[4] = {v[0] * (1 - loss), v[1] * (1 - loss), v[2] * (1 - loss), v[3]};
        reinterpret_cast<SetVelFn>(dvt[0x290 / 8])(dyn, nv);
        *lossOut   = loss;
        *beforeMph = speed * 2.23694f;
        *afterMph  = speed * (1 - loss) * 2.23694f;
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ---- extra break effects ------------------------------------------------------
// The game spawns a smashable's break effect with 0x1403820C0(mgr, const std::string& name,
// const Matrix44* where, const vec4* dir, const vec4* velocity, float scale, bool, bool, car*),
// the name being "SMASHABLE_" + a BreakEffects.xml entry (see 0x140377DF0).
char g_bigTreeEffects[256] = "Break_WoodDust,Break_Wood";
char g_treeEffects[256]    = "";
float g_effectScale        = 1.0f;
using SpawnEffectFn = void (*)(uint8_t* mgr, const MsvcString* name, const float* where, const float* dir, const float* vel,
                               float scale, uint8_t a, uint8_t b, void* car);

void spawn_effect(uint8_t* mgr, const char* effect, float x, float y, float z, const float* carVel) {
    char full[96];
    snprintf(full, sizeof full, "SMASHABLE_%s", effect);
    MsvcString name{};
    size_t len = std::strlen(full);
    if (len < 16) { std::memcpy(name.buf, full, len + 1); name.cap = 15; }
    else { name.ptr = full; name.cap = len; }
    name.size = len;
    float vx = carVel[0], vy = carVel[1], vz = carVel[2];
    float sp = std::sqrt(vx * vx + vy * vy + vz * vz);
    alignas(16) float where[16] = {1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, x + vx * 0.015f, y + 0.3f, z + vz * 0.015f, 1};
    alignas(16) float dir[4]    = {0, 1, 0, 0};
    if (sp > 0.1f) {
        float dx = vx / sp, dy = vy / sp + 0.4f, dz = vz / sp;
        float l  = std::sqrt(dx * dx + dy * dy + dz * dz);
        dir[0] = dx / l; dir[1] = dy / l; dir[2] = dz / l;
    }
    alignas(16) float vel[4] = {vx * 0.67f, vy * 0.67f, vz * 0.67f, 0};
    reinterpret_cast<SpawnEffectFn>(g_base + 0x3820C0)(mgr, &name, where, dir, vel, g_effectScale, 0, 0, nullptr);
}

void spawn_effects(uint8_t* mgr, const char* list, float x, float y, float z, const float* carVel) {
    char item[64];
    const char* r = list;
    while (*r) {
        int k = 0;
        while (*r && *r != ',' && k < 63) item[k++] = *r++;
        item[k] = 0;
        if (*r == ',') ++r;
        while (k && item[k - 1] == ' ') item[--k] = 0;
        const char* it = item;
        while (*it == ' ') ++it;
        if (*it) {
            __try {
                spawn_effect(mgr, it, x, y, z, carVel);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                dlogf("effect %s failed", it);
            }
        }
    }
}

// ---- logs: a loose piece of another smashable thrown out of a broken tree ------
// CWorldCollObjMan::SpawnObject 0x140369690(mgr, flag, defId, slot, const vec4* pos,
// Matrix34* rot (orthonormalised in place), int, const Key* key) builds a collidable in a
// free slot. The break-into-pieces code (0x14036AC80) then: copies the parent's key to
// +0xC8, sets state 5, binds the render model (0x140377340), adds the body to the
// physics world (0x140369E80), body->vt[0x70](7), body->vt[0x128](0,0), and sets the
// linear/angular velocity through the object's vtable +0x88/+0x90.
// A defId with a piece index in the high 16 bits spawns that one piece on its own.
struct LogCandidate { uint16_t def, piece; };
LogCandidate g_logCands[32];
LogCandidate g_logModel{0xFFFF, 0xFFFF}; // whose model the log is drawn with (0xFFFF = its own)
LogCandidate g_poleLook{97, 4};           // and the falling telephone pole
// Models are only streamed in near where the game uses them: the apple tree's pieces are only
// loaded near orchards. A piece drawn with a model that isn't loaded is invisible, so logs, poles
// and tree pieces fall back on the first of these whose models are loaded where they break.
LogCandidate g_lookAlts[8] = {{97, 4}, {262, 4}, {98, 4}, {263, 4}};
int g_nLookAlts = 4;
bool model_resident(int model);
bool def_resident(uint8_t* table, int def);
char g_logModelScale[256] = "948:0.85/0.4";   // LogModelScale: "model:length/thickness,..." extra per-model factors
// A stand-in model's size relative to the preferred one, from LogModelScale ("model:length/thickness").
// a look's definition on this map (def 0xFFFE: this map hasn't got it)
LogCandidate nlc(uint8_t* table, LogCandidate c) {
    if (c.def == 0xFFFF) return c;
    int v = nd(table, c.def);
    return LogCandidate{(uint16_t)(v < 0 ? 0xFFFE : v), c.piece};
}
// LogModelScale's model numbers are the mainland's; the models we know them as, on this map
int native_model(uint8_t* table, int m) {
    DefMap* dm = g_lastDefMap.load(std::memory_order_acquire);
    if (!dm || dm->table != table || !dm->moved) return m;
    static const struct { int model, def, piece; } known[] = {{948, 262, 4}};   // the olive trunk
    for (const auto& k : known)
        if (k.model == m) {
            int d = nd(table, k.def);
            return d < 0 ? -1 : o_defModel(table, ((uint32_t)k.piece << 16) | (uint32_t)d);
        }
    return -1;
}
void look_factors(uint8_t* table, int got, float* lenK, float* thickK) {
    *lenK = *thickK = 1.0f;
    for (const char* r = g_logModelScale; *r;) {
        while (*r == ' ' || *r == ',') ++r;
        if (!*r) break;
        int m = 0; float l = 1, t = 1;
        if (sscanf(r, "%d:%f/%f", &m, &l, &t) >= 2 && native_model(table, m) == got) { *lenK *= l; *thickK *= (t > 0 ? t : l); }
        while (*r && *r != ',') ++r;
    }
}
// the first look whose model is loaded (the preferred one first), or the preferred one.
// `want` is in this map's numbers; *loaded says whether the one returned is loaded.
LogCandidate pick_look(uint8_t* table, LogCandidate want, bool* loaded = nullptr) {
    if (loaded) *loaded = true;
    if (want.def == 0xFFFF || !g_loadedLooks) return want;
    if (want.def != 0xFFFE && model_resident(o_defModel(table, ((uint32_t)want.piece << 16) | want.def))) return want;
    for (int k = 0; k < g_nLookAlts; ++k) {
        LogCandidate c = nlc(table, g_lookAlts[k]);
        if (c.def == 0xFFFE) continue;
        uint8_t* whole = o_defLookup(table, 0xFFFF0000u | c.def);
        if (!whole || !readable(whole, 0xA0) || c.piece >= *reinterpret_cast<uint32_t*>(whole + 0x94)) continue;
        if (model_resident(o_defModel(table, ((uint32_t)c.piece << 16) | c.def))) return c;
    }
    if (loaded) *loaded = false;
    return want;
}
std::atomic<int> g_lookSwaps{0};
int g_nLogCands = 0;
int g_logs = 1, g_logCycle = 1, g_logAxis = 1;
float g_logLengthJitter = 0.25f, g_logLengthSizeExp = 1.0f, g_logLengthMin = 1.5f, g_logLengthMax = 5.0f;
float g_logLength = 1.0f, g_logThickness = 1.0f, g_logMinSize = 1.6f, g_logHeight = 1.0f, g_logAhead = 0.0f, g_logSpeed = 0.7f, g_logUp = 2.0f, g_logSpin = 3.0f,
      g_logTilt = 90.0f;
// LogFromCar: a log appears standing just ahead of the car, leaning forward, so the nose knocks
// it over, instead of at the tree, a metre up, where the car ran under it and carried it on its
// roof. LogCarAhead: metres ahead of the car's centre; LogCarHeight: its base's clearance over
// the ground; LogCarTilt: its forward lean (degrees); LogCarUp: its upward speed (m/s).
int g_logFromCar = 1;
float g_logCarAhead = 3.0f, g_logCarHeight = 0.1f, g_logCarUp = 0.0f, g_logCarTilt = 8.0f;
std::atomic<int> g_logNext{0}, g_logsSpawned{0};

struct PendingLog { uint8_t* mgr; float x, y, z, vx, vy, vz; uint8_t key[16]; uint32_t b0, b4; LogCandidate c; int n; float length;
                    bool pole; float thick, tilt, spin, speedK, up, height;
                    int ownModel; float r0[9], scale; };   // a pole's own model, rotation rows (unit) and scale; ownModel -1: none
int g_poleOwnModel = 1;   // PoleOwnModel: a falling pole is drawn with the pole's own model
std::mutex g_pendLock;
PendingLog g_pend[16];
int g_nPend = 0;

void parse_log_candidates(const char* s) {
    g_nLogCands = 0;
    while (*s && g_nLogCands < 32) {
        while (*s == ' ' || *s == ',') ++s;
        if (!*s) break;
        char* e;
        long d = std::strtol(s, &e, 10);
        if (e == s) break;
        long p = 0xFFFF;
        s = e;
        if (*s == ':') { p = std::strtol(s + 1, &e, 10); s = e; }
        if (d >= 0 && d < 0xF000) g_logCands[g_nLogCands++] = LogCandidate{(uint16_t)d, (uint16_t)(p < 0 ? 0xFFFF : p)};
        while (*s && *s != ',') ++s;
    }
}

// Each log's length: LogLength x tree size^LogLengthSizeExponent, varied by up to +/-LogLengthJitter,
// kept within LogLengthMin..LogLengthMax (multiples of the model's own length).
float log_length_for(float size) {
    static std::atomic<uint32_t> seed{0};
    uint32_t x = seed.load();
    if (!x) x = GetTickCount() | 1;
    x ^= x << 13; x ^= x >> 17; x ^= x << 5;
    seed.store(x);
    float r = (x & 0xFFFFFF) / float(0xFFFFFF) * 2 - 1;   // -1..1
    if (!(size > 0)) size = 1;
    float len = g_logLength * std::pow(size, g_logLengthSizeExp) * (1 + r * g_logLengthJitter);
    if (len < g_logLengthMin) len = g_logLengthMin;
    if (len > g_logLengthMax) len = g_logLengthMax;
    return len;
}

void push_pending(const PendingLog& p) {
    std::lock_guard<std::mutex> l(g_pendLock);
    if (g_nPend < 16) g_pend[g_nPend++] = p;
}
int take_pending(PendingLog* out) {
    std::lock_guard<std::mutex> l(g_pendLock);
    int n = g_nPend;
    std::memcpy(out, g_pend, sizeof(PendingLog) * n);
    g_nPend = 0;
    return n;
}
void queue_log(uint8_t* obj, float x, float y, float z, const float* carVel, float size) {
    if (!g_logs || !g_nLogCands) return;
    int n = g_logCycle ? g_logNext.fetch_add(1) : 0;
    PendingLog p{};
    p.mgr = *reinterpret_cast<uint8_t**>(obj + 0x40);
    p.x = x; p.y = y; p.z = z;
    p.vx = carVel[0]; p.vy = carVel[1]; p.vz = carVel[2];
    std::memcpy(p.key, obj + 0xB8, 16);
    p.b0 = *reinterpret_cast<uint32_t*>(obj + 0xB0);
    p.b4 = *reinterpret_cast<uint32_t*>(obj + 0xB4);
    p.c = g_logCands[n % g_nLogCands];
    p.n = n;
    p.length = log_length_for(size);
    p.pole = false; p.thick = g_logThickness * std::min(1.0f, std::max(0.35f, size));   // smaller trees, thinner logs
    p.length = std::max(p.length, 0.9f);
    p.tilt = g_logTilt; p.spin = g_logSpin; p.speedK = g_logSpeed; p.up = g_logUp; p.height = g_logHeight;
    std::lock_guard<std::mutex> l(g_pendLock);
    if (g_nPend < 16) g_pend[g_nPend++] = p;
}

using SpawnObjectFn = int (*)(uint8_t* mgr, uint8_t flag, uint32_t id, int slot, const float* pos, float* rot, int a7, void* key);
using RecycleSlotFn = int (*)(uint8_t* mgr);
using BindModelFn   = void (*)(uint8_t* mgr, int slot, uint8_t* obj, int model, const float* m);
using AddPhysicsFn  = void (*)(uint8_t* mgr, uint8_t* obj);
using BodyIntFn     = void (*)(void* body, int v);
using BodyWakeFn    = void (*)(void* body, int a, int b);
using ObjVecFn      = void (*)(uint8_t* obj, const float* v);

void track_log(int slot, uint8_t* obj, uint32_t id, float length, float thick);
// a render-only copy of a piece's definition with its culling radius scaled, one per piece and use
uint8_t* render_def_for(uint8_t* table, uint32_t mid, float scale, bool pole) {
    struct Entry { uint32_t mid; bool pole; uint8_t* def; };
    static Entry cache[16];
    static int n = 0;
    for (int k = 0; k < n; ++k) if (cache[k].mid == mid && cache[k].pole == pole) return cache[k].def;
    uint8_t* md = o_defLookup(table, mid);
    if (!md || !readable(md, DEF_SIZE) || n >= 16) return nullptr;
    uint8_t* d = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!d) return nullptr;
    std::memcpy(d, md, DEF_SIZE);
    *reinterpret_cast<float*>(d + 0x98) *= scale;
    cache[n++] = Entry{mid, pole, d};
    dlogf("%s drawn with model %d (%u:%u)", pole ? "falling poles can be" : "logs can be", o_defModel(table, mid), mid & 0xFFFF, mid >> 16);
    return d;
}
// A render-only copy of the falling pole's body definition for drawing the pole's own model:
// the base half a pole below the body (see the centre of mass below) and a
// culling radius that covers the whole pole. One per half height, to a centimetre.
uint8_t* pole_own_def(uint8_t* table, uint32_t id, float half) {
    struct Entry { uint32_t id; int h; uint8_t* def; };
    static Entry cache[64];
    static int n = 0;
    const int hq = (int)std::lround(half * 100.0f);
    for (int k = 0; k < n; ++k) if (cache[k].id == id && cache[k].h == hq) return cache[k].def;
    uint8_t* md = o_defLookup(table, id);
    if (!md || !readable(md, DEF_SIZE) || n >= 64) return nullptr;
    uint8_t* d = static_cast<uint8_t*>(VirtualAlloc(nullptr, 0x1000, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!d) return nullptr;
    std::memcpy(d, md, DEF_SIZE);
    // The game places a body at its model's origin minus the centre of mass (SpawnCell,
    // 0x1403690A2) and draws the model back at the body plus it, so a model that starts at its
    // base, drawn on a body centred halfway up, wants the centre of mass at minus half the height.
    float* com = reinterpret_cast<float*>(d + 0x68);
    com[0] = 0; com[1] = -half; com[2] = 0;
    float* radius = reinterpret_cast<float*>(d + 0x98);
    *radius = std::max(*radius, half + 1.5f);
    cache[n++] = Entry{id, hq, d};
    return d;
}

void spawn_log(const PendingLog& p) {
    uint8_t* mgr   = p.mgr;
    uint8_t* table = *reinterpret_cast<uint8_t**>(mgr + 0x4C8);
    // this map's numbers for the log's own piece and the looks it's drawn with
    const LogCandidate poleLook = nlc(table, g_poleLook), logModel = nlc(table, g_logModel);
    LogCandidate lc = nlc(table, p.c);
    if (lc.def == 0xFFFE) {
        // the piece that's normally its body (the wooden pylon's pole, 28:3) isn't on this map
        // (Storm Island): the trunk it's drawn with is its body instead
        LogCandidate want = p.pole ? poleLook : logModel;
        LogCandidate look = want.def != 0xFFFF ? pick_look(table, want) : want;
        static std::atomic<int> nm{0};
        if (look.def >= 0xFFFE) {
            if (++nm <= 10) logf("%s %d: definition %u isn't on this map, nor a trunk to use instead; left out", p.pole ? "pole" : "log", p.n, p.c.def);
            return;
        }
        if (++nm <= 3) dlogf("%s %d: definition %u isn't on this map; %u:%u is its body instead", p.pole ? "pole" : "log", p.n, p.c.def, look.def, look.piece);
        lc = look;
    }
    uint32_t id    = ((uint32_t)lc.piece << 16) | lc.def;
    // the definition and piece must exist
    uint8_t* whole = o_defLookup(table, 0xFFFF0000u | lc.def);
    if (!whole || *reinterpret_cast<uintptr_t*>(whole) != g_base + 0x2EB5BD8) { dlogf("log %d: %u is not a definition", p.n, lc.def); return; }
    if (lc.piece != 0xFFFF && lc.piece >= *reinterpret_cast<uint32_t*>(whole + 0x94)) {
        dlogf("log %d: definition %u has no piece %u", p.n, lc.def, lc.piece);
        return;
    }
    // free slot: the set at mgr+0x508 (value at node+0x1C), else recycle the farthest loose piece
    int slot;
    uint8_t* head  = *reinterpret_cast<uint8_t**>(mgr + 0x508);
    uint8_t* first = *reinterpret_cast<uint8_t**>(head);
    slot = first != head ? *reinterpret_cast<int32_t*>(first + 0x1C) : reinterpret_cast<RecycleSlotFn>(g_base + 0x37A7D0)(mgr);
    if (slot < 0 || slot >= 0x1000) { dlogf("log %d: no free slot", p.n); return; }

    // orientation: the piece's long axis (LogAxis) tilted LogTilt degrees from up toward the direction of travel
    float sp = std::sqrt(p.vx * p.vx + p.vz * p.vz);
    float fx = 1, fz = 0;
    if (sp > 0.5f) { fx = p.vx / sp; fz = p.vz / sp; }
    // a log from the car stands up just ahead of it, leaning LogCarTilt degrees forward
    const bool fromCar = !p.pole && g_logFromCar && sp > 2.0f;
    float t = (fromCar ? g_logCarTilt : p.tilt) * 3.14159265f / 180.0f, ct = std::cos(t), st = std::sin(t);
    float a[3] = {st * fx, ct, st * fz};   // long axis
    float b[3] = {fz, 0, -fx};            // side (horizontal, perpendicular to travel)
    float c[3] = {a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]};
    alignas(16) float rot[12] = {};
    const float* basis[3] = {a, b, c};
    for (int k = 0; k < 3; ++k) {
        int row = (g_logAxis + k) % 3;
        rot[row * 4 + 0] = basis[k][0]; rot[row * 4 + 1] = basis[k][1]; rot[row * 4 + 2] = basis[k][2];
    }
    alignas(16) float pos[4] = {p.x + fx * g_logAhead, p.y + p.height, p.z + fz * g_logAhead, 1};
    if (fromCar) {
        // standing on its base: the car's centre is about 0.45 m above the ground; the centre of
        // the body goes half the log up from there (at least half the body's own shape, about
        // 3.2 m long, so it never starts in the ground), plus LogCarHeight of clearance
        const float* car = reinterpret_cast<const float*>(mgr + 0x4F0);
        const float half = std::max(p.length * 0.5f, 1.6f);
        pos[0] = car[0] + fx * g_logCarAhead + a[0] * half;
        pos[1] = car[1] - 0.45f + g_logCarHeight + a[1] * half;
        pos[2] = car[2] + fz * g_logCarAhead + a[2] * half;
    }
    // A pole with its own model: it starts from how the pole stood (its own rotation, so an
    // N-shaped pole's crossarms keep their direction), tipped by the same tilt toward the
    // direction of travel, with its body centred halfway up it.
    const bool own = p.pole && p.ownModel > 0 && g_logAxis == 1;
    if (own) {
        for (int r = 0; r < 3; ++r) {
            const float* v = p.r0 + r * 3;   // rotate about b by t (Rodrigues)
            float bxv[3] = {b[1] * v[2] - b[2] * v[1], b[2] * v[0] - b[0] * v[2], b[0] * v[1] - b[1] * v[0]};
            float bd = b[0] * v[0] + b[1] * v[1] + b[2] * v[2];
            for (int k = 0; k < 3; ++k) rot[r * 4 + k] = v[k] * ct + bxv[k] * st + b[k] * bd * (1 - ct);
        }
        const float half = p.height * p.scale;
        pos[0] = p.x + fx * g_logAhead + rot[4] * half;
        pos[1] = p.y + rot[5] * half;
        pos[2] = p.z + fz * g_logAhead + rot[6] * half;
    }

    slot = reinterpret_cast<SpawnObjectFn>(g_base + 0x369690)(mgr, 0, id, slot, pos, rot, 0, nullptr);
    if (slot < 0) { dlogf("log %d: spawn refused", p.n); return; }
    uint8_t* obj = reinterpret_cast<uint8_t**>(vec_at(mgr + 0x290)->first)[slot];
    if (!obj) { dlogf("log %d: spawn made nothing", p.n); return; }
    std::memcpy(obj + 0xC8, p.key, 16);   // parent key: lets the game recycle it like any loose piece
    *reinterpret_cast<int32_t*>(obj + 0x148)  = 5;
    *reinterpret_cast<uint32_t*>(obj + 0xB0) = p.b0;
    *reinterpret_cast<uint32_t*>(obj + 0xB4) = p.b4;
    int model = o_defModel(table, id);
    // Drawn with another piece's model (a tree trunk, for its bark): bind with a copy of that
    // piece's definition so its centre-of-mass offset and culling radius are used.
    uint8_t* renderDef = nullptr;
    float lenK = 1.0f, thickK = 1.0f;
    if (own) {
        // the pole's own model, bound with a copy of the body's definition that puts the model's
        // base half a pole below the body and whose culling radius covers it
        renderDef = pole_own_def(table, id, p.height * p.scale);
        if (renderDef) {
            g_poleRenderDef.store(renderDef);
            model = p.ownModel;
            static int said = 0;
            if (said++ < 10) dlogf("pole %d: drawn with its own model %d (scale %.2f)", p.n, model, p.scale);
        }
    }
    if (p.pole && !renderDef) {
        // a pole: drawn with PoleModel (the apple trunk's bark by default), with a copy of that
        // piece's definition whose culling radius covers the stretch
        LogCandidate look = pick_look(table, poleLook);
        uint32_t mid = look.def < 0xFFFE ? ((uint32_t)look.piece << 16) | look.def : id;
        model = o_defModel(table, mid);
        look_factors(table, model, &lenK, &thickK);
        renderDef = render_def_for(table, mid, std::max(p.length * lenK, p.thick * thickK) + 1.0f, true);
        g_poleRenderDef.store(renderDef);
        if (look.def != poleLook.def || look.piece != poleLook.piece) {
            int k = ++g_lookSwaps;
            if (k <= 20) dlogf("pole %d: %u:%u isn't loaded here; drawn with model %d (%u:%u) instead", p.n, poleLook.def, poleLook.piece,
                              model, look.def, look.piece);
        }
    } else if (!p.pole && logModel.def != 0xFFFF && pick_look(table, logModel).def < 0xFFFE) {
        LogCandidate look = pick_look(table, logModel);
        uint32_t mid = ((uint32_t)look.piece << 16) | look.def;
        int got = o_defModel(table, mid);
        look_factors(table, got, &lenK, &thickK);
        renderDef = render_def_for(table, mid, std::max(std::max(g_logLength, g_logLengthMax) * lenK, g_logThickness * thickK) + 1.0f, false);
        if (renderDef) {
            g_logRenderDef.store(renderDef);
            model = got;
            if (look.def != logModel.def || look.piece != logModel.piece) {
                int k = ++g_lookSwaps;
                if (k <= 20) dlogf("log %d: %u:%u isn't loaded here; drawn with model %d (%u:%u) instead", p.n, logModel.def, logModel.piece,
                                  model, look.def, look.piece);
            }
        }
    }
    alignas(16) float m[16] = {rot[0], rot[1], rot[2], 0, rot[4], rot[5], rot[6], 0, rot[8], rot[9], rot[10], 0, pos[0], pos[1], pos[2], 1};
    // experimental: scale the render model (the collision shape keeps its own size)
    const bool ownDrawn = own && model == p.ownModel;
    const float lenS = ownDrawn ? p.scale : p.length * lenK, thickS = ownDrawn ? p.scale : p.thick * thickK;
    for (int r = 0; r < 3; ++r) {
        float k = r == g_logAxis ? lenS : thickS;
        m[r * 4 + 0] *= k; m[r * 4 + 1] *= k; m[r * 4 + 2] *= k;
    }
    if (renderDef) *reinterpret_cast<uint32_t*>(obj + 0x124) = 0xFFFF0000u | (p.pole ? POLE_RENDER_DEF : LOG_RENDER_DEF);
    reinterpret_cast<BindModelFn>(g_base + 0x377340)(mgr, slot, obj, model, m);
    *reinterpret_cast<uint32_t*>(obj + 0x124) = id;
    reinterpret_cast<AddPhysicsFn>(g_base + 0x369E80)(mgr, obj);
    void* body = *reinterpret_cast<void**>(obj + 8);
    if (body) {
        void** bvt = *reinterpret_cast<void***>(body);
        reinterpret_cast<BodyIntFn>(bvt[0x70 / 8])(body, 7);
        reinterpret_cast<BodyWakeFn>(bvt[0x128 / 8])(body, 0, 0);
    }
    void** ovt = *reinterpret_cast<void***>(obj);
    alignas(16) float lin[4] = {p.vx * p.speedK, p.vy * p.speedK + (fromCar ? g_logCarUp : p.up), p.vz * p.speedK, 0};
    alignas(16) float ang[4] = {b[0] * p.spin, 0, b[2] * p.spin, 0}; // tumble forward
    reinterpret_cast<ObjVecFn>(ovt[0x88 / 8])(obj, lin);
    reinterpret_cast<ObjVecFn>(ovt[0x90 / 8])(obj, ang);
    track_log(slot, obj, id, lenS, thickS);
    if (lenK != 1.0f || thickK != 1.0f) {
        static int said = 0;
        if (said++ < 20) dlogf("log %d: stand-in model %d scaled %.2f long x %.2f thick to match", p.n, model, lenK, thickK);
    }
    int total = g_logsSpawned.fetch_add(1) + 1;
    dlogf("log %d: spawned %u:%u (model %d, mass %.2f, radius %.2f, stretched %.2f long) in slot %d at (%.1f, %.1f, %.1f); %d logs so far", p.n,
         lc.def, lc.piece, model, *reinterpret_cast<float*>(o_defLookup(table, id) + 0x9C),
         *reinterpret_cast<float*>(o_defLookup(table, id) + 0x98), p.length, slot, pos[0], pos[1], pos[2], total);
}


// ---- stretching logs: finding where the renderer keeps a loose piece's transform -----
// CTrackPresentation::BindCollObjInstanceToModel (0x14132B390, reached through the deferred
// presentation) makes a 0x70-byte model instance at this+0x38+slot*8 (+0x38 position, +0x44
// rotation as 9 halfs) and hands the matrix to a render node, (this+0xF630)[index]. Something
// then moves the node every frame from the physics body, which undoes any stretch. This build
// watches both, finds the per-frame matrix, and re-stretches it every frame.
using TpBindFn = uint64_t (*)(uint8_t* tp, int slot, int model, const float* m, void* key);
TpBindFn o_tpBind = nullptr;
struct SlotRender { std::atomic<uint8_t*> inst{nullptr}, node{nullptr}; };
SlotRender g_slotRender[0x1000];
std::atomic<uint8_t*> g_slotTp[0x1000];   // the presentation each slot was bound in
std::atomic<uint8_t*> g_trackPres{nullptr};

// Is this render model streamed in? (tp+0xF618)[model] is the model's entry; its resource
// (+0x08) is null while the model isn't loaded, and a node bound to it is never drawn.
bool model_resident(int model) {
    uint8_t* tp = g_trackPres.load(std::memory_order_relaxed);
    if (!tp || model < 0 || !readable(tp + 0xF618, 0x10)) return true;   // can't tell: assume it is
    uint8_t** mb = *reinterpret_cast<uint8_t***>(tp + 0xF618);
    uint8_t** me = *reinterpret_cast<uint8_t***>(tp + 0xF620);
    if (!mb || model >= me - mb) return true;
    uint8_t* md = mb[model];
    if (!md || !readable(md, 0x28)) return false;
    return *reinterpret_cast<uint64_t*>(md + 8) != 0;
}
// every piece of a definition has its model loaded
bool def_resident(uint8_t* table, int def) {
    uint8_t* whole = o_defLookup(table, 0xFFFF0000u | (uint32_t)def);
    if (!whole || !readable(whole, 0xA0)) return false;
    uint32_t n = *reinterpret_cast<uint32_t*>(whole + 0x94);
    if (n > 64) return false;
    for (uint32_t k = 0; k < n; ++k)
        if (!model_resident(o_defModel(table, (k << 16) | (uint32_t)def))) return false;
    return true;
}

// Breaking into pieces (0x14036AC80(mgr, obj)) spawns one loose piece per child of obj+0x124's
// definition. For a tree whose pieces aren't loaded here, it breaks as another tree whose are.
using BreakPiecesFn = void (*)(uint8_t* mgr, uint8_t* obj);
BreakPiecesFn o_breakPieces = nullptr;
std::atomic<int> g_treeSwaps{0};
void d_breakPieces(uint8_t* mgr, uint8_t* obj) {
    uint32_t keep = 0;
    bool swapped = false;
    __try {
        uint32_t id = *reinterpret_cast<uint32_t*>(obj + 0x124);
        int def = (int)(id & 0xFFFF);
        uint8_t* table = *reinterpret_cast<uint8_t**>(mgr + 0x4C8);
        // this map's numbers for the trees
        int treeDef = table ? tree_def(table, false) : g_treeDef, bigDef = table ? tree_def(table, true) : g_bigTreeDef;
        bool tree = (id >> 16) == 0xFFFF && (def == treeDef || def == bigDef || (def >= CLONE_BASE && def < CLONE_BASE + NUM_TIERS));
        for (int k = 0; k < g_nTreeAlts && !tree && table; ++k) tree = (id >> 16) == 0xFFFF && def == nd(table, g_treeAlts[k]);
        if (tree && table) {
            int pieceDef = def >= CLONE_BASE ? treeDef : def;   // tiered copies break into the tree definition's pieces
            if (!def_resident(table, pieceDef)) {
                for (int k = 0; k < g_nTreeAlts; ++k) {
                    int alt = nd(table, g_treeAlts[k]);
                    if (alt < 0 || alt == pieceDef || !def_resident(table, alt)) continue;
                    keep = id;
                    *reinterpret_cast<uint32_t*>(obj + 0x124) = 0xFFFF0000u | (uint32_t)alt;
                    swapped = true;
                    int n = ++g_treeSwaps;
                    if (n <= 20 || n % 100 == 0)
                        dlogf("tree pieces: definition %d isn't loaded here; broke as %d instead (%d so far)", pieceDef, alt, n);
                    break;
                }
                if (!swapped) {
                    static int none = 0;
                    if (none++ < 10) logf("tree pieces: no tree's pieces are loaded here; they won't be visible");
                }
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    o_breakPieces(mgr, obj);
    if (swapped) *reinterpret_cast<uint32_t*>(obj + 0x124) = keep;
}
int g_logStretch = 1, g_logProbe = 0;

// ---- loose fences drawn with their own model (platform 2.6.0 / API 7) ------------------------
// A breakable with no pieces (a fence, most clutter) breaks off as a fresh copy of itself, and
// BreakIntoPieces draws that copy with its definition's model (0x1412CD300, then the bind at
// 0x14036AEC0). A fence made breakable by a name rule borrows another fence's definition, so the
// copy came out as that fence: a green fence flew off as the rusty silver one. An instruction hook
// at 0x14036AEAC, just after the model comes back in eax (r15 = the object hit, r14 = the
// manager), puts in the model the procedural actually draws it with:
//   object +0xC0 = its procedural's cell id, +0xC4 = its child; children are numbered through the
//   procedural's groups in order, and a group's +4 is its batch (D_MODELS, 0x58 each);
//   batch +0x50 -> render data R; R[+0] -> E; E[+0x10] -> the template instance it's copied from;
//   that placed instance (track presentation +0xFB78, 0x70 each) has the model at +0x10.
// For the game's own breakables that's the definition's model anyway; it only changes anything
// when it differs, and only when that model is loaded, so a piece is never drawn invisible.
int g_ownModels = 1;   // OwnModels
std::atomic<int> g_ownSwaps{0};

// Reads that can't fault: these follow pointers in game data that may be going away.
bool sread(uint64_t p, void* out, size_t n) {
    if (p < 0x10000) return false;
    SIZE_T got = 0;
    return ReadProcessMemory(GetCurrentProcess(), reinterpret_cast<const void*>(p), out, n, &got) && got == n;
}
template <class T> T srd(uint64_t p) { T v{}; sread(p, &v, sizeof v); return v; }

// strict: 1 only when the track presentation is known and the model's resource is there
int model_loaded(uint8_t* tp, int model) {
    if (!tp || model < 0) return -1;
    uint64_t mb = srd<uint64_t>(reinterpret_cast<uint64_t>(tp) + 0xF618), me = srd<uint64_t>(reinterpret_cast<uint64_t>(tp) + 0xF620);
    if (!mb || me <= mb || (uint64_t)model >= (me - mb) / 8) return -1;
    uint64_t md = srd<uint64_t>(mb + (uint64_t)model * 8);
    return md && srd<uint64_t>(md + 8) ? 1 : 0;
}
// a placed (track) instance's model, or -1
int placed_model(uint8_t* tp, int index) {
    uint64_t pb = srd<uint64_t>(reinterpret_cast<uint64_t>(tp) + 0xFB78), pe = srd<uint64_t>(reinterpret_cast<uint64_t>(tp) + 0xFB80);
    if (index < 0 || !pb || pe <= pb || (uint64_t)index >= (pe - pb) / 0x70) return -1;
    return srd<int32_t>(pb + (uint64_t)index * 0x70 + 0x10);
}
// the model a collidable's procedural draws it with, or -1; name: its batch's model name
int procedural_model(uint8_t* tp, uint8_t* mgr, uint8_t* obj, const char** name) {
    const uint64_t o = reinterpret_cast<uint64_t>(obj), m = reinterpret_cast<uint64_t>(mgr);
    const int proc = srd<int32_t>(o + 0xC0), child = srd<int32_t>(o + 0xC4);
    uint64_t data = 0;
    for (uint64_t c = srd<uint64_t>(m + 0x518), e = srd<uint64_t>(m + 0x520); c && c < e; c += 0x40)
        if (srd<int32_t>(c + 0x28) == proc) {
            uint64_t prov = srd<uint64_t>(c + 0x20);
            if (srd<uintptr_t>(prov) == g_base + RVA_MODELS_VTABLE) data = srd<uint64_t>(prov + 0xF8);
            break;
        }
    if (!data || child < 0) return -1;
    int first = 0, batch = -1;
    for (uint64_t g = srd<uint64_t>(data + D_GROUPS), ge = srd<uint64_t>(data + D_GROUPS + 8); g && g < ge; g += sizeof(Group)) {
        int cnt = srd<int32_t>(g + 0x0C);
        if (child < first + cnt) { batch = srd<int32_t>(g + 4); break; }
        first += cnt;
    }
    uint64_t mb = srd<uint64_t>(data + D_MODELS), me = srd<uint64_t>(data + D_MODELS + 8);
    if (batch < 0 || !mb || mb + (uint64_t)(batch + 1) * BATCH_SIZE > me) return -1;
    const uint64_t b = mb + (uint64_t)batch * BATCH_SIZE;
    if (name && readable(reinterpret_cast<void*>(b + B_NAME), 0x20)) *name = str_at(reinterpret_cast<uint8_t*>(b + B_NAME));
    uint64_t R = srd<uint64_t>(b + 0x50), E = R ? srd<uint64_t>(R) : 0, I = E ? srd<uint64_t>(E + 0x10) : 0;
    return I ? placed_model(tp, srd<int32_t>(I)) : -1;
}
void on_whole_model(FH2Context* c, void*) {
    if (!g_ownModels) return;
    uint8_t* obj = reinterpret_cast<uint8_t*>(c->r15);
    uint8_t* mgr = reinterpret_cast<uint8_t*>(c->r14);
    uint8_t* tp  = g_trackPres.load(std::memory_order_relaxed);
    if (!tp || !obj || !mgr) return;
    const int model = static_cast<int>(c->rax);
    const char* name = "?";
    const int own = procedural_model(tp, mgr, obj, &name);
    if (own < 0 || own == model || model_loaded(tp, own) != 1) return;
    c->rax = static_cast<uint32_t>(own);
    const int k = ++g_ownSwaps;
    if (k <= 10 || k % 200 == 0)
        dlogf("own model: %s (definition %u) drawn with its model %d instead of %d; %d so far", name,
              srd<uint32_t>(reinterpret_cast<uint64_t>(obj) + 0x124) & 0xFFFF, own, model, k);
}

uint64_t d_tpBind(uint8_t* tp, int slot, int model, const float* m, void* key) {
    uint64_t r = o_tpBind(tp, slot, model, m, key);
    __try {
        if (slot >= 0 && slot < 0x1000) {
            g_trackPres.store(tp);
            g_slotTp[slot].store(tp);
            auto cnt = [&](size_t off) {
                uint8_t* b = *reinterpret_cast<uint8_t**>(tp + off);
                uint8_t* e = *reinterpret_cast<uint8_t**>(tp + off + 8);
                return b ? (size_t)(e - b) / 0x70 : 0;
            };
            size_t idx   = cnt(0xFB78) + cnt(0xFBC0) + (size_t)slot;
            uint8_t* nodes = *reinterpret_cast<uint8_t**>(tp + 0xF630);
            g_slotRender[slot].inst.store(*reinterpret_cast<uint8_t**>(tp + 0x38 + slot * 8));
            g_slotRender[slot].node.store(nodes ? reinterpret_cast<uint8_t**>(nodes)[idx] : nullptr);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return r;
}

struct LiveLog {
    int slot; uint8_t* obj; uint32_t id; int frames;
    float length;  // model stretch along its length for this log
    float thick;   // and across it
};
LiveLog g_live[64];
int g_nLive = 0;
std::mutex g_liveLock;

void track_log(int slot, uint8_t* obj, uint32_t id, float length, float thick) {
    std::lock_guard<std::mutex> l(g_liveLock);
    if (g_nLive == 64) { std::memmove(g_live, g_live + 1, sizeof(LiveLog) * 63); --g_nLive; }
    g_live[g_nLive++] = LiveLog{slot, obj, id, 0, length, thick};
}

float len3(const float* v) { return std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]); }
bool near3(const float* a, const float* b, float tol) {
    return std::fabs(a[0] - b[0]) < tol && std::fabs(a[1] - b[1]) < tol && std::fabs(a[2] - b[2]) < tol;
}
float half_to_float(uint16_t h) {
    uint32_t s = (h >> 15) & 1, e = (h >> 10) & 31, m = h & 1023;
    float v = e == 0 ? std::ldexp((float)m, -24) : e == 31 ? INFINITY : std::ldexp((float)(m | 1024), (int)e - 25);
    return s ? -v : v;
}

void stretch_rows(float* r, int stride, float length = -1, float thick = -1) {
    if (length <= 0) length = g_logLength;
    if (thick <= 0) thick = g_logThickness;
    for (int k = 0; k < 3; ++k) {
        float* row = r + k * stride;
        float want = k == g_logAxis ? length : thick;
        float l    = len3(row);
        if (l < 0.001f || std::fabs(l - want) < 0.01f) continue;
        float f = want / l;
        row[0] *= f; row[1] *= f; row[2] *= f;
    }
}


// ---- stretching via the render-node update ---------------------------------------
// Every writer of a render node's matrix (0x14139F860, 0x14139D720, 0x1413A4F80) then
// calls 0x1413A41E0(a, b, c, d, node, ...), which reads the new matrix (node+0xA0 = it
// changed) to update bounds and draw data. Stretching the node's rows on the way in makes
// every draw of it use the stretched matrix.
using NodeUpdateFn = uint64_t (*)(void*, void*, void*, uint64_t, uint8_t*, uint64_t, uint64_t, uint64_t);
NodeUpdateFn o_nodeUpdate = nullptr;
constexpr int NSTRETCH = 64;
std::atomic<uint8_t*> g_stretchNode[NSTRETCH];
std::atomic<float> g_stretchLen[NSTRETCH], g_stretchThick[NSTRETCH];
std::atomic<int> g_stretchCount{0};

uint64_t d_nodeUpdate(void* a, void* b, void* c, uint64_t d, uint8_t* node, uint64_t f, uint64_t g, uint64_t h) {
    if (g_stretchCount.load(std::memory_order_relaxed) && node) {
        for (int i = 0; i < NSTRETCH; ++i)
            if (g_stretchNode[i].load(std::memory_order_relaxed) == node) {
                stretch_rows(reinterpret_cast<float*>(node + 0x20), 4, g_stretchLen[i].load(std::memory_order_relaxed),
                             g_stretchThick[i].load(std::memory_order_relaxed));
                break;
            }
    }
    return o_nodeUpdate(a, b, c, d, node, f, g, h);
}
void stretch_node_add(uint8_t* node, float length, float thick) {
    if (!node || !o_nodeUpdate) return;
    for (int i = 0; i < NSTRETCH; ++i) if (g_stretchNode[i].load() == node) return;
    for (int i = 0; i < NSTRETCH; ++i) {
        uint8_t* expect = nullptr;
        if (g_stretchNode[i].load() != nullptr) continue;
        g_stretchLen[i].store(length);   // the slot is free: set its length before publishing the node
        g_stretchThick[i].store(thick);
        if (g_stretchNode[i].compare_exchange_strong(expect, node)) { g_stretchCount.fetch_add(1); return; }
    }
}
void stretch_node_remove(uint8_t* node) {
    if (!node) return;
    for (int i = 0; i < NSTRETCH; ++i) {
        uint8_t* expect = node;
        if (g_stretchNode[i].compare_exchange_strong(expect, nullptr)) g_stretchCount.fetch_sub(1);
    }
}

// A loose piece's render node is (tp+0xF630)[placed + streamed + slot], where placed and streamed
// are the sizes of the instance vectors at tp+0xFB78 and tp+0xFBC0 (as 0x14132B390 binds it).
// The streamed count changes as the track streams in and out, so the node a slot uses moves:
// it has to be looked up again every frame, not kept from the bind.
uint8_t* current_node(int slot) {
    uint8_t* tp = g_slotTp[slot].load(std::memory_order_relaxed);
    if (!tp || !readable(tp + 0xF630, 0x10) || !readable(tp + 0xFB78, 0x60)) return nullptr;
    auto cnt = [&](size_t off) {
        uint8_t* b = *reinterpret_cast<uint8_t**>(tp + off);
        uint8_t* e = *reinterpret_cast<uint8_t**>(tp + off + 8);
        return b && e > b ? (size_t)(e - b) / 0x70 : 0;
    };
    size_t idx = cnt(0xFB78) + cnt(0xFBC0) + (size_t)slot;
    uint8_t** nb = *reinterpret_cast<uint8_t***>(tp + 0xF630);
    uint8_t** ne = *reinterpret_cast<uint8_t***>(tp + 0xF638);
    if (!nb || ne <= nb || idx >= (size_t)(ne - nb)) return nullptr;
    return nb[idx];
}
void update_live_logs(uint8_t* mgr) {
    std::lock_guard<std::mutex> l(g_liveLock);
    MsvcVector* slots = vec_at(mgr + 0x290);
    int w = 0;
    for (int i = 0; i < g_nLive; ++i) {
        LiveLog& L = g_live[i];
        uint8_t* cur = (size_t)L.slot < slots->count(8) ? reinterpret_cast<uint8_t**>(slots->first)[L.slot] : nullptr;
        if (cur != L.obj || *reinterpret_cast<uint32_t*>(cur + 0x124) != L.id) {
            stretch_node_remove(g_slotRender[L.slot].node.load());
            continue;
        }
        ++L.frames;
        const float* pos = reinterpret_cast<const float*>(L.obj + 0xA0);
        // Debug: where a new log or pole is over its first second, against the car, to see
        // whether it's pushed out of something the moment it appears.
        if (g_debug && (L.frames <= 4 || L.frames == 8 || L.frames == 15 || L.frames == 30 || L.frames == 60)) {
            static int traced = 0;
            if (L.frames == 1) ++traced;
            if (traced <= 12) {
                const float* car = reinterpret_cast<const float*>(mgr + 0x4F0);
                dlogf("trace slot %d frame %d: body (%.2f, %.2f, %.2f); car (%.2f, %.2f, %.2f), %.2f m away level, %.2f m above it",
                      L.slot, L.frames, pos[0], pos[1], pos[2], car[0], car[1], car[2],
                      std::sqrt((pos[0] - car[0]) * (pos[0] - car[0]) + (pos[2] - car[2]) * (pos[2] - car[2])), pos[1] - car[1]);
            }
        }
        // follow the slot's render node as it moves, and only stretch it when it really is this log
        uint8_t* old = g_slotRender[L.slot].node.load();
        uint8_t* now = current_node(L.slot);
        if (now != old) {
            stretch_node_remove(old);
            g_slotRender[L.slot].node.store(now);
        }
        bool mine = now && readable(now + 0x20, 0x40) && near3(reinterpret_cast<const float*>(now + 0x50), pos, 6.0f);
        if (L.frames >= 2) {
            if (mine) stretch_node_add(now, L.length, L.thick);
            else stretch_node_remove(now);
        }
        g_live[w++] = L;
    }
    g_nLive = w;
}

uint8_t* track_presentation(uint8_t* mgr);
bool pole_model(int m);
// a placed model's scale: the mean length of the three rotation rows (nine halfs at +0x44)
float instance_scale(const uint8_t* inst) {
    const uint16_t* h = reinterpret_cast<const uint16_t*>(inst + 0x44);
    float sum = 0;
    for (int r = 0; r < 3; ++r) {
        float a = half_to_float(h[r * 3]), b = half_to_float(h[r * 3 + 1]), c = half_to_float(h[r * 3 + 2]);
        sum += std::sqrt(a * a + b * b + c * c);
    }
    return sum / 3.0f;
}
void mark_dump(uint8_t* mgr) {
    const float* p = reinterpret_cast<const float*>(mgr + 0x4F0);
    uint32_t f = g_frame.load();
    logf("==== MARK at (%.1f, %.1f, %.1f) ====", p[0], p[1], p[2]);
    // barrier and tree points within 8 m
    const TrunkNode* pts[256];
    int np = trunk_find(p[0] - 8, p[2] - 8, p[0] + 8, p[2] + 8, pts, 256);
    int shown = 0;
    for (int i = 0; i < np && shown < 40; ++i, ++shown) {
        bool pole = pts[i]->size <= -100.0f, bar = pts[i]->size < 0 && !pole;
        logf("  %s point (%.1f, %.1f, %.1f) radius %.2f%s", pole ? "pole" : bar ? "barrier" : "tree", pts[i]->x - p[0], pts[i]->y - p[1],
             pts[i]->z - p[2], pole ? -pts[i]->size - 100.0f : bar ? -pts[i]->size : pts[i]->size,
             bar ? (bar_ok(pts[i]) ? " (ground both sides)" : " (SOLID: no ground seen on one side)") : "");
    }
    logf("  %d points within 8 m (positions are relative to the car)", np);
    // collidable objects within 10 m
    MsvcVector* slots = vec_at(mgr + 0x290);
    int nobj = 0;
    for (size_t i = 0; i < slots->count(8) && nobj < 40; ++i) {
        uint8_t* o = reinterpret_cast<uint8_t**>(slots->first)[i];
        if (!o || !readable(o, 0x150)) continue;
        const float* t = reinterpret_cast<const float*>(o + 0xA0);
        float dx = t[0] - p[0], dy = t[1] - p[1], dz = t[2] - p[2];
        if (dx * dx + dz * dz > 100) continue;
        int proc = *reinterpret_cast<int32_t*>(o + 0xC0);
        logf("  object slot %zu def %u state %d at (%.1f, %.1f, %.1f)%s%s", i, *reinterpret_cast<uint32_t*>(o + 0x124) & 0xFFFF,
             *reinterpret_cast<int32_t*>(o + 0x148), dx, dy, dz, is_barrier_proc(proc) ? " barrier" : "", is_tree_proc(proc) ? " tree" : "");
        ++nobj;
    }
    // collision triangles near the car in the last ~20 frames
    int logged = 0;
    for (int i = 0; i < NRECENT && logged < 80; ++i) {
        const RecentTri& r = g_recent[i];
        if (!r.T || f - r.frame > 20) continue;
        float cx = (r.v[0] + r.v[3] + r.v[6]) / 3, cz = (r.v[2] + r.v[5] + r.v[8]) / 3;
        if ((cx - p[0]) * (cx - p[0]) + (cz - p[2]) * (cz - p[2]) > 49) continue;
        logf("  kept triangle T=%llX (%.1f,%.1f,%.1f) (%.1f,%.1f,%.1f) (%.1f,%.1f,%.1f)", (unsigned long long)r.T,
             r.v[0] - p[0], r.v[1] - p[1], r.v[2] - p[2], r.v[3] - p[0], r.v[4] - p[1], r.v[5] - p[2], r.v[6] - p[0], r.v[7] - p[1], r.v[8] - p[2]);
        ++logged;
    }
    // every collision query of the last few frames (local-space ones show up with small boxes)
    int ql = 0;
    for (int i = 0; i < NRQ && ql < 60; ++i) {
        const RecentQuery& q = g_rq[i];
        if (!q.T || f - q.frame > 3) continue;
        logf("  query T=%llX box (%.1f,%.1f,%.1f)-(%.1f,%.1f,%.1f) returned %d removed %d", (unsigned long long)q.T, q.box[0], q.box[1],
             q.box[2], q.box[3], q.box[4], q.box[5], q.n, q.removed);
        ++ql;
    }
    // placed track models within 12 m (telephone poles and the like)
    if (uint8_t* tp = track_presentation(mgr)) {
        uint8_t* b = *reinterpret_cast<uint8_t**>(tp + 0xFB78);
        uint8_t* e = *reinterpret_cast<uint8_t**>(tp + 0xFB80);
        int shown = 0;
        for (uint8_t* it = b; it && it < e && shown < 30; it += 0x70) {
            const float* q = reinterpret_cast<const float*>(it + 0x38);
            float ex = q[0] - p[0], ez = q[2] - p[2];
            if (ex * ex + ez * ez > 144) continue;
            const uint16_t* lod = reinterpret_cast<const uint16_t*>(it + 0x30);
            logf("  track model %lld: model %d at (%.1f, %.1f, %.1f) scale %.2f, drawn to %.0f m%s", (long long)((it - b) / 0x70),
                 *reinterpret_cast<int32_t*>(it + 0x10), ex, q[1] - p[1], ez, instance_scale(it), half_to_float(lod[0]),
                 pole_model(*reinterpret_cast<int32_t*>(it + 0x10)) ? " POLE" : "");
            ++shown;
        }
    }
    logf("==== end of MARK ====");
}

// ---- telephone poles: placed track models, not procedural ones ----------------------
// The track's own model instances live in CTrackPresentation+0xFB78 (0x70 bytes each:
// +0x10 model = anthemout.NNNNN, +0x38 position). The real presentation is
// *(*(mgr+0x10)+0x20) (CPresentation, vtable 0x142EB7BA0), and its +0x38 is the
// CTrackPresentation (vtable 0x142EB8590). IPresentation::SetModelInstanceHidden is slot
// 0x178 (index*, hidden, include LODs); called on mgr+0x10 it goes through the deferred queue.
// Poles have no collidable object, so the mod watches the car: when it reaches one, the pole
// is hidden, its collision (removed up front, like tree trunks) stays gone, and a stretched
// wooden pole piece falls in its place.
int g_poles = 1;
// Model numbers are each map's own (its bin.zip AssetManifest's NNNNN.modelbin). The mainland
// (anthemout): GEN_OBJ_Pylon_Generic_Wooden 8953/8954, GEN_OBJ_Pylon_Nshaped_Wooden 8619-8621.
// Storm Island (test03out): Generic_Wooden 1748/1749, Nshaped_Wooden 1501-1503.
char g_poleModels[256] = "8953,8954,8619,8620,8621";
char g_islandPoleModels[256] = "1748,1749,1501,1502,1503";
bool g_islandMap = false;          // the map being scanned is Storm Island (see scan_poles)
uint8_t* g_poleTp = nullptr;       // the track presentation the poles were found in
float g_poleSpeedLoss = 0.15f, g_poleLength = 8.5f, g_poleThick = 3.5f, g_poleHeight = 4.3f, g_poleRadius = 0.9f, g_poleMaxScale = 1.3f;
struct Pole { int index; float x, y, z; bool broken; int model; };   // model: its lowest-numbered (LOD0) one
std::vector<Pole> g_poleList;
std::atomic<int> g_polesBroken{0};
uint8_t* g_poleScanBase = nullptr;
size_t g_poleScanCount = 0;
bool pole_model(int m) {
    const char* r = g_islandMap ? g_islandPoleModels : g_poleModels;
    while (*r) {
        while (*r == ' ' || *r == ',') ++r;
        if (!*r) break;
        if (std::atoi(r) == m) return true;
        while (*r && *r != ',') ++r;
    }
    return false;
}
uint8_t* real_presentation(uint8_t* mgr) {
    uint8_t* pres = *reinterpret_cast<uint8_t**>(mgr + 0x10);
    if (!readable(pres, 0x28)) return nullptr;
    // CPresentationDeferred (ctor 0x14131E9B0) keeps the real presentation at +0x38, i.e. +0x28 from
    // the sub-object mgr+0x10 points at; the deferred calls pass +0x20 to the queue. Try both.
    for (int off : {0x20, 0x28}) {
        uint8_t* real = *reinterpret_cast<uint8_t**>(pres + off);
        // CPresentation, or the TRefCountedObjectThreadSafe<CPresentation> the game actually makes
        if (readable(real, 0x40) && (*reinterpret_cast<uintptr_t*>(real) == g_base + 0x2EB7BA0 ||
                                     *reinterpret_cast<uintptr_t*>(real) == g_base + 0x2EBC148))
            return real;
    }
    return nullptr;
}
uint8_t* track_presentation(uint8_t* mgr) {
    uint8_t* real = real_presentation(mgr);
    uint8_t* tp = real ? *reinterpret_cast<uint8_t**>(real + 0x38) : nullptr;
    if (tp && readable(tp, 8) && readable(tp + 0xFB78, 0x10) && *reinterpret_cast<uintptr_t*>(tp) == g_base + 0x2EB8590) return tp;
    // fall back on the one seen binding a loose piece
    tp = g_trackPres.load();
    if (tp && readable(tp, 8) && readable(tp + 0xFB78, 0x10)) return tp;
    static int diag = 0;
    if (diag++ % 600 == 0 && diag < 3000) {
        uint8_t* pres = *reinterpret_cast<uint8_t**>(mgr + 0x10);
        auto vt = [](uint8_t* o) -> unsigned long long {
            return o && readable(o, 8) ? (unsigned long long)(*reinterpret_cast<uintptr_t*>(o) - g_base) : 0ull;
        };
        uint8_t* r2 = pres && readable(pres, 0x30) ? *reinterpret_cast<uint8_t**>(pres + 0x28) : nullptr;
        uint8_t* t2 = r2 && readable(r2, 0x40) ? *reinterpret_cast<uint8_t**>(r2 + 0x38) : nullptr;
        dlogf("poles: can't find the track presentation: mgr+0x10=%p (vt +%llX), +0x28=%p (vt +%llX), +0x38=%p (vt +%llX)", pres, vt(pres), r2,
             vt(r2), t2, vt(t2));
        if (pres && readable(pres, 0x60))
            for (int o = 0; o < 0x60; o += 8) {
                uint8_t* q = *reinterpret_cast<uint8_t**>(pres + o);
                dlogf("    mgr+0x10 +0x%02X = %p (vt +%llX)", o, q, vt(q));
            }
    }
    return nullptr;
}
// ---- solid pylons: placed models the game gives no collision ------------------------
// FRA_OBJ_Pylon_ArrowShaped (8616-8618, the big square concrete electricity pylons) has no
// collision at all in the game. CollisionWorld::GetPolys (0x1412CC730(world, box, max, tris,
// verts, flags)) returns pointers to 32-byte triangles (+0 u8 flags, bit 0 = already taken;
// +1 surface; +2 u16; +4/+8/+0xC vertex indexes into a vec4 array; +0x10 a vec4 normal) and the
// vertex array each one indexes. The mod appends a box of triangles for each such pylon the
// query box touches. The normal's convention is learned from the game's own triangles first.
char g_solidModels[128] = "8616";
int g_solidOn = 1;   // SolidPylons: the switch on the Mods page
float g_solidHalf = 0.32f, g_solidHeight = 14.0f;
struct SolidPole {
    float minx, miny, minz, maxx, maxy, maxz;
    alignas(16) float v[8][4];
    alignas(16) uint8_t tri[10][32];
};
std::atomic<SolidPole*> g_solids{nullptr};
std::atomic<int> g_nSolids{0};
std::atomic<int> g_triConv{0};        // 0 = still learning, 1 = known, -1 = gave up
float g_triSign = 1.0f;               // stored normal = sign * cross(p1 - p0, p2 - p0) normalised
int g_triW = 0;                       // w of the stored normal: 0 = 0, 1 = dot(n, p0), 2 = -dot(n, p0)
std::atomic<uint32_t> g_wallTriHead{0};   // first 4 bytes of a game wall triangle (flags, surface, +2)
std::atomic<long long> g_solidAdded{0};
bool solid_model(int m) {
    for (const char* r = g_solidModels; *r;) {
        while (*r == ' ' || *r == ',') ++r;
        if (!*r) break;
        if (std::atoi(r) == m) return true;
        while (*r && *r != ',') ++r;
    }
    return false;
}
// fill in the triangles of one pylon box (once the normal convention is known)
void solid_fill_tris(SolidPole& sp) {
    // corners: 0-3 bottom, 4-7 top, counter-clockwise seen from above
    static const int faces[5][4] = {{0, 1, 5, 4}, {1, 2, 6, 5}, {2, 3, 7, 6}, {3, 0, 4, 7}, {4, 5, 6, 7}};
    uint32_t head = g_wallTriHead.load();
    if (!head) head = 0x0010000Cu;    // flags 0x0C, surface 0, +2 = 0x10 (as on the game's tree trunks)
    int t = 0;
    float cx = (sp.minx + sp.maxx) * 0.5f, cz = (sp.minz + sp.maxz) * 0.5f, cy = (sp.miny + sp.maxy) * 0.5f;
    for (int f = 0; f < 5; ++f)
        for (int h = 0; h < 2; ++h) {
            int i0 = faces[f][0], i1 = faces[f][h ? 2 : 1], i2 = faces[f][h ? 3 : 2];
            const float *a = sp.v[i0], *b = sp.v[i1], *c = sp.v[i2];
            float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
            float nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
            float l = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (l < 1e-6f) l = 1;
            nx /= l; ny /= l; nz /= l;
            // outward: away from the box centre
            float mx = (a[0] + b[0] + c[0]) / 3 - cx, my = (a[1] + b[1] + c[1]) / 3 - cy, mz = (a[2] + b[2] + c[2]) / 3 - cz;
            bool out = nx * mx + ny * my + nz * mz > 0;
            // the game stores normal = sign * cross: order the corners so that comes out outward
            if (out != (g_triSign > 0)) { std::swap(i1, i2); nx = -nx; ny = -ny; nz = -nz; }
            float on[3] = {out ? nx : -nx, out ? ny : -ny, out ? nz : -nz};   // the outward normal
            if (!out) { on[0] = -on[0]; on[1] = -on[1]; on[2] = -on[2]; }
            // recompute from the final order to be sure
            const float *A = sp.v[i0], *B = sp.v[i1], *C = sp.v[i2];
            ux = B[0] - A[0]; uy = B[1] - A[1]; uz = B[2] - A[2]; wx = C[0] - A[0]; wy = C[1] - A[1]; wz = C[2] - A[2];
            nx = uy * wz - uz * wy; ny = uz * wx - ux * wz; nz = ux * wy - uy * wx;
            l = std::sqrt(nx * nx + ny * ny + nz * nz);
            if (l < 1e-6f) l = 1;
            float sn[3] = {g_triSign * nx / l, g_triSign * ny / l, g_triSign * nz / l};   // what the game would store
            (void)on;
            uint8_t* T = sp.tri[t++];
            std::memset(T, 0, 32);
            std::memcpy(T, &head, 4);
            T[0] &= 0xFE;
            uint32_t idx[3] = {(uint32_t)i0, (uint32_t)i1, (uint32_t)i2};
            std::memcpy(T + 4, idx, 12);
            float d = sn[0] * A[0] + sn[1] * A[1] + sn[2] * A[2];
            float nv[4] = {sn[0], sn[1], sn[2], g_triW == 1 ? d : g_triW == 2 ? -d : 0.0f};
            std::memcpy(T + 16, nv, 16);
        }
}
void build_solids(uint8_t* b, size_t n) {
    std::vector<SolidPole> out;
    for (size_t i = 0; i < n; ++i) {
        uint8_t* inst = b + i * 0x70;
        if (!solid_model(*reinterpret_cast<int32_t*>(inst + 0x10))) continue;
        const float* p = reinterpret_cast<const float*>(inst + 0x38);
        bool dup = false;
        for (const SolidPole& q : out)
            if (std::fabs((q.minx + q.maxx) * 0.5f - p[0]) < 0.5f && std::fabs((q.minz + q.maxz) * 0.5f - p[2]) < 0.5f) { dup = true; break; }
        if (dup) continue;
        SolidPole sp{};
        // the model's own x and z axes (rotation rows, nine halfs at +0x44), flattened and normalised
        const uint16_t* h = reinterpret_cast<const uint16_t*>(inst + 0x44);
        float ax[3] = {half_to_float(h[0]), 0, half_to_float(h[2])}, az[3] = {half_to_float(h[6]), 0, half_to_float(h[8])};
        float la = std::sqrt(ax[0] * ax[0] + ax[2] * ax[2]), lz = std::sqrt(az[0] * az[0] + az[2] * az[2]);
        if (la < 0.1f || lz < 0.1f) { ax[0] = 1; ax[2] = 0; az[0] = 0; az[2] = 1; la = lz = 1; }
        ax[0] /= la; ax[2] /= la; az[0] /= lz; az[2] /= lz;
        float H = g_solidHalf;
        const float sx[4] = {-1, 1, 1, -1}, sz[4] = {-1, -1, 1, 1};
        for (int c = 0; c < 8; ++c) {
            int k = c & 3;
            sp.v[c][0] = p[0] + (ax[0] * sx[k] + az[0] * sz[k]) * H;
            sp.v[c][2] = p[2] + (ax[2] * sx[k] + az[2] * sz[k]) * H;
            sp.v[c][1] = c < 4 ? p[1] - 1.0f : p[1] + g_solidHeight;
            sp.v[c][3] = 1.0f;
        }
        sp.minx = sp.maxx = sp.v[0][0]; sp.minz = sp.maxz = sp.v[0][2];
        for (int c = 1; c < 8; ++c) {
            sp.minx = std::min(sp.minx, sp.v[c][0]); sp.maxx = std::max(sp.maxx, sp.v[c][0]);
            sp.minz = std::min(sp.minz, sp.v[c][2]); sp.maxz = std::max(sp.maxz, sp.v[c][2]);
        }
        sp.miny = p[1] - 1.0f; sp.maxy = p[1] + g_solidHeight;
        out.push_back(sp);
    }
    if (out.empty()) return;
    SolidPole* arr = static_cast<SolidPole*>(VirtualAlloc(nullptr, out.size() * sizeof(SolidPole), MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    if (!arr) return;
    std::memcpy(arr, out.data(), out.size() * sizeof(SolidPole));
    if (g_triConv.load() == 1) for (size_t k = 0; k < out.size(); ++k) solid_fill_tris(arr[k]);
    g_nSolids.store(0);
    g_solids.store(arr);   // the old array (if any) is left alone: a query may still be reading it
    g_nSolids.store((int)out.size());
    logf("solid pylons: %zu found (models %s), %.2f m wide, %.1f m tall", out.size(), g_solidModels, g_solidHalf * 2, g_solidHeight);
}

// learn how the game stores a triangle's normal, from its own triangles
void learn_tri_convention(uint8_t** tris, uint8_t** verts, int n) {
    static int seen = 0, plusW0 = 0, minusW0 = 0, wD = 0, wMD = 0, w0 = 0, odd = 0;
    for (int k = 0; k < n && g_triConv.load() == 0; ++k) {
        const uint8_t* T = tris[k];
        const float* v = reinterpret_cast<const float*>(verts[k]);
        const float* a = v + *reinterpret_cast<const uint32_t*>(T + 4) * 4;
        const float* b = v + *reinterpret_cast<const uint32_t*>(T + 8) * 4;
        const float* c = v + *reinterpret_cast<const uint32_t*>(T + 12) * 4;
        float ux = b[0] - a[0], uy = b[1] - a[1], uz = b[2] - a[2], wx = c[0] - a[0], wy = c[1] - a[1], wz = c[2] - a[2];
        float nx = uy * wz - uz * wy, ny = uz * wx - ux * wz, nz = ux * wy - uy * wx;
        float l = std::sqrt(nx * nx + ny * ny + nz * nz);
        if (l < 1e-4f) continue;
        nx /= l; ny /= l; nz /= l;
        const float* f = reinterpret_cast<const float*>(T + 16);
        float dot = f[0] * nx + f[1] * ny + f[2] * nz;
        float d = f[0] * a[0] + f[1] * a[1] + f[2] * a[2];
        ++seen;
        if (std::fabs(dot - 1) < 0.02f) ++plusW0; else if (std::fabs(dot + 1) < 0.02f) ++minusW0; else ++odd;
        if (std::fabs(f[3]) < 0.01f) ++w0; else if (std::fabs(f[3] - d) < 0.05f) ++wD; else if (std::fabs(f[3] + d) < 0.05f) ++wMD;
        // remember the header of a steep (wall) triangle for our own
        if (std::fabs(ny) < 0.3f && !g_wallTriHead.load()) {
            uint32_t hd;
            std::memcpy(&hd, T, 4);
            g_wallTriHead.store(hd & ~1u);
        }
        if (seen == 1 || seen == 2)
            dlogf("triangle sample: head %02X %02X %04X, stored (%.3f %.3f %.3f %.3f), from corners (%.3f %.3f %.3f), d %.3f", T[0], T[1],
                 *reinterpret_cast<const uint16_t*>(T + 2), f[0], f[1], f[2], f[3], nx, ny, nz, d);
        if (seen >= 40) {
            int signVotes = std::max(plusW0, minusW0);
            int wVotes = std::max(w0, std::max(wD, wMD));
            dlogf("triangle normals: %d samples, stored = +cross %d, -cross %d, other %d; w: 0 %d, d %d, -d %d", seen, plusW0, minusW0, odd, w0, wD, wMD);
            if (signVotes >= seen * 8 / 10 && wVotes >= seen * 8 / 10) {
                g_triSign = plusW0 >= minusW0 ? 1.0f : -1.0f;
                g_triW = w0 >= wD && w0 >= wMD ? 0 : wD >= wMD ? 1 : 2;
                SolidPole* arr = g_solids.load();
                int ns = g_nSolids.load();
                for (int i = 0; arr && i < ns; ++i) solid_fill_tris(arr[i]);
                g_triConv.store(1);
                logf("triangle normals learned: sign %+.0f, w %s; solid pylons are on", g_triSign, g_triW == 0 ? "0" : g_triW == 1 ? "d" : "-d");
            } else {
                    g_triConv.store(-1);
                    logf("triangle normals: no clear convention; solid pylons stay off");
            }
        }
    }
}

void learn_locked(uint8_t** tris, uint8_t** verts, int n) {
    static std::mutex lm;
    std::lock_guard<std::mutex> l(lm);
    learn_tri_convention(tris, verts, n);
}
using GetPolysFn = int (*)(void* world, const float* box, int max, uint8_t** tris, uint8_t** verts, uint8_t flags);
GetPolysFn o_getPolys = nullptr;
int d_getPolys(void* world, const float* box, int max, uint8_t** tris, uint8_t** verts, uint8_t flags) {
    int n = o_getPolys(world, box, max, tris, verts, flags);
    if (!g_solidOn) return n;
    __try {
        int conv = g_triConv.load(std::memory_order_relaxed);
        if (conv == 0 && n > 0) learn_locked(tris, verts, n);
        if (conv != 1) return n;
        SolidPole* arr = g_solids.load(std::memory_order_acquire);
        int ns = g_nSolids.load(std::memory_order_acquire);
        for (int i = 0; arr && i < ns; ++i) {
            const SolidPole& sp = arr[i];
            if (sp.maxx < box[0] || sp.minx > box[4] || sp.maxz < box[2] || sp.minz > box[6] || sp.maxy < box[1] || sp.miny > box[5]) continue;
            if (flags && !(flags & sp.tri[0][0])) continue;
            for (int t = 0; t < 10 && n < max; ++t) {
                tris[n]  = const_cast<uint8_t*>(sp.tri[t]);
                verts[n] = reinterpret_cast<uint8_t*>(const_cast<float*>(&sp.v[0][0]));
                ++n;
            }
            g_solidAdded.fetch_add(1, std::memory_order_relaxed);
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    return n;
}

// A different map (the mainland and Storm Island): what was found on the last one goes, its
// indexes mean nothing here.
void forget_poles() {
    g_poleList.clear();
    g_poleScanBase  = nullptr;
    g_poleScanCount = 0;
}
void scan_poles(uint8_t* mgr) {
    uint8_t* tp = track_presentation(mgr);
    if (!tp) return;
    if (tp != g_poleTp) {
        if (g_poleTp) logf("poles: the map changed; looking for them again");
        forget_poles();
        g_poleTp = tp;
    }
    uint8_t* b = *reinterpret_cast<uint8_t**>(tp + 0xFB78);
    uint8_t* e = *reinterpret_cast<uint8_t**>(tp + 0xFB80);
    if (!b || e <= b) return;
    size_t n = (size_t)(e - b) / 0x70;
    if (b == g_poleScanBase && n == g_poleScanCount) return;
    if (!readable(b, 0x70) || !readable(e - 0x70, 0x70)) return;   // still being built
    g_poleScanBase = b; g_poleScanCount = n;
    // Which map: the breakable definitions say (Storm Island numbers them differently), or,
    // before any has been seen, its size (the mainland has about 88,000 placed models, the
    // island about 14,000).
    const DefMap* dm = g_lastDefMap.load(std::memory_order_acquire);
    const bool island = dm ? dm->moved : n < 40000;
    if (island != g_islandMap) logf("poles: on %s", island ? "Storm Island" : "the mainland");
    g_islandMap = island;
    if (g_solidModels[0] && !island) build_solids(b, n);   // the concrete pylons are only on the mainland
    std::vector<Pole> found;
    int perModel = 0, tooBig = 0;
    int scaleHist[8] = {};   // 0.25-wide buckets from 0.5
    for (size_t i = 0; i < n; ++i) {
        uint8_t* inst = b + i * 0x70;
        int m = *reinterpret_cast<int32_t*>(inst + 0x10);
        if (!pole_model(m)) continue;
        ++perModel;
        const float* p = reinterpret_cast<const float*>(inst + 0x38);
        float sc = instance_scale(inst);
        int hb = (int)((sc - 0.5f) / 0.25f);
        ++scaleHist[hb < 0 ? 0 : hb > 7 ? 7 : hb];
        if (sc > g_poleMaxScale) { ++tooBig; continue; }   // the big ones stay as they are: solid
        bool dup = false;
        for (Pole& q : found)
            if (std::fabs(q.x - p[0]) < 0.5f && std::fabs(q.z - p[2]) < 0.5f) {
                dup = true;
                if (m < q.model) q.model = m;   // the LODs are the same instance; LOD0 has the lowest number
                break;
            }
        if (dup) continue;
        Pole np{(int)i, p[0], p[1], p[2], false, m};
        for (const Pole& old : g_poleList)   // keep what was already broken
            if (old.broken && std::fabs(old.x - p[0]) < 0.5f && std::fabs(old.z - p[2]) < 0.5f) np.broken = true;
        found.push_back(np);
        if (g_filterCollision >= 2) trunk_add(p[0], p[1], p[2], -100.0f - g_poleRadius);   // pole point: its collision goes
    }
    g_poleList.swap(found);
    if (g_poleList.empty()) {
        // none recognised: say what models stand near the car, to check the numbering
        static int diag = 0;
        if (diag++ < 3) {
            const float* P = reinterpret_cast<const float*>(mgr + 0x4F0);
            int shown = 0;
            for (size_t i = 0; i < n && shown < 25; ++i) {
                const float* q = reinterpret_cast<const float*>(b + i * 0x70 + 0x38);
                float ex = q[0] - P[0], ez = q[2] - P[2];
                if (ex * ex + ez * ez > 40 * 40) continue;
                dlogf("  track model %zu: model %d at (%.1f, %.1f, %.1f) from the car", i, *reinterpret_cast<int32_t*>(b + i * 0x70 + 0x10), ex,
                     q[1] - P[1], ez);
                ++shown;
            }
        }
    }
    static int logged = 0;
    if (logged++ < 20) {
        logf("poles: %zu found among %zu track models (%d instances incl. LODs; %d left solid for being scaled over %.2f)", g_poleList.size(), n,
             perModel, tooBig, g_poleMaxScale);
        dlogf("poles: scales <0.75:%d 0.75:%d 1.0:%d 1.25:%d 1.5:%d 1.75:%d 2.0:%d >=2.25:%d", scaleHist[0], scaleHist[1], scaleHist[2],
             scaleHist[3], scaleHist[4], scaleHist[5], scaleHist[6], scaleHist[7]);
    }
}
void break_pole(uint8_t* mgr, Pole& pl, const float* vel) {
    pl.broken = true;
    uint8_t* pres = *reinterpret_cast<uint8_t**>(mgr + 0x10);
    using HideFn = void (*)(uint8_t* self, const int* index, bool hidden, bool lods);
    int idx = pl.index;
    reinterpret_cast<HideFn>((*reinterpret_cast<void***>(pres))[0x178 / 8])(pres, &idx, true, true);
    // the falling pole
    PendingLog p{};
    p.mgr = mgr;
    p.x = pl.x; p.y = pl.y; p.z = pl.z;
    p.vx = vel[0]; p.vy = 0; p.vz = vel[2];
    int32_t key[4] = {idx, 0x504F4C45, -2, 0};   // any key but the default one, so the game can recycle it
    std::memcpy(p.key, key, 16);
    // +0xB0/+0xB4 are the x and z the piece broke from: the game fades out a piece that is far from
    // them (0x140384A10), so they must be the pole's own position or it vanishes at once
    std::memcpy(&p.b0, &pl.x, 4);
    std::memcpy(&p.b4, &pl.z, 4);
    p.c = LogCandidate{28, 3};
    p.n = 10000 + g_polesBroken.load();
    p.pole = true; p.length = g_poleLength; p.thick = g_poleThick; p.tilt = 6.0f; p.spin = 1.4f; p.speedK = 0.25f; p.up = 0.3f;
    p.height = g_poleHeight;
    // the pole's own model and how it stands: its placed instance's rotation (nine halfs at +0x44,
    // scale included) and its LOD0 model, drawn instead of a stretched trunk when it's loaded
    p.ownModel = -1;
    if (g_poleOwnModel && g_poleTp && pl.model > 0 && model_loaded(g_poleTp, pl.model) == 1) {
        uint64_t pb = srd<uint64_t>(reinterpret_cast<uint64_t>(g_poleTp) + 0xFB78);
        uint16_t h[9];
        if (pb && sread(pb + (uint64_t)pl.index * 0x70 + 0x44, h, sizeof h)) {
            float sc = 0;
            for (int r = 0; r < 3; ++r) {
                float v[3] = {half_to_float(h[r * 3]), half_to_float(h[r * 3 + 1]), half_to_float(h[r * 3 + 2])};
                float l = std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
                if (l < 1e-3f) { sc = 0; break; }
                for (int k = 0; k < 3; ++k) p.r0[r * 3 + k] = v[k] / l;
                sc += l / 3.0f;
            }
            if (sc > 0.3f && p.r0[4] > 0.8f) {   // standing up (its up axis near vertical)
                p.ownModel = pl.model;
                p.scale    = sc;
            }
        }
    }
    push_pending(p);
    // speed loss on the player's car (known from its last smash), and some wood flying
    if (uint8_t* dyn = g_lastCarDyn.load()) {
        __try {
            void** dvt = *reinterpret_cast<void***>(dyn);
            if (reinterpret_cast<uintptr_t>(dvt[0x290 / 8]) == g_base + 0x12E3CF0) {
                alignas(16) float v[4];
                reinterpret_cast<float* (*)(void*, float*)>(dvt[0x288 / 8])(dyn, v);
                alignas(16) float nv[4] = {v[0] * (1 - g_poleSpeedLoss), v[1], v[2] * (1 - g_poleSpeedLoss), v[3]};
                reinterpret_cast<void (*)(void*, const float*)>(dvt[0x290 / 8])(dyn, nv);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
    }
    float cv[4] = {vel[0], vel[1], vel[2], 0};
    spawn_effects(mgr, "Break_Wood,Break_WoodDust", pl.x, pl.y + 0.5f, pl.z, cv);
    int k = ++g_polesBroken;
    dlogf("pole broke: track model %d at (%.1f, %.1f, %.1f), car at %.0f mph; %d poles so far", pl.index, pl.x, pl.y, pl.z,
         std::sqrt(vel[0] * vel[0] + vel[2] * vel[2]) * 2.23694f, k);
}
void update_poles(uint8_t* mgr) {
    static int frame = 0;
    if ((frame++ % 30) == 0) scan_poles(mgr);
    if (g_poleList.empty()) return;
    static float px = 0, py = 0, pz = 0;
    static LARGE_INTEGER last{};
    LARGE_INTEGER now, f;
    QueryPerformanceCounter(&now);
    QueryPerformanceFrequency(&f);
    const float* P = reinterpret_cast<const float*>(mgr + 0x4F0);
    float dt = last.QuadPart ? (float)(now.QuadPart - last.QuadPart) / (float)f.QuadPart : 0.0f;
    last = now;
    float dx = P[0] - px, dy = P[1] - py, dz = P[2] - pz;
    px = P[0]; py = P[1]; pz = P[2];
    float step = std::sqrt(dx * dx + dz * dz);
    if (dt <= 0 || dt > 0.25f || step < 0.02f || step > 20.0f) return;
    float vel[3] = {dx / dt, dy / dt, dz / dt};
    float hx = dx / step, hz = dz / step;
    // the car as a capsule along its direction of travel, reaching one more frame ahead
    float ax = P[0] - hx * 2.3f, az = P[2] - hz * 2.3f;
    float bx = P[0] + hx * (2.3f + step), bz = P[2] + hz * (2.3f + step);
    float sx = bx - ax, sz = bz - az, sl2 = sx * sx + sz * sz;
    for (Pole& pl : g_poleList) {
        if (pl.broken) continue;
        float ex = pl.x - P[0], ez = pl.z - P[2];
        if (ex * ex + ez * ez > 100 || std::fabs(pl.y - P[1]) > 4) continue;
        float t = ((pl.x - ax) * sx + (pl.z - az) * sz) / sl2;
        t = t < 0 ? 0 : t > 1 ? 1 : t;
        float cx = ax + sx * t - pl.x, cz = az + sz * t - pl.z;
        if (cx * cx + cz * cz < 1.15f * 1.15f) break_pole(mgr, pl, vel);
    }
}

// ---- debris clean-up -------------------------------------------------------------
// A loose piece (state 5) normally stays until the player is ~150 m away from where it broke
// (0x140384A10). This fades pieces out sooner: after DebrisLifetime seconds, or once the player
// is DebrisDistance metres from the piece. Writing a fade time (half float) to +0x120 makes the
// game's own update fade the piece over that many seconds and then free its slot.
float g_debrisLifetime = 45.0f, g_debrisDistance = 120.0f;
int g_debrisCleanup = 1;   // DebrisCleanup: the switch on the Mods page
uint8_t* g_debrisSeen[0x1000];
double g_debrisBorn[0x1000];
std::atomic<int> g_debrisCleared{0};
double now_seconds() {
    static LARGE_INTEGER f{};
    if (!f.QuadPart) QueryPerformanceFrequency(&f);
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)c.QuadPart / (double)f.QuadPart;
}
void update_debris(uint8_t* mgr) {
    if (!g_debrisCleanup || (g_debrisLifetime <= 0 && g_debrisDistance <= 0)) return;
    static int frame = 0;
    if ((frame++ % 10) != 0) return;
    const int32_t defaultGlobal = *reinterpret_cast<const int32_t*>(g_base + 0x2C6881C);
    MsvcVector* slots = vec_at(mgr + 0x290);
    size_t n = slots->count(8);
    if (n > 0x1000) n = 0x1000;
    const float* P = reinterpret_cast<const float*>(mgr + 0x4F0);
    double t = now_seconds();
    float d2max = g_debrisDistance * g_debrisDistance;
    for (size_t i = 0; i < n; ++i) {
        uint8_t* o = reinterpret_cast<uint8_t**>(slots->first)[i];
        if (!o || *reinterpret_cast<int32_t*>(o + 0x148) != 5) { g_debrisSeen[i] = nullptr; continue; }
        // pieces with the default key are the game's own placed objects, not debris
        if (*reinterpret_cast<int32_t*>(o + 0xCC) == defaultGlobal && *reinterpret_cast<int32_t*>(o + 0xC8) == -1 &&
            *reinterpret_cast<int32_t*>(o + 0xD0) == -1) { g_debrisSeen[i] = nullptr; continue; }
        if (g_debrisSeen[i] != o) { g_debrisSeen[i] = o; g_debrisBorn[i] = t; continue; }
        if (*reinterpret_cast<uint16_t*>(o + 0x120) != 0) continue;   // already fading
        const float* q = reinterpret_cast<const float*>(o + 0xA0);
        float dx = q[0] - P[0], dz = q[2] - P[2];
        bool old = g_debrisLifetime > 0 && t - g_debrisBorn[i] > g_debrisLifetime;
        bool distant = g_debrisDistance > 0 && dx * dx + dz * dz > d2max;
        if (!old && !distant) continue;
        *reinterpret_cast<uint16_t*>(o + 0x120) = 0x4000;   // 2.0 s fade, as the game uses
        int k = ++g_debrisCleared;
        if (k <= 5 || k % 100 == 0) dlogf("debris: faded a piece (def %u) %s; %d so far", *reinterpret_cast<uint32_t*>(o + 0x124) & 0xFFFF,
                                         old ? "for its age" : "for distance", k);
    }
}

// ---- diagnostics: are loose pieces being drawn where they are? ---------------------
// Every 2 s: the streamed-instance count (tp+0xFBC0, which shifts every loose piece's render
// node index), and for the loose pieces within 40 m: how many have a render node at their body,
// and how many still remember where they broke from (+0xB0/+0xB4; the game fades a piece that
// is far from that).
void piece_report(uint8_t* mgr) {
    static double last = 0;
    double t = now_seconds();
    if (t - last < 2.0) return;
    last = t;
    uint8_t* tp = g_trackPres.load();
    if (!tp || !readable(tp + 0xFB78, 0x60)) return;
    auto cnt = [&](size_t off) {
        uint8_t* b = *reinterpret_cast<uint8_t**>(tp + off);
        uint8_t* e = *reinterpret_cast<uint8_t**>(tp + off + 8);
        return b && e > b ? (long long)((e - b) / 0x70) : 0ll;
    };
    static long long lastStreamed = -1;
    long long streamed = cnt(0xFBC0);
    static int changes = 0;
    if (streamed != lastStreamed) {
        ++changes;
        if (lastStreamed >= 0 && (changes <= 60 || changes % 50 == 0))
            dlogf("render list: streamed instances %lld -> %lld (placed %lld); every loose piece's render node index moves by %lld", lastStreamed,
                 streamed, cnt(0xFB78), streamed - lastStreamed);
        lastStreamed = streamed;
    }
    const int32_t defaultGlobal = *reinterpret_cast<const int32_t*>(g_base + 0x2C6881C);
    MsvcVector* slots = vec_at(mgr + 0x290);
    size_t n = slots->count(8);
    if (n > 0x1000) n = 0x1000;
    const float* P = reinterpret_cast<const float*>(mgr + 0x4F0);
    int nNear = 0, drawn = 0, noNode = 0, farOrigin = 0, fading = 0;
    for (size_t i = 0; i < n; ++i) {
        uint8_t* o = reinterpret_cast<uint8_t**>(slots->first)[i];
        if (!o || *reinterpret_cast<int32_t*>(o + 0x148) != 5) continue;
        if (*reinterpret_cast<int32_t*>(o + 0xCC) == defaultGlobal && *reinterpret_cast<int32_t*>(o + 0xC8) == -1 &&
            *reinterpret_cast<int32_t*>(o + 0xD0) == -1) continue;
        const float* q = reinterpret_cast<const float*>(o + 0xA0);
        float dx = q[0] - P[0], dz = q[2] - P[2];
        if (dx * dx + dz * dz > 40 * 40) continue;
        ++nNear;
        if (*reinterpret_cast<uint16_t*>(o + 0x120)) ++fading;
        float bx = *reinterpret_cast<float*>(o + 0xB0), bz = *reinterpret_cast<float*>(o + 0xB4);
        if ((bx - q[0]) * (bx - q[0]) + (bz - q[2]) * (bz - q[2]) > 150 * 150) ++farOrigin;
        uint8_t* node = current_node((int)i);
        if (!node || !readable(node + 0x20, 0x40)) { ++noNode; continue; }
        if (near3(reinterpret_cast<const float*>(node + 0x50), q, 6.0f)) ++drawn;
    }
    static int lastNear = -1, quiet = 0;
    if (nNear != lastNear || (nNear && ++quiet % 5 == 0)) {
        if (nNear || lastNear > 0)
            dlogf("loose pieces within 40 m: %d; render node at the piece: %d; no node: %d; fading: %d; broke-from point over 150 m away: %d (streamed %lld)",
                 nNear, drawn, noNode, fading, farOrigin, streamed);
        lastNear = nNear;
    }
}

// CWorldCollObjMan update (0x140384C00): pieces of broken objects are made here, on the
// game's own thread, so the queued logs are spawned here too.
using MgrUpdateFn = void (*)(uint8_t* mgr, float a, float b);
MgrUpdateFn o_mgrUpdate = nullptr;
void d_mgrUpdate(uint8_t* mgr, float a, float b) {
    o_mgrUpdate(mgr, a, b);
    g_worldMgr.store(mgr, std::memory_order_relaxed);
    g_frame.fetch_add(1, std::memory_order_relaxed);
    if (g_poles) {
        // a crash (a map being swapped out under it, say) starts the poles over, and after a
        // pause they're looked for again; only one that keeps crashing turns them off
        static int crashes = 0, pause = 0;
        if (pause > 0) {
            --pause;
        } else {
            __try {
                update_poles(mgr);
            } __except (EXCEPTION_EXECUTE_HANDLER) {
                forget_poles();
                g_poleTp = nullptr;
                pause    = 120;
                if (++crashes >= 20) { logf("poles crashed %d times; turned off", crashes); g_poles = 0; }
                else logf("poles crashed; trying again shortly");
            }
        }
    }
    if (g_logProbe) { __try { piece_report(mgr); } __except (EXCEPTION_EXECUTE_HANDLER) {} }
    __try { update_debris(mgr); } __except (EXCEPTION_EXECUTE_HANDLER) { logf("debris clean-up crashed; turned off"); g_debrisLifetime = g_debrisDistance = 0; }
    if (g_markEnabled) {
        static bool down = false;
        bool now = (GetAsyncKeyState(VK_F8) & 0x8000) != 0;
        if (now && !down) {
            __try { mark_dump(mgr); } __except (EXCEPTION_EXECUTE_HANDLER) { logf("mark failed"); }
        }
        down = now;
    }
    if (g_nLive) {
        __try {
            update_live_logs(mgr);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            logf("log stretching crashed; turned off");
            g_logStretch = 0; g_logProbe = 0; g_nLive = 0;
        }
    }
    if (!g_nPend) return;
    PendingLog local[16];
    int n = take_pending(local);
    for (int i = 0; i < n; ++i) {
        if (local[i].mgr != mgr) continue;
        __try {
            spawn_log(local[i]);
        } __except (EXCEPTION_EXECUTE_HANDLER) {
            logf("log %d: spawning %u:%u crashed; logs are off for this session", local[i].n, local[i].c.def, local[i].c.piece);
            g_logs = 0;
        }
    }
}

void add_broken(float x, float y, float z) {
    std::lock_guard<std::mutex> l(g_brokenLock);
    int i = g_nBroken.load();
    if (i < MAX_BROKEN) {
        g_broken[i] = Broken{x, y, z};
        trunk_add(x, y, z);
        g_nBroken.store(i + 1, std::memory_order_release);
    }
}
using OnCollisionFn = void (*)(uint8_t* obj, void* contact);
OnCollisionFn o_onCollision = nullptr;

void d_onCollision(uint8_t* obj, void* contact) {
    int before = 0, proc = -1;
    float x = 0, y = 0, z = 0;
    __try {
        before = *reinterpret_cast<int32_t*>(obj + 0x148);
        proc   = *reinterpret_cast<int32_t*>(obj + 0xC0);
        const float* t = reinterpret_cast<const float*>(obj + 0xA0);
        x = t[0]; y = t[1]; z = t[2];
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
    if (before == 0 && g_barrierNeedGround && is_barrier_proc(proc)) {
        bool solid = false;
        __try {
            const TrunkNode* pts[64];
            int np = trunk_find(x - 1.5f, z - 1.5f, x + 1.5f, z + 1.5f, pts, 64);
            const TrunkNode* best = nullptr;
            float bd = 1e9f;
            for (int i = 0; i < np; ++i) {
                if (pts[i]->size >= 0 || pts[i]->size <= -100.0f) continue;   // barrier points only
                float d = (pts[i]->x - x) * (pts[i]->x - x) + (pts[i]->z - z) * (pts[i]->z - z);
                if (d < bd) { bd = d; best = pts[i]; }
            }
            solid = best && !bar_ok(best);
            if (solid) {
                static std::atomic<int> ns{0};
                if (++ns <= 40)
                    dlogf("barrier kept solid (no ground seen behind it yet): proc=%d child=%d at (%.1f, %.1f, %.1f), sides %d", proc,
                         *reinterpret_cast<int32_t*>(obj + 0xC4), x, y, z, g_barInfo[best - g_trunkPool].flags.load() & 3);
            }
        } __except (EXCEPTION_EXECUTE_HANDLER) {}
        if (solid) return;   // no break: it stays a solid barrier, like the game made it
    }
    o_onCollision(obj, contact);
    __try {
        int after = *reinterpret_cast<int32_t*>(obj + 0x148);
        if (before == 0 && after == 0 && is_barrier_proc(proc)) {
            static std::atomic<int> nh{0};
            if (++nh <= 60)
                dlogf("barrier hit but not broken: def %u proc=%d child=%d at (%.1f, %.1f, %.1f)", *reinterpret_cast<uint32_t*>(obj + 0x124) & 0xFFFF,
                     proc, *reinterpret_cast<int32_t*>(obj + 0xC4), x, y, z);
        }
        if (before == 0 && after != 0 && !is_tree_proc(proc) && is_barrier_proc(proc)) {
            float loss = 0, bm = 0, am = 0, cv[4] = {0, 0, 0, 0};
            apply_speed_loss(contact, 1.0f, &loss, &bm, &am, cv, g_barrierSpeedLoss);
            static std::atomic<int> nb{0};
            int k = ++nb;
            if (k <= 200)
                dlogf("barrier broke: def %d proc=%d child=%d at (%.1f, %.1f, %.1f); speed %.0f -> %.0f mph; %d so far",
                     *reinterpret_cast<int32_t*>(obj + 0x124), proc, *reinterpret_cast<int32_t*>(obj + 0xC4), x, y, z, bm, am, k);
        }
        if (before == 0 && after != 0 && is_tree_proc(proc)) {
            add_broken(x, y, z);
            // size of this tree, from its definition copy
            uint32_t lo = (uint32_t)*reinterpret_cast<int32_t*>(obj + 0x124) & 0xFFFF;
            float size  = (lo >= (uint32_t)CLONE_BASE && lo < (uint32_t)(CLONE_BASE + NUM_TIERS)) ? tier_size((int)(lo - CLONE_BASE)) : 1.0f;
            {
                uint8_t* mgr      = *reinterpret_cast<uint8_t**>(obj + 0x40);
                MsvcVector* cells = vec_at(mgr + 0x518);
                for (uint8_t* c = cells->first; c && c < cells->last; c += 0x40)
                    if (*reinterpret_cast<int32_t*>(c + 0x28) == proc) {
                        uint8_t* prov = *reinterpret_cast<uint8_t**>(c + 0x20);
                        if (prov && *reinterpret_cast<uintptr_t*>(prov) == g_base + RVA_MODELS_VTABLE) {
                            float s2 = child_size(*reinterpret_cast<uint8_t**>(prov + 0xF8), *reinterpret_cast<int32_t*>(obj + 0xC4));
                            if (s2 > 0) size = s2;
                        }
                        break;
                    }
            }
            float loss = 0, before_mph = 0, after_mph = 0;
            float carVel[4] = {0, 0, 0, 0};
            uint8_t* defTable = *reinterpret_cast<uint8_t**>(*reinterpret_cast<uint8_t**>(obj + 0x40) + 0x4C8);
            if ((g_bushDef > 0 && (int)lo == nd(defTable, g_bushDef)) || bush_like_def(defTable, (int)lo)) {
                // a bush or crop: its own puff and sound come with BushDef; no log, no Lumberjack
                apply_speed_loss(contact, size, &loss, &before_mph, &after_mph, carVel, g_bushSpeedLoss);
                if (*g_bushEffects) spawn_effects(*reinterpret_cast<uint8_t**>(obj + 0x40), g_bushEffects, x, y, z, carVel);
                static std::atomic<int> nb{0};
                int k = ++nb;
                if (k <= 200)
                    dlogf("bush broke: proc=%d child=%d at (%.1f, %.1f, %.1f); speed %.0f -> %.0f mph (-%.0f%%); %d so far", proc,
                         *reinterpret_cast<int32_t*>(obj + 0xC4), x, y, z, before_mph, after_mph, loss * 100, k);
            } else {
            apply_speed_loss(contact, size, &loss, &before_mph, &after_mph, carVel);
            {
                uint8_t* mgr = *reinterpret_cast<uint8_t**>(obj + 0x40);
                const char* list = size >= g_lumberjackMinSize - 0.01f ? g_bigTreeEffects : g_treeEffects;
                if (*list) spawn_effects(mgr, list, x, y, z, carVel);
            }
            if (size >= g_logMinSize - 0.01f) queue_log(obj, x, y, z, carVel, size);
            static std::atomic<int> checked{0};
            if (checked.fetch_add(1) == 0)
                if (uint8_t* d = g_destruction.load())
                    if (*reinterpret_cast<uintptr_t*>(d) == g_base + RVA_DESTRUCTION_VTABLE)
                        dlogf("Wreckage RetriggerDelay is now %.2f", *reinterpret_cast<float*>(d + 0x24));
            dlogf("tree broke: size %.2f proc=%d child=%d state %d->%d at (%.1f, %.1f, %.1f); speed %.0f -> %.0f mph (-%.0f%%); %d broken so far",
                 size, proc, *reinterpret_cast<int32_t*>(obj + 0xC4), before, after, x, y, z, before_mph, after_mph, loss * 100, g_nBroken.load());
            }
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {}
}

// ---- setup ------------------------------------------------------------------
std::wstring g_dir; // fh2-mods folder

// Written to fh2-mods\WreckingBall.ini the first time the mod runs. Every setting not listed
// here keeps its built-in default (see the README for the advanced ones).
const char* const DEFAULT_INI =
    "; WreckingBall settings. Changes here apply the next time you start the game. The main switches\r\n"
    "; are also in the game, under Pause > Settings > MODS.\r\n"
    "; Delete this file to get the defaults back.\r\n"
    "[WreckingBall]\r\n"
    "\r\n"
    "; ---- trees ----\r\n"
    "; 1 = trees and bushes break when you hit them, 0 = they stay solid\r\n"
    "InjectTrees=1\r\n"
    "; share of your speed a normal tree takes (0.12 = 12%); bigger trees take more, up to SpeedLossMax\r\n"
    "SpeedLoss=0.12\r\n"
    "SpeedLossMax=0.45\r\n"
    "; trees at least this big count as big (1 = a normal tree): they throw splinters and give Lumberjack\r\n"
    "LumberjackMinSize=1.6\r\n"
    "BigTreeEffects=Break_WoodDust,Break_Wood\r\n"
    "\r\n"
    "; ---- bushes and crops ----\r\n"
    "; models whose name contains one of these flatten like the game's own bushes: a small puff of leaves,\r\n"
    "; no log, no Lumberjack (empty = they break like trees)\r\n"
    "BushModels=Bush_,Crop_,VINEYARD_\r\n"
    "; share of your speed a bush takes\r\n"
    "BushSpeedLoss=0.04\r\n"
    "; models whose name contains one of these stay solid, as without the mod\r\n"
    "SkipModels=\r\n"
    "\r\n"
    "; ---- logs: a log thrown out of each broken tree ----\r\n"
    "Logs=1\r\n"
    "; trees smaller than this don't throw one (bushes never do)\r\n"
    "LogMinSize=0.4\r\n"
    "; log size, as multiples of the model's own: length and thickness. Bigger trees throw longer logs,\r\n"
    "; kept between LogLengthMin and LogLengthMax\r\n"
    "LogLength=3.0\r\n"
    "LogThickness=4.5\r\n"
    "LogLengthMin=1.5\r\n"
    "LogLengthMax=5.0\r\n"
    "; away from the orchards, logs use the olive trunk; this keeps them the same size (model:length/thickness)\r\n"
    "LogModelScale=948:0.85/0.4\r\n"
    "\r\n"
    "; ---- fences, railings and roadside clutter ----\r\n"
    "Barriers=1\r\n"
    "; signs, bins, lamp posts, bollards... that have a breakable version somewhere in the game\r\n"
    "Clutter=1\r\n"
    "; anything whose model name contains one of these stays solid\r\n"
    "BarrierExclude=Crowd,FEST_,Carmeet,Banner,Festival,Concrete,Blast,Wall,Bridge,Gate,Hydraulic,Tyre,Tire\r\n"
    "; 1 = barriers that only appear for an event (race and crowd barriers) stay solid\r\n"
    "BarrierSkipEvents=1\r\n"
    "; 1 = a barrier only breaks where there's ground on both sides, so ones holding you back from a drop stay solid\r\n"
    "BarrierNeedGround=1\r\n"
    "BarrierSpeedLoss=0.04\r\n"
    "\r\n"
    "; ---- telephone poles ----\r\n"
    "Poles=1\r\n"
    "PoleSpeedLoss=0.15\r\n"
    "; the big square concrete pylons have no collision in the game; the mod makes them solid (empty = off)\r\n"
    "SolidModels=8616\r\n"
    "; 1 = those pylons are solid, 0 = they're left as the game has them\r\n"
    "SolidPylons=1\r\n"
    "\r\n"
    "; ---- clean-up ----\r\n"
    "; 1 = broken pieces fade out after a while, 0 = they stay until the game clears them\r\n"
    "DebrisCleanup=1\r\n"
    "; ...after this many seconds, or once you're this many metres away (0 = no limit of that kind)\r\n"
    "DebrisLifetime=45\r\n"
    "DebrisDistance=120\r\n"
    "\r\n"
    "; ---- other ----\r\n"
    "; 1 = the title screen says when a newer WreckingBall is out\r\n"
    "CheckUpdates=1\r\n"
    "; 1 = detailed WreckingBall.log, F8 writes what's around the car to it, and the game's breakable\r\n"
    "; definitions go to WreckingBall-defs.txt (for reporting problems)\r\n"
    "Debug=0\r\n";

int g_checkUpdates = 1;
bool g_rowsOff = false;   // the world update couldn't be hooked

void load_settings() {
    std::wstring ini = g_dir + L"\\WreckingBall.ini";
    if (GetFileAttributesW(ini.c_str()) == INVALID_FILE_ATTRIBUTES) {
        if (FILE* f = _wfopen(ini.c_str(), L"wb")) {
            fputs(DEFAULT_INI, f);
            fclose(f);
        }
    }
    g_inject      = GetPrivateProfileIntW(L"WreckingBall", L"InjectTrees", 1, ini.c_str());
    g_treeDef     = GetPrivateProfileIntW(L"WreckingBall", L"TreeDef", 97, ini.c_str());
    g_bigTreeDef  = GetPrivateProfileIntW(L"WreckingBall", L"BigTreeDef", 98, ini.c_str());
    g_logLibs     = GetPrivateProfileIntW(L"WreckingBall", L"LogLibraries", 1, ini.c_str());
    g_maxLogLines = GetPrivateProfileIntW(L"WreckingBall", L"MaxLogLines", 4000, ini.c_str());
    g_filterCollision = GetPrivateProfileIntW(L"WreckingBall", L"FilterCollision", 2, ini.c_str());
    {
        wchar_t r[32];
        GetPrivateProfileStringW(L"WreckingBall", L"TrunkRadius", L"1.6", r, 32, ini.c_str());
        float f = (float)_wtof(r);
        if (f > 0.2f && f < 10.0f) g_trunkRadius = f;
    }
    auto getf = [&](const wchar_t* key, float def) {
        wchar_t b[64], d[32];
        swprintf(d, 32, L"%g", def);
        GetPrivateProfileStringW(L"WreckingBall", key, d, b, 64, ini.c_str());
        return (float)_wtof(b);
    };
    wchar_t big[512];
    GetPrivateProfileStringW(L"WreckingBall", L"Prefix", L"*", big, 512, ini.c_str());
    WideCharToMultiByte(CP_UTF8, 0, big, -1, g_prefix, sizeof g_prefix, nullptr, nullptr);
    if (!std::strcmp(g_prefix, "Trees_")) std::strcpy(g_prefix, "*"); // 0.1-0.4 default: now everything
    GetPrivateProfileStringW(L"WreckingBall", L"SizeRules", L"Bush_:0.5,Crop_:0.7,VINEYARD_:0.5,Skinny:0.8,Palm:0.9,Pine:1.3,Tall:1.3,Trees_:1.0", big, 512, ini.c_str());
    WideCharToMultiByte(CP_UTF8, 0, big, -1, g_sizeRules, sizeof g_sizeRules, nullptr, nullptr);
    g_sizeTiers      = GetPrivateProfileIntW(L"WreckingBall", L"SizeTiers", 0, ini.c_str());
    g_massScale      = getf(L"MassScale", 4.0f);
    g_massMax        = getf(L"MassMax", 25.0f);
    g_massExponent   = getf(L"MassExponent", 2.0f);
    g_breakSpeedScale = getf(L"BreakSpeedScale", 1.0f);
    g_leanFactor     = getf(L"LeanFactor", 0.35f);
    g_speedLoss         = getf(L"SpeedLoss", 0.12f);
    g_speedLossExponent = getf(L"SpeedLossExponent", 1.5f);
    g_speedLossMax      = getf(L"SpeedLossMax", 0.45f);
    g_wreckRetrigger    = getf(L"WreckageRetrigger", 1.0f);
    g_lumberjackRetrigger = getf(L"LumberjackRetrigger", 2.5f);
    g_sideswipeRetrigger  = getf(L"SideswipeRetrigger", 5.0f);
    g_treeObjectType    = GetPrivateProfileIntW(L"WreckingBall", L"TreeObjectType", 5, ini.c_str());
    g_lumberjackMinSize = getf(L"LumberjackMinSize", 1.6f);
    g_effectScale       = getf(L"EffectScale", 1.0f);
    GetPrivateProfileStringW(L"WreckingBall", L"BigTreeEffects", L"Break_WoodDust,Break_Wood", big, 512, ini.c_str());
    WideCharToMultiByte(CP_UTF8, 0, big, -1, g_bigTreeEffects, sizeof g_bigTreeEffects, nullptr, nullptr);
    GetPrivateProfileStringW(L"WreckingBall", L"TreeEffects", L"", big, 512, ini.c_str());
    WideCharToMultiByte(CP_UTF8, 0, big, -1, g_treeEffects, sizeof g_treeEffects, nullptr, nullptr);
    GetPrivateProfileStringW(L"WreckingBall", L"BushModels", L"Bush_,Crop_,VINEYARD_", big, 512, ini.c_str());
    if (std::wcscmp(big, L"Bush_,Crop_") == 0) std::wcscpy(big, L"Bush_,Crop_,VINEYARD_");   // the old default: vineyards were trees
    WideCharToMultiByte(CP_UTF8, 0, big, -1, g_bushModels, sizeof g_bushModels, nullptr, nullptr);
    GetPrivateProfileStringW(L"WreckingBall", L"SkipModels", L"", big, 512, ini.c_str());
    WideCharToMultiByte(CP_UTF8, 0, big, -1, g_skipModels, sizeof g_skipModels, nullptr, nullptr);
    GetPrivateProfileStringW(L"WreckingBall", L"BushEffects", L"", big, 512, ini.c_str());
    WideCharToMultiByte(CP_UTF8, 0, big, -1, g_bushEffects, sizeof g_bushEffects, nullptr, nullptr);
    g_bushDef       = GetPrivateProfileIntW(L"WreckingBall", L"BushDef", 110, ini.c_str());
    {
        wchar_t w[64];
        GetPrivateProfileStringW(L"WreckingBall", L"BushDefs", L"266", w, 64, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, w, -1, g_bushDefs, sizeof g_bushDefs, nullptr, nullptr);
    }
    g_bushSpeedLoss = getf(L"BushSpeedLoss", 0.04f);
    g_barriers          = GetPrivateProfileIntW(L"WreckingBall", L"Barriers", 1, ini.c_str());
    g_barrierSkipEvents = GetPrivateProfileIntW(L"WreckingBall", L"BarrierSkipEvents", 1, ini.c_str());
    g_barrierSpeedLoss  = getf(L"BarrierSpeedLoss", 0.04f);
    g_barrierNeedGround = GetPrivateProfileIntW(L"WreckingBall", L"BarrierNeedGround", 1, ini.c_str());
    g_clutter           = GetPrivateProfileIntW(L"WreckingBall", L"Clutter", 1, ini.c_str());
    g_poles             = GetPrivateProfileIntW(L"WreckingBall", L"Poles", 1, ini.c_str());
    g_poleSpeedLoss     = getf(L"PoleSpeedLoss", 0.15f);
    g_poleLength        = getf(L"PoleLength", 8.5f);
    g_poleThick         = getf(L"PoleThickness", 3.5f);
    {
        wchar_t w[64];
        GetPrivateProfileStringW(L"WreckingBall", L"PoleModel", L"97:4", w, 64, ini.c_str());
        int d = -1, pc = 0xFFFF;
        if (swscanf(w, L"%d:%d", &d, &pc) >= 1 && d >= 0 && d < 0xF000) g_poleLook = LogCandidate{(uint16_t)d, (uint16_t)pc};
        else g_poleLook = LogCandidate{0xFFFF, 0xFFFF};
    }
    g_poleHeight        = getf(L"PoleCentreHeight", 4.3f);
    g_poleRadius        = getf(L"PoleCollisionRadius", 0.9f);
    g_poleMaxScale      = getf(L"PoleMaxScale", 1.3f);
    g_debrisLifetime    = getf(L"DebrisLifetime", 45.0f);
    g_debrisDistance    = getf(L"DebrisDistance", 120.0f);
    g_debrisCleanup     = GetPrivateProfileIntW(L"WreckingBall", L"DebrisCleanup", 1, ini.c_str());
    {
        wchar_t w[256], d[256];
        MultiByteToWideChar(CP_UTF8, 0, g_poleModels, -1, d, 256);
        GetPrivateProfileStringW(L"WreckingBall", L"PoleModels", d, w, 256, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, w, -1, g_poleModels, sizeof g_poleModels, nullptr, nullptr);
        MultiByteToWideChar(CP_UTF8, 0, g_islandPoleModels, -1, d, 256);
        GetPrivateProfileStringW(L"WreckingBall", L"IslandPoleModels", d, w, 256, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, w, -1, g_islandPoleModels, sizeof g_islandPoleModels, nullptr, nullptr);
    }
    g_barrierProbe      = getf(L"BarrierGroundProbe", 3.0f);
    g_barrierPointRadius = getf(L"BarrierCollisionRadius", 0.9f);
    {
        auto gets = [&](const wchar_t* key, const char* def, char* out, size_t cap) {
            wchar_t wdef[512], w[512];
            MultiByteToWideChar(CP_UTF8, 0, def, -1, wdef, 512);
            GetPrivateProfileStringW(L"WreckingBall", key, wdef, w, 512, ini.c_str());
            WideCharToMultiByte(CP_UTF8, 0, w, -1, out, (int)cap, nullptr, nullptr);
        };
        char tmp[512];
        std::strcpy(tmp, g_barrierLibs);    gets(L"BarrierLibraries", tmp, g_barrierLibs, sizeof g_barrierLibs);
        std::strcpy(tmp, g_barrierRules);   gets(L"BarrierRules", tmp, g_barrierRules, sizeof g_barrierRules);
        std::strcpy(tmp, g_barrierExclude); gets(L"BarrierExclude", tmp, g_barrierExclude, sizeof g_barrierExclude);
    }
    g_logs       = GetPrivateProfileIntW(L"WreckingBall", L"Logs", 1, ini.c_str());
    g_logCycle   = GetPrivateProfileIntW(L"WreckingBall", L"LogCycle", 0, ini.c_str());
    g_logAxis    = GetPrivateProfileIntW(L"WreckingBall", L"LogAxis", 1, ini.c_str()) % 3;
    if (g_logAxis < 0) g_logAxis = 1;
    g_logMinSize = getf(L"LogMinSize", 0.4f);
    g_logTilt    = getf(L"LogTilt", 90.0f);
    g_logHeight  = getf(L"LogHeight", 1.0f);
    g_logAhead   = getf(L"LogAhead", 0.0f);
    g_logSpeed   = getf(L"LogSpeed", 0.7f);
    g_logUp      = getf(L"LogUp", 2.0f);
    g_logSpin    = getf(L"LogSpin", 3.0f);
    g_logFromCar   = GetPrivateProfileIntW(L"WreckingBall", L"LogFromCar", 1, ini.c_str());
    g_logCarAhead  = getf(L"LogCarAhead", 3.0f);
    g_logCarHeight = getf(L"LogCarHeight", 0.1f);
    g_logCarUp     = getf(L"LogCarUp", 0.0f);
    g_logCarTilt   = getf(L"LogCarTilt", 8.0f);
    g_logStretch   = GetPrivateProfileIntW(L"WreckingBall", L"LogStretch", 1, ini.c_str());
    g_logLength    = getf(L"LogLength", 3.0f);
    g_logThickness = getf(L"LogThickness", 4.5f);
    g_logLengthJitter = getf(L"LogLengthJitter", 0.25f);
    g_logLengthSizeExp = getf(L"LogLengthSizeExponent", 1.0f);
    g_logLengthMin = getf(L"LogLengthMin", 1.5f);
    g_logLengthMax = getf(L"LogLengthMax", 5.0f);
    if (g_logLengthJitter < 0) g_logLengthJitter = 0;
    if (g_logLengthJitter > 0.9f) g_logLengthJitter = 0.9f;
    if (!(g_logLengthMin > 0.05f)) g_logLengthMin = 0.05f;
    if (!(g_logLengthMax >= g_logLengthMin)) g_logLengthMax = g_logLengthMin;
    if (!(g_logLength > 0.05f && g_logLength < 20)) g_logLength = 1;
    if (!(g_logThickness > 0.05f && g_logThickness < 20)) g_logThickness = 1;
    g_debug        = GetPrivateProfileIntW(L"WreckingBall", L"Debug", 0, ini.c_str());
    g_dumpDefs     = g_debug;
    g_logProbe     = g_debug;
    g_markEnabled  = g_debug;
    g_checkUpdates = GetPrivateProfileIntW(L"WreckingBall", L"CheckUpdates", 1, ini.c_str());
    g_loadedLooks = GetPrivateProfileIntW(L"WreckingBall", L"LoadedModelFallback", 1, ini.c_str());
    g_ownModels   = GetPrivateProfileIntW(L"WreckingBall", L"OwnModels", 1, ini.c_str());
    g_poleOwnModel = GetPrivateProfileIntW(L"WreckingBall", L"PoleOwnModel", 1, ini.c_str());
    {
        GetPrivateProfileStringW(L"WreckingBall", L"SolidModels", L"8616", big, 128, ini.c_str());
        g_solidOn = GetPrivateProfileIntW(L"WreckingBall", L"SolidPylons", 1, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, big, -1, g_solidModels, sizeof g_solidModels, nullptr, nullptr);
        auto gf = [&](const wchar_t* k, float d) {
            wchar_t w[64];
            GetPrivateProfileStringW(L"WreckingBall", k, L"", w, 64, ini.c_str());
            return w[0] ? (float)_wtof(w) : d;
        };
        g_solidHalf   = gf(L"SolidWidth", 0.64f) * 0.5f;
        g_solidHeight = gf(L"SolidHeight", 14.0f);
    }
    {
        char alts[256];
        GetPrivateProfileStringW(L"WreckingBall", L"TreeFallbacks", L"97,262,98,263", big, 256, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, big, -1, alts, sizeof alts, nullptr, nullptr);
        g_nTreeAlts = 0;
        for (const char* r = alts; *r && g_nTreeAlts < 8;) {
            while (*r == ' ' || *r == ',') ++r;
            if (!*r) break;
            int d = std::atoi(r);
            if (d > 0 && d < 0xF000) g_treeAlts[g_nTreeAlts++] = d;
            while (*r && *r != ',') ++r;
        }
        GetPrivateProfileStringW(L"WreckingBall", L"LogFallbacks", L"97:4,262:4,98:4,263:4", big, 256, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, big, -1, alts, sizeof alts, nullptr, nullptr);
        g_nLookAlts = 0;
        for (const char* r = alts; *r && g_nLookAlts < 8;) {
            while (*r == ' ' || *r == ',') ++r;
            if (!*r) break;
            int d = 0, pc = 0;
            if (sscanf(r, "%d:%d", &d, &pc) == 2 && d > 0 && d < 0xF000) g_lookAlts[g_nLookAlts++] = LogCandidate{(uint16_t)d, (uint16_t)pc};
            while (*r && *r != ',') ++r;
        }
        if (!g_loadedLooks) { g_nLookAlts = 0; g_nTreeAlts = 0; }
        GetPrivateProfileStringW(L"WreckingBall", L"LogModelScale", L"948:0.85/0.4", big, 256, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, big, -1, g_logModelScale, sizeof g_logModelScale, nullptr, nullptr);
    }
    {
        char cands[512];
        GetPrivateProfileStringW(L"WreckingBall", L"LogCandidates", L"28:3", big, 512, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, big, -1, cands, sizeof cands, nullptr, nullptr);
        parse_log_candidates(cands);
        GetPrivateProfileStringW(L"WreckingBall", L"LogModel", L"97:4", big, 512, ini.c_str());
        WideCharToMultiByte(CP_UTF8, 0, big, -1, cands, sizeof cands, nullptr, nullptr);
        LogCandidate keep[32];
        int nkeep = g_nLogCands;
        std::memcpy(keep, g_logCands, sizeof keep);
        parse_log_candidates(cands);
        g_logModel = g_nLogCands ? g_logCands[0] : LogCandidate{0xFFFF, 0xFFFF};
        std::memcpy(g_logCands, keep, sizeof keep);
        g_nLogCands = nkeep;
    }
}

bool code_matches(uintptr_t rva, const uint8_t* expect, size_t n) {
    return std::memcmp(reinterpret_cast<void*>(g_base + rva), expect, n) == 0;
}

// ---- the Mods page (Pause > Settings > MODS, platform 2.3.0 / API 4) ----------------------
// The main switches, as On/Off rows. A change is saved to WreckingBall.ini straight away.
// Trees, fences and clutter are added to the world's breakables as each area loads, so they
// take a restart; the rest switch on and off as you go.
struct ModsRow {
    const char* key;      // also the ini key
    const char* label;
    const char* description;
    int* flag;
    bool restart;
};
const ModsRow kModsRows[] = {
    {"InjectTrees", "Breakable trees", "Trees and bushes break when you hit them, and the biggest count for Lumberjack.", &g_inject, true},
    {"Logs", "Logs", "Every broken tree throws a log, sized to the tree.", &g_logs, false},
    {"Barriers", "Breakable fences and railings",
     "Wooden and wire fences, railings, guardrails and bollards break apart. Festival fences, crowd barriers and ones with a drop behind them stay solid.",
     &g_barriers, true},
    {"Clutter", "Breakable roadside clutter", "Signs, bins, lamp posts and other clutter that's breakable somewhere in the game break everywhere.",
     &g_clutter, true},
    {"Poles", "Falling telephone poles", "Wooden telephone poles fall over when you hit them.", &g_poles, false},
    {"SolidPylons", "Solid concrete pylons", "The big square concrete pylons, which the game left without collision, are solid.", &g_solidOn, false},
    {"DebrisCleanup", "Debris clean-up", "Broken pieces fade out after a while, or once you're far enough away (DebrisLifetime and DebrisDistance in the ini).",
     &g_debrisCleanup, false},
};

void mods_row_changed(void* user, const char* key, int value) {
    const ModsRow* r = static_cast<const ModsRow*>(user);
    if (!r->restart) *r->flag = value;
    wchar_t k[64], v[8];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, k, 64);
    swprintf(v, 8, L"%d", value);
    WritePrivateProfileStringW(L"WreckingBall", k, v, (g_dir + L"\\WreckingBall.ini").c_str());
    logf("%s set to %d on the Mods page%s", key, value, r->restart ? " (from the next start)" : "");
}

// The slowdown numbers, as sliders (platform 2.4.0 / API 5). They apply straight away.
struct ModsSlider {
    const char* key;      // also the ini key
    const char* label;
    const char* description;
    float* value;
    float min, max, step, scale;
    int decimals;
};
const ModsSlider kModsSliders[] = {
    {"SpeedLoss", "Tree slowdown", "How much of your speed a normal-sized tree takes, in percent. Bigger trees take more, smaller ones less.",
     &g_speedLoss, 0.0f, 0.40f, 0.01f, 100.0f, 0},
    {"SpeedLossMax", "Biggest tree slowdown", "The most of your speed any one tree can take, in percent, however big it is.",
     &g_speedLossMax, 0.0f, 0.90f, 0.01f, 100.0f, 0},
    {"SpeedLossExponent", "Tree size effect",
     "How much more big trees slow you down than small ones. 0 = every tree the same, 1 = in proportion to size, higher = big trees much more.",
     &g_speedLossExponent, 0.0f, 3.0f, 0.1f, 1.0f, 1},
    {"BushSpeedLoss", "Bush slowdown", "How much of your speed a bush, hedge or crop takes, in percent.", &g_bushSpeedLoss, 0.0f, 0.20f, 0.01f,
     100.0f, 0},
    {"BarrierSpeedLoss", "Fence slowdown", "How much of your speed a fence, railing or piece of clutter takes, in percent.", &g_barrierSpeedLoss,
     0.0f, 0.30f, 0.01f, 100.0f, 0},
    {"PoleSpeedLoss", "Telephone pole slowdown", "How much of your speed a telephone pole takes, in percent.", &g_poleSpeedLoss, 0.0f, 0.50f,
     0.01f, 100.0f, 0},
};

void mods_slider_changed(void* user, const char* key, float value) {
    const ModsSlider* r = static_cast<const ModsSlider*>(user);
    *r->value           = value;
    wchar_t k[64], v[32];
    MultiByteToWideChar(CP_UTF8, 0, key, -1, k, 64);
    swprintf(v, 32, L"%.4g", (double)value);
    WritePrivateProfileStringW(L"WreckingBall", k, v, (g_dir + L"\\WreckingBall.ini").c_str());
    logf("%s set to %.4g on the Mods page", key, (double)value);
}

void add_mods_rows() {
    if (g_api->api_version < 4) return;   // an older platform: no Mods page
    for (const ModsRow& r : kModsRows) {
        FH2Setting s{};
        s.size          = sizeof s;
        s.key           = r.key;
        s.label         = r.label;
        s.description   = r.description;
        s.value         = *r.flag ? 1 : 0;
        s.needs_restart = r.restart;
        s.on_change     = mods_row_changed;
        s.user          = const_cast<ModsRow*>(&r);
        g_api->add_setting(g_mod, &s);
    }
    if (g_api->api_version < 5) return;   // sliders came with platform 2.4.0
    for (const ModsSlider& r : kModsSliders) {
        FH2Setting s{};
        s.size          = sizeof s;
        s.key           = r.key;
        s.label         = r.label;
        s.description   = r.description;
        s.kind          = FH2_SETTING_SLIDER;
        s.min           = r.min;
        s.max           = r.max;
        s.step          = r.step;
        s.slider_value  = *r.value;
        s.display_scale = r.scale;
        s.decimals      = r.decimals;
        s.on_slide      = mods_slider_changed;
        s.user          = const_cast<ModsSlider*>(&r);
        g_api->add_setting(g_mod, &s);
    }
}

DWORD WINAPI setup_thread(LPVOID) {
    void* base = g_api->wait_for_code(0xFFFFFFFFu);
    if (!base) {
        logf("game code never became ready; nothing hooked");
        return 0;
    }
    g_base = reinterpret_cast<uintptr_t>(base);
    logf("game at %p", base);

    // Sanity checks on the first bytes of what we call or hook.
    static const uint8_t deser[]  = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x50};             // push rdi; sub rsp,50h
    static const uint8_t resize[] = {0x48, 0x89, 0x4C, 0x24, 0x08, 0x57};             // mov [rsp+8],rcx; push rdi
    static const uint8_t add[]    = {0x48, 0x89, 0x5C, 0x24, 0x08, 0x57};             // mov [rsp+8],rbx; push rdi
    static const uint8_t tq[]     = {0x48, 0x89, 0x5C, 0x24, 0x20, 0x57, 0x41, 0x54}; // mov [rsp+20h],rbx; push rdi; push r12
    static const uint8_t hl[]     = {0x40, 0x57, 0x48, 0x83, 0xEC, 0x40};             // push rdi; sub rsp,40h
    static const uint8_t dl[]     = {0x48, 0x8B, 0x41, 0x08, 0x44, 0x0F, 0xB7, 0xC2, 0xC1, 0xEA, 0x10}; // def lookup
    static const uint8_t dm[]     = {0x48, 0x8B, 0x41, 0x08, 0x44, 0x0F, 0xB7, 0xC2, 0xC1, 0xEA, 0x10}; // def render model
    if (!code_matches(RVA_DESERIALIZE, deser, sizeof deser) || !code_matches(RVA_GROUP_RESIZE, resize, sizeof resize) ||
        !code_matches(RVA_CELL_ADD, add, sizeof add) ||
        *reinterpret_cast<uintptr_t*>(g_base + RVA_MODELS_VTABLE + 21 * 8) != g_base + 0x1887290 ||
        *reinterpret_cast<uintptr_t*>(g_base + RVA_COLLOBJ_VTABLE) != g_base + 0x378230 ||
        !code_matches(RVA_TEMPLATE_QUERY, tq, sizeof tq) || !code_matches(RVA_HOLDER_LOAD, hl, sizeof hl) ||
        !code_matches(0x12CC010, dl, sizeof dl) || !code_matches(0x12CD300, dm, sizeof dm)) {
        logf("ERROR: game code doesn't look like the 24 Nov 2015 build; nothing hooked");
        g_api->log(g_mod, "game code doesn't match; WreckingBall is off");
        return 0;
    }

    bool ok = true;
    ok &= g_api->hook_function(g_mod, "ProceduralModelsData::Serialize", reinterpret_cast<void*>(g_base + RVA_DESERIALIZE),
                               reinterpret_cast<void*>(d_deserialize), reinterpret_cast<void**>(&o_deserialize)) != nullptr;
    ok &= g_api->hook_function(g_mod, "CWorldCollObjMan::AddCell", reinterpret_cast<void*>(g_base + RVA_CELL_ADD),
                               reinterpret_cast<void*>(d_cellAdd), reinterpret_cast<void**>(&o_cellAdd)) != nullptr;
    ok &= g_api->hook_function(g_mod, "CWorldCollObjMan::SpawnCell", reinterpret_cast<void*>(g_base + RVA_CELL_SPAWN),
                               reinterpret_cast<void*>(d_cellSpawn), reinterpret_cast<void**>(&o_cellSpawn)) != nullptr;
    ok &= g_api->hook_function(g_mod, "CWorldCollObjMan::MakeDynamic", reinterpret_cast<void*>(g_base + RVA_SWAP),
                               reinterpret_cast<void*>(d_swap), reinterpret_cast<void**>(&o_swap)) != nullptr;
    ok &= g_api->hook_function(g_mod, "CollObj definition lookup", reinterpret_cast<void*>(g_base + 0x12CC010),
                               reinterpret_cast<void*>(d_defLookup), reinterpret_cast<void**>(&o_defLookup)) != nullptr;
    ok &= g_api->hook_function(g_mod, "CollObj definition render model", reinterpret_cast<void*>(g_base + 0x12CD300),
                               reinterpret_cast<void*>(d_defModel), reinterpret_cast<void**>(&o_defModel)) != nullptr;
    if (*reinterpret_cast<uintptr_t*>(g_base + RVA_DESTRUCTION_VTABLE + 14 * 8) == g_base + 0xA81770)
        ok &= g_api->hook_pointer(g_mod, "Skills::Destruction::Load", reinterpret_cast<void**>(g_base + RVA_DESTRUCTION_VTABLE + 14 * 8),
                                  reinterpret_cast<void*>(d_destructionLoad), reinterpret_cast<void**>(&o_destructionLoad)) != nullptr;
    if (*reinterpret_cast<uintptr_t*>(g_base + RVA_LUMBERJACK_VTABLE + 14 * 8) == g_base + 0xA81930)
        ok &= g_api->hook_pointer(g_mod, "Skills::Lumberjack::Load", reinterpret_cast<void**>(g_base + RVA_LUMBERJACK_VTABLE + 14 * 8),
                                  reinterpret_cast<void*>(d_lumberjackLoad), reinterpret_cast<void**>(&o_lumberjackLoad)) != nullptr;
    if (*reinterpret_cast<uintptr_t*>(g_base + RVA_SIDESWIPE_VTABLE + 14 * 8) == g_base + 0xA80D90)
        ok &= g_api->hook_pointer(g_mod, "Skills::SideSwipe::Load", reinterpret_cast<void**>(g_base + RVA_SIDESWIPE_VTABLE + 14 * 8),
                                  reinterpret_cast<void*>(d_sideswipeLoad), reinterpret_cast<void**>(&o_sideswipeLoad)) != nullptr;
    static const uint8_t gp[] = {0x48, 0x8B, 0xC4, 0x44, 0x89, 0x40, 0x18, 0x48, 0x89, 0x48, 0x08, 0x55};   // GetPolys
    if (g_solidModels[0] && code_matches(0x12CC730, gp, sizeof gp))
        ok &= g_api->hook_function(g_mod, "CollisionWorld::GetPolys", reinterpret_cast<void*>(g_base + 0x12CC730),
                                   reinterpret_cast<void*>(d_getPolys), reinterpret_cast<void**>(&o_getPolys)) != nullptr;
    else if (g_solidModels[0]) logf("solid pylons: game code doesn't match, off");
    static const uint8_t bp[] = {0x48, 0x8B, 0xC4, 0x48, 0x89, 0x50, 0x10, 0x48, 0x89, 0x48, 0x08, 0x55}; // break into pieces
    if (g_loadedLooks && code_matches(0x36AC80, bp, sizeof bp))
        ok &= g_api->hook_function(g_mod, "CWorldCollObjMan::BreakIntoPieces", reinterpret_cast<void*>(g_base + 0x36AC80),
                                   reinterpret_cast<void*>(d_breakPieces), reinterpret_cast<void**>(&o_breakPieces)) != nullptr;
    else if (g_loadedLooks) logf("tree piece fallback: game code doesn't match, off");
    // Own models for loose fences: an instruction hook, so platform 2.6.0 (API 7) or later. Not
    // part of `ok`: without it fences just keep the look of the definition they break as.
    static const uint8_t wm[] = {0x48, 0x8D, 0x4D, 0xC0, 0x48, 0x89, 0x4C, 0x24, 0x20};   // lea rcx,[rbp-40h]; mov [rsp+20h],rcx
    if (g_ownModels && g_api->api_version >= 7 && code_matches(0x36AEAC, wm, sizeof wm)) {
        if (!g_api->hook_instruction(g_mod, "BreakIntoPieces: the loose copy's model", reinterpret_cast<void*>(g_base + 0x36AEAC),
                                     &on_whole_model, nullptr))
            logf("own models: the platform didn't allow the hook (see platform.log); fences keep their stand-in's look");
    } else if (g_ownModels) {
        logf(g_api->api_version < 7 ? "own models: needs FH2 Mod Platform 2.6.0 or later; fences keep their stand-in's look"
                                    : "own models: game code doesn't match, off");
        g_ownModels = 0;
    }
    static const uint8_t mu[] = {0x48, 0x8B, 0xC4, 0x55, 0x56, 0x57, 0x41, 0x54, 0x41, 0x55}; // CWorldCollObjMan update
    static const uint8_t so[] = {0x48, 0x8B, 0xC4, 0x44, 0x89, 0x40, 0x18, 0x88, 0x50, 0x10}; // SpawnObject
    // Logs, poles and debris clean-up all run from here, and each can be switched on and off
    // in the game (Pause > Settings > MODS), so it's hooked whatever they're set to now.
    if (code_matches(0x384C00, mu, sizeof mu) && code_matches(0x369690, so, sizeof so))
        ok &= g_api->hook_function(g_mod, "CWorldCollObjMan::Update", reinterpret_cast<void*>(g_base + 0x384C00),
                                   reinterpret_cast<void*>(d_mgrUpdate), reinterpret_cast<void**>(&o_mgrUpdate)) != nullptr;
    else {
        logf("world update: game code doesn't match, logs, poles and debris clean-up are off");
        g_logs = g_poles = g_debrisCleanup = 0;
        g_rowsOff = true;
    }
    static const uint8_t nu[] = {0x48, 0x8B, 0xC4, 0x44, 0x89, 0x48, 0x20, 0x4C, 0x89, 0x40, 0x18}; // render node update
    if (g_logStretch && !g_rowsOff) {
        if (*reinterpret_cast<uintptr_t*>(g_base + 0x2EB8590 + 0x338) == g_base + 0x132B390 && code_matches(0x13A41E0, nu, sizeof nu)) {
            ok &= g_api->hook_function(g_mod, "CTrackPresentation::BindCollObjInstanceToModel", reinterpret_cast<void*>(g_base + 0x132B390),
                                       reinterpret_cast<void*>(d_tpBind), reinterpret_cast<void**>(&o_tpBind)) != nullptr;
            ok &= g_api->hook_function(g_mod, "Presentation11 render node update", reinterpret_cast<void*>(g_base + 0x13A41E0),
                                           reinterpret_cast<void*>(d_nodeUpdate), reinterpret_cast<void**>(&o_nodeUpdate)) != nullptr;
        } else {
            logf("log stretching: game code doesn't match, off");
            g_logStretch = g_logProbe = 0;
        }
    }
    ok &= g_api->hook_pointer(g_mod, "CWorldCollObj::OnCollision", reinterpret_cast<void**>(g_base + RVA_COLLOBJ_VTABLE),
                              reinterpret_cast<void*>(d_onCollision), reinterpret_cast<void**>(&o_onCollision)) != nullptr;
    ok &= g_api->hook_function(g_mod, "StreamedPhysics::CollisionInstance template load", reinterpret_cast<void*>(g_base + RVA_HOLDER_LOAD),
                               reinterpret_cast<void*>(d_holderLoad), reinterpret_cast<void**>(&o_holderLoad)) != nullptr;
    ok &= g_api->hook_function(g_mod, "StreamedPhysics collision template query", reinterpret_cast<void*>(g_base + RVA_TEMPLATE_QUERY),
                               reinterpret_cast<void*>(d_templateQuery), reinterpret_cast<void**>(&o_templateQuery)) != nullptr;
    if (!ok || !g_api->enable_hooks(g_mod, nullptr)) {
        logf("ERROR: hooking failed (see platform.log)");
        return 0;
    }
    logf("on: trees %s, logs %s, fences and railings %s, roadside clutter %s, telephone poles %s, solid pylons %s, debris clean-up %s%s",
         g_inject ? "yes" : "no", g_logs ? "yes" : "no", g_barriers ? "yes" : "no", g_clutter ? "yes" : "no", g_poles ? "yes" : "no",
         g_solidModels[0] && g_solidOn ? "yes" : "no", g_debrisCleanup ? "yes" : "no",
         g_debug ? " (debug log on, F8 snapshot)" : "");
    return 0;
}

} // namespace

FH2MOD_EXPORT int fh2mod_init(const FH2ModApi* api) {
    if (api->api_version < 3) return 1;
    g_api = api;
    FH2ModInfo info{};
    info.size             = sizeof info;
    info.name             = "WreckingBall";
    info.version          = WB_VERSION;
    info.github_repo      = "Trinculo54/WreckingBall";
    info.platform_version = FH2MOD_PLATFORM_VERSION;
    info.description      = "Lets you drive through the scenery like the later Horizon games: trees snap and throw logs, fences and railings burst apart, roadside clutter goes flying and telephone poles fall over. Festival fences and the barriers in front of crowds stay solid.";
    g_dir = std::wstring(api->azimuth_dir) + L"\\fh2-mods";
    load_settings();
    info.check_updates    = g_checkUpdates;
    g_mod                 = api->register_mod(&info);
    if (!g_mod) return 1;
    g_trunkPool = static_cast<TrunkNode*>(VirtualAlloc(nullptr, sizeof(TrunkNode) * TRUNK_POOL, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    for (auto& h : g_trunkHead) h.store(-1);
    g_barInfo = static_cast<BarInfo*>(VirtualAlloc(nullptr, sizeof(BarInfo) * TRUNK_POOL, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
    g_log = _wfopen((g_dir + L"\\WreckingBall.log").c_str(), L"w");
    logf("WreckingBall %s", WB_VERSION);
    api->log(g_mod, "logging to fh2-mods\\WreckingBall.log");
    add_mods_rows();

    if (HANDLE t = CreateThread(nullptr, 0, setup_thread, nullptr, 0, nullptr)) CloseHandle(t);
    return 0;
}
