# Chromium through WebView2

The Internet Channel shows real web pages with Microsoft Edge WebView2, the Chromium engine that comes with Windows 11.
This folder holds the part of Microsoft's software development kit that WeeOS needs to talk
to that engine. Nothing here is our code; our own readable wrapper is `source/Browser`.

| What | Where |
|---|---|
| Package | `Microsoft.Web.WebView2` from nuget.org, version **1.0.4191.47** (SHA-256 of the .nupkg: `f492bbf547d0da329553b6727435b677579b1e9f91cc9e4a1ad029366d5f23d0`) |
| Headers | `include/WebView2.h`, `include/WebView2EnvironmentOptions.h` (unchanged from `build/native/include`) |
| Loader | `x64/WebView2Loader.dll` and its import library `x64/WebView2Loader.dll.lib` (unchanged from `build/native/x64`); the build copies the DLL beside each program that uses the browser |
| Licence | BSD-style licence of Microsoft, `LICENSE.txt`; the third-party notices of the package, `NOTICE.txt` |

The engine itself is not here: it is the WebView2 Runtime that Windows 11 installs and keeps up to date (version 152.0.4191.66 on the
development PC on 2026-11-01). The SDK version was chosen to match it.
