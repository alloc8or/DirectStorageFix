# DirectStorageFix

DirectStorageFix prevents GTAV Enhanced from opening a handle for every single file in the game directory when DirectStorage is enabled,
as this would block any attempt to write to log files or config files. Instead, only `.rpf` and `.cache` files are allowed.

This fix is currently only needed for game version 1.0.1013.17 and later.