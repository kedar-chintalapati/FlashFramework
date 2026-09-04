import json
import pathlib
import sys


def main() -> int:
    try:
        from openapi_spec_validator import validate
    except ModuleNotFoundError:
        print("openapi-spec-validator is not installed")
        return 77
    path = pathlib.Path(sys.argv[1])
    validate(json.loads(path.read_text(encoding="utf-8")))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
