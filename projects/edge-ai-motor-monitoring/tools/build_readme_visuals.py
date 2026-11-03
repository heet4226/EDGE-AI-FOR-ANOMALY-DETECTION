#!/usr/bin/env python3
"""
Build the clickable, animated visuals shown in this project's README.

GitHub READMEs cannot run JavaScript, but they do show SVG images (including their
animations), and every image can be a link. From results/model_comparison.csv and
tools/viz_config.json this script writes:

  assets/viz/pipeline-{light,dark}.svg   animated pipeline (links to the interactive page)
  assets/viz/lb-head-{light,dark}.svg    leaderboard header + legend
  assets/viz/rows/NN-{light,dark}.svg    one bar per model; each row links to the model's folder
  assets/viz/cards/*-{light,dark}.svg    dataset and output cards
  models/NN_name/README.md               per-model page (only the generated block is replaced)
  <pages_dir>/viz_data.json              the same numbers for the interactive page

and replaces the block between <!-- VIZ:START --> and <!-- VIZ:END --> in README.md.

Standard library only:  python tools/build_readme_visuals.py
"""
import csv, json, math, re, html
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CFG = json.loads((ROOT / "tools" / "viz_config.json").read_text(encoding="utf-8"))
OUT = ROOT / "assets" / "viz"
for sub in ("rows", "cards"):
    (OUT / sub).mkdir(parents=True, exist_ok=True)

THEMES = {  # GitHub's own page colours, so the images blend into the README
    "light": dict(i=0, bg="#FFFFFF", panel="#F6F8FA", ink="#1F2328", ink2="#59636E", ink3="#818B98", rule="#D1D9E0", track="#EAEEF2", ok="#1A7F37"),
    "dark":  dict(i=1, bg="#0D1117", panel="#151B23", ink="#E6EDF3", ink2="#9198A1", ink3="#6E7681", rule="#3D444D", track="#212830", ok="#3FB950"),
}
SANS = "-apple-system,BlinkMacSystemFont,'Segoe UI','Noto Sans',Helvetica,Arial,sans-serif"
MONO = "ui-monospace,SFMono-Regular,'SF Mono',Menlo,Consolas,'Liberation Mono',monospace"
E = lambda s: html.escape(str(s), quote=True)

# ---------------------------------------------------------------- data
FAM = {f["id"]: f for f in CFG["families"]}
MET = CFG["metric"]
KEY, HIB = MET["key"], MET.get("higher_is_better", True)


def num(v):
    v = (v or "").strip()
    try:
        return float(v)
    except ValueError:
        return None


models = []
with open(ROOT / CFG["results_csv"], newline="", encoding="utf-8") as f:
    for r in csv.DictReader(f):
        m = {k: num(v) for k, v in r.items()}
        m.update(id=int(r["id"]), name=r["name"], fam=r["family"], folder=r["folder"].strip().rstrip("/"),
                 settings=r.get("settings", ""), note=r.get("note", ""), deployed=r.get("deployed", "yes").strip().lower() != "no")
        if m["fam"] not in FAM:
            raise SystemExit(f"Unknown family '{m['fam']}' for {m['name']}: add it to viz_config.json")
        models.append(m)
ranked = sorted(models, key=lambda m: -(m[KEY] or 0) if HIB else (m[KEY] or 1e18))
for k, m in enumerate(ranked, 1):
    m["rank"] = k
best = ranked[0]
vmax = MET.get("scale_max") or max(m[KEY] or 0 for m in models) * 1.02


def fmt_metric(v):
    return "–" if v is None else f"{v:.{MET.get('decimals', 2)}f}{MET.get('suffix', '')}"


def right_text(m):
    rr = CFG["row_right"]
    try:
        return rr["template"].format(**{k: (v if v is not None else float("nan")) for k, v in m.items()}) if m["deployed"] else rr["fallback"]
    except (KeyError, ValueError, TypeError):
        return rr.get("fallback", "")


def short(name):
    return re.sub(r"\s*\(.*?\)", "", name)


def is_file(p):
    return bool(re.search(r"\.(py|ipynb|r|m|jl|c)$", p, re.I))


def gh_link(p):
    return p if is_file(p) else p + "/"


def style(t, extra=""):
    return (f"<style>.s{{font-family:{SANS}}} .m{{font-family:{MONO}}} .t{{fill:{t['ink']}}} .t2{{fill:{t['ink2']}}} .t3{{fill:{t['ink3']}}}"
            f"{extra}@media (prefers-reduced-motion: reduce){{*{{animation:none!important}}}}</style>")


def write(name, svg):
    (OUT / name).write_text(svg, encoding="utf-8")


def pic(path_noext, alt, width=None):
    w = f' width="{width}"' if width else ""
    return (f'<picture><source media="(prefers-color-scheme: dark)" srcset="{path_noext}-dark.svg">'
            f'<img src="{path_noext}-light.svg" alt="{E(alt)}"{w}></picture>')


# ---------------------------------------------------------------- pipeline (animated)
def column(items, x, w, h, top, bottom):
    n = len(items)
    gap = (bottom - top - n * h) / max(1, n - 1) if n > 1 else 0
    y0 = top if n > 1 else (top + bottom - h) / 2
    return [dict(it, x=x, y=y0 + k * (h + gap), w=w, h=h) for k, it in enumerate(items)]


def cubic(p0, p1):
    (x1, y1), (x2, y2) = p0, p1
    if abs(x1 - x2) < 1:
        return f"L{x2:.1f} {y2:.1f}", [(x1, y1), (x2, y2)]
    dx = (x2 - x1) * 0.5
    c1, c2 = (x1 + dx, y1), (x2 - dx, y2)
    pts = []
    for i in range(21):
        t = i / 20
        a, b, c, d = (1 - t) ** 3, 3 * (1 - t) ** 2 * t, 3 * (1 - t) * t ** 2, t ** 3
        pts.append((a * x1 + b * c1[0] + c * c2[0] + d * x2, a * y1 + b * c1[1] + c * c2[1] + d * y2))
    return f"C{c1[0]:.1f} {c1[1]:.1f} {c2[0]:.1f} {c2[1]:.1f} {x2:.1f} {y2:.1f}", pts


def plen(pts):
    return sum(math.dist(pts[i], pts[i + 1]) for i in range(len(pts) - 1))


def pipeline(theme):
    t = THEMES[theme]; ci = t["i"]
    W, H, T, B = 1200, 500, 48, 462
    X = [18, 262, 506, 780, 1034]; WD = [190, 190, 220, 200, 150]
    cols = []
    for k, spec in enumerate(CFG["pipeline"]):
        if spec["kind"] == "models":
            items = [dict(f, kind="fam") for f in CFG["families"]]
            boxes = column(items, X[k], WD[k], 86 if len(items) <= 4 else 74, T, B)
        else:
            items = [dict(b, kind=spec["kind"]) for b in spec["boxes"]]
            if spec.get("add_best"):
                items.append({"name": "Best model", "detail": short(best["name"]), "big": fmt_metric(best[KEY]), "unit": MET["label"], "kind": "out", "best": True})
            h = {"data": 92, "prep": 104, "eval": 112, "out": 112}[spec["kind"]]
            pad = {"data": 0, "prep": 12, "eval": 60, "out": 0}[spec["kind"]]
            boxes = column(items, X[k], WD[k], h, T + pad, B - pad)
        cols.append((spec["title"], boxes))
    data, prep, fams, ev, out = [c[1] for c in cols]
    C = lambda b: (b["x"] + b["w"] / 2, b["y"] + b["h"] / 2)

    css = [f".wire{{fill:none;stroke:{t['rule']};stroke-width:1.4}}", f".box{{fill:{t['bg']};stroke:{t['rule']};stroke-width:1.2}}"]
    for f in CFG["families"]:
        css.append(f"@keyframes b_{f['id']}{{0%,22%,100%{{fill:{t['track']}}}6%{{fill:{f['color'][ci]}}}}}"
                   f".l_{f['id']}{{fill:{t['track']};animation:b_{f['id']} 3.2s infinite}}")
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">', style(t, "".join(css)),
         f'<rect width="{W}" height="{H}" rx="12" fill="{t["panel"]}" stroke="{t["rule"]}"/>']
    for title, boxes in cols:
        s.append(f'<text x="{boxes[0]["x"]}" y="32" class="m t3" font-size="11" letter-spacing="1">{E(title)}</text>')

    segs = set()
    for d in data: segs.add((C(d), C(prep[0])))
    for a, b in zip(prep, prep[1:]): segs.add((C(a), C(b)))
    for f in fams: segs.add((C(prep[-1]), C(f))); segs.add((C(f), C(ev[0])))
    for a, b in zip(ev, ev[1:]): segs.add((C(a), C(b)))
    for o in out: segs.add((C(ev[-1]), C(o)))
    for a, b in sorted(segs):
        s.append(f'<path class="wire" d="M{a[0]:.1f} {a[1]:.1f} {cubic(a, b)[0]}"/>')

    n_dots, dur = 16, 7.0
    for k in range(n_dots):
        d = data[k % len(data)]; f = fams[(k * 3) % len(fams)]; o = out[(k * 2) % len(out)]
        chain = [d] + prep + [f] + ev + [o]
        pts = [C(b) for b in chain]
        path, lens, at_f = f"M{pts[0][0]:.1f} {pts[0][1]:.1f}", [], 0
        for i in range(len(pts) - 1):
            seg, sp = cubic(pts[i], pts[i + 1]); path += " " + seg; lens.append(plen(sp))
            if chain[i + 1] is f: at_f = sum(lens)
        c1, c2 = d["color"][ci], f["color"][ci]
        begin = -(k * dur / n_dots)
        s.append(f'<circle r="4.5" fill="{c1}" stroke="{t["panel"]}" stroke-width="1.5">'
                 f'<animateMotion dur="{dur}s" begin="{begin:.2f}s" repeatCount="indefinite" path="{path}"/>'
                 f'<animate attributeName="fill" dur="{dur}s" begin="{begin:.2f}s" repeatCount="indefinite" calcMode="discrete" values="{c1};{c2}" keyTimes="0;{at_f/sum(lens):.3f}"/></circle>')

    for title, boxes in cols:
        for b in boxes:
            x, y, w, h = b["x"], b["y"], b["w"], b["h"]
            color = b["color"][ci] if b.get("color") else (t["ok"] if b.get("best") else t["ink3"])
            s.append(f'<rect class="box" x="{x}" y="{y:.1f}" width="{w}" height="{h}" rx="8"/>'
                     f'<rect x="{x}" y="{y+10:.1f}" width="3" height="{h-20}" rx="1.5" fill="{color}"/>'
                     f'<text x="{x+14}" y="{y+22:.1f}" class="s t" font-size="14" font-weight="600">{E(b["name"])}</text>')
            if b["kind"] == "fam":
                ms = [m for m in models if m["fam"] == b["id"]]
                s.append(f'<text x="{x+14}" y="{y+38:.1f}" class="m t3" font-size="10.5">{len(ms)} models</text>')
                for j, m in enumerate(ms):
                    s.append(f'<circle cx="{x+20+j*17}" cy="{y+56:.1f}" r="5.5" class="l_{b["id"]}" stroke="{t["rule"]}" style="animation-delay:{(m["id"]*0.73)%3.2:.2f}s"/>')
                bf = max(ms, key=lambda m: (m[KEY] or 0) if HIB else -(m[KEY] or 1e18))
                s.append(f'<text x="{x+w-12}" y="{y+h-12:.1f}" text-anchor="end" class="m t2" font-size="10.5">best {fmt_metric(bf[KEY])}</text>')
            else:
                s.append(f'<text x="{x+14}" y="{y+38:.1f}" class="m t3" font-size="10.5">{E(b.get("detail",""))}</text>'
                         f'<text x="{x+14}" y="{y+h-16:.1f}" class="s t" font-size="24" font-weight="700">{E(b["big"])}'
                         f'<tspan class="m t2" font-size="10.5" font-weight="400" dx="6">{E(b["unit"])}</tspan></text>')
    s.append(f'<text x="{W-18}" y="{H-14}" text-anchor="end" class="m t3" font-size="11">▶ CLICK FOR THE INTERACTIVE VERSION</text></svg>')
    write(f"pipeline-{theme}.svg", "\n".join(s))


# ---------------------------------------------------------------- leaderboard
RW, RH, BX0, BX1 = 880, 32, 250, 560


def lb_head(theme):
    t = THEMES[theme]; ci = t["i"]
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {RW} 52" width="{RW}" height="52">', style(t),
         f'<text x="8" y="18" class="m t3" font-size="11" letter-spacing="1">{len(models)} MODELS · {E(MET["label"].upper())} · '
         f'{"HIGHER" if HIB else "LOWER"} IS BETTER · CLICK A ROW FOR CODE &amp; RESULTS</text>']
    x = 8
    for f in CFG["families"]:
        s.append(f'<rect x="{x}" y="31" width="10" height="10" rx="2" fill="{f["color"][ci]}"/><text x="{x+16}" y="40" class="s t2" font-size="12.5">{E(f["name"])}</text>')
        x += 30 + len(f["name"]) * 7.2
    s.append(f'<text x="{RW-8}" y="40" text-anchor="end" class="m t3" font-size="10.5">{E(CFG["row_right"]["header"])}</text></svg>')
    write(f"lb-head-{theme}.svg", "\n".join(s))


def lb_row(m, theme):
    t = THEMES[theme]; col = FAM[m["fam"]]["color"][t["i"]]
    v = m[KEY] or 0
    bw = max(3, (BX1 - BX0) * v / vmax)
    hl = m is best
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {RW} {RH}" width="{RW}" height="{RH}">', style(t),
         f'<rect width="{RW}" height="{RH}" rx="6" fill="{t["panel"] if hl else t["bg"]}"/>',
         f'<line x1="0" x2="{RW}" y1="{RH-.5}" y2="{RH-.5}" stroke="{t["track"]}"/>',
         f'<text x="10" y="21" class="m t3" font-size="11">{m["rank"]:02d}</text>',
         f'<rect x="38" y="11" width="10" height="10" rx="2" fill="{col}"/>',
         f'<text x="56" y="21" class="s t" font-size="13.5" font-weight="{600 if hl else 400}">{E(m["name"])}</text>',
         f'<rect x="{BX0}" y="9" width="{BX1-BX0}" height="14" rx="3" fill="{t["track"]}" opacity=".55"/>',
         f'<rect x="{BX0}" y="9" width="{bw:.1f}" height="14" rx="3" fill="{col}"/>',
         f'<text x="{BX0+bw+8:.1f}" y="21" class="m t" font-size="12" font-weight="600">{fmt_metric(m[KEY])}{" ★" if hl else ""}</text>',
         f'<text x="{RW-30}" y="21" text-anchor="end" class="m {"t2" if m["deployed"] else "t3"}" font-size="11.5">{E(right_text(m))}</text>',
         f'<text x="{RW-12}" y="21" text-anchor="end" class="s t3" font-size="14">›</text></svg>']
    write(f"rows/{m['id']:02d}-{theme}.svg", "\n".join(s))


# ---------------------------------------------------------------- cards
def card(slug, title, big, unit, detail, color, theme, w=290, h=108):
    t = THEMES[theme]
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {w} {h}" width="{w}" height="{h}">', style(t),
         f'<rect x=".5" y=".5" width="{w-1}" height="{h-1}" rx="10" fill="{t["panel"]}" stroke="{t["rule"]}"/>',
         f'<rect x="0" y="14" width="4" height="{h-28}" rx="2" fill="{color}"/>',
         f'<text x="18" y="28" class="s t" font-size="14" font-weight="600">{E(title)}</text>',
         f'<text x="18" y="46" class="m t3" font-size="10.5">{E(detail)}</text>',
         f'<text x="18" y="{h-20}" class="s t" font-size="28" font-weight="700">{E(big)}<tspan class="m t2" font-size="11" font-weight="400" dx="8">{E(unit)}</tspan></text>',
         f'<text x="{w-16}" y="{h-20}" text-anchor="end" class="s t3" font-size="16">›</text></svg>']
    write(f"cards/{slug}-{theme}.svg", "\n".join(s))


# ---------------------------------------------------------------- per-model README
def fmt_field(m, key, f):
    v = m.get(key)
    if v is None:
        return "–"
    try:
        return f.format(v)
    except (ValueError, TypeError):
        return str(v)


def model_readme(m):
    if is_file(m["folder"]):
        return
    folder = ROOT / m["folder"]
    folder.mkdir(parents=True, exist_ok=True)
    depth = "../" * len(Path(m["folder"]).parts)
    code = sorted(p for p in folder.iterdir() if p.is_file() and p.suffix.lower() in (".py", ".ipynb", ".c", ".h"))
    figs = sorted((folder / "figures").glob("*.png")) if (folder / "figures").exists() else []
    fields = [(lab, fmt_field(m, key, f)) for lab, key, f in CFG["card_fields"]]
    fields = [x for x in fields if x[1] != "–"]
    L = [f"# {m['name']}", "",
         f"**Rank {m['rank']} of {len(models)}** by {MET['label'].lower()} · {FAM[m['fam']]['name']} · "
         + ("runs on the STM32H573" if m["deployed"] else "**not deployable on the MCU**")
         + f" · [← all models]({depth}README.md#all-models)", "",
         pic(f"{depth}assets/viz/rows/{m['id']:02d}", f"{m['name']} result"), "",
         "| " + " | ".join(l for l, _ in fields) + " |", "|" + "---:|" * len(fields), "| " + " | ".join(v for _, v in fields) + " |", ""]
    if CFG.get("baseline_note"): L += [f"<sub>{CFG['baseline_note']}</sub>", ""]
    if m["settings"]: L += [f"**Model:** {m['settings']}", ""]
    if m["note"]: L += [f"> {m['note']}", ""]
    if code: L += ["### Code", ""] + [f"- [`{p.name}`]({p.name})" for p in code] + [""]
    if figs:
        L += ["### Results", ""]
        L += [f'<a href="figures/{p.name}"><img src="figures/{p.name}" width="49%" alt="{m["name"]} result plot {k+1}"></a>' for k, p in enumerate(figs[:6])]
        if len(figs) > 6:
            L += ["", "<details><summary>More plots (" + str(len(figs) - 6) + ")</summary>", ""]
            L += [f'<a href="figures/{p.name}"><img src="figures/{p.name}" width="49%"></a>' for p in figs[6:]] + ["", "</details>"]
        L += [""]
    replace_block(folder / "README.md", "\n".join(L), default_after="\n")


def replace_block(path, body, default_after=""):
    start, end = "<!-- VIZ:START (generated by tools/build_readme_visuals.py, do not edit by hand) -->", "<!-- VIZ:END -->"
    block = f"{start}\n{body}\n{end}"
    rx = re.compile(r"<!-- VIZ:START.*?-->.*?<!-- VIZ:END -->", re.S)
    if path.exists():
        txt = path.read_text(encoding="utf-8")
        txt = rx.sub(lambda _: block, txt) if rx.search(txt) else txt.rstrip() + "\n\n" + block + "\n"
    else:
        txt = block + default_after
    path.write_text(txt, encoding="utf-8")


# ---------------------------------------------------------------- main README block
def main_readme():
    L = [f'<a href="{CFG["pages_url"]}">{pic("assets/viz/pipeline", "Animated pipeline. Click to open the interactive version", "100%")}</a>', "",
         f'<p align="center"><a href="{CFG["pages_url"]}"><b>▶ Open the interactive overview</b></a></p>', "",
         '<h2 id="all-models">All 22 models, ranked</h2>', "",
         f"<sub>{CFG.get('baseline_note','')} Click a row to open the model's notebook, metrics and plots.</sub>", "",
         pic("assets/viz/lb-head", "Legend", "100%") + "<br>"]
    for m in ranked:
        src = "assets/viz/rows/%02d" % m["id"]
        alt = "%d. %s: %s" % (m["rank"], m["name"], fmt_metric(m[KEY]))
        L.append(f'<a href="{gh_link(m["folder"])}">{pic(src, alt, "100%")}</a><br>')
    L += ["", "<h2>Dataset</h2>", "", "<p>"]
    for k, g in enumerate(CFG["data_cards"]):
        L.append(f'<a href="{CFG["data_link"]}">{pic(f"assets/viz/cards/data{k}", g["name"], "32%")}</a>')
    L += ["</p>", "", "<h2>Hardware and software</h2>", "", "<p>"]
    outs = [b for spec in CFG["pipeline"] if spec["kind"] == "out" for b in spec["boxes"]]
    for k, o in enumerate(outs):
        L.append(f'<a href="{o["link"]}">{pic(f"assets/viz/cards/out{k}", o["name"], "32%")}</a>')
    L.append(f'<a href="{gh_link(best["folder"])}">{pic("assets/viz/cards/best", "Best model", "32%")}</a>')
    L.append("</p>")
    replace_block(ROOT / "README.md", "\n".join(L))
    return outs


def portfolio_card(theme):
    """Card for the portfolio landing page (repo root README)."""
    pc = CFG.get("portfolio_card")
    if not pc:
        return
    t = THEMES[theme]; ci = t["i"]
    W, H = 880, 282
    s = [f'<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 {W} {H}" width="{W}" height="{H}">', style(t),
         f'<rect x=".5" y=".5" width="{W-1}" height="{H-1}" rx="14" fill="{t["panel"]}" stroke="{t["rule"]}"/>',
         f'<text x="28" y="40" class="m t3" font-size="11" letter-spacing="1">{E(pc["eyebrow"])}</text>',
         f'<text x="28" y="72" class="s t" font-size="25" font-weight="700">{E(CFG["title"])}</text>',
         f'<text x="28" y="98" class="s t2" font-size="14">{E(pc["subtitle"])}</text>']
    for k, (big, lab) in enumerate(pc["stats"]):
        x = 28 + k * 128
        s.append(f'<text x="{x}" y="150" class="s t" font-size="26" font-weight="700">{E(big)}</text><text x="{x}" y="170" class="s t2" font-size="12">{E(lab)}</text>')
    # mini leaderboard (top 5)
    x0, y0 = 560, 122
    s.append(f'<text x="{x0}" y="{y0-6}" class="m t3" font-size="10.5" letter-spacing="1">TOP 5 · {E(MET["label"].upper())}</text>')
    for k, m in enumerate(ranked[:5]):
        y = y0 + 6 + k * 19; bw = 120 * (m[KEY] or 0) / vmax
        s.append(f'<text x="{x0}" y="{y+11}" class="s t2" font-size="11.5">{E(short(m["name"]))}</text>'
                 f'<rect x="{x0+148}" y="{y+2}" width="{bw:.1f}" height="11" rx="2" fill="{FAM[m["fam"]]["color"][ci]}"/>'
                 f'<text x="{x0+154+bw:.1f}" y="{y+11}" class="m t" font-size="11">{fmt_metric(m[KEY])}</text>')
    x = 28
    for tag in pc["tags"]:
        w = 16 + len(tag) * 6.6
        s.append(f'<rect x="{x}" y="232" width="{w:.0f}" height="24" rx="12" fill="{t["bg"]}" stroke="{t["rule"]}"/><text x="{x+w/2:.0f}" y="248" text-anchor="middle" class="m t2" font-size="11">{E(tag)}</text>')
        x += w + 8
    s.append(f'<text x="{W-24}" y="248" text-anchor="end" class="s t3" font-size="13">Open project ›</text></svg>')
    d = (ROOT / pc["out_dir"]).resolve(); d.mkdir(parents=True, exist_ok=True)
    (d / f"{pc['slug']}-{theme}.svg").write_text("\n".join(s), encoding="utf-8")


def page_json():
    d = (ROOT / CFG["pages_dir"]).resolve()
    d.mkdir(parents=True, exist_ok=True)
    keys = ["name", "fam", "test_score", "cv_f1", "detection", "false_alarm", "class_acc", "flash_kib", "ram_kib", "infer_ms", "macc", "train_s", "deployed", "settings", "note", "folder"]
    data = dict(repoUrl=CFG["repo_url"], branch=CFG.get("branch", "main"), projectPath=CFG.get("project_path", ""),
                models=[{k: m[k] for k in keys} for m in sorted(models, key=lambda m: m["id"])])
    (d / "viz_data.json").write_text(json.dumps(data, ensure_ascii=False, indent=1), encoding="utf-8")


if __name__ == "__main__":
    outs = [b for spec in CFG["pipeline"] if spec["kind"] == "out" for b in spec["boxes"]]
    for th, t in THEMES.items():
        ci = t["i"]
        pipeline(th); lb_head(th)
        for m in models: lb_row(m, th)
        for k, g in enumerate(CFG["data_cards"]):
            card(f"data{k}", g["name"], g["big"], g["unit"], g["detail"], g["color"][ci], th)
        for k, o in enumerate(outs):
            card(f"out{k}", o["name"], o["big"], o["unit"], o["detail"], t["ink3"], th)
        card("best", "Best model · " + short(best["name"]), fmt_metric(best[KEY]), MET["label"], best["folder"], t["ok"], th)
        portfolio_card(th)
    for m in models: model_readme(m)
    main_readme(); page_json()
    print(f"Built visuals for {len(models)} models. Best: {best['name']} ({fmt_metric(best[KEY])}).")
