# SÉRÉNA 3D bracelet

Run `python manage.py run` from the repository root, then open **http://localhost:5176/bracelet/**. Existing `python nora-design/serve.py` and PowerShell launchers delegate to the same combined server.

The model is procedural Three.js, with a compact case joined into a narrower wrist band, hinged slate cap, illustrative emergency insert, lift marker and USB-C port/attached cover. The latest user-supplied concept is in `assets/reference.png`. It is a design reference, not manufactured hardware.

Brief press requests a call; hold two seconds to request hang-up. The hardware construction view and links remain available, with parts explicitly separate from validated enclosure fit. No geometric scaling proves that electronics fit.

Vendor Three.js files are local and retain their MIT license. There is no runtime CDN or frontend build step. Tests for the gesture and phone adapter are under `tests/` and use a fake provider. The mobile QA fixture under `qa/` is a developer-only viewport aid, not a second presentation app.

See [../README.md](../README.md) for setup and [../docs/VERIFICATION.md](../docs/VERIFICATION.md) for tested behaviour and limitations.
