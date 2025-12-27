# DirectStorageFix

DirectStorageFix prevents GTAV Enhanced from opening a handle for every single file in the game directory when DirectStorage is enabled.
Instead, only `.rpf` and `.cache` files are allowed.

If you use scripts that write to existing log files, config files, or other data files in the game directory, this fix will restore their ability to write to those files.
Additionally, script developers can also reload their scripts at run-time again. It is currently only needed for game version 1.0.1013.17 and later.