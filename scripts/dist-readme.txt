WreckingBall
============

Breakable trees, fences, railings, roadside clutter and telephone poles
for Forza Horizon 2 (Azimuth). Trees snap and throw a log, fences and
railings burst apart, poles fall over. Festival fences and the barriers in
front of crowds stay solid. The game's files are never touched.

Project page and source: https://github.com/Trinculo54/WreckingBall


Requirements
~~~~~~~~~~~~

  - Forza Horizon 2 running through Azimuth.


Install
~~~~~~~

1. Close Azimuth.
2. Extract this zip into your Azimuth folder (the one with azimuth.exe).
   You should end up with:
       d3dcompiler_47.dll            next to azimuth.exe
       fh2-mods\WreckingBall.dll
3. Start the game and drive into a tree.

d3dcompiler_47.dll is the FH2 Mod Platform, the shared base for FH2 mods
(https://github.com/Trinculo54/FH2-Mod-Platform). It loads every mod in
fh2-mods, and every mod built on it ships the same file, so it's fine to
overwrite.


In the game
~~~~~~~~~~~

Pause > Settings > MODS has WreckingBall's main switches: breakable trees,
logs, fences and railings, roadside clutter, falling telephone poles, solid
concrete pylons and debris clean-up. Logs, poles, pylons and clean-up
switch straight away. Trees, fences and clutter take a restart, and the
game offers to restart for you when you leave the page. Changes are saved
to WreckingBall.ini.


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

When a newer version is out, the title screen says so after
"PRESS (A) TO START".


Troubleshooting
~~~~~~~~~~~~~~~

fh2-mods\WreckingBall.log is written fresh each launch. A working launch
ends with "on: trees yes, logs yes, ...". If nothing breaks and there's no
log, check fh2-mods\platform.log. To report an invisible wall or something
that should break, set Debug=1, drive up to it, press F8 and attach
WreckingBall.log.


Uninstall
~~~~~~~~~

Delete fh2-mods\WreckingBall.dll and fh2-mods\WreckingBall.ini. Delete
d3dcompiler_47.dll and fh2-mods too if no other FH2 mods use them.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
