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
  - FH2ML, the Forza Horizon 2 mod loader
    (https://github.com/Refilewski/FH2ML).
  - Forza Horizon 2 Extra Options.


Install
~~~~~~~

With the FH2ML Manager: close Azimuth, open the Manager, choose Install
mod and pick this zip.

Or by hand: close Azimuth and extract this zip into your Azimuth folder
(the one with azimuth.exe). You should end up with:
    mods\wreckingball\mod.json
    mods\wreckingball\WreckingBall.dll

Then start the game and drive into a tree.

Upgrading from WreckingBall 1.3 or older: those came with a mod loader of
their own. Let the FH2ML Manager remove it when it offers to, so the old
WreckingBall isn't loaded as well. Your WreckingBall.ini is copied over
the first time you start the game with this version.


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


Settings (FH2ML\data\wreckingball\WreckingBall.ini)
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Created on first launch, with a comment next to each setting, in the
Azimuth folder's FH2ML\data\wreckingball. It's kept when you update or
remove the mod. Changes made here apply the next time you start the game;
delete the file to get the defaults back.

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
  Debug=0              1 = detailed log, F8 snapshot (for bug reports)

When a newer version is out, the title screen says so after
"PRESS (A) TO START". FH2ML checks GitHub for it; to turn that off, add
"updateChecks": false to wreckingball's entry under "mods" in
FH2ML\config\FH2ML.json.


Troubleshooting
~~~~~~~~~~~~~~~

FH2ML\data\wreckingball\WreckingBall.log is written fresh each launch. A
working launch ends with "on: trees yes, logs yes, ...". If nothing breaks
and there's no log, check FH2ML\logs\FH2ML.log: it should say
"wreckingball: lifecycle v4 initialized". To report an invisible wall or
something that should break, set Debug=1, drive up to it, press F8 and
attach WreckingBall.log.


Uninstall
~~~~~~~~~

Remove WreckingBall in the FH2ML Manager, or delete mods\wreckingball.
Your settings stay in FH2ML\data\wreckingball; delete that folder too if
you don't want them any more. Nothing else was changed.


License
~~~~~~~

GPLv3. Unofficial fan-made mod; not affiliated with Playground Games,
Turn 10, Microsoft or the Azimuth developers.
