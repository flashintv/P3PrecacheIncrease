# P3PrecacheIncrease

As the name suggests, this project increases the precaching limits within Postal III's engine. Whose values are based on Source Engine 2007, as can be found within it's leaked engine code.
This code is meant to be put alongside a .VDF file in addons folder, or injected with LoadLibrary in a custom `P3.exe` or `client.dll`.

Values are increased to:
- Generic stays the same at `512`
- Decals stay the same at `512`
- Models from `1024` to `4096`
- Sounds from `8192` to `16384`

Code works by hooking into CGameServer and CClientState Get/Set and Precache functions to point towards the new precache arrays, and by memory patching the previous values to new ones during server StringTable initialization.
