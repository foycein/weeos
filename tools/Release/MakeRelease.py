"""Builds the release of WeeOS: the portable folder, its zip and the installer weeossetup.exe.

python -I tools/Release/MakeRelease.py [--to-release]

Everything goes into the staging folder out/Release of this repository:
    WeeOS/                      the portable folder as people get it: WeeOS.exe, the shaders and an empty SDCard folder. No Wii
                                files, no NAND and no identity: the first start imports the person's own Wii from its BootMii backup.
    WeeOS-<version>-portable.zip   that folder as a zip
    weeossetup.exe              the installer, which carries the zip (tools/Setup)

--to-release also copies the clean source (the files of the last commit, which leaves out assets, reference, the worktrees, the build
folders and personal data, because git ignores them) and the three things above into D:/Projects/WeeOS/RELEASE. It needs a work tree
without uncommitted changes, so that the program and the source are the same.

tools/Release/CheckRelease.py checks what this script made.
"""

import re
import shutil
import subprocess
import sys
import tarfile
import zipfile
from io import BytesIO
from pathlib import Path

RepositoryFolder = Path(__file__).resolve().parents[2]
BuildFolder = RepositoryFolder / "build-release"
StagingFolder = RepositoryFolder / "out" / "Release"
BuildEnvironment = RepositoryFolder / "tools" / "vcenv.cmd"
VersionHeader = RepositoryFolder / "source" / "SystemMenu" / "WeeOSVersion.h"

ProgramFolderName = "WeeOS"
SetupFileName = "weeossetup.exe"
SourceFolderName = "Source"
ReadmeFileName = "README.md"
ToReleaseOption = "--to-release"

# What WeeOS.exe needs beside it, in the build folder of the program (source/WeeOS/CMakeLists.txt). WebView2Loader.dll is the loader of
# the browser engine of the Internet Channel (external/Chromium).
ProgramFiles = ["WeeOS.exe", "Mesh.vert.spv", "Quad.frag.spv", "Quad.vert.spv", "WebView2Loader.dll"]
WiiFilesFolderName = "WiiFiles"
SdCardFolderName = "SDCard"

# Neither Nintendo's files nor a console ever go into the release: the first start of every install imports the person's own Wii.
ConsoleNames = ["NAND", "Identity", "WeeOS.settings.txt", WiiFilesFolderName]

# Names that must never be in the clean source. external/Chromium (the WebView2 SDK) is part of it: the source does not build without it.
NamesNeverInTheSource = ["assets", "reference", "Worktrees", "data", "out", "build", "build-release"]

BestZipCompression = 9


def ReadVersion():
    match = re.search(r'WeeOSVersion = u"([^"]+)"', VersionHeader.read_text(encoding="utf-8"))
    if match is None:
        raise SystemExit(f"The version is not in {VersionHeader}")
    return match.group(1)


def ZipPathOf(folder, version):
    return folder / f"WeeOS-{version}-portable.zip"


def RunInBuildEnvironment(arguments):
    command = [str(BuildEnvironment)] + arguments
    print(">", " ".join(arguments), flush=True)
    subprocess.run(command, cwd=RepositoryFolder, check=True)


def Build(target, zipPath):
    RunInBuildEnvironment([
        "cmake", "-S", str(RepositoryFolder), "-B", str(BuildFolder), "-G", "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        "-DCMAKE_MSVC_RUNTIME_LIBRARY=MultiThreaded",
        f"-DWeeOSSetupPackage={zipPath.as_posix()}",
    ])
    RunInBuildEnvironment(["cmake", "--build", str(BuildFolder), "--target", target])


def RemoveIfThere(path):
    if path.is_dir():
        shutil.rmtree(path)
    elif path.exists():
        path.unlink()


def StageProgramFolder(programFolder):
    builtFolder = BuildFolder / "source" / "WeeOS"
    RemoveIfThere(programFolder)
    programFolder.mkdir(parents=True)

    for fileName in ProgramFiles:
        shutil.copy2(builtFolder / fileName, programFolder / fileName)
    (programFolder / SdCardFolderName).mkdir()

    for name in ConsoleNames:
        if (programFolder / name).exists():
            raise SystemExit(f"{name} must not be in the release")


# The zip holds the folder WeeOS, so that unpacking it anywhere gives one folder. Empty folders (SDCard) get an entry of their own.
def WriteZip(programFolder, zipPath):
    RemoveIfThere(zipPath)
    with zipfile.ZipFile(zipPath, "w", compression=zipfile.ZIP_DEFLATED, compresslevel=BestZipCompression) as archive:
        archive.write(programFolder, ProgramFolderName + "/")
        for path in sorted(programFolder.rglob("*")):
            nameInZip = ProgramFolderName + "/" + path.relative_to(programFolder).as_posix()
            if path.is_dir():
                archive.write(path, nameInZip + "/")
            else:
                archive.write(path, nameInZip)


def RequireCommittedWork():
    changes = subprocess.run(["git", "status", "--porcelain"], cwd=RepositoryFolder, capture_output=True, text=True, check=True).stdout
    if changes.strip():
        raise SystemExit(f"{ToReleaseOption} needs a work tree without uncommitted changes:\n{changes}")


def FindReleaseFolder():
    commonGitFolder = subprocess.run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], cwd=RepositoryFolder,
                                     capture_output=True, text=True, check=True).stdout.strip()
    developmentFolder = Path(commonGitFolder).parent
    return developmentFolder.parent / "RELEASE"


def WriteCleanSource(sourceFolder):
    archive = subprocess.run(["git", "archive", "--format=tar", "HEAD"], cwd=RepositoryFolder, capture_output=True, check=True).stdout
    with tarfile.open(fileobj=BytesIO(archive)) as sourceFiles:
        for name in sourceFiles.getnames():
            if name.split("/")[0] in NamesNeverInTheSource:
                raise SystemExit(f"{name} must not be in the clean source")
        sourceFiles.extractall(sourceFolder, filter="data")


def WriteReadme(releaseFolder, version):
    zipName = ZipPathOf(Path(), version).name
    lines = [
        f"# WeeOS {version}",
        "",
        "A hand-written reconstruction of the Wii system for the PC.",
        "",
        f"- `{SetupFileName}` installs WeeOS, by default into Programs/WeeOS of your local application data, with its shortcuts and an uninstaller.",
        f"- `{zipName}` is the same WeeOS as a portable folder: unpack it anywhere and start `WeeOS.exe`.",
        f"- `{ProgramFolderName}` is that folder, unpacked.",
        f"- `{SourceFolderName}` is the source code it was built from.",
    ]
    (releaseFolder / ReadmeFileName).write_text("\n".join(lines) + "\n", encoding="utf-8")


# RELEASE holds only what this script puts there (and the README that waited for it). Anything else in it stops the copy, so nothing
# of the owner's is replaced.
def CopyToRelease(version):
    RequireCommittedWork()
    releaseFolder = FindReleaseFolder()
    releaseNames = [SourceFolderName, ProgramFolderName, ZipPathOf(Path(), version).name, SetupFileName, ReadmeFileName]

    releaseFolder.mkdir(exist_ok=True)
    for path in releaseFolder.iterdir():
        if path.name not in releaseNames:
            raise SystemExit(f"{path} is not from this script; RELEASE is left as it is")
    for name in releaseNames:
        RemoveIfThere(releaseFolder / name)

    WriteCleanSource(releaseFolder / SourceFolderName)
    shutil.copytree(StagingFolder / ProgramFolderName, releaseFolder / ProgramFolderName)
    shutil.copy2(ZipPathOf(StagingFolder, version), releaseFolder)
    shutil.copy2(StagingFolder / SetupFileName, releaseFolder)
    WriteReadme(releaseFolder, version)
    print(f"Copied the release into {releaseFolder}")


def Main():
    copyToRelease = ToReleaseOption in sys.argv[1:]
    version = ReadVersion()
    programFolder = StagingFolder / ProgramFolderName
    zipPath = ZipPathOf(StagingFolder, version)
    StagingFolder.mkdir(parents=True, exist_ok=True)

    Build("WeeOS", zipPath)
    StageProgramFolder(programFolder)
    WriteZip(programFolder, zipPath)

    Build("weeossetup", zipPath)
    shutil.copy2(BuildFolder / "tools" / "Setup" / SetupFileName, StagingFolder / SetupFileName)
    print(f"WeeOS {version} is ready in {StagingFolder}")

    if copyToRelease:
        CopyToRelease(version)


Main()
