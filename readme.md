aimeio-pcsc
---

PC/SC-based Aime card reader for `segatools`. `aimeio-pcsc` allows you to use PC/SC compliant smart card readers as your Aime card reader in `segatools`. `aimeio-pcsc` only properly supports the old Mifare Classic 1K-based Aime cards.

If you scan a newer AIC-based AIME, its FeliCa IDm will be provided to the game. The game will not see the correct "access code," but the IDm should be unique to each card so that particular card can still track your plays. 

Tested on SONY's PaSoRi RC-S300. Other readers should, in theory, also work.

### Usage

To test if your card reader is supported, run `aimereader.exe` and try read your Aime card.

To use it with a game, copy `aimeio.dll` to your `segatools` folder and add the following to your `segatools.ini`:

```ini
[aimeio]
path=aimeio.dll
```

### Build

On Linux:

```sh
meson setup --cross cross-mingw-64.txt b64
ninja -C b64
```