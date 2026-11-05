# Third-party components

- **Three.js 0.170.0** and bundled addons: MIT. See [THREE-LICENSE.txt](nora-design/vendor/THREE-LICENSE.txt). Kept locally so the 3D renderer does not depend on a CDN.
- **pyserial 3.5**: BSD-3-Clause. Installed through `requirements.txt`; not vendored.
- **PlatformIO Core 6.2.0**: Apache-2.0. Installed as a development dependency; not vendored.
- **ESP32 Arduino framework, Adafruit libraries and build tools**: retrieved by PlatformIO from the versions declared in `nora-embedded/platformio.ini`. Their own licenses apply; generated packages are excluded from Git.
- **Concept reference image** in `nora-design/assets/reference.png`: user-supplied design reference, not a photograph of manufactured hardware.

The discarded browser-calling experiment and its vendor packages are not part of this repository. Publishing the source does not change the licenses of third-party components or grant a project-wide license to the original work.
