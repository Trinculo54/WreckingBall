<h1 align="center" id="title">WreckingBall</h1>

A mod for **Forza Horizon 2** on PC that lets you drive through the scenery the way later Horizon games do. In the original game most trees are solid walls, every fence and railing outside a festival is an invisible barrier, and a wooden telephone pole will stop a hypercar dead. WreckingBall makes all of those break: trees snap and throw a log, fences and railings burst apart, roadside clutter goes flying and telephone poles fall over. Festival fences and the barriers in front of crowds stay exactly as solid as before.

It works on the mainland and on Storm Island. It's a single DLL, loaded by [FH2ML](https://github.com/Refilewski/FH2ML). The game's files are never touched, so removing it puts everything back the way it was.

## Features

**Trees and bushes.** Every tree and bush in the open world breaks when you hit it, using the game's own smashable tree physics, so it sounds and looks like the handful of breakable trees the game already had. Bigger trees slow you down more than small ones, and the biggest count as log piles, so they give you Lumberjack skill points. They also throw wood splinters and dust on top of the usual burst of leaves. Bushes, hedges and crops flatten like the game's own breakable bushes instead: a small puff of leaves, with no log and only a light slowdown.

**Logs.** Every broken tree throws a log, sized to the tree it came from. It pops up just ahead of your car, leaning away from you, for the nose to knock over, and its collision is the size of the log you see, so it rolls and settles where it looks like it should.

**Fences, railings and roadside clutter.** Wooden fences, wire fences, wicker and tarp fencing, metal railings, guardrails and bollards break apart when you hit them, and the pieces keep the look of the fence they came from, paint and all. So does roadside clutter that has a breakable version somewhere else in the game, like signs, bins and lamp posts. A few things always stay solid:
- the fences and barriers of the Festival and its events;
- barriers that only appear for a race, including those in front of the crowds;
- walls, bridges, concrete barriers and gates;
- any barrier with nothing but a drop behind it, so you can't fly off the edge of a mountain road.

**Telephone poles.** The wooden telephone poles along the roads fall over when you hit them: the pole itself tips over from where it stood and rolls on its full length, with collision the size of the pole. The big square concrete pylons work the other way round: the game forgot to give them any collision at all, so you could drive straight through them. WreckingBall makes them solid.

**Debris clean-up.** Broken pieces fade out after 45 seconds, or once you're 120 m away, so the world doesn't fill up with wreckage.

**Switches in the game.** The pause menu's MODS tab (Installed mods > WreckingBall), and Pause > Settings > MODS, have WreckingBall's main switches: breakable trees, logs, fences and railings, roadside clutter, falling telephone poles, solid concrete pylons and debris clean-up. Logs, poles, pylons and clean-up change straight away. Trees, fences and clutter are added to the world as each area loads, so they take a restart, and the game offers to restart for you when you leave the page. Under the switches are sliders for how much each thing slows you down: trees, the biggest trees, how much more big trees take than small ones, bushes, fences and telephone poles. They apply as you move them. Every change is saved to `WreckingBall.ini`.

## Install

You'll need Forza Horizon 2 running on PC through Azimuth, with [FH2ML](https://github.com/Refilewski/FH2ML), the Forza Horizon 2 mod loader, and Forza Horizon 2 Extra Options installed. Download the latest zip from the [Releases](../../releases) page and close Azimuth. Then either:

- open the FH2ML Manager, choose **Install mod** and pick the zip; or
- extract the zip into your Azimuth folder, the one with `azimuth.exe` in it. You should end up with `mod.json` and `WreckingBall.dll` in `mods\wreckingball`.

Start the game and drive into a tree.

**Upgrading from 1.3 or older.** Those versions came with a mod loader of their own. When the FH2ML Manager offers to remove it, let it, so the old WreckingBall isn't loaded as well as the new one. Your `WreckingBall.ini` is copied over to its new place (see [Settings](#settings)) the first time you start the game with this version; nothing is copied after that.

### Updates

When a newer WreckingBall is out, the title screen says so next to "PRESS (A) TO START", and the MODS tab marks it. FH2ML asks GitHub at most every six hours. To turn that off, add `"updateChecks": false` to `wreckingball`'s entry under `"mods"` in `FH2ML\config\FH2ML.json` (or set it at the top level for every mod). The version you have is in the first line of `FH2ML\data\wreckingball\WreckingBall.log`, in `mods\wreckingball\mod.json` and in the DLL's file properties.

### Using it with other FH2 mods

[FH2ML](https://github.com/Refilewski/FH2ML) loads every mod in the `mods` folder and gives them shared services: hooks that let several mods change the same game code without stepping on each other, the MODS page and tab in the game, update checks and crash reports. Install, enable, disable and order mods with the FH2ML Manager.

**Playing with a racing wheel.** WreckingBall no longer puts anything next to `azimuth.exe`, so it doesn't get in the way of [fh2wheel](https://github.com/iamAzoDx/fh2wheel). If you installed fh2wheel alongside WreckingBall 1.3 or older, removing the old mod loader removes the file fh2wheel needs too: install fh2wheel again as its own instructions say.

## Settings

The main switches are in the game, under Pause > Settings > MODS (see above). Everything, those included, is in `WreckingBall.ini`, in the Azimuth folder's `FH2ML\data\wreckingball`, created the first time you launch with comments next to everything. It's kept when you update or remove the mod. Changes made in the file apply the next time you start the game, and deleting it gets you the defaults back.

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
| `OwnModels` | `1` | A loose fence or piece of clutter is drawn with its own model, so it keeps its paint. `0` draws it as the breakable it borrows, as before 1.3.0. |
| `Poles` | `1` | `0` keeps telephone poles solid. |
| `PoleSpeedLoss` | `0.15` | Share of your speed a telephone pole takes. |
| `PoleOwnModel` | `1` | The falling pole is the pole's own model, tipping over from where it stood. `0` drops a stretched tree trunk instead, as before 1.3.0. |
| `PoleOwnBody` | `1` | That pole's collision is a box the length of the pole. `0` uses a street light's pole piece instead. |
| `SolidPylons` | `1` | `0` leaves the concrete pylons without collision, as the game has them. |
| `SolidModels` | `8616` | Models made solid (8616 = the concrete pylons). |
| `DebrisCleanup` | `1` | `0` leaves broken pieces until the game clears them. |
| `DebrisLifetime`, `DebrisDistance` | `45`, `120` | Broken pieces fade out after this many seconds, or once you're this many metres away. `0` turns either limit off. |
| `Debug` | `0` | `1` writes a detailed log. F8 then adds a snapshot of the collision around the car, and the game's breakable definitions go to `WreckingBall-defs.txt`, next to the ini. Turn it on when reporting a problem. |

The Wreckage, Lumberjack and Sideswipe skills award at most once every 1, 2.5 and 5 seconds respectively (`WreckageRetrigger`, `LumberjackRetrigger`, `SideswipeRetrigger`), so plowing through a forest doesn't flood the skill chain. These change the skills for everything you smash, not just trees, and `-1` leaves the game's own values. More settings, mostly the numbers behind the tree sizes, log throwing and pole physics, are read from the ini too. They're listed with their defaults at the top of `load_settings()` in [`src/wreckingball.cpp`](src/wreckingball.cpp).

## Uninstall

Remove WreckingBall in the FH2ML Manager, or delete the `mods\wreckingball` folder. Your settings stay in `FH2ML\data\wreckingball`; delete that folder too if you don't want them any more. Nothing else was changed.

## Known limitations

- **Telephone wires:** when a pole falls, its wires stay hanging. The game draws a whole run of wire as one object, so a single span can't be taken down.
- **Log and branch models:** the game only loads a model's 3D data near where it uses it, and its breakable apple tree only appears near the orchards. Elsewhere, broken trees use the olive tree's pieces and logs use the olive trunk, sized to match. In the rare spot where neither is loaded, the pieces can't be drawn.
- **Game version:** WreckingBall is written for the 24 November 2015 build of Forza Horizon 2 that Azimuth runs. On anything else it checks the game code first and turns itself off instead of patching the wrong place.

## How it works

FH2 places its trees, fences and clutter as "procedurals", big batches of instances read from the track data, and only the ones listed in a procedural's smashable groups can break. When the game loads a procedural, WreckingBall adds groups of its own:
- **trees:** the game's breakable apple tree definitions;
- **bushes and crops:** the game's flattening bush (`GEN_OBJ_Bush_Flatten`);
- **fences and railings:** whichever breakable fence the game already has that matches the model's name;
- **clutter:** the breakable version of the same object from elsewhere in the game.

The game's own smashable code then handles everything from there: the hit, the pieces, the sounds and the skills. Trees and fences also keep a solid collision mesh in the track's physics data, so the mod filters those triangles out of the game's collision queries around every breakable trunk and fence. The ground under and around them is always left alone. Before letting a fence go, it looks for ground on both sides of it; if it can't find any, the fence keeps its collision.

A fence that breaks as another fence's definition would come off looking like that fence. So an instruction hook in the game's break code swaps in the model the procedural itself draws, found through the procedural's own data, whenever that model is loaded.

Logs are loose pieces spawned through the game's own object manager, drawn with the apple tree's trunk and stretched to the right size by adjusting their render matrix every frame. Telephone poles aren't breakable objects in the game at all, just placed models. So the mod hides a pole's model when your car reaches it, removes its collision, and drops the pole's own model, standing where it stood, as a falling piece in its place. A breakable's collision is a short list of simple shapes in its definition, which the game reads every time it makes a body, so logs and poles get definitions of their own: a box the size of the log or pole as it's drawn. The concrete pylons get a box of collision triangles added to the game's collision queries instead.

Broken pieces normally only disappear once you're about 150 m from where they broke. WreckingBall starts the game's own two-second fade earlier, by age or by distance. Before a tree breaks, it also checks that the pieces' models are actually loaded nearby, and switches to a tree that is if they aren't.

## Build from source

The mod is built with [llvm-mingw](https://github.com/mstorsjo/llvm-mingw/releases) and CMake, as an FH2ML v4 mod. The one header it needs from FH2ML, `FH2ML_v4.h`, is in `third_party/fh2ml` with its MIT license. On Linux:

```bash
./scripts/build.sh
```

`dist/` then holds `mods/wreckingball/` with `mod.json`, `WreckingBall.dll`, `README.txt` and `LICENSE.txt`, ready to zip up: the zip extracts straight into the Azimuth folder, and the FH2ML Manager installs it as it is. GitHub Actions does the same on every push and uploads the result as a build artifact. Pushing a tag like `v1.0.0` also creates a draft release with the zip attached, ready to be published. The version lives in `project(... VERSION x.y.z)` in `CMakeLists.txt`; the mod reports it to FH2ML, and `mod.json` is generated from it (`cmake/mod.json.in`), so the two always match. Bump it before tagging: the tag build refuses to run if the two don't match, and it's what FH2ML compares against the latest GitHub release.

## Troubleshooting

`FH2ML\data\wreckingball\WreckingBall.log` starts fresh every launch. A working launch ends with a line like `on: trees yes, logs yes, fences and railings yes, ...`, and every change made on the MODS page is logged. FH2ML writes `FH2ML\logs\FH2ML.log`, which shows whether the mod was loaded at all (`wreckingball: lifecycle v4 initialized`), how many hooks it put in, and why if one was refused. If the game crashes, FH2ML saves a report in `FH2ML\logs\crashes` that says which mod, if any, was involved.

| Symptom | Fix |
|---|---|
| Nothing breaks and there's no `WreckingBall.log` | Make sure FH2ML is installed, `mods\wreckingball` holds `mod.json` and `WreckingBall.dll`, and WreckingBall is enabled in the FH2ML Manager, then check `FH2ML\logs\FH2ML.log`. |
| The log says the game code doesn't match | You're not on the game build Azimuth uses. The mod turns itself off rather than patch the wrong place. |
| An invisible wall where something used to be, or something solid you think should break | Set `Debug=1`, drive up to it, press F8, and open an issue with `WreckingBall.log`. |
| You fell through the ground near a fence | Make sure `BarrierNeedGround=1`, then report it the same way, with F8 at the spot. |

## License

Released under the [GNU General Public License v3.0](LICENSE). `third_party/fh2ml/FH2ML_v4.h` is FH2ML's, under the MIT license in [`third_party/fh2ml/LICENSE`](third_party/fh2ml/LICENSE).

## Disclaimer

Unofficial fan-made mod. Not affiliated with, endorsed by, or connected to Playground Games, Turn 10 Studios, Xbox Game Studios, Microsoft, or the Azimuth developers. Forza Horizon is a trademark of Microsoft. Use at your own risk.
