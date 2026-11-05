#!/usr/bin/env python3
"""Export the public roadmap from GitHub issues labelled `roadmap`.

Usage:  scripts/roadmap/export_roadmap.py [--out PATH] [--dry-run]
Needs the `gh` CLI, authenticated. Reads site-only data (translations, layer,
category) from scripts/roadmap/overlay.json, keyed by "owner/repo#N".

Status (from GitHub, no Project needed):
  closed                         -> completed
  open + any open blocker        -> blocked
  open + label status:review     -> review
  open + label status:progress   -> in-progress
  otherwise                      -> planned
Safe on failure: nothing is written unless the whole graph validates.
"""
import os, json, subprocess, sys, argparse, pathlib, datetime

REPOS = ["marinaMoji/marinaMoji", "marinaMoji/plugin"]
LABEL = os.environ.get("ROADMAP_LABEL", "roadmap")
HERE = pathlib.Path(__file__).resolve().parent
OVERLAY = HERE / "overlay.json"
DEFAULT_OUT = pathlib.Path("roadmap.json")
MAX_NODES = 200

QUERY = """query($o:String!,$n:String!,$l:[String!]){repository(owner:$o,name:$n){
 issues(first:100,labels:$l){nodes{number title state url
  labels(first:20){nodes{name}} milestone{title} assignees(first:3){nodes{login}}
  blockedBy(first:30){nodes{number state repository{nameWithOwner}}}
  subIssuesSummary{total completed}}}}}"""

def fetch(repo):
    o, n = repo.split("/")
    r = subprocess.run(["gh", "api", "graphql", "-f", f"query={QUERY}", "-f", f"o={o}",
                        "-f", f"n={n}", "-F", f"l[]={LABEL}"],
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"GitHub query failed for {repo}: {r.stderr.strip()}")
    return json.loads(r.stdout)["data"]["repository"]["issues"]["nodes"]

def key(repo, num): return f"{repo}#{num}"

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default=str(DEFAULT_OUT))
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    overlay = json.loads(OVERLAY.read_text()) if OVERLAY.exists() else {}

    issues = {}
    for repo in REPOS:
        for i in fetch(repo):
            issues[key(repo, i["number"])] = (repo, i)
    if not issues:
        sys.exit("No roadmap issues returned; refusing to overwrite snapshot.")
    if len(issues) > MAX_NODES:
        sys.exit(f"Too many nodes ({len(issues)}).")

    warnings, nodes = [], []
    for k, (repo, i) in sorted(issues.items()):
        ov = overlay.get(k, {})
        labels = {l["name"] for l in i["labels"]["nodes"]}
        blockers = [key(b["repository"]["nameWithOwner"], b["number"]) for b in i["blockedBy"]["nodes"]]
        open_blockers = [b for b in i["blockedBy"]["nodes"] if b["state"] == "OPEN"]
        if i["state"] == "CLOSED": status = "completed"
        elif "status:blocked" in labels: status = "blocked"
        elif "status:progress" in labels: status = "in-progress"
        elif open_blockers: status = "blocked"
        elif "status:review" in labels: status = "review"
        elif "status:progress" in labels: status = "in-progress"
        else: status = "planned"
        deps = [b for b in blockers if b in issues]
        for b in blockers:
            if b not in issues:
                warnings.append(f"{k}: blocker {b} lacks the '{LABEL}' label; arrow omitted")
        if not ov.get("title_en"):
            ov = {**ov, "title_en": i["title"]}
        for lang in ("fr", "ja"):
            if not ov.get(f"title_{lang}"):
                warnings.append(f"{k}: no {lang} title; falling back to English")
        s = i["subIssuesSummary"]
        nodes.append({
            "id": k.replace("marinaMoji/marinaMoji", "issue").replace("marinaMoji/", "").replace("/", "-").replace("#", "-").lower(),
            "ref": k, "github_url": i["url"], "state": i["state"], "status": status,
            "milestone": (i["milestone"] or {}).get("title"),
            "assignee": (i["assignees"]["nodes"] or [{}])[0].get("login"),
            "progress": {"done": s["completed"], "total": s["total"]} if s["total"] else None,
            "category": ov.get("category"), "type": ov.get("type", "capability"),
            "layer": ov.get("layer"),
            **{f"title_{l}": ov.get(f"title_{l}") or ov["title_en"] for l in ("en", "fr", "ja")},
            **{f"description_{l}": ov.get(f"description_{l}") for l in ("en", "fr", "ja")},
            "_deps": deps,
        })

    ids = {n["ref"]: n["id"] for n in nodes}
    for n in nodes:
        n["depends_on"] = ",".join(ids[d] for d in n.pop("_deps")) or None

    # cycle check (DFS)
    dep = {n["id"]: (n["depends_on"] or "").split(",") if n["depends_on"] else [] for n in nodes}
    state = {}
    def visit(x, path):
        if state.get(x) == 1: warnings.append("cycle: " + " -> ".join(path + [x])); return
        if state.get(x) == 2: return
        state[x] = 1
        for y in dep[x]: visit(y, path + [x])
        state[x] = 2
    for x in dep: visit(x, [])
    for n in nodes:
        if n["layer"] is None:
            warnings.append(f"{n['ref']}: no 'layer' in overlay; renderer will need a default")

    out = {"generated": datetime.datetime.now(datetime.timezone.utc).isoformat(timespec="seconds"),
           "nodes": nodes}
    for w in warnings: print("warning:", w, file=sys.stderr)
    text = json.dumps(out, indent=2, ensure_ascii=False) + "\n"
    if a.dry_run: print(text)
    else:
        p = pathlib.Path(a.out); p.parent.mkdir(parents=True, exist_ok=True)
        tmp = p.with_suffix(".tmp"); tmp.write_text(text); tmp.replace(p)
        print(f"wrote {p} ({len(nodes)} nodes)")

main()
