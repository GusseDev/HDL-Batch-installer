# HDL Batch installer

> **This is an unofficial personal fork by [GusseDev](https://github.com/GusseDev).**
> All credit for this tool goes to **Matias Israelson (El_isra)** and the [original team](https://github.com/israpps/HDL-Batch-installer) — their work is genuinely extraordinary, and this fork only adds a few quality-of-life tweaks on top of it. For the reference, maintained version, please use the [upstream project](https://github.com/israpps/HDL-Batch-installer).
>
> **What this fork adds (v3.8.0 rev8):** unified game list with highlighted pending installs and drag & drop · live per-game install progress streamed into an embedded console · pause / resume installs · optional auto-download of assets (art / CFG / CHT) after install and one-click copy to `+OPL` on the HDD · artwork preview on double-click · search field, clickable column sorting with indicator arrows, sizes shown in MB and GB · automatic HDD detection at startup · reproducible MSYS2 / MINGW64 build via `build.sh`. See the [release notes](https://github.com/GusseDev/HDL-Batch-installer/releases/tag/v3.8.0-rev8).


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
