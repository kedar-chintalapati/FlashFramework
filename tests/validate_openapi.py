import json
import pathlib
import sys


def main() -> int:
    path = pathlib.Path(sys.argv[1])
    encoded = path.read_bytes()
    document = json.loads(encoded)
    if document.get("openapi") != "3.1.1":
        raise ValueError("expected OpenAPI 3.1.1")
    if not isinstance(document.get("info"), dict):
        raise ValueError("missing info object")
    if not isinstance(document.get("paths"), dict):
        raise ValueError("missing paths object")
    schemas = document.get("components", {}).get("schemas", {})
    if "Problem" not in schemas:
        raise ValueError("missing problem schema")
    if len(sys.argv) == 3:
        golden = pathlib.Path(sys.argv[2]).read_bytes()
        if encoded != golden:
            raise ValueError("generated OpenAPI does not match the golden file")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
