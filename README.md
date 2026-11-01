<h1 align="center" id="title">WreckingBall</h1>

A mod for **Forza Horizon 2** on PC that lets you drive through the scenery the way later Horizon games do. In the original game most trees are solid walls, every fence and railing outside a festival is an invisible barrier, and a wooden telephone pole will stop a hypercar dead. WreckingBall makes all of those break: trees snap and throw a log, fences and railings burst apart, roadside clutter goes flying and telephone poles fall over. Festival fences and the barriers in front of crowds stay exactly as solid as before.

It works on the mainland and on Storm Island. It's a single DLL. The game's files are never touched, so removing it puts everything back the way it was.

## Features

**Trees and bushes.** Every tree and bush in the open world breaks when you hit it, using the game's own smashable tree physics, so it sounds and looks like the handful of breakable trees the game already had. Bigger trees slow you down more than small ones, and the biggest count as log piles, so they give you Lumberjack skill points. They also throw wood splinters and dust on top of the usual burst of leaves. Bushes, hedges and crops flatten like the game's own breakable bushes instead: a small puff of leaves, with no log and only a light slowdown.

**Logs.** Every broken tree throws a log, sized to the tree it came from. It pops up just ahead of your car, leaning away from you, for the nose to knock over, and its collision is the size of the log you see, so it rolls and settles where it looks like it should.

**Fences, railings and roadside clutter.** Wooden fences, wire fences, wicker and tarp fencing, metal railings, guardrails and bollards break apart when you hit them, and the pieces keep the look of the fence they came from, paint and all. So does roadside clutter that has a breakable version somewhere else in the game, like signs, bins and lamp posts. A few things always stay solid:
- the fences and barriers of the Festival and its events;
- barriers that only appear for a race, including those in front of the crowds;
- walls, bridges, concrete barriers and gates;
- any barrier with nothing but a drop behind it, so you can't fly off the edge of a mountain road.

**Telephone poles.** The wooden telephone poles along the roads fall over when you hit them: the pole itself tips over from where it stood and rolls on its full length, with collision the size of the pole. Only wooden telephone poles fall: metal, railway and concrete pylons stay solid on purpose. The big square concrete pylons work the other way round: the game forgot to give them any collision at all, so you could drive straight through them. WreckingBall makes them solid, whether the falling poles are on or off.

**Debris clean-up.** Broken pieces fade out after 45 seconds, or once you're 120 m away, so the world doesn't fill up with wreckage.

**Switches in the game.** The pause menu's MODS tab (Installed mods > WreckingBall), and Pause > Settings > MODS, have WreckingBall's main switches: breakable trees, logs, fences and railings, roadside clutter, falling telephone poles, solid concrete pylons and debris clean-up. Logs, poles, pylons and clean-up change straight away. Trees, fences and clutter are added to the world as each area loads, so they take a restart, and the game offers to restart for you when you leave the page. Under the switches are sliders for how much each thing slows you down: trees, the biggest trees, how much more big trees take than small ones, bushes, fences and telephone poles. They apply as you move them. Every change is saved to `WreckingBall.ini`.

## New in 1.3.3

- **Works next to mods that hook the same game code**, such as NitroSystem (FnFNitro). With one of those installed, 1.3.0 turned the falling telephone poles, logs, debris clean-up and solid concrete pylons off, though the MODS page still showed them On. Now both mods run.
- **A hook the platform won't allow now turns off only what needs it**, for everything but the breakable trees, fences and clutter: the falling poles, logs and debris clean-up (the world update), the solid pylons, the size of logs and falling poles, the pieces broken trees throw away from the orchards, fences keeping their own look, and the skills' timing. `WreckingBall.log` says which. The trees, fences and clutter still need all of their hooks: if the platform refuses one of those, WreckingBall changes nothing, and its log says "hooking failed".
- **A crash when hitting a telephone pole is fixed.** It could happen once the last car to break something had gone, such as an AI car that drove out of range, or after a car change. A pole's slowdown now only ever goes to your own car, never an AI car.
- **With the falling poles switched off**, the concrete pylons are still solid, broken trees away from the orchards still throw pieces you can see, and loose fences still keep their own look.
- **The zip has FH2 Mod Platform 2.8.0**, as both `d3dcompiler_47.dll` and `DWrite.dll`, so it works on Azimuth 1.0.3. If another mod's zip puts a platform older than 2.2.0 back, the title screen now says to reinstall WreckingBall, instead of the mod quietly not starting.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth (from Azimuth 1.0.3, the platform is the `DWrite.dll` in this zip). Download the latest zip from the [Releases](../../releases) page, close Azimuth, and extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `d3dcompiler_47.dll` and `DWrite.dll` next to `azimuth.exe`, and `WreckingBall.dll` inside a `fh2-mods` folder. Start the game and drive into a tree.

WreckingBall doesn't need Forza Horizon 2 Extra Options, but it works fine alongside it.

### Updates

When a newer WreckingBall is out, the title screen says so under "PRESS (A) TO START". The mod asks GitHub at most every six hours. The version you have is in the first line of `fh2-mods\WreckingBall.log` and in the DLL's file properties.

### Using it with other FH2 mods

`d3dcompiler_47.dll` and `DWrite.dll` are the same file: the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), the shared base for FH2 mods. Azimuth 1.0.3 loads `DWrite.dll`, older versions `d3dcompiler_47.dll`, and where both load only one of them runs. It loads every mod in the `fh2-mods` folder and lets them hook the game without stepping on each other. Each mod's zip ships the platform it was built with, and older ones only have `d3dcompiler_47.dll`. So extract WreckingBall last, or extract it again after installing a mod with an older platform. [Achievements](https://github.com/Trinculo54/fh2-achievements), [WheresMyBuddy](https://github.com/Trinculo54/WheresMyBuddy) and [FH2 Universal Radio](https://github.com/Trinculo54/FH2-Universal-Radio) are built on it too.

WreckingBall's zip has platform 2.8.0, and WreckingBall runs on 2.2.0 or later. The MODS page in the game comes with platform 2.3.0, its sliders with 2.4.0, the pause menu's MODS tab with 2.5.0, and loose fences keeping their own look with 2.6.0; on an older platform everything else works, those settings are in the ini only, and fence pieces look like the breakable fence they borrow. If another mod's zip puts an older platform back, the title screen says "Old FH2 Mod Platform: reinstall WreckingBall" under "PRESS (A) TO START". With a platform older than 2.2.0, WreckingBall doesn't start at all, and its log says it "needs FH2 Mod Platform 2.2.0 or newer". Either way, extract this zip again.

**Mods that hook the same game code.** The platform lets several mods hook the same place in the game, and runs their hooks one after the other. NitroSystem (FnFNitro), for one, hooks the world update that WreckingBall's falling poles, logs and debris clean-up run from. Before WreckingBall changes anything, it checks that the game's code is the build it was made for, and from 1.3.3 it reads code another mod has already hooked from the game's exe on disk. So it runs alongside such mods instead of switching those features off, and `platform.log` lists the hook as "shared with" both. If the platform won't allow one of WreckingBall's hooks (because another mod changed those bytes without going through the platform, say), `platform.log` has a "can't hook" line for it that says why. For the falling poles, logs, debris clean-up, solid pylons, the size of logs and poles, tree pieces away from the orchards, fences' own look or a skill's timing, only that feature goes off, and `WreckingBall.log` says which, with "the platform didn't allow the hook". The breakable trees, fences and clutter need all of their hooks, though: if one of those is refused, WreckingBall changes nothing, and its log says "hooking failed".

**Playing with a racing wheel.** [fh2wheel](https://github.com/iamAzoDx/fh2wheel) ships its own `D3DCOMPILER_47.dll`, and only one can sit next to `azimuth.exe`. Install fh2wheel first, then extract WreckingBall's zip and let its `d3dcompiler_47.dll` overwrite fh2wheel's. Keep `fh2wheel.dll` and `fh2wheel-config.exe` next to `azimuth.exe`: the platform (2.6.1 or later) loads fh2wheel itself, and it's listed on the MODS tab. Azimuth 1.0.3 doesn't load any `d3dcompiler_47.dll`, so fh2wheel's own loader doesn't run there, but the platform's `DWrite.dll` still loads `fh2wheel.dll`.

### On Linux (Wine or Proton)

There's nothing Linux-specific about WreckingBall: a player's log has shown it loading and hooking the game just as on Windows. On Azimuth 1.0.3 the platform is `DWrite.dll`, and Wine has its own copy of that: if `fh2-mods\platform.log` never appears, add `dwrite` to the DLL override, `WINEDLLOVERRIDES="winmm,dwrite=n,b"`. Getting FH2 mods running on Linux in general is covered in the platform's [Linux notes](https://github.com/Trinculo54/FH2-Mod-Platform/blob/main/docs/linux.md).

## Settings

The main switches are in the game, under Pause > Settings > MODS (see above). Everything, those included, is in `fh2-mods\WreckingBall.ini`, created the first time you launch with comments next to everything. Changes made in the file apply the next time you start the game, and deleting it gets you the defaults back.

| Setting | Default | What it does |
|---|---|---|
| `InjectTrees` | `1` | `0` keeps trees and bushes solid. |
| `SpeedLoss` | `0.12` | Share of your speed a normal-sized tree takes. Bigger trees take more. |
| `SpeedLossMax` | `0.45` | The most a single tree can take. |
| `LumberjackMinSize` | `1.6` | Trees at least this big (1 = a normal tree) count as log piles for Lumberjack and throw splinters. |
| `BigTreeEffects` | `Break_WoodDust,Break_Wood` | Extra break effects for big trees, from the game's own list. Empty = none. |
| `BushModels` | `Bush_,Crop_` | Models whose name contains one of these flatten like the game's own bushes: a small puff of leaves, no log, no Lumberjack. Empty = they break like trees, as in 1.1. |
| `BushSpeedLoss` | `0.04` | Share of your speed a bush takes. |
| `SkipModels` | (empty) | Models whose name contains one of these stay solid. With `Debug=1` the log lists every tree and bush model as it loads, with how it was sorted. |
| `Logs` | `1` | `0` turns the logs off. |
| `LogMinSize` | `0.4` | Trees smaller than this don't throw a log. Bushes never do. |
| `LogLength`, `LogThickness` | `3.0`, `4.5` | Log size, as multiples of the trunk model's own size. Bigger trees throw longer logs, between `LogLengthMin` and `LogLengthMax`. |
| `LogFromCar` | `1` | A log appears standing just ahead of the car, for the nose to knock over. `0` throws it from the tree instead, as before 1.3.0. |
| `LogCarAhead`, `LogCarHeight`, `LogCarTilt` | `3.0`, `0.1`, `8` | Where that log appears: metres ahead of the car's centre, its base's clearance over the ground, and how far it leans forward (degrees). |
| `LogOwnBody` | `1` | A log's collision is a box the size of the log as it's drawn. `0` gives every log the same pole-shaped body, as before 1.3.0. |
| `LogModelScale` | `948:0.85/0.4` | Size correction for the olive trunk that stands in for the apple trunk away from the orchards (see below). |
| `Barriers` | `1` | `0` keeps fences and railings solid. |
| `Clutter` | `1` | `0` keeps roadside clutter as the game had it. |
| `BarrierExclude` | `Crowd,FEST_,...` | Anything whose model name contains one of these stays solid. |
| `BarrierSkipEvents` | `1` | Barriers that only appear for an event stay solid. |
| `BarrierNeedGround` | `1` | A barrier only breaks where there's ground on both sides of it. `0` lets everything break, drops and all. |
| `BarrierSpeedLoss` | `0.04` | Share of your speed a fence or railing takes. |
| `OwnModels` | `1` | A loose fence or piece of clutter is drawn with its own model, so it keeps its paint. `0` draws it as the breakable it borrows, as before 1.3.0. Needs platform 2.6.0. |
| `Poles` | `1` | `0` keeps telephone poles solid. |
| `PoleSpeedLoss` | `0.15` | Share of your speed a telephone pole takes. |
| `PoleOwnModel` | `1` | The falling pole is the pole's own model, tipping over from where it stood. `0` drops a stretched tree trunk instead, as before 1.3.0. |
| `PoleOwnBody` | `1` | That pole's collision is a box the length of the pole. `0` uses a street light's pole piece instead. |
| `SolidPylons` | `1` | `0` leaves the concrete pylons without collision, as the game has them. |
| `SolidModels` | `8616` | Models made solid (8616 = the concrete pylons). |
| `DebrisCleanup` | `1` | `0` leaves broken pieces until the game clears them. |
| `DebrisLifetime`, `DebrisDistance` | `45`, `120` | Broken pieces fade out after this many seconds, or once you're this many metres away. `0` turns either limit off. |
| `CheckUpdates` | `1` | `0` stops the update check. |
| `Debug` | `0` | `1` writes a detailed log. F8 then adds a snapshot of the collision around the car, and the game's breakable definitions go to `WreckingBall-defs.txt`. Turn it on when reporting a problem. |

The Wreckage, Lumberjack and Sideswipe skills award at most once every 1, 2.5 and 5 seconds respectively (`WreckageRetrigger`, `LumberjackRetrigger`, `SideswipeRetrigger`), so plowing through a forest doesn't flood the skill chain. These change the skills for everything you smash, not just trees, and `-1` leaves the game's own values. More settings, mostly the numbers behind the tree sizes, log throwing and pole physics, are read from the ini too. They're listed with their defaults at the top of `load_settings()` in [`src/wreckingball.cpp`](src/wreckingball.cpp).

## Uninstall

Delete `fh2-mods\WreckingBall.dll`, `fh2-mods\WreckingBall.ini` and `fh2-mods\WreckingBall.log` (and `WreckingBall-defs.txt`, if `Debug=1` made one). The next time the game starts, the platform takes the MODS tile back out (unless another mod has switches there). If you don't use any other FH2 mods, start the game once more after that, then delete `d3dcompiler_47.dll`, `DWrite.dll` and the `fh2-mods` folder too. Nothing else was changed.

## Known limitations

- **Telephone wires:** when a pole falls, its wires stay hanging. The game draws a whole run of wire as one object, so a single span can't be taken down.
- **Log and branch models:** the game only loads a model's 3D data near where it uses it, and its breakable apple tree only appears near the orchards. Elsewhere, broken trees use the olive tree's pieces and logs use the olive trunk, sized to match. In the rare spot where neither is loaded, the pieces can't be drawn.
- **Game version:** WreckingBall is written for the 24 November 2015 build of Forza Horizon 2 that Azimuth runs. On anything else it checks the game code first and turns itself off instead of patching the wrong place. Code that another mod has already hooked through the platform is checked against the game's exe on disk instead, so the two mods share it.

## How it works

FH2 places its trees, fences and clutter as "procedurals", big batches of instances read from the track data, and only the ones listed in a procedural's smashable groups can break. When the game loads a procedural, WreckingBall adds groups of its own:
- **trees:** the game's breakable apple tree definitions;
- **bushes and crops:** the game's flattening bush (`GEN_OBJ_Bush_Flatten`);
- **fences and railings:** whichever breakable fence the game already has that matches the model's name;
- **clutter:** the breakable version of the same object from elsewhere in the game.

The game's own smashable code then handles everything from there: the hit, the pieces, the sounds and the skills. Trees and fences also keep a solid collision mesh in the track's physics data, so the mod filters those triangles out of the game's collision queries around every breakable trunk and fence. The ground under and around them is always left alone. Before letting a fence go, it looks for ground on both sides of it; if it can't find any, the fence keeps its collision.

A fence that breaks as another fence's definition would come off looking like that fence. So, with platform 2.6.0, an instruction hook in the game's break code swaps in the model the procedural itself draws, found through the procedural's own data, whenever that model is loaded.

Logs are loose pieces spawned through the game's own object manager, drawn with the apple tree's trunk and stretched to the right size by adjusting their render matrix every frame. Telephone poles aren't breakable objects in the game at all, just placed models. So the mod hides a pole's model when your car reaches it, removes its collision, and drops the pole's own model, standing where it stood, as a falling piece in its place. A breakable's collision is a short list of simple shapes in its definition, which the game reads every time it makes a body, so logs and poles get definitions of their own: a box the size of the log or pole as it's drawn. The concrete pylons get a box of collision triangles added to the game's collision queries instead.

Broken pieces normally only disappear once you're about 150 m from where they broke. WreckingBall starts the game's own two-second fade earlier, by age or by distance. Before a tree breaks, it also checks that the pieces' models are actually loaded nearby, and switches to a tree that is if they aren't.

## Build from source

The mod is built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake, on top of the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), which is a git submodule in `platform/`. Clone with `--recursive`, or run `git submodule update --init`; the build script does it for you if it's missing. On Linux:

```bash
./scripts/build.sh
```

`dist/` then holds the platform as `d3dcompiler_47.dll` and `DWrite.dll` (the same file), `fh2-mods/WreckingBall.dll`, a `README.txt` and the licences, ready to zip up. GitHub Actions does the same on every push, checks that the platform is there under both names, and uploads the result as a build artifact. Pushing a tag like `v1.0.0` also creates a draft release with the zip attached, ready to be published. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`. Bump it before tagging: the tag build refuses to run if the two don't match, and it's what the mod compares against the latest GitHub release.

## Troubleshooting

`fh2-mods\WreckingBall.log` starts fresh every launch. A working launch ends with a line like `on: trees yes, logs yes, fences and railings yes, ...`, and every change made on the MODS page is logged. The FH2 Mod Platform writes `fh2-mods\platform.log`, which shows whether the mod was loaded at all and lists every hook it put in, and which of them it shares with another mod. If the game crashes, the platform saves a report in `fh2-mods\crashes` that says which mod, if any, was involved.

| Symptom | Fix |
|---|---|
| Nothing breaks, and there's no `WreckingBall.log` or it wasn't written at this launch (check its time) | The platform isn't loading WreckingBall. Make sure `d3dcompiler_47.dll` and `DWrite.dll` are both next to `azimuth.exe` (Azimuth 1.0.3 only loads `DWrite.dll`) and `WreckingBall.dll` is inside `fh2-mods`, then check `fh2-mods\platform.log`: its first line says which platform version is running. If `fh2-mods\platform-standby.log` is from your last launch, an older platform from another mod's zip ran instead of this one: extract this zip again. (That file stays once it's written, so check its date.) |
| `WreckingBall.log` says it "needs FH2 Mod Platform 2.2.0 or newer", or the title screen says "Old FH2 Mod Platform: reinstall WreckingBall" | Another mod's zip put an older platform back. Extract this zip again. |
| The log says the game code doesn't match | If it's "game code doesn't look like the 24 Nov 2015 build; nothing hooked", you're not on the game build Azimuth uses, and the mod turns itself off rather than patch the wrong place. If the line names one feature ("solid pylons: game code doesn't match, off", say), only that feature is off. For telephone poles, see the next row. |
| Telephone poles don't fall | Only wooden telephone poles fall: metal, railway and concrete pylons stay solid on purpose. Check that "Falling telephone poles" is On (on the MODS tab, or `Poles=1` in the ini), then look in `WreckingBall.log` for a line starting "world update:", which says why the poles are off. "Game code doesn't match" there means the game isn't the build WreckingBall was made for. On 1.3.0 it was usually another mod, such as NitroSystem, that had hooked the same code first; 1.3.3 shares it, so update. "The platform didn't allow the hook" means another mod changed that code in a way the two can't share, and the "can't hook" line in `platform.log` says how. |
| An invisible wall where something used to be, or something solid you think should break | Set `Debug=1`, drive up to it, press F8, and open an issue with `WreckingBall.log`. |
| You fell through the ground near a fence | Make sure `BarrierNeedGround=1`, then report it the same way, with F8 at the spot. |
| Things break, but WreckingBall's switches aren't on the MODS tab or in Pause > Settings > MODS | Those come from files the platform gives the game through the `mediaoverride` folder. Platform 2.7.3 and newer make the game read that folder when Extra Options isn't loaded; with Extra Options, its `EnableMediaOverrides=1` in `FH2_ExtraOptions.ini` does it, so keep that on. The second line of `FH2_ExtraOptions.log` should start with `OK`. If it starts with `REFUSED` (for example "unsupported Azimuth build") or `DISABLED`, Extra Options changed nothing, so nothing reads the folder: install the Extra Options release made for your Azimuth version, or remove its `winmm.dll` so the platform reads the folder itself. The settings in `WreckingBall.ini` work either way. |

## License

Released under the [GNU General Public License v3.0](LICENSE). The FH2 Mod Platform includes [MinHook](https://github.com/TsudaKageyu/minhook), whose license ships in the zip as `LICENSE-MinHook.txt`.

## Disclaimer

Unofficial fan-made mod. Not affiliated with, endorsed by, or connected to Playground Games, Turn 10 Studios, Xbox Game Studios, Microsoft, or the Azimuth developers. Forza Horizon is a trademark of Microsoft. Use at your own risk.
