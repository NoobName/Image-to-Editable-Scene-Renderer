"""Small, fail-closed validator for the keyword profile in our shared JSON Schema.

This is not a general Draft 2020-12 implementation. No third-party packages,
network schema resolution, DX12 bindings or model runtimes are required.
"""
from __future__ import annotations
import copy
import json
import math
from pathlib import Path

SCHEMA_PATH = Path(__file__).resolve().parents[2] / "schemas/scene-package.schema.json"
KEYWORDS = {"$schema", "$id", "title", "description", "$defs", "$ref", "type",
            "properties", "required", "additionalProperties", "items", "minItems",
            "maxItems", "minimum", "maximum", "minLength", "enum", "const", "default", "oneOf"}


def _pairs(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError(f"Duplicate JSON key: {key}")
        result[key] = value
    return result


def _finite_tree(value, depth=0):
    if depth > 64:
        raise ValueError("JSON nesting exceeds 64")
    if isinstance(value, (float, int)):
        try:
            finite = math.isfinite(value)
        except OverflowError:
            finite = False
        if not finite:
            raise ValueError("JSON numbers must be finite")
    if isinstance(value, dict):
        for item in value.values():
            _finite_tree(item, depth + 1)
    elif isinstance(value, list):
        for item in value:
            _finite_tree(item, depth + 1)
    elif isinstance(value, str):
        value.encode("utf-8", errors="strict")  # Reject lone surrogate escapes like the C++ parser.


def read_json(path):
    path = Path(path)
    if path.stat().st_size > 4 * 1024 * 1024:
        raise ValueError("JSON exceeds 4 MiB")
    value = json.loads(path.read_text(encoding="utf-8-sig"), object_pairs_hook=_pairs)
    _finite_tree(value)
    return value


def _check_profile(rule):
    unknown = rule.keys() - KEYWORDS
    if unknown:
        raise ValueError(f"Unsupported schema keywords: {sorted(unknown)}")
    if "$ref" in rule and not rule["$ref"].startswith("#/$defs/"):
        raise ValueError("Only local $defs references are supported")
    for key in ("$defs", "properties"):
        for child in rule.get(key, {}).values():
            _check_profile(child)
    if "items" in rule:
        _check_profile(rule["items"])
    for child in rule.get("oneOf", []):
        _check_profile(child)


SCHEMA = read_json(SCHEMA_PATH)
_check_profile(SCHEMA)


def _resolve(rule, schema=SCHEMA):
    return schema["$defs"][rule["$ref"].split("/")[-1]] if "$ref" in rule else rule


def _equal(a, b):
    # bool is an int subclass in Python, but is a separate type in JSON Schema.
    return a == b and (isinstance(a, bool) == isinstance(b, bool))


def validate(value, rule=SCHEMA, at="$", schema=SCHEMA):
    rule = _resolve(rule, schema)
    def fail(reason):
        raise ValueError(f"ScenePackage {at}: {reason}")
    if "oneOf" in rule:
        matches = 0
        for branch in rule["oneOf"]:
            try:
                validate(value, branch, at, schema)
                matches += 1
            except ValueError:
                pass
        if matches != 1:
            fail("must match exactly one supported variant")
    if "const" in rule and not _equal(value, rule["const"]):
        fail(f"expected {rule['const']!r}")
    if "enum" in rule and not any(_equal(value, v) for v in rule["enum"]):
        fail("unsupported enum value")
    types = {"object": isinstance(value, dict), "array": isinstance(value, list),
             "string": isinstance(value, str), "boolean": isinstance(value, bool),
             "number": isinstance(value, (int, float)) and not isinstance(value, bool),
             "integer": type(value) is int or (type(value) is float and math.isfinite(value) and value.is_integer())}
    if "type" in rule and not types.get(rule["type"], False):
        fail(f"expected {rule['type']}")
    if types["number"]:
        if not math.isfinite(value):
            fail("number must be finite")
        if value < rule.get("minimum", -math.inf) or value > rule.get("maximum", math.inf):
            fail("number outside range")
    if isinstance(value, str) and len(value) < rule.get("minLength", 0):
        fail("string cannot be empty")
    if isinstance(value, list):
        if not rule.get("minItems", 0) <= len(value) <= rule.get("maxItems", math.inf):
            fail("wrong number of items")
        if "items" in rule:
            for i, child in enumerate(value):
                validate(child, rule["items"], f"{at}[{i}]", schema)
    if isinstance(value, dict):
        for key in rule.get("required", []):
            if key not in value:
                fail(f"missing {key}")
        for key, child in value.items():
            if key in rule.get("properties", {}):
                validate(child, rule["properties"][key], f"{at}.{key}", schema)
            elif rule.get("additionalProperties") is False:
                fail(f"unknown field {key}")


def apply_defaults(value, rule=SCHEMA):
    rule = _resolve(rule)
    if "oneOf" in rule:
        for branch in rule["oneOf"]:
            try:
                validate(value, branch)
            except ValueError:
                continue
            apply_defaults(value, branch)
            break
    if isinstance(value, dict):
        for key, child in rule.get("properties", {}).items():
            child = _resolve(child)
            if key not in value and "default" in child:
                value[key] = copy.deepcopy(child["default"])
            if key in value:
                apply_defaults(value[key], child)
    if isinstance(value, list) and "items" in rule:
        for item in value:
            apply_defaults(item, rule["items"])


def normalize(data):
    _finite_tree(data)
    validate(data)
    result = copy.deepcopy(data)
    apply_defaults(result)
    validate(result)
    return result
