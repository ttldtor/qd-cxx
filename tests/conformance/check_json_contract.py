"""QDCXX-008 example/evidence checks; no XML importer or production validator."""

import copy
import hashlib
import json
from pathlib import Path
import re


ROOT = Path(__file__).resolve().parents[2]
EXAMPLES = ROOT / "spec/schema/examples"
FIXTURES = ROOT / "tests/fixtures/schema"
PROFILE = {"id": "qd-3.355-xml-captured", "version": 1}
FIELD_KEYS = "id semanticName effectiveName fullName localName propertyName serialTypeId"
SLOT_KEYS = "ordinal role fieldId fullName localName propertyName serialTypeId"
NAMES = ("fullName", "localName", "propertyName")


def require(condition, message):
    if not condition:
        raise ValueError(message)


def keys(obj, required, optional=""):
    require(type(obj) is dict, "object required")
    require(set(required.split()) <= obj.keys(), "missing member")
    require(obj.keys() <= set((required + " " + optional).split()), "unknown member")


def integer(value):
    require(type(value) is int and 0 <= value <= 2147483647, "integer domain")


def string(value):
    require(type(value) is str and bool(value), "nonempty string required")
    value.encode("utf-8", errors="strict")


def array(value):
    require(type(value) is list, "array required")


def unique(pairs):
    result = {}
    for key, value in pairs:
        require(key not in result, "duplicate JSON key")
        result[key] = value
    return result


def load(path):
    def bad_constant(value):
        raise ValueError("non-JSON number: " + value)
    return json.loads(path.read_text(encoding="utf-8"), object_pairs_hook=unique,
                      parse_constant=bad_constant)


def validate(doc):
    require(doc.get("kind") in ("local-schema", "resolution-failure"), "kind")
    body = "records" if doc["kind"] == "local-schema" else "diagnostics"
    keys(doc, "formatVersion profile kind provenance " + body, "extensions")
    integer(doc["formatVersion"])
    require(doc["formatVersion"] == 1, "unknown format")
    keys(doc["profile"], "id version")
    integer(doc["profile"]["version"])
    require(doc["profile"] == PROFILE, "unknown profile")
    if "extensions" in doc:
        require(type(doc["extensions"]) is dict, "extensions object")
        for key in doc["extensions"]:
            require(":" in key and all(key.split(":", 1)), "namespaced extension")
    p = doc["provenance"]
    keys(p, "inputs profileEvidence expectationRef")
    array(p["inputs"])
    for item in p["inputs"]:
        keys(item, "ref sha256")
        string(item["ref"])
        require(type(item["sha256"]) is str and re.fullmatch(r"[0-9a-f]{64}", item["sha256"]), "input hash")
    array(p["profileEvidence"])
    require(len(p["profileEvidence"]) == 2, "metadata and environment references")
    for ref in p["profileEvidence"] + [p["expectationRef"]]:
        string(ref)
    array(doc[body])
    if body == "diagnostics":
        require(bool(doc[body]), "nonempty diagnostics")
        for d in doc[body]:
            keys(d, "code stage recognition sourceRefs subject serialTypeId evidenceRef oracleStage")
            string(d["subject"])
            string(d["evidenceRef"])
            array(d["sourceRefs"])
            require(d["sourceRefs"] == [i["ref"] for i in p["inputs"]], "example diagnostic sources")
            if d["serialTypeId"] is not None:
                integer(d["serialTypeId"])
            combos = {
                "no-source-inputs": ("input", "not-reached", {None}),
                "all-index-fields-disabled": ("local-construction", "succeeded", {None}),
                "unsupported-local-type": ("local-construction", "succeeded", {1, 3}),
            }
            require(d["code"] in combos, "diagnostic code")
            stage, recognition, types = combos[d["code"]]
            require((d["stage"], d["recognition"]) == (stage, recognition) and d["serialTypeId"] in types, "diagnostic boundary")
            require(d["oracleStage"] in ("model.load", "runtime.load"), "oracle attempt")
            require(d["code"] == "no-source-inputs" or d["oracleStage"] == "runtime.load", "construction attempt")
            require(bool(p["inputs"]) == (d["code"] != "no-source-inputs"), "input precondition")
        return
    record_names, full_names, scheme_keys = set(), set(), set()
    for rid, record in enumerate(doc[body]):
        keys(record, "id name hasTime fields indexes javaSlots lookups")
        integer(record["id"])
        require(record["id"] == rid, "record order/id")
        string(record["name"])
        require(record["name"] not in record_names, "duplicate record")
        record_names.add(record["name"])
        require(type(record["hasTime"]) is bool, "hasTime boolean")
        for name in ("fields", "indexes", "lookups"):
            array(record[name])
        fields = record["fields"]
        semantic_names = set()
        for fid, f in enumerate(fields):
            keys(f, FIELD_KEYS)
            integer(f["id"])
            integer(f["serialTypeId"])
            require(f["id"] == fid, "field order/id")
            require(f["serialTypeId"] in (8, 9, 10, 104, 120), "unsupported local type")
            for name in ("semanticName", "effectiveName") + NAMES:
                string(f[name])
            require(f["semanticName"] not in semantic_names, "duplicate semantic name")
            semantic_names.add(f["semanticName"])
            require(f["effectiveName"] == f["localName"] and f["fullName"] == record["name"] + "." + f["localName"], "effective names")
        slots = record["javaSlots"]
        keys(slots, "int obj")
        heads = []
        for cat, values in slots.items():
            array(values)
            for ordinal, slot in enumerate(values):
                keys(slot, SLOT_KEYS)
                integer(slot["ordinal"])
                integer(slot["serialTypeId"])
                require(slot["ordinal"] == ordinal, "slot ordinal")
                for name in NAMES:
                    string(slot[name])
                require(slot["fullName"] not in full_names, "duplicate slot full name")
                full_names.add(slot["fullName"])
                fid = slot["fieldId"]
                if fid is not None:
                    integer(fid)
                    require(fid < len(fields), "dangling field")
                if slot["role"] == "head":
                    require(fid is not None, "head owner")
                    f = fields[fid]
                    require(all(slot[k] == f[k] for k in NAMES + ("serialTypeId",)), "head projection")
                    require(cat == ("obj" if f["serialTypeId"] in (9, 10) else "int"), "head category")
                    heads.append(fid)
                    if f["serialTypeId"] in (104, 120):
                        require(ordinal + 1 < len(values) and values[ordinal + 1]["role"] == "tail", "missing tail")
                elif slot["role"] == "tail":
                    require(cat == "int" and ordinal > 0 and slot["serialTypeId"] == 0 and fid is not None, "tail kind")
                    head = values[ordinal - 1]
                    require(head["role"] == "head" and head["serialTypeId"] in (104, 120) and head["fieldId"] == fid, "tail owner")
                    require(slot["fullName"] == head["fullName"] + "$VoidTail" and slot["localName"] == head["localName"] + "$VoidTail" and slot["propertyName"] == slot["localName"], "tail names")
                else:
                    require(slot["role"] == "index-placeholder" and cat == "int" and ordinal < 2 and record["hasTime"] and fid is None and slot["serialTypeId"] == 0, "placeholder ownership")
                    require([slot[k] for k in NAMES] == [record["name"] + ".$VoidTimeField", "$VoidTimeField", "$VoidTimeField"], "placeholder names")
        require(sorted(heads) == list(range(len(fields))), "one head per field")
        if record["hasTime"]:
            require(len(record["indexes"]) == 2 and len(slots["int"]) >= 2, "time positions")
            require(any(i["fieldId"] is not None for i in record["indexes"]), "all indexes absent")
        else:
            require(record["indexes"] == [], "unexpected indexes")
        owners = []
        for pos, index in enumerate(record["indexes"]):
            keys(index, "position fieldId")
            integer(index["position"])
            require(index["position"] == pos, "index position")
            slot = slots["int"][pos]
            require(index["fieldId"] == slot["fieldId"], "index target")
            if index["fieldId"] is not None:
                integer(index["fieldId"])
                owners.append(index["fieldId"])
                require(slot["role"] == "head" and slot["serialTypeId"] == 8, "index type")
            else:
                require(slot["role"] == "index-placeholder", "index placeholder")
        require(len(owners) == len(set(owners)), "duplicate index")
        lookup_keys = set()
        for lookup in record["lookups"]:
            keys(lookup, "scope name category ordinal")
            string(lookup["name"])
            integer(lookup["ordinal"])
            scope, cat = lookup["scope"], lookup["category"]
            require(scope in ("record", "scheme-int", "scheme-obj") and cat in slots, "lookup scope/category")
            require(lookup["ordinal"] < len(slots[cat]), "dangling lookup")
            require(scope == "record" or scope == "scheme-" + cat, "scheme category")
            key = (scope, lookup["name"])
            require(key not in lookup_keys, "duplicate lookup")
            lookup_keys.add(key)
            if scope != "record":
                require(key not in scheme_keys, "duplicate scheme lookup")
                scheme_keys.add(key)
        lookup_map = {(x["scope"], x["name"]): (x["category"], x["ordinal"]) for x in record["lookups"]}
        for cat, values in slots.items():
            for s in values:
                for scope, name in (("record", s["fullName"]), ("record", s["localName"]), ("scheme-" + cat, s["fullName"])):
                    require(lookup_map.get((scope, name)) == (cat, s["ordinal"]), "required lookup")


def compare(doc, case):
    """Independent checks of JSON members against raw TSV columns, without XML parsing."""
    p = doc["provenance"]
    evidence = (EXAMPLES / p["expectationRef"]).resolve()
    require(evidence == FIXTURES / "expected" / (case["id"] + ".tsv"), "case evidence")
    rows = [line.split("\t") for line in evidence.read_text().splitlines()]
    require([r[1].replace("<INPUT>/", "") for r in rows if r[0] == "input"] == case["inputs"], "TSV input order")
    require([(EXAMPLES / i["ref"]).resolve() for i in p["inputs"]] == [FIXTURES / "inputs" / f for f in case["inputs"]], "input order")
    for item in p["inputs"]:
        require(hashlib.sha256((EXAMPLES / item["ref"]).read_bytes()).hexdigest() == item["sha256"], "source bytes")
    require(p["profileEvidence"] == ["../../../tests/fixtures/schema/captures/final-a/metadata.json", "../../../tests/fixtures/schema/captures/final-a/environment.tsv"], "profile evidence order")
    for ref in p["profileEvidence"]:
        require((EXAMPLES / ref).is_file(), "missing profile evidence")
    success = any(r[0] == "runtime" for r in rows)
    require(success == (doc["kind"] == "local-schema"), "outcome")
    if not success:
        require([d["oracleStage"] for d in doc["diagnostics"]] == [r[1] for r in rows if r[0] == "error"], "attempts")
        code, subject, typ = {
            "S01-no-urls": ("no-source-inputs", "inputs", None),
            "S05-disabled-both": ("all-index-fields-disabled", "Indexed.indexes", None),
            "S15-byte": ("unsupported-local-type", "Unsupported.Value", 1),
            "S15-short": ("unsupported-local-type", "Unsupported.Value", 3),
        }[case["id"]]
        for d in doc["diagnostics"]:
            require((d["code"], d["subject"], d["serialTypeId"], d["evidenceRef"]) == (code, subject, typ, p["expectationRef"]), "diagnostic evidence")
        require(any(r[0] == "model" for r in rows) == (typ is not None or code == "all-index-fields-disabled"), "recognition evidence")
        return
    records = doc["records"]
    require(len(records) == int(next(r[1] for r in rows if r[0] == "runtime")), "record count")
    models, disabled = {}, set()
    current = None
    for row in rows:
        if row[0] == "model.record":
            current = row[1]
            models[current] = []
            if row[2] == "true":
                disabled.add(current)
        elif row[0] == "model.field" and row[5] == "false":
            models[current].append((row[1], row[7], int(row[3])))
    require([r["name"] for r in records] == [r[2] for r in rows if r[0] == "generation" and r[2] not in disabled], "generation order")
    actual_slots, expected_slots = [], []
    for record in records:
        require([(f["semanticName"], f["effectiveName"], f["serialTypeId"]) for f in record["fields"]] == models[record["name"]], "logical declaration order/types/names")
        for cat, values in record["javaSlots"].items():
            for s in values:
                actual_slots.append((record["id"], cat, s["ordinal"], *(s[k] for k in NAMES), s["serialTypeId"]))
    current = None
    for row in rows:
        if row[0] == "record":
            current = records[int(row[1])]
            require((current["name"], current["hasTime"], len(current["javaSlots"]["int"]), len(current["javaSlots"]["obj"])) == (row[2], row[4] == "true", int(row[5]), int(row[6])), "record projection")
        elif row[0] == "slot":
            expected_slots.append((current["id"], row[1], int(row[2]), *row[3:6], int(row[6])))
        elif row[0] == "record.lookup":
            found = next((str(r["id"]) for r in records if r["name"] == row[1]), "\\N")
            require(found == row[2], "record lookup probe")
        elif row[0] == "lookup":
            for scope, expected in zip(("record", "scheme-int", "scheme-obj"), row[2:5]):
                candidates = [current] if scope == "record" else records
                found = [r["javaSlots"][l["category"]][l["ordinal"]]["fullName"] for r in candidates for l in r["lookups"] if (l["scope"], l["name"]) == (scope, row[1])]
                require(found == ([] if expected == "\\N" else [expected]), "lookup probe: " + row[1])
    require(actual_slots == expected_slots, "exact slot projection/order")


def main():
    cases = load(FIXTURES / "cases.json")
    require(len(cases) == 17, "17-case baseline")
    require({p.stem for p in EXAMPLES.glob("*.json")} == {c["id"] for c in cases}, "example matrix")
    docs = {}
    for case in cases:
        doc = load(EXAMPLES / (case["id"] + ".json"))
        validate(doc)
        compare(doc, case)
        docs[case["id"]] = doc
    mutations = [
        ("unknown format", "S15-slots", lambda d: d.update(formatVersion=2)),
        ("unknown profile", "S15-slots", lambda d: d["profile"].update(version=2)),
        ("unknown member", "S15-slots", lambda d: d["records"][0].update(wireId=0)),
        ("boolean ID", "S15-slots", lambda d: d["records"][0].update(id=False)),
        ("null collection", "S15-slots", lambda d: d["records"][0].update(fields=None)),
        ("dangling owner", "S15-slots", lambda d: d["records"][0]["javaSlots"]["int"][0].update(fieldId=999)),
        ("wrong tail owner", "S15-slots", lambda d: d["records"][0]["javaSlots"]["int"][2].update(fieldId=0)),
        ("slot order", "S15-slots", lambda d: d["records"][0]["javaSlots"]["int"].reverse()),
        ("duplicate lookup", "S15-slots", lambda d: d["records"][0]["lookups"].append(d["records"][0]["lookups"][0])),
        ("index target", "S05-first", lambda d: d["records"][0]["indexes"][1].update(fieldId=0)),
        ("failure as success", "S15-byte", lambda d: d.update(records=[])),
        ("recognition stage", "S15-byte", lambda d: d["diagnostics"][0].update(recognition="not-reached")),
    ]
    for label, cid, mutate in mutations:
        doc = copy.deepcopy(docs[cid])
        mutate(doc)
        try:
            validate(doc)
        except ValueError:
            continue
        raise ValueError("mutation accepted: " + label)
    try:
        json.loads('{"id":0,"id":1}', object_pairs_hook=unique)
    except ValueError:
        pass
    else:
        raise ValueError("duplicate key accepted")
    print("PASS: 17 examples (13 schemas, 4 failures); TSV inputs, outcomes, fields, slots and lookup probes; 13 negative checks.")


if __name__ == "__main__":
    main()
