# P3PrecacheIncrease

As the name suggests, this project increases the precaching limits within Postal III's engine. Whose values are based on Source Engine 2007, as can be found within its leaked code's headers and engine binaries. <br />
This code is meant to be put alongside a .VDF file in addons folder, or injected with slight modifications using LoadLibrary in a custom `P3.exe` or `client.dll`.

<!-- looks beautiful when formatted right. -->
----------------------------------------------
| Table       | Vanilla | P3PrecacheIncrease |
| ----------- | ------- | ------------------ |
| **Generic** | `512`   | `512`              |
| **Decals**  | `512`   | `512`              |
| **Models**  | `1024`  | `4096`             |
| **Sounds**  | `8192`  | `16384`            |
----------------------------------------------

What code does:
- *Memory patching*: 
  - changing the Vanilla precache values to new modified ones during server's StringTable initialization,
  - patching out various references to old index bits of the __Sounds__ table inside of __SVC_Prefetch__ and __SoundInfo_t__'s Write and Read functions to the new index bits.
- *Detours*:
  - Get`XXXXXX`[^1], Set`XXXXXX`[^1], and Precache`XXXXXX`[^1] functions inside of __CGameServer__ and __CClientState__ to point towards the new precache arrays.
<br />

[^1]: `XXXXXX` being the name of a specific StringTable
