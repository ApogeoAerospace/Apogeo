Package: flatbuffers:arm64-osx@25.2.10

**Host Environment**

- Host: arm64-osx
- Compiler: AppleClang 15.0.0.15000309
-    vcpkg-tool version: 2025-07-21-d4b65a2b83ae6c3526acd1c6f3b51aff2a884533
    vcpkg-scripts version: 333e0d58bc 2025-08-01 (3 hours ago)

**To Reproduce**

`vcpkg install `

**Failure logs**

```
Downloading https://github.com/google/flatbuffers/archive/v25.2.10.tar.gz -> google-flatbuffers-v25.2.10.tar.gz
Successfully downloaded google-flatbuffers-v25.2.10.tar.gz
-- Extracting source /Users/johancastrillon/Documents/MoLab/vcpkg/downloads/google-flatbuffers-v25.2.10.tar.gz
-- Applying patch fix-uwp-build.patch
-- Using source at /Users/johancastrillon/Documents/MoLab/vcpkg/buildtrees/flatbuffers/src/v25.2.10-7dac777f39.clean
-- Configuring arm64-osx
-- Building arm64-osx-dbg
-- Building arm64-osx-rel
-- Fixing pkgconfig file: /Users/johancastrillon/Documents/MoLab/vcpkg/packages/flatbuffers_arm64-osx/lib/pkgconfig/flatbuffers.pc
CMake Error at scripts/cmake/vcpkg_find_acquire_program.cmake:166 (message):
  Could not find pkg-config.  Please install it via your package manager:

      brew install pkg-config
Call Stack (most recent call first):
  scripts/cmake/vcpkg_fixup_pkgconfig.cmake:193 (vcpkg_find_acquire_program)
  ports/flatbuffers/portfile.cmake:34 (vcpkg_fixup_pkgconfig)
  scripts/ports.cmake:206 (include)



```

**Additional context**

<details><summary>vcpkg.json</summary>

```
{
  "name": "molab",
  "version-string": "0.1.0",
  "dependencies": [
    "flatbuffers",
    "nlohmann-json"
  ]
}

```
</details>
