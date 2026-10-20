// SPDX-License-Identifier: MIT
// FH2ML SDK v4, separate from frozen v1/v2/v3. See include/LICENSE.
//
// v4 is the full mod API: FH2ML loads the mod and gives it every shared
// service a Forza Horizon 2 mod needs, so mods don't each reinvent them and
// don't step on each other's changes: hooks and byte patches with conflict
// checks, string-table edits merged across mods, file overrides, SQL against
// one shared copy of the game database, interfaces between mods, title-screen
// notices, in-game settings rows, pages and pause-menu tiles, crash reports and
// update checks.
//
// A v4 mod is a 64-bit Windows DLL in mods\<id>\ (with its mod.json) that exports
//
//     FH2ML_V4_EXPORT const FH2ML_ModAPI_v4 *FH2ML_GetModAPI_v4(uint32_t host_version);
//
// returning a static descriptor for host_version 4 (null otherwise). The query
// must not do any work; the host copies the descriptor straight away.
//
// Lifecycle, all on the FH2ML runtime worker (never under the loader lock):
//   Initialize    once, when Azimuth starts and before the game is mapped, in
//                 load order. Read your settings, register string edits, files,
//                 SQL, settings rows and pages, publish interfaces, and start
//                 your own threads for anything slow. String-table, file and
//                 database changes made here are always in time for the game.
//   OnGameReady   optional, once the supported game is mapped and Extra Options
//                 has finished patching it: safe to hook game code. Keep it
//                 short; it runs on the same worker as every other mod.
//   OnShutdown    optional, once after Initialize was entered, including when
//                 Initialize failed. Stop and join your own threads.
//
// Every string passed in is copied; nothing returned needs freeing. Services
// are thread-safe unless a comment says otherwise, and stay valid for the life
// of the process. Strings are UTF-8 unless typed wchar_t. Never throw C++
// exceptions across this C ABI.
//
// Compatibility: FH2ML_Services_v4.size only ever grows and members are only
// added at the end. Check `size` before using a member newer than your header.
#ifndef FH2ML_V4_H
#define FH2ML_V4_H
#include <stddef.h>
#include <stdint.h>
#include <wchar.h>
#ifdef __cplusplus
extern "C" {
#endif

#define FH2ML_API_VERSION_V4 4u

#ifdef __cplusplus
#define FH2ML_V4_EXPORT extern "C" __declspec(dllexport)
#else
#define FH2ML_V4_EXPORT __declspec(dllexport)
#endif

/* Opaque handle for your mod (FH2ML_ModContext_v4.mod); pass it to services. */
typedef struct FH2ML_Mod FH2ML_Mod;
/* Opaque handle for one hook or byte patch. */
typedef struct FH2ML_Hook FH2ML_Hook;
typedef struct FH2ML_Setting FH2ML_Setting;
typedef struct FH2ML_Page FH2ML_Page;
typedef struct FH2ML_PageRow FH2ML_PageRow;

/* The CPU's registers at an instruction hook (HookInstruction). rsp is what it
   was at the hooked instruction; changing it has no effect. */
typedef struct FH2ML_Xmm {
    union {
        float f[4];
        double d[2];
        uint32_t u32[4];
        uint64_t u64[2];
    };
} FH2ML_Xmm;
typedef struct FH2ML_Registers {
    uint64_t rax, rcx, rdx, rbx, rsp, rbp, rsi, rdi;
    uint64_t r8, r9, r10, r11, r12, r13, r14, r15;
    uint64_t rflags;
    uint64_t reserved;
    FH2ML_Xmm xmm[16];
} FH2ML_Registers;
typedef void (*FH2ML_InstructionFn)(FH2ML_Registers *registers, void *user);

typedef struct FH2ML_Services_v4 {
    uint32_t size;                    /* sizeof(FH2ML_Services_v4) as the runtime knows it */
    uint32_t version;                 /* FH2ML_API_VERSION_V4 */
    const char *runtime_version;      /* "0.2.0" */
    const wchar_t *azimuth_directory; /* folder with azimuth.exe, no trailing slash */
    const wchar_t *game_directory;    /* the game's Mount folder (game:\), or "" if not found */

    /* One line to FH2ML\logs\FH2ML.log, prefixed with your mod's ID. */
    void (*Log)(FH2ML_Mod *mod, const char *line);

    /* ---- Title screen ------------------------------------------------------
       Lines shown under "PRESS (A) TO START", with every other mod's. Keep each
       under ~40 characters. "\n" separates lines; NULL or "" clears. Shown when
       ApplyStrings runs (it does after every Initialize). */
    void (*SetNotice)(FH2ML_Mod *mod, const char *lines);

    /* ---- The game ----------------------------------------------------------
       Azimuth maps forza_durango_release_final.exe (the 24 Nov 2015 Xbox One
       build) into its own process when the player presses Play. */
    /* Base address of the mapped game, or NULL if it isn't mapped yet. */
    void *(*GameBase)(void);
    /* Waits until the game is mapped (or timeout_ms passes; 0xFFFFFFFF waits
       forever) and returns its base, or NULL. Call it from your own thread. */
    void *(*WaitForGame)(uint32_t timeout_ms);
    /* Like WaitForGame, but also waits until Extra Options has finished patching
       the game: returns the base when it's safe to hook game code, or NULL. Hooks
       in game code have to wait for this (or for OnGameReady). Call it from your
       own thread. */
    void *(*WaitForCode)(uint32_t timeout_ms);

    /* ---- Other mods --------------------------------------------------------
       Mods can offer each other functions: publish a struct of function
       pointers under a name like "WheresMyBuddy.Friends", and treat it like this
       API (never change a member, only add at the end and bump the version). */
    /* Offer `iface` (which must stay valid) as `name`, version `version`. NULL
       withdraws it. Another mod can't take a name you published. */
    void (*PublishInterface)(FH2ML_Mod *mod, const char *name, uint32_t version, const void *iface);
    /* The interface published as `name`, if its version is at least
       `min_version`; otherwise NULL. Mods initialize in load order, so look
       interfaces up after WaitForMods (or declare a dependency in mod.json). */
    const void *(*GetInterface)(const char *name, uint32_t min_version);
    /* Waits until every v4 mod's Initialize has returned (or timeout_ms passes).
       Returns 1 when they have. From inside Initialize it returns 0 at once. */
    int (*WaitForMods)(uint32_t timeout_ms);
    /* The version of the loaded mod with this mod.json ID (or display name,
       case-insensitively), or NULL if it isn't loaded. */
    const char *(*ModVersion)(const char *id);

    /* ---- String tables -----------------------------------------------------
       The game's text lives in string tables (media\Stripped\StringTables\
       <language>.zip\<Table>.str). Edits apply to every language, are merged with
       every other mod's edits to the same table, and are written to Extra
       Options' mediaoverride folder. The game reads string tables once at
       startup: edits made in Initialize are always in time; later ones show from
       the next launch. Later mods win on the same key; the log says so. */
    /* Set the string with `key` in `table` ("Main.str") to `text`. */
    void (*SetString)(FH2ML_Mod *mod, const char *table, uint16_t key, const wchar_t *text);
    /* Replace every string in `table` that is exactly `from` with `to`. */
    void (*ReplaceString)(FH2ML_Mod *mod, const char *table, const wchar_t *from, const wchar_t *to);
    /* Forget your edits to `table`, or to every table if NULL. */
    void (*ClearStrings)(FH2ML_Mod *mod, const char *table);
    /* Write the changed tables now. Done for you after every Initialize; call
       it after edits you make later. */
    void (*ApplyStrings)(FH2ML_Mod *mod);

    /* ---- Game files --------------------------------------------------------
       Replace a file the game reads from game:\media, through Extra Options'
       mediaoverride folder. `media_path` is relative to media, with a zip's name
       standing in for the zip: "drivatarglobalcache/DefaultCache.bin", or
       "UI/somefile.xml" for a file inside media\UI.zip. One mod per file: returns
       1 if written (or already identical), 0 if another mod already replaces it
       or the write failed (the log says which). String tables and the database
       have their own services and can't be replaced whole. */
    int (*SetFile)(FH2ML_Mod *mod, const char *media_path, const void *data, size_t size);
    /* Stop replacing a file (or all of yours, if NULL). Overrides no mod owns any
       more (a mod removed) are cleaned up shortly after startup. */
    void (*ClearFile)(FH2ML_Mod *mod, const char *media_path);

    /* ---- Game database -----------------------------------------------------
       media\Stripped\gamedbRC.slt is a SQLite database with most of the game's
       data (cars, events, AI drivers). AddSql queues statements (several per
       call is fine) to run against one shared copy: every mod's statements run
       in load order, each mod's in a transaction of its own, and the copy goes
       to mediaoverride. If a mod's SQL fails, only that mod's changes are rolled
       back, and the error is logged. The copy is rebuilt only when something
       changed. Statements added in Initialize are applied for you. */
    void (*AddSql)(FH2ML_Mod *mod, const char *sql);
    /* Drop all of your statements (call ApplyDatabase after). */
    void (*ClearSql)(FH2ML_Mod *mod);
    /* Rebuild the copy now if anything changed. Slow (seconds): call it from
       your own thread. The game reads the database once, at startup. */
    void (*ApplyDatabase)(FH2ML_Mod *mod);

    /* ---- Hooks -------------------------------------------------------------
       Change what game code does through FH2ML, so every mod's changes to the
       same code fit together: several mods can hook one function, each calls on
       to the next through `original`, and they can be switched on and off in any
       order. Hooks on exactly the same function, call site or pointer share it;
       a hook whose bytes overlap another hook, or game code that Extra Options
       (or a mod patching by itself) already changed, is refused and the log says
       why. Crash reports list every hook in place.

       Game code has to wait for WaitForCode (or OnGameReady); code elsewhere (an
       Azimuth or Windows DLL) can be hooked at any time. Each call makes a hook,
       switched off, and returns it (or NULL, with the reason in the log). `name`
       is for the log and crash reports, like "RadioStreamFmod::createSound"; it
       may be NULL. `*original` is set before the call returns: call it from your
       detour to run the code you replaced (and any other mod's detour behind
       yours); while your hook is off it goes straight to the game's code. When
       several mods hook the same place, the most recently enabled runs first. */
    /* An inline hook at the start of a function. */
    FH2ML_Hook *(*HookFunction)(FH2ML_Mod *mod, const char *name, void *target, void *detour, void **original);
    /* One CALL rel32 or JMP rel32 instruction: only that call goes to your
       detour; `*original` is the function it called. */
    FH2ML_Hook *(*HookCall)(FH2ML_Mod *mod, const char *name, void *call_site, void *detour, void **original);
    /* A function pointer in memory, like a vtable slot. */
    FH2ML_Hook *(*HookPointer)(FH2ML_Mod *mod, const char *name, void **slot, void *detour, void **original);
    /* `size` bytes (at most 32) written at `at`, which has to hold `expect` first
       (NULL: whatever matches the exe on disk). Disabling puts the original bytes
       back. Only one patch can cover a byte, and none can overlap a hook. */
    FH2ML_Hook *(*PatchBytes)(FH2ML_Mod *mod, const char *name, void *at, const void *expect, const void *bytes,
                              uint32_t size);
    /* An instruction hook: when the game reaches the instruction at `at`
       (anywhere in a function), `fn(registers, user)` runs before it, and the
       game carries on with any registers you change (not rsp). `at` has to start
       an instruction after the prolog, with nothing jumping into the bytes the
       hook replaces (checked; the log says why when refused). `fn` runs on the
       game's thread, often every frame: keep it short. If it crashes, that hook
       is switched off and the crash is logged. */
    FH2ML_Hook *(*HookInstruction)(FH2ML_Mod *mod, const char *name, void *at, FH2ML_InstructionFn fn, void *user);
    /* Switch `hook` on or off, or all of your hooks if NULL (in one go: the
       game's threads are paused once). Returns 1 if they all are now, 0 if any
       couldn't be (the log says why). */
    int (*EnableHooks)(FH2ML_Mod *mod, FH2ML_Hook *hook);
    int (*DisableHooks)(FH2ML_Mod *mod, FH2ML_Hook *hook);

    /* ---- In-game settings --------------------------------------------------
       Rows on the game's own Mods page (Pause > Settings > MODS): switches,
       sliders, and links (on your mod's page under MODS > Installed mods). Add
       rows from Initialize; the page is written once every Initialize has
       returned, so rows added later only show from the next launch.
       Your mod keeps owning the value: pass the current one in, and when the
       player changes it, the callback gives you the new one to save in your own
       settings file. Rows with needs_restart get a note, and changing one offers
       to restart the game when the player leaves the page. Returns 1 if the row
       was added, 0 if not (the log says why). */
    int (*AddSetting)(FH2ML_Mod *mod, const FH2ML_Setting *setting);
    /* A switch's current value, or -1 if you have no row called `key`. */
    int (*GetSetting)(FH2ML_Mod *mod, const char *key);
    /* You changed it yourself (a hotkey, say): the page shows it next time it
       opens. on_change isn't called. */
    void (*SetSetting)(FH2ML_Mod *mod, const char *key, int value);
    /* A slider's current value, or 0 if you have no slider called `key`. */
    float (*GetSlider)(FH2ML_Mod *mod, const char *key);
    /* You changed a slider yourself; on_slide isn't called. */
    void (*SetSlider)(FH2ML_Mod *mod, const char *key, float value);

    /* ---- Pages -------------------------------------------------------------
       A page of your own in the game's menus, laid out like the Mods page: rows
       with a label on the left and a value on the right, and the selected row's
       description beside the list. Your mod's page under MODS > Installed mods
       gets a row that opens it. A page can also take over a pause-menu tile that
       does nothing on PC (FH2ML_Page.tile). Add pages from Initialize. Returns 1
       if added, 0 if not (the log says why). */
    int (*AddPage)(FH2ML_Mod *mod, const FH2ML_Page *page);
    /* Page `page_key` now has these rows (copied), shown next time it opens.
       Call it whenever they change, or from on_open. */
    int (*SetPageRows)(FH2ML_Mod *mod, const char *page_key, const FH2ML_PageRow *rows, uint32_t count);
    /* A page of your own design on a tile: XAML using the game's controls,
       styles and textures, inside the frame of its options pages. `content`
       replaces what's between the title and the help bar and must name the
       element to focus first x:Name="FirstItem"; `resources` (may be NULL) goes
       into the page's ResourceDictionary; `help` (may be NULL) replaces the help
       bar (NULL leaves only Back). UTF-8 fragments in the page's xmlns (local: is
       ForzaUI). Slot n (0..15) is an int the XAML can bind two-way as
       "FH2ML_Slot_<n>"; "FH2ML_Slot_<n>_Is_<v>" is true while it's v (0..9999),
       for Visibility with the converters FH2ML_ShowIf, FH2ML_ShowIfHidden and
       FH2ML_HideIf that FH2ML puts in the page's resources. Slots are 0 when the
       page opens; set others in on_open. Returns 0 if `content` has no FirstItem
       or a fragment's tags don't balance (the page keeps the XAML it had). */
    int (*SetPageXaml)(FH2ML_Mod *mod, const char *page_key, const char *resources, const char *content,
                       const char *help);
    /* A slot's value (0 if there's no such page or slot). */
    int (*GetPageSlot)(FH2ML_Mod *mod, const char *page_key, int slot);
    /* Sets a slot. From on_button or on_slot the page's bindings see it straight
       away (from on_slot, except the slot being set); from on_open it's what the
       page opens with; anywhere else it's only stored. */
    int (*SetPageSlot)(FH2ML_Mod *mod, const char *page_key, int slot, int value);
    /* Moves the focus to the Button, Slider or SpinnerControl named `name` in
       the page's XAML, from on_button. Returns 1 if it did. */
    int (*FocusPageElement)(FH2ML_Mod *mod, const char *page_key, const char *name);
    /* A line on the tile your page took over, bottom left, in the game's style
       for a tile's count: a score, say. Plain ASCII, up to 40 characters; NULL
       or "" shows nothing. Any thread, whenever it changes. */
    int (*SetPageTileText)(FH2ML_Mod *mod, const char *page_key, const char *text);
    /* That tile's colours as 0xAARRGGBB: `background`, and `text` for its title
       and your line. 0 keeps the game's own. The icon takes the background's
       colour. */
    int (*SetPageTileColors)(FH2ML_Mod *mod, const char *page_key, uint32_t background, uint32_t text);
} FH2ML_Services_v4;

/* Called on the game's UI thread when the player changes a row: keep it short
   (save the value, flip a flag) and don't call back into the game. */
typedef void (*FH2ML_SettingChangedFn)(void *user, const char *key, int value);
typedef void (*FH2ML_SliderChangedFn)(void *user, const char *key, float value);

#define FH2ML_SETTING_SWITCH 0
#define FH2ML_SETTING_SLIDER 1
#define FH2ML_SETTING_LINK 2

struct FH2ML_Setting {
    uint32_t size;                    /* sizeof(FH2ML_Setting) */
    const char *key;                  /* unique within your mod: letters, digits and '_', like "Trees" */
    const char *label;                /* "Breakable trees" */
    const char *description;          /* shown beside the list while selected; may be NULL */
    int value;                        /* a switch's current value: 0 = Off, 1 = On */
    int needs_restart;                /* nonzero: a change only takes effect next time the game starts */
    FH2ML_SettingChangedFn on_change; /* may be NULL */
    void *user;                       /* passed to on_change and on_slide */
    int kind;                         /* FH2ML_SETTING_SWITCH, _SLIDER or _LINK */
    float min, max;                   /* a slider's range */
    float step;                       /* one press of left/right; 0 = a hundredth of the range */
    float slider_value;               /* a slider's current value */
    float display_scale;              /* the number shown is value times this (100 shows 0.12 as 12); 0 = 1 */
    int decimals;                     /* digits after the decimal point, 0 to 3 */
    FH2ML_SliderChangedFn on_slide;   /* may be NULL */
    const char *url;                  /* a link's "http://..." or "https://..." address */
};

/* Page callbacks run on the game's UI thread: keep them short and don't call
   into the game. */
/* About to be shown: set the rows now if you haven't kept them current.
   Opening MODS > Installed mods writes every mod's pages, so it's called then. */
typedef void (*FH2ML_PageOpenFn)(void *user, const char *page_key);
/* The player pressed A on a row that has a key. */
typedef void (*FH2ML_PageSelectFn)(void *user, const char *page_key, const char *row_key);
/* A page of your own design: a button was pressed (FH2ML_BUTTON_...). Return
   FH2ML_PAGE_IGNORED for the usual (B leaves, A does nothing), or a HANDLED value. */
typedef int (*FH2ML_PageButtonFn)(void *user, const char *page_key, int button);
/* A page of your own design: a binding changed slot `slot` to `value`. */
typedef void (*FH2ML_PageSlotFn)(void *user, const char *page_key, int slot, int value);

#define FH2ML_BUTTON_A 1
#define FH2ML_BUTTON_B 2
#define FH2ML_BUTTON_X 3
#define FH2ML_BUTTON_Y 4
#define FH2ML_BUTTON_LB 5
#define FH2ML_BUTTON_RB 6
#define FH2ML_PAGE_IGNORED 0
#define FH2ML_PAGE_HANDLED 1        /* nothing else happens */
#define FH2ML_PAGE_HANDLED_SELECT 2 /* and the menus' select sound plays */
#define FH2ML_PAGE_HANDLED_BACK 3   /* and the menus' back sound plays */
#define FH2ML_PAGE_SLOTS 16

struct FH2ML_Page {
    uint32_t size;               /* sizeof(FH2ML_Page) */
    const char *key;             /* unique within your mod: letters, digits and '_', like "List" */
    const char *title;           /* "Achievements" (shown in capitals) */
    const char *label;           /* the row on your mod's page that opens it; NULL = the title */
    const char *description;     /* that row's description; may be NULL */
    const char *tile;            /* a pause-menu tile to take over ("Achievements"), or NULL; one mod per tile */
    FH2ML_PageOpenFn on_open;    /* may be NULL */
    FH2ML_PageSelectFn on_select; /* may be NULL */
    void *user;                  /* passed to every callback */
    FH2ML_PageButtonFn on_button; /* pages of your own design; may be NULL */
    FH2ML_PageSlotFn on_slot;     /* pages of your own design; may be NULL */
};

#define FH2ML_ROW_NORMAL 0
#define FH2ML_ROW_DIM 1     /* label and value in grey: locked, not available yet */
#define FH2ML_ROW_HEADING 2 /* label in the game's orange: a heading over the rows after it */

struct FH2ML_PageRow {
    uint32_t size;           /* sizeof(FH2ML_PageRow) */
    const char *key;         /* passed to on_select when A is pressed; NULL: A does nothing */
    const char *label;       /* left: "Island Explorer" */
    const char *value;       /* right: "12 / 50"; may be NULL */
    const char *description; /* beside the list while selected; may be NULL */
    const char *footer;      /* a line at the bottom of the description panel; may be NULL */
    int style;               /* FH2ML_ROW_NORMAL, _DIM or _HEADING */
    int value_alert;         /* nonzero: the value in red */
};

/* Shutdown reasons are FH2ML_ShutdownReason values from FH2ML.h:
   0 requested, 1 Initialize failed, 2 OnGameReady failed, 3 runtime failed. */
typedef struct FH2ML_ModContext_v4 {
    uint32_t size, version;
    const FH2ML_Services_v4 *services;
    FH2ML_Mod *mod;                /* your handle for every service */
    const char *id;                /* your mod.json ID */
    const wchar_t *mod_directory;  /* mods\<id>: your installed files; read-only */
    const wchar_t *data_directory; /* FH2ML\data\<id>: your settings, logs and state; kept across updates */
} FH2ML_ModContext_v4;

/* Don't check for new releases of this mod on GitHub. */
#define FH2ML_V4_NO_UPDATE_CHECK 1u

typedef struct FH2ML_ModAPI_v4 {
    uint32_t size, version; /* sizeof(FH2ML_ModAPI_v4), FH2ML_API_VERSION_V4 */
    void *user;             /* passed to every callback */
    const char *name;       /* display name: "WheresMyBuddy" */
    const char *mod_version; /* "1.3.0" */
    const char *github_repo; /* "Owner/Repo" to check for new releases (title notice, MODS tab), or NULL */
    const char *description; /* a sentence or two for MODS > Installed mods; may be NULL */
    uint32_t flags;          /* FH2ML_V4_... */
    int (*Initialize)(const FH2ML_ModContext_v4 *context, void *user);           /* required; nonzero = success */
    int (*OnGameReady)(const FH2ML_ModContext_v4 *context, void *user, uintptr_t game_module); /* may be NULL */
    int (*OnShutdown)(const FH2ML_ModContext_v4 *context, void *user, uint32_t reason);         /* may be NULL */
} FH2ML_ModAPI_v4;

typedef const FH2ML_ModAPI_v4 *(*FH2ML_GetModAPI_v4Fn)(uint32_t host_version);

#ifdef __cplusplus
}
#endif
#endif
