<!-- markdownlint-disable-file MD033 -->

# SpaceCadetPinball

## Resumen

Ingenieria Inversa del juego `3D Pinball for Windows - Space Cadet`, para portearlo a diferentes plataformas.

## Como Jugar

Pon el ejecutable de la plataforma que vas a utilizar junto con los archivos del juego (no incluido).\
Compatible con los datos de juego de la version de Windows y la version completa del juego.

## Ports conocidos

| Platform           | Author          | URL                                                                                                        |
| ------------------ | --------------- | ---------------------------------------------------------------------------------------------------------- |
| PS Vita            | Axiom           | <https://github.com/suicvne/SpaceCadetPinball_Vita>                                                        |
| Emscripten         | alula           | <https://github.com/alula/SpaceCadetPinball> <br> Play online: <https://alula.github.io/SpaceCadetPinball> |
| Nintendo Switch    | averne          | <https://github.com/averne/SpaceCadetPinball-NX>                                                           |
| webOS TV           | mariotaku       | <https://github.com/webosbrew/SpaceCadetPinball>                                                           |
| Android (WIP)      | Iscle           | https://github.com/Iscle/SpaceCadetPinball                                                                 |
| Nintendo Wii       | MaikelChan      | https://github.com/MaikelChan/SpaceCadetPinball                                                            |
| Nintendo 3DS       | MaikelChan      | https://github.com/MaikelChan/SpaceCadetPinball/tree/3ds                                                   |
| Nintendo Wii U     | IntriguingTiles | https://github.com/IntriguingTiles/SpaceCadetPinball-WiiU                                                  |
| MorphOS            | BeWorld         | https://www.morphos-storage.net/?id=1688897                                                                |
| AmigaOS 4          | rjd324          | http://aminet.net/package/game/actio/spacecadetpinball-aos4                                                |
| Android (WIP)      | fexed           | https://github.com/fexed/Pinball-on-Android                                                                |
| Web                | stech11845      | https://github.com/stech11845/SpaceCadetPinball-web

Plataformas cubiertas por este proyecto: Wii U.



## Fuente

* `pinball.exe` de `Windows XP` (SHA-1 `2A5B525E0F631BB6107639E2A69DF15986FB0D05`) and its public PDB
* `CADET.EXE` version de 32 bit de `Full Tilt! Pinball` (SHA-1 `3F7B5699074B83FD713657CD94671F2156DBEDC4`)

## Herramientas usadas

`Ghidra`, `Ida`, `Visual Studio`

## Compilando

### Para Wii U

1. Instalar [devkitPro](https://devkitpro.org/wiki/Getting_Started), luego instalar los paquetes `wiiu-dev`, `wiiu-sdl2_mixer`, y `wiiu-cmake`.
2. `git clone --recurse https://github.com/IntriguingTiles/SpaceCadetPinball-WiiU && cd SpaceCadetPinball-WiiU` 
3. Si no esta usando Aroma, compile e instale [libromfs-wiiu](https://github.com/yawut/libromfs-wiiu).
4.
    - Para Aroma, ejecute `mkdir build && cd build && $DEVKITPRO/portlibs/wiiu/bin/powerpc-eabi-cmake -DCMAKE_BUILD_TYPE=Release ..`
    - Para otros cfw de Wii U, ejecute `mkdir build && cd build && $DEVKITPRO/portlibs/wiiu/bin/powerpc-eabi-cmake -DCMAKE_BUILD_TYPE=Release -DUSE_ROMFS=yes ..`
5. Ponga la data del juego de pinball en `res` y ejecute `make`.
6. Para poner la musica del juego, convierta `PINBALL.MID` a MP3.
