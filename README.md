<h1 align="center" id="title">WreckingBall</h1>

A mod for **Forza Horizon 2** on PC that lets you drive through the scenery the way later Horizon games do. In the original game most trees are solid walls, every fence and railing outside a festival is an invisible barrier, and a wooden telephone pole will stop a hypercar dead. WreckingBall makes all of those break: trees snap and throw a log, fences and railings burst apart, roadside clutter goes flying and telephone poles fall over. Festival fences and the barriers in front of crowds stay exactly as solid as before.

It's a single DLL. The game's files are never touched, so removing it puts everything back the way it was.

## Features

**Trees and bushes.** Every tree and bush in the open world breaks when you hit it, using the game's own smashable tree physics, so it sounds and looks like the handful of breakable trees the game already had. Bigger trees slow you down more than small ones, and the biggest count as log piles, so they give you Lumberjack skill points. They also throw wood splinters and dust on top of the usual burst of leaves.

**Logs.** Every broken tree throws a log, sized to the tree it came from, that tumbles away from the car and rolls to a stop.

**Fences, railings and roadside clutter.** Wooden fences, wire fences, wicker and tarp fencing, metal railings, guardrails and bollards break apart when you hit them. So does roadside clutter that has a breakable version somewhere else in the game, like signs, bins and lamp posts. A few things always stay solid:
- the fences and barriers of the Festival and its events;
- barriers that only appear for a race, including those in front of the crowds;
- walls, bridges, concrete barriers and gates;
- any barrier with nothing but a drop behind it, so you can't fly off the edge of a mountain road.

**Telephone poles.** The wooden telephone poles along the roads fall over when you hit them, and a pole tumbles away in their place. The big square concrete pylons work the other way round: the game forgot to give them any collision at all, so you could drive straight through them. WreckingBall makes them solid.

**Debris clean-up.** Broken pieces fade out after 45 seconds, or once you're 120 m away, so the world doesn't fill up with wreckage.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth. Download the latest zip from the [Releases](../../releases) page, close Azimuth, and extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `d3dcompiler_47.dll` next to `azimuth.exe` and `WreckingBall.dll` inside a `fh2-mods` folder. Start the game and drive into a tree.

WreckingBall doesn't need Forza Horizon 2 Extra Options, but it works fine alongside it.

### Updates

When a newer WreckingBall is out, the title screen says so next to "PRESS (A) TO START". The mod asks GitHub at most every six hours. The version you have is in the first line of `fh2-mods\WreckingBall.log` and in the DLL's file properties.

### Using it with other FH2 mods

`d3dcompiler_47.dll` is the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), the shared base for FH2 mods. It loads every mod in the `fh2-mods` folder and lets them hook the game without stepping on each other. Every mod built on it ships the same file, so you can install them in any order and let them overwrite it. [WheresMyBuddy](https://github.com/Trinculo54/WheresMyBuddy) and [FH2 Universal Radio](https://github.com/Trinculo54/FH2-Universal-Radio) are built on it too. WreckingBall needs platform 2.2.0 or later, which is the one in its zip.

## Settings

`fh2-mods\WreckingBall.ini` is created the first time you launch, with comments next to everything. Changes apply the next time you start the game, and deleting the file gets you the defaults back.

| Setting | Default | What it does |
|---|---|---|
| `InjectTrees` | `1` | `0` keeps trees and bushes solid. |
| `SpeedLoss` | `0.12` | Share of your speed a normal-sized tree takes. Bigger trees take more. |
| `SpeedLossMax` | `0.45` | The most a single tree can take. |
| `LumberjackMinSize` | `1.6` | Trees at least this big (1 = a normal tree) count as log piles for Lumberjack and throw splinters. |
| `BigTreeEffects` | `Break_WoodDust,Break_Wood` | Extra break effects for big trees, from the game's own list. Empty = none. |
| `Logs` | `1` | `0` turns the logs off. |
| `LogMinSize` | `0.4` | Trees smaller than this don't throw a log. Bushes are about 0.5. |
| `LogLength`, `LogThickness` | `3.0`, `4.5` | Log size, as multiples of the trunk model's own size. Bigger trees throw longer logs, between `LogLengthMin` and `LogLengthMax`. |
| `LogModelScale` | `948:0.85/0.4` | Size correction for the olive trunk that stands in for the apple trunk away from the orchards (see below). |
| `Barriers` | `1` | `0` keeps fences and railings solid. |
| `Clutter` | `1` | `0` keeps roadside clutter as the game had it. |
| `BarrierExclude` | `Crowd,FEST_,...` | Anything whose model name contains one of these stays solid. |
| `BarrierSkipEvents` | `1` | Barriers that only appear for an event stay solid. |
| `BarrierNeedGround` | `1` | A barrier only breaks where there's ground on both sides of it. `0` lets everything break, drops and all. |
| `BarrierSpeedLoss` | `0.04` | Share of your speed a fence or railing takes. |
| `Poles` | `1` | `0` keeps telephone poles solid. |
| `PoleSpeedLoss` | `0.15` | Share of your speed a telephone pole takes. |
| `SolidModels` | `8616` | Models the mod makes solid (8616 = the concrete pylons). Empty = off. |
| `DebrisLifetime`, `DebrisDistance` | `45`, `120` | Broken pieces fade out after this many seconds, or once you're this many metres away. `0` turns either off. |
| `CheckUpdates` | `1` | `0` stops the update check. |
| `Debug` | `0` | `1` writes a detailed log. F8 then adds a snapshot of the collision around the car, and the game's breakable definitions go to `WreckingBall-defs.txt`. Turn it on when reporting a problem. |

The Wreckage, Lumberjack and Sideswipe skills award at most once every 1, 2.5 and 5 seconds respectively (`WreckageRetrigger`, `LumberjackRetrigger`, `SideswipeRetrigger`), so plowing through a forest doesn't flood the skill chain. These change the skills for everything you smash, not just trees, and `-1` leaves the game's own values. More settings, mostly the numbers behind the tree sizes, log throwing and pole physics, are read from the ini too. They're listed with their defaults at the top of `load_settings()` in [`src/wreckingball.cpp`](src/wreckingball.cpp).

## Uninstall

Delete `fh2-mods\WreckingBall.dll` and `fh2-mods\WreckingBall.ini`. If you don't use any other FH2 mods, delete `d3dcompiler_47.dll` and the `fh2-mods` folder too. Nothing else was changed.

## Known limitations

- **Fence debris colour:** pieces of painted fences come out in the game's plain metal colour.
- **Telephone wires:** when a pole falls, its wires stay hanging. The game draws a whole run of wire as one object, so a single span can't be taken down.
- **Log and branch models:** the game only loads a model's 3D data near where it uses it, and its breakable apple tree only appears near the orchards. Elsewhere, broken trees use the olive tree's pieces and logs use the olive trunk, sized to match. In the rare spot where neither is loaded, the pieces can't be drawn.
- **Game version:** WreckingBall is written for the 24 November 2015 build of Forza Horizon 2 that Azimuth runs. On anything else it checks the game code first and turns itself off instead of patching the wrong place.

## How it works

FH2 places its trees, fences and clutter as "procedurals", big batches of instances read from the track data, and only the ones listed in a procedural's smashable groups can break. When the game loads a procedural, WreckingBall adds groups of its own:
- **trees and bushes:** the game's breakable apple tree definitions;
- **fences and railings:** whichever breakable fence the game already has that matches the model's name;
- **clutter:** the breakable version of the same object from elsewhere in the game.

The game's own smashable code then handles everything from there: the hit, the pieces, the sounds and the skills. Trees and fences also keep a solid collision mesh in the track's physics data, so the mod filters those triangles out of the game's collision queries around every breakable trunk and fence. The ground under and around them is always left alone. Before letting a fence go, it looks for ground on both sides of it; if it can't find any, the fence keeps its collision.

Logs are loose pieces spawned through the game's own object manager, drawn with the apple tree's trunk and stretched to the right size by adjusting their render matrix every frame. Telephone poles aren't breakable objects in the game at all, just placed models. So the mod hides a pole's model when your car reaches it, removes its collision, and drops a falling pole in its place. The concrete pylons get a box of collision triangles added to the game's collision queries instead.

Broken pieces normally only disappear once you're about 150 m from where they broke. WreckingBall starts the game's own two-second fade earlier, by age or by distance. Before a tree breaks, it also checks that the pieces' models are actually loaded nearby, and switches to a tree that is if they aren't.

## Build from source

The mod is built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake, on top of the [FH2 Mod Platform](https://github.com/Trinculo54/FH2-Mod-Platform), which is a git submodule in `platform/`. Clone with `--recursive`, or run `git submodule update --init`; the build script does it for you if it's missing. On Linux:

```bash
./scripts/build.sh
```

`dist/` then holds `d3dcompiler_47.dll`, `fh2-mods/WreckingBall.dll` and a `README.txt`, ready to zip up. GitHub Actions does the same on every push and uploads the result as a build artifact. Pushing a tag like `v1.0.0` also creates a draft release with the zip attached, ready to be published. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`. Bump it before tagging: the tag build refuses to run if the two don't match, and it's what the mod compares against the latest GitHub release.

## Troubleshooting

`fh2-mods\WreckingBall.log` starts fresh every launch. A working launch ends with a line like `on: trees yes, logs yes, fences and railings yes, ...`. The FH2 Mod Platform writes `fh2-mods\platform.log`, which shows whether the mod was loaded at all and lists every hook it put in. If the game crashes, the platform saves a report in `fh2-mods\crashes` that says which mod, if any, was involved.

| Symptom | Fix |
|---|---|
| Nothing breaks and there's no `WreckingBall.log` | Make sure `d3dcompiler_47.dll` is next to `azimuth.exe` and `WreckingBall.dll` is inside `fh2-mods`, then check `fh2-mods\platform.log`. |
| The log says the game code doesn't match | You're not on the game build Azimuth uses. The mod turns itself off rather than patch the wrong place. |
| An invisible wall where something used to be, or something solid you think should break | Set `Debug=1`, drive up to it, press F8, and open an issue with `WreckingBall.log`. |
| You fell through the ground near a fence | Make sure `BarrierNeedGround=1`, then report it the same way, with F8 at the spot. |

## License

Released under the [GNU General Public License v3.0](LICENSE). The FH2 Mod Platform includes [MinHook](https://github.com/TsudaKageyu/minhook), whose license ships in the zip as `LICENSE-MinHook.txt`.

## Disclaimer

Unofficial fan-made mod. Not affiliated with, endorsed by, or connected to Playground Games, Turn 10 Studios, Xbox Game Studios, Microsoft, or the Azimuth developers. Forza Horizon is a trademark of Microsoft. Use at your own risk.
