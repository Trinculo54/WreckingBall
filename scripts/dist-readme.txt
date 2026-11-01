WreckingBall
============

Breakable trees, fences, railings, roadside clutter and telephone poles
for Forza Horizon 2 (Azimuth). Trees snap and throw a log, fences and
railings burst apart, poles fall over. Festival fences and the barriers in
front of crowds stay solid. The game's files are never touched.

Project page and source: https://github.com/Trinculo54/WreckingBall


Requirements
~~~~~~~~~~~~

  - Forza Horizon 2 running through Azimuth (from Azimuth 1.0.3, the
    platform is the DWrite.dll in this zip).


Install
~~~~~~~

1. Close Azimuth.
2. Extract this zip into your Azimuth folder (the one with azimuth.exe).
   You should end up with:
       d3dcompiler_47.dll            next to azimuth.exe
       DWrite.dll                    next to azimuth.exe
       fh2-mods\WreckingBall.dll
3. Start the game and drive into a tree.

d3dcompiler_47.dll and DWrite.dll are the same file: the FH2 Mod
Platform, the shared base for FH2 mods
(https://github.com/Trinculo54/FH2-Mod-Platform). Azimuth 1.0.3 loads
DWrite.dll, older versions d3dcompiler_47.dll. It loads every mod in
fh2-mods. Each mod's zip ships the platform it was built with, and older
ones only have d3dcompiler_47.dll, so extract this zip last, or extract
it again after installing a mod with an older platform.

This zip has platform 2.8.0, and WreckingBall runs on 2.2.0 or newer.
The switches in the game need 2.3.0 (the sliders 2.4.0, the MODS tab
2.5.0), and loose fences keep their own look from 2.6.0. If the title
screen says "Old FH2 Mod Platform: reinstall WreckingBall", another
mod's zip put an older platform back. With a platform older than 2.2.0,
WreckingBall doesn't start at all, and its log says it "needs FH2 Mod
Platform 2.2.0 or newer". Either way, extract this zip again.

With a racing wheel (fh2wheel): install fh2wheel first, then extract
this zip and let its d3dcompiler_47.dll overwrite fh2wheel's. Keep
fh2wheel.dll and fh2wheel-config.exe next to azimuth.exe: the platform
loads fh2wheel.dll itself, on Azimuth 1.0.3 too.


In the game
~~~~~~~~~~~

The pause menu's MODS tab (Installed mods > WreckingBall) and
Pause > Settings > MODS have WreckingBall's main switches: breakable trees,
logs, fences and railings, roadside clutter, falling telephone poles, solid
concrete pylons and debris clean-up. Logs, poles, pylons and clean-up
switch straight away. Trees, fences and clutter take a restart, and the
game offers to restart for you when you leave the page. Under the switches
are sliders for how much trees, bushes, fences and telephone poles slow you
down; they apply as you move them. Changes are saved to WreckingBall.ini.


Settings (fh2-mods\WreckingBall.ini)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Created on first launch, with a comment next to each setting. Changes
made here apply the next time you start the game; delete the file to get
the defaults back.

  InjectTrees=1        0 = trees and bushes stay solid
  SpeedLoss=0.12       share of your speed a normal tree takes
  Logs=1               0 = no logs from broken trees
  Barriers=1           0 = fences and railings stay solid
  Clutter=1            0 = roadside clutter as the game had it
  Poles=1              0 = telephone poles stay solid
  SolidPylons=1        0 = concrete pylons left without collision
  DebrisCleanup=1      0 = broken pieces stay until the game clears them
  DebrisLifetime=45    seconds before broken pieces fade out (0 = off)
  DebrisDistance=120   metres away before they fade out (0 = off)
  CheckUpdates=1       0 = don't check GitHub for a newer version
  Debug=0              1 = detailed log, F8 snapshot (for bug reports)

When a newer version is out, the title screen says so under
"PRESS (A) TO START".


Troubleshooting
~~~~~~~~~~~~~~~

fh2-mods\WreckingBall.log is written fresh each launch. A working launch
ends with "on: trees yes, logs yes, ...". To report an invisible wall or
something that should break, set Debug=1, drive up to it, press F8 and
attach WreckingBall.log.

  - Nothing breaks, and there's no WreckingBall.log or it wasn't written
    at this launch (check its time): the platform isn't loading
    WreckingBall. Check that d3dcompiler_47.dll and DWrite.dll are both
    next to azimuth.exe (Azimuth 1.0.3 only loads DWrite.dll), and
    WreckingBall.dll is in fh2-mods. The first line of
    fh2-mods\platform.log says which platform version is running. If
    fh2-mods\platform-standby.log is from your last launch, an older
    platform from another mod's zip ran instead of this one: extract
    this zip again. (That file stays once it's written, so check its
    date.)
  - WreckingBall.log says "needs FH2 Mod Platform 2.2.0 or newer", or
    the title screen says "Old FH2 Mod Platform: reinstall
    WreckingBall": another mod's zip put an older platform back.
    Extract this zip again.
  - Things break, but WreckingBall's switches aren't on the MODS tab or
    in Pause > Settings > MODS: these come through the mediaoverride
    folder. With Extra Options installed, keep EnableMediaOverrides=1 in
    FH2_ExtraOptions.ini, and check that the second line of
    FH2_ExtraOptions.log starts with OK. If it starts with REFUSED (for
    example "unsupported Azimuth build") or DISABLED, Extra Options
    changed nothing, so nothing reads mediaoverride: install the Extra
    Options release made for your Azimuth version, or remove its
    winmm.dll so the platform reads the folder itself. WreckingBall.ini
    works either way.
  - On Linux (Wine or Proton) with Azimuth 1.0.3, if
    fh2-mods\platform.log never appears, add dwrite to the DLL override:
    WINEDLLOVERRIDES="winmm,dwrite=n,b"


Uninstall
~~~~~~~~~

Delete fh2-mods\WreckingBall.dll, fh2-mods\WreckingBall.ini and
fh2-mods\WreckingBall.log (and WreckingBall-defs.txt, if Debug=1 made
one). Delete d3dcompiler_47.dll, DWrite.dll and fh2-mods too if no other
FH2 mods use them.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
