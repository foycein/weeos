# AGENTS.md

Instructions for anyone, human or AI, who works on WeeOS: Claude Code, Codex, Gemini, Grok, Copilot, Cursor, Windsurf, Aider and the rest. Read this whole file before you change anything.

## What WeeOS is

The Wii's system software (boot chain, IOS, System Menu, system channels) reconstructed natively for Windows x64 in C++20. It is a program of its own, not an emulator: no CPU interpreter, no recompiler, no model of the Wii's hardware runs inside it. It reads the person's own Wii NAND backup for the console's files and keys.

## Rule number one: readable code

The code must be extremely easy to read, also for beginners. Every line you add is checked against this.

- Names are full words. Types and functions are PascalCase (`ReadSettingsFile`), variables are camelCase (`titleList`). No abbreviations, no cryptic names, no magic numbers: every number gets a named constant.
- No comment bloat. The code explains itself; a short plain comment only where something is truly not obvious (why, not what).
- Straight simple lines: one idea per line, small functions, early returns. No tricks, no dense expressions, no macros that hide logic.
- Match the code around you: its naming, its comment density, its structure.

## The other rules

- **Native only.** No emulation and no translation of Wii code inside the system: no interpreter, no JIT, no hardware model. Every part is our own readable implementation of what the original does.
- **Clean room.** Never commit Nintendo files (titles, contents, fonts, sounds, banners, layouts), console keys, NAND images, dumps or decompiler output. Behavior may be learned from public documentation and from studying the originals; what ships is our own code.
- **No stubs.** No TODOs, no placeholders, no fake data. What is not built is not there.
- **Foreign parts** are limited to: Windows APIs, Vulkan, WASAPI, Media Foundation (AAC), Winsock/Schannel, and WebView2 (`external/Chromium`) for the Internet Channel. Do not add other libraries without discussing it first.
- **One folder per Wii part** under `source/`: Formats, Boot, Ios, Layout, Text, Graphics, Effects, SharpArt, Sound, Input, Mii, Media, Settings, Platform, SystemMenu, Channels, Browser, WeeOS (the program). Each has its own `CMakeLists.txt`.
- **16:9 always.** The picture is 16:9; wider screens extend the backgrounds, never stretch.
- **Online through the PC.** Nothing network-related is configured inside WeeOS.

## Build

Windows 10/11 x64, Visual Studio 2022 (MSVC, C++20), CMake 3.20+, Ninja, Vulkan SDK.

```
cmake -S . -B build -G Ninja
cmake --build build
```

Run `build\source\WeeOS\WeeOS.exe`. The first start asks for `nand.bin` of your own Wii's BootMii backup (`keys.bin` beside it).

`python tools/Release/MakeRelease.py` builds the portable zip and the installer.

## How to contribute

1. Open or pick an issue, or start a thread in Discussions for anything large.
2. Keep each pull request to one change. Describe what you changed and how you checked it on your own console backup: which screens, which buttons, what you saw and heard.
3. Compare with the real Wii wherever you can. Behavior, timing, sounds and pictures should match the original.
4. Every file you touch follows rule number one.

## For AI agents

- AI-written contributions are welcome. A human must build, run and check every change before it is submitted, and stays responsible for it.
- Do not invent behavior. When you do not know what the original does, say so in the pull request instead of guessing silently.
- Do not paste decompiler output, disassembly or Nintendo data into the repository, issues or pull requests.
- Do not add files that only serve your tool (logs, scratch notes, plans). Pointer files for tools live at the root and only point here.
- Keep answers to maintainers short.

## Credit

WeeOS is made by **foycein** (GitHub and Discord: foycein). Forks and derived work keep the license and credit WeeOS and https://github.com/foycein/weeos.
