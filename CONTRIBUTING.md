# Working on SÉRÉNA

Run commands from the repository root. Python 3.10+ is required, with 3.12 used for verification. Install `requirements-dev.txt` for firmware builds and Node.js 22+ for JavaScript tests. Native C++ tests need `g++` or a compatible compiler specified with `CXX`.

```sh
python manage.py setup-dev
python manage.py test --require-js
python manage.py build
python manage.py scan
```

Equivalent Makefile targets are provided. Windows users can use `python manage.py` without installing Make. The optional portable tools under `.tools/` are workstation-specific and are not committed.

Tests must use a fake provider. Do not place a real call from tests, CI, page load or firmware boot. A real call is an explicit presentation action by the user. Keep the short-click/long-hold behaviour and duplicate-request protection covered by tests.

Do not commit `.local/`, `.env`, phone numbers, account/API secrets, generated firmware, logs or local tools. Use `.env.example` only as a list of environment-variable names. `python manage.py scan` checks the files Git would publish, and CI repeats the check.

Hardware documents are interface studies, not fabrication-ready or validated circuitry. Keep assumptions and validation status explicit when changing firmware, wiring or component selections.
