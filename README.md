# HDL Batch installer — GusseDev fork

> **Unofficial personal fork.** All credit for this tool belongs to **Matias Israelson (El_isra)** and the [original HDL-Batch-installer team](https://github.com/israpps/HDL-Batch-installer) — their work is genuinely extraordinary, and this fork only adds a few quality-of-life tweaks on top of it. For the reference, maintained version, please use the [upstream project](https://github.com/israpps/HDL-Batch-installer).

## Latest release — [v3.8.0 Revision 14](https://github.com/GusseDev/HDL-Batch-installer/releases/latest)

### What this fork adds

- **Unified game list** — games waiting to be installed are highlighted at the top of the same list; the Install button appears automatically. Drag & drop ISO files or supported archives right onto it.
- **Fully fluid list** — artwork thumbnails load in a **background thread**, so scrolling stays perfectly smooth even on large libraries. A progress bar (styled like the disk-capacity bar) shows loading, with a **Stop** button, and the rows you look at load first.
- **Persistent thumbnail cache** — thumbnails are cached to disk (`Downloads/_artcache`) and locally downloaded assets (`Downloads/ART`) are reused first, so on restart the icons appear almost instantly with no HDD access when the cache is warm.
- **Live install progress** — `HDL.EXE` output streamed into an embedded console, per-game progress bar, pause / resume.
- **Assets made easy** — optional auto-download of art / CFG / CHT after install; one-click copy to `+OPL` on the HDD; double-click a game to preview its media.
- **Quality of life** — search field, clickable column sorting with indicator arrows, sizes in MB and GB, automatic HDD detection at startup, and blocking libpng (iCCP) warning popups suppressed.

**Download:** grab `HDL-Batch-installer-v3.8.0-rev14-x64.zip` from the [releases page](https://github.com/GusseDev/HDL-Batch-installer/releases/latest), extract anywhere, and run `HDL-Batch-installer.exe` **as administrator** (first launch downloads the OSD icon database automatically). Windows x64, built with a reproducible MSYS2 / MINGW64 toolchain via `build.sh`.

---

## Original project

[![wxWidgets version](https://img.shields.io/badge/wxWidgets-3.0.5-blue)](https://www.wxwidgets.org/downloads/#v3.0.5)
![project status](https://img.shields.io/badge/Project%20status-Active-00cc22)

![os](https://img.shields.io/badge/Windows-x64-green)
![os](https://img.shields.io/badge/Windows-x86-green)

[![GitHub release (by tag)](https://img.shields.io/github/downloads/israpps/HDL-Batch-installer/Latest/total?label=Downloads%20%5BLatest%5D)](https://github.com/israpps/HDL-Batch-installer/releases)
[![Discord](https://img.shields.io/discord/859508044340920370?label=HDLBinst%20server&logo=discord&logoColor=white)](https://discord.gg/wczxvrkZk6)

#### A GUI for [HDL Dump](https://github.com/israpps/hdl-dump).

#### Learn more [here](https://israpps.github.io/HDL-Batch-installer/)

 Made by Matias Israelson (AKA:El_isra)

> Originally this was a personal project to practice C++ & give a try to wxWidgets...
>
> But at the end I decided to share it here on github.


__If this software was useful, please consider giving it a star here on GitHub, a rating on PSX-place or a donation via [PayPal](https://www.paypal.com/paypalme/ElisraPS2)__


### Currently implemented features (unchecked elements are WIP)

----

- [x] Install multiple Games at once
- [x] Extract multiple Games at once
- [x] Automatically assign the original Game Title before Installation
- [X] Inject MBR.KELF into the HDD
- [x] Rename Game
- [x] View game information
- [x] Extract MBR program from HDD
- [x] Download Artwork, Widescreen Cheats and Game settings for OPL
- [x] Massive `KELF` and icon injection to every installed game
- [x] Mount __any PFS Partition__ as if it was a windows supported storage device
- [x] Automatically transfer Downloaded files to mounted partition with 1 click (use only on OPL data partition) 
- [x] Batch game transfer between two ps2 HDDs
- [x] Delete and create Partitions
- [x] Format any Hard Drive into the required format


---

## compilation

HDL Batch Installer was built based on wxWidgets 3.0.5 stable release, built as a win32 Monolithic unicode-enabled static library, it uses Code::blocks wxwidgets project build system, to manage the UI elements and their declaration/implementation automatically, saving precious time.

Eventually, i'll upgrade into the latest stable release 3.2.0, wich I hope doesn´t break anything :D


### cross compilation

 Currently the project is windows only, in order to make it cross platform, it will not only require to port parts of the code (wich sould not be too hard, since i tried to stick to wxWidgets as much as i could), but also the replacement of certain parts of the code (ie: PFSFuse manager UI)

Since no one was interested on colaborating with this project so far, i don´t expect this to happen soon... or any time...
