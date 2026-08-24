# Nick’s fork of PokéFinder


This is a fork of the program “PokeFinder” by AdmiralFish. More on it down below.

[Latest Official Release](https://github.com/NickPlayeZ/PokeFinder/releases/latest)

[Latest Nightly Build](https://github.com/NickPlayeZ/PokeFinder/actions)

Please report any bugs either via opening an issue on this repository or by @ing me (username nickplayez) on "I'm a Blisy"s RNG manipulation Discord server (don't DM me I literally never check DMs and will ignore them for weeks).


This fork includes the following new features and quality of life additions:


Gen 3 Emerald
- added multi lead search so users don’t have to search using at most 1 lead at a time, but can now select any number of leads to search with at once

Gen 4
- added Step Encounter RNG support (Wild RNG without Sweet Scent / Honey)
- added items to HGSS Rock Smash generator
- PokeFinder now shows all Advances advance number, Chatot Pitch and Call Letter, even those that do not yield an encounter for things like Fishing or Rock Smash RNG, so that users can easier track what Advance they're on at all times 
-added multi lead search, so users don’t have to search using at most 1 lead at a time, but can now select any number of leads to search with at once

Gen 5
- added Pickup RNG support
- added Step Encounter RNG support (Wild RNG without Sweet Scent / Honey)
- expanded / finished Phenomena RNG support
- added Wild Swarm RNG support
- Hidden Grotto item searcher now lets you search for multiple items within the selected Advance range; very useful for PP Max farming
- PokeFinder now shows all Advances advance number, Chatot Pitch and Call Letter, even those that do not yield an encounter for things like Fishing or Grotto RNG, so that users can easier track what Advance they're on at all times 
- added multi lead search, so users don’t have to search using at most 1 lead at a time, but can now select any number of leads to search with at once
- added multi pass power search, so user don’t have to search using at most 1 pass power / level at a time, but can now select any number of pass powers / levels of the same pass power to search with at once
- fixed a bug where Grotto Power did not work correctly
- added “N’s Pokémon released” checkbox to BW2 profiles with memory link, as releasing them impacts the generation of Wild encounters


Most of these will eventually be brought into the main PokeFinder, though that will take some time and not all of these features may make it, so for the time being I will try to keep this fork updated with any new changes AdmiralFish makes to the main PokeFinder. I will keep a list of all that’s been brought over right below.


Features that have already been brought over to the main PokeFinder:
- Added Advance Finder for Gens 4 and 5, so users can easier find the Advance they’re on after losing track
- Added Save Needle display for Advances in Gen 5
- Added Delay and Hour output to relevant Gen 4 searcher tabs
- Added level searcher, so users can search for Pokemon that have a specific level; mostly useful for Surfing and Fishing RNG, as those can have a range levels within the same encounter slot
- Added Suction Cups / Sticky Hold lead for Gen 5 Fishing
- Added correct Characteristic names based on Generation and language; previously PokeFinder used the Gen 6+ Characteristic names which was wrong for Gens 4 and 5
- Expanded Gen 4 Seed to Time Coin Flip / Call searcher window to display the results within the small popup window instead of users always having to close them to see what they hit
- PokeFinder now returns an error message if the user is trying to get a Hidden Ability offspring with a parent combination that cannot produce a Hidden Ability offspring in Gens 5 and 8


# PokéFinder

Join the PokéFinder Discord server to talk about development and contribute.

[![PokéFinder](https://discordapp.com/assets/07dca80a102d4149e9736d4b162cff6f.ico)](https://discord.gg/XmgQF9X)

This will be a RNG Tool for all main Pokémon games generations 3-8. It currently supports generations 3/4 and parts of generation 5/8.

# Download

[Latest Official Release](https://github.com/Admiral-Fish/PokeFinder/releases/latest)

[Latest Nightly Build](https://github.com/Admiral-Fish/PokeFinder/actions)

# Features
Gen 3
- Egg
- GameCube
- IDs
- Static
- Wild

Gen 4
- Egg
- Event
- IDs
- Static
- Wild

Gen 5
- Dream Radar
- Egg
- Event
- Hidden Grotto
- IDs
- Static
- Wild

Gen 8
- Egg
- Event
- IDs
- Raid
- Static
- Underground
- Wild

# Supported Platforms

Windows
- Windows 10
- Windows 11

MacOS
- MacOS Sonoma
- MacOS Sequoia
- MacOS Tahoe

Linux
- Ubuntu 22.04
- Ubuntu 24.04

Qt
- 6.10 or newer

# Installing

Windows
- Install the [Microsoft Visual C++ Redistributable](https://learn.microsoft.com/en-us/cpp/windows/latest-supported-vc-redist?view=msvc-170)
- Download the win zip folder from the [releases page](https://github.com/Admiral-Fish/PokeFinder/releases/latest)
- Extract PokéFinder from the zip folder

MacOS
- Download the macos zip folder from the [releases page](https://github.com/Admiral-Fish/PokeFinder/releases/latest)
- Extract PokéFinder from the zip folder

Linux
- Install Qt 6
  - [Qt website](https://www.qt.io/download)
  - sudo apt install qt6-base-dev
- Download the linux zip folder from the [releases page](https://github.com/Admiral-Fish/PokeFinder/releases/latest)
- Extract PokéFinder from the zip folder

# Building

Windows
- Install the dependencies
  - [Qt 6](https://www.qt.io/download)
  - [Build tools for Visual Studio](https://visualstudio.microsoft.com/downloads/)
  - [Python 3.14](https://www.python.org/downloads/)
- Build
  - git submodule update
  - mkdir build
  - cd build
  - cmake -G"NMake Makefiles" -DCMAKE_BUILD_TYPE=RELEASE ../
  - cmake --build .
- Bundle
  - mk PokeFinder-windows
  - move release\PokeFinder.exe PokeFinder-windows\PokeFinder.exe 
  - windeployqt --release --no-translations --no-angle --no-plugins --no-opengl-sw PokeFinder.exe
  - xcopy /I "QTPath"\plugins\platforms\qwindows.dll PokeFinder-windows\platforms\
  - xcopy /I "QTPath"\plugins\styles\qwindowsvistastyle.dll PokeFinder-windows\styles\

MacOS
- Install the dependencies
  - Qt 6 ([brew](https://formulae.brew.sh/formula/qt) or the [Qt website](https://www.qt.io/download))
  - [Python 3.14](https://www.python.org/downloads/)
- Build
  - git submodule update
  - mkdir build
  - cd build
  - PATH="PATH=$PATH:$HOME/Qt/6.10/macos/bin" cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=RELEASE ../
    - Replace Qt path/version as necessary
  - cmake --build .
- Bundle
  - macdeployqt PokeFinder.app -dmg -verbose=2

Linux
- Install the dependencies
  - Qt 6
    - [Qt website](https://www.qt.io/download)
    - sudo apt install qt6-base-dev qt6-tools-dev qt6-tools-dev-tools qt6-l10n-tools
  - [Python 3.14](https://www.python.org/downloads/)
  - sudo apt install build-essential libgl1-mesa-dev
- Build
  - git submodule update
  - mkdir build
  - cd build
  - cmake -G"Unix Makefiles" -DCMAKE_BUILD_TYPE=RELEASE -DCMAKE_PREFIX_PATH=$HOME/Qt/6.10/gcc_64 ../
    - Replace Qt path/version as necessary
  - cmake --build .

# Credits (in no particular order)
- Bill Young, Mike Suleski, and Andrew Ringer for [RNG Reporter](https://github.com/Slashmolder/RNGReporter)
- chiizu for [PPRNG](https://github.com/chiizu/PPRNG)
- wwwwwwzx for [3DSRNG Tool](https://github.com/wwwwwwzx/3DSRNGTool)
- The PokemonRNG team for various contributions and research [zaksabeast](https://github.com/zaksabeast), [EzPzstreamz](https://github.com/SteveCookTU), [Shiny_Sylveon](https://github.com/ShinySylveon04), [Vlad](https://github.com/RichardPaulAstley), [Real96](https://github.com/Real96)
- Other great people for various help and research ([OmegaDonut](https://github.com/OmegaDonut), [Bond697](https://github.com/Bond697), [Kaphotics](https://github.com/kwsch), [SciresM](https://github.com/SciresM), Zari, amab, Marin, [Lean](https://github.com/Leanny), etc)
- Sans for initial GUI design
