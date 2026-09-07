import argparse
import json
import shutil
from pathlib import Path


def component(entity, type_name):
    for c in entity.get("m_native_components", []):
        if c["m_type_name"] == type_name:
            return c
    return None


def parent_uuid(entity):
    c = component(entity, "HierarchyComponent")
    return json.loads(c["m_component"]).get("parent_uuid", 0) if c else 0


def subtree(root, children_of):
    out = [root]
    stack = [root["m_uuid"]]
    while stack:
        u = stack.pop()
        for c in children_of.get(u, []):
            out.append(c)
            stack.append(c["m_uuid"])
    return out


def dump_comp(obj):
    return json.dumps(obj, ensure_ascii=False, sort_keys=True, separators=(",", ":"))


def export(scene_path, prefab_rel, root_uuid):
    scene_path = Path(scene_path)
    prefab_dir = scene_path.parents[1] / "Prefabs"
    prefab_dir.mkdir(parents=True, exist_ok=True)

    data = json.load(open(scene_path, encoding="utf-8"))
    ents = data["m_entities"]
    index = {e["m_uuid"]: i for i, e in enumerate(ents)}

    root = ents[index[root_uuid]]
    name = root["m_name"]

    children_of = {}
    for e in ents:
        p = parent_uuid(e)
        if p:
            children_of.setdefault(p, []).append(e)

    sub = subtree(root, children_of)

    hc = component(root, "HierarchyComponent")
    if hc:
        h = json.loads(hc["m_component"])
        h["parent_uuid"] = 0
        hc["m_component"] = dump_comp(h)

    prefab = {"m_entities": sub, "m_name": name}
    prefab_path = prefab_dir / f"{name}.prefab"
    with open(prefab_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(prefab, f, ensure_ascii=False, indent=4)
        f.write("\n")

    tc = json.loads(component(root, "TransformComponent")["m_component"])
    inst = {
        "position": tc["position"],
        "prefab": {"asset_id": 0, "legacy_path": prefab_rel, "sub_object_id": 0},
        "rotation": tc["rotation"],
        "scale": tc["scale"],
    }
    marker = {
        "m_managed_components": root.get("m_managed_components", []),
        "m_name": root["m_name"],
        "m_native_components": [
            component(root, "IDComponent"),
            {"m_component": dump_comp(inst), "m_type_name": "PrefabInstanceComponent"},
        ],
        "m_uuid": root["m_uuid"],
    }

    sub_ids = {e["m_uuid"] for e in sub}
    pos = index[root_uuid]
    cleaned = [e for e in ents if e["m_uuid"] not in sub_ids]
    cleaned.insert(pos, marker)
    ents[:] = cleaned

    ids = {e["m_uuid"] for e in ents}
    dangling = [e["m_name"] for e in ents if (p := parent_uuid(e)) and p not in ids]

    print(f"prefab: {prefab_path} ({len(sub)} entities)")
    print(f"scene entities: {len(ents)} dangling: {dangling}")

    with open(scene_path, "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, ensure_ascii=False, indent=4)
        f.write("\n")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("scene", help="path to .doscn scene file")
    parser.add_argument("root_uuid", type=int, help="uuid of the subtree root entity")
    parser.add_argument("--prefab", required=True, help="prefab path relative to Assets, e.g. Prefabs/Marry.prefab")
    parser.add_argument("--backup", action="store_true", help="write scene.bak before modifying")
    args = parser.parse_args()

    if args.backup:
        shutil.copyfile(args.scene, str(args.scene) + ".bak")
    export(args.scene, args.prefab, args.root_uuid)


if __name__ == "__main__":
    main()
