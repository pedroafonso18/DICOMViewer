# DICOM Viewer

[Leia em português](README.md)

A desktop viewer for local DICOM files, built with C++20 and Qt 6 Widgets. Images open in their original orientation with the aspect ratio specified by the DICOM file. Decoding and rendering run in the background. If a file fails to open, the viewer identifies it, offers error details, and keeps the previous image available.

## Build on Linux

You need a C++20 compiler, CMake 3.24 or later, DCMTK 3.6.9 or later, Qt 6 with Widgets and Concurrent, and GoogleTest for the test suites.

### Fedora Linux 45

```sh
sudo dnf install git cmake gcc-c++ make qt6-qtbase-devel dcmtk-devel gtest-devel gmock
```

The validation environment uses Qt 6.11.2 and DCMTK 3.6.9.

### Ubuntu 26.04 LTS

```sh
sudo apt-get update
sudo apt-get install -y --no-install-recommends \
  git build-essential cmake qt6-base-dev qt6-qpa-plugins \
  libdcmtk-dev libjpeg-dev libpng-dev libtiff-dev libgtest-dev libgmock-dev
```

The validation environment uses Qt 6.10.2. The JPEG, PNG, and TIFF development packages provide libraries used by DCMTK's exported CMake targets.

### Build and run

These commands work on both distributions. If you already cloned the repository, start with the first `cmake` command from the repository root.

```sh
git clone https://github.com/pedroafonso18/DICOMViewer.git
cd DICOMViewer
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
./build/dicom_viewer
```

## Use the viewer

| Control | Behavior |
| --- | --- |
| **Open…** or **Ctrl+O** | Opens the file picker for `.dcm` and `.DCM` files. |
| Mouse wheel and drag | Zoom around the cursor and pan the image. |
| **Rotate Left** / **Rotate Right** | Rotate the image in 90° steps. |
| **Fit to Window** | Center and fit the image at its current rotation. |
| **Restore View** | Return to the first frame and restore its initial orientation, framing, and contrast. |
| **Previous** / **Next** | Change frames while preserving rotation, zoom, pan, and manual contrast. |
| **Preset**, **Window Level**, and **Window Width** | Select a stored window or adjust the contrast of a monochrome image. |
| **Metadata** | Show or hide the side panel with dimensions, pixel spacing, orientation, laterality, and transfer syntax when available. |

**File default** uses the frame's stored window or the file's contrast lookup table (VOI LUT) when available. The viewer calculates an automatic window when no applicable transform is present. Choose **Custom** to replace a VOI LUT with a manual adjustment. The interface follows the system theme, and the image area has a dark background.

## DICOM compatibility

- **Photometric interpretations** — `MONOCHROME1`, `MONOCHROME2`, `RGB`, `PALETTE COLOR`, `YBR_FULL`, `YBR_FULL_422`, and `YBR_PARTIAL_422`. Color images are displayed in RGB.
- **Encoding** — native pixel data, conventional JPEG (including Baseline and Lossless), JPEG-LS, and RLE Lossless.
- **Multiframe studies** — navigate within the open file as frames are decoded on demand. When needed, the core decodes fragmented JPEG frames sequentially if their offset table is empty.

JPEG 2000, HTJ2K, JPEG XL, encapsulated video, and floating-point pixel data are outside the current format support. The viewer opens one file at a time. Series navigation across files, volume reconstruction, and PACS access are outside the project scope.

## Project structure

| Location | Responsibility |
| --- | --- |
| [`src/dicomviewer/`](src/dicomviewer/) | Qt-independent C++ core. Validates files, owns the DCMTK session, applies contrast, returns pixels and metadata, and reports typed errors. |
| [`src/viewer/`](src/viewer/) | Qt Widgets interface. The loader uses Qt Concurrent and copies pixels into a `QImage`. The viewport handles zoom, pan, and rotation. |
| [`tests/`](tests/) | GoogleTest suites run through CTest. |
| [`.github/workflows/build.yml`](.github/workflows/build.yml) | Build and test jobs for Fedora 45, Ubuntu 26.04 LTS, and Windows Server 2025 (x64). |

On Linux, subsequent reads stay tied to the file that the session originally opened. The interface applies only the result of the latest frame or contrast request.

The Windows job uses MSVC 2022, Qt 6.8.3, and the DCMTK and GoogleTest packages from vcpkg. The Fedora and Ubuntu jobs use their distribution packages.

## Run the tests

```sh
ctest --test-dir build --output-on-failure
```

| CTest suite | Coverage |
| --- | --- |
| `dicom.unit` | Window Level and Window Width validation. |
| `dicom.integration` | DCMTK decoding, file errors, polarity, VOI LUT, per-frame windows, color channels, and navigation through native or compressed frames. |
| `dicom.viewer` | Offscreen Qt tests for failed opens, contrast, frame navigation, zoom, rotation, and reset. |

The suites generate their own DICOM fixtures. To include the reference mammogram from the exercise, set the path to your local copy. The reference check is skipped when the variable is unset.

```sh
DICOMVIEWER_REFERENCE_FILE="$HOME/Downloads/anonymized_mamo.dcm" \
  ctest --test-dir build -R '^dicom\.integration$' --output-on-failure
```

To build only the core and its test suites, use a separate build directory and disable the Qt application.

```sh
cmake -S . -B build-core -DCMAKE_BUILD_TYPE=Release -DDICOMVIEWER_BUILD_APP=OFF
cmake --build build-core --parallel
ctest --test-dir build-core --output-on-failure
```
