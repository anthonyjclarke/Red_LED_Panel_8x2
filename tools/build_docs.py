#!/usr/bin/env python3
"""Build the local HTML documentation and ZIP from the tracked Markdown sources.

Requires Python 3.10+ and tools/docs-requirements.txt. Run from any directory.
All assets stay local. The firmware is read for the share bundle, never modified.
"""
from __future__ import annotations

import hashlib
import html
import json
import os
import re
import unicodedata
import zipfile
from html.parser import HTMLParser
from pathlib import Path
from urllib.parse import quote, unquote, urlsplit, urlunsplit

try:
    from markdown_it import MarkdownIt
except ImportError:
    raise SystemExit("Install documentation dependencies: python -m pip install -r tools/docs-requirements.txt")

ROOT = Path(__file__).resolve().parents[1]
PAGES = [
    ("README.md", "Overview", "Project overview and quick start"),
    ("docs/PANEL_GUIDE.md", "Panel guide", "Hardware, wiring, firmware, and daily use"),
    ("docs/PHOTO_LOG.md", "Hardware photos", "Panel, connector, and component reference photos"),
    ("CHANGELOG.md", "Changelog", "Firmware and documentation changes"),
    ("CLAUDE.md", "Maintainer notes", "Architecture, constraints, and evidence limits"),
    ("images/README.md", "Image archive", "Image filenames and provenance"),
    ("docs/BRING_UP_REPORT.md", "Archived bring-up", "Historical investigation and panel replacement"),
]
OUTPUTS = {ROOT / source: (ROOT / source).with_suffix(".html") for source, _, _ in PAGES}
MANIFEST = json.loads((ROOT / "images/manifest.json").read_text())
IMAGE_INFO = {ROOT / "images" / p["file"]: p for p in MANIFEST["photos"]}
ARCHIVE = ROOT / "Red_LED_Panel_8x2_Documentation.zip"


def relative(target: Path, parent: Path) -> str:
    return quote(Path(os.path.relpath(target, parent)).as_posix(), safe="/-._~")


def rewrite_url(url: str, source: Path, output: Path) -> str:
    parts = urlsplit(url)
    if parts.scheme or parts.netloc or not parts.path:
        return url
    target = (source.parent / unquote(parts.path)).resolve()
    if target in OUTPUTS:
        return urlunsplit(("", "", relative(OUTPUTS[target], output.parent), parts.query, parts.fragment))
    return url


def slug(text: str) -> str:
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]+", "-", text).strip("-") or "section"


def render_page(source: Path, output: Path, label: str, description: str) -> str:
    md = MarkdownIt("commonmark", {"html": False}).enable("table")
    tokens = md.parse(source.read_text())
    headings = []
    used = {}
    title = label
    for i, token in enumerate(tokens):
        if token.type == "heading_open":
            inline = tokens[i + 1]
            text = md.renderer.renderInlineAsText(inline.children or [], md.options, {})
            base = slug(text)
            used[base] = used.get(base, 0) + 1
            anchor = base if used[base] == 1 else f"{base}-{used[base]}"
            token.attrSet("id", anchor)
            if token.tag == "h1":
                title = text
            elif token.tag in ("h2", "h3"):
                headings.append((token.tag, text, anchor))
        if token.type == "inline":
            for child in token.children or []:
                key = "href" if child.type == "link_open" else "src" if child.type == "image" else None
                if key:
                    old = child.attrGet(key)
                    if old:
                        child.attrSet(key, rewrite_url(old, source, output))

    original_image = md.renderer.rules["image"]
    def image_rule(items, index, options, env):
        item = items[index]
        src = item.attrGet("src") or ""
        image_path = (output.parent / unquote(urlsplit(src).path)).resolve()
        info = IMAGE_INFO.get(image_path)
        if info:
            item.attrSet("width", str(info["width"]))
            item.attrSet("height", str(info["height"]))
        item.attrSet("loading", "eager")
        item.attrSet("decoding", "sync")
        image = original_image(items, index, options, env)
        return f'<a class="image-link" href="{html.escape(src, quote=True)}" target="_blank" rel="noopener" aria-label="Open photograph at available full size">{image}</a>'
    md.renderer.rules["image"] = image_rule
    md.renderer.rules["table_open"] = lambda *_: '<div class="table-wrap"><table>\n'
    md.renderer.rules["table_close"] = lambda *_: '</table></div>\n'
    article = md.renderer.render(tokens, md.options, {})

    nav = []
    for filename, name, _ in PAGES:
        target = OUTPUTS[ROOT / filename]
        current = ' aria-current="page"' if source == ROOT / filename else ""
        nav.append(f'<a href="{relative(target, output.parent)}"{current}>{html.escape(name)}</a>')
    toc_links = "\n".join(
        f'<a href="#{anchor}" class="{"sub" if tag == "h3" else "section"}">{html.escape(text)}</a>'
        for tag, text, anchor in headings
    )
    toc = f'<aside class="toc" aria-label="On this page"><div class="toc-title">On this page</div>{toc_links}</aside>' if headings else ""
    mobile_toc = f'<details class="mobile-toc"><summary>On this page</summary><nav aria-label="Section navigation">{toc_links}</nav></details>' if headings else ""
    home = relative(ROOT / "index.html", output.parent)
    css = relative(ROOT / "docs/assets/docs.css", output.parent)
    markdown = relative(source, output.parent)
    led_dots = '<i></i>' * 16
    return f'''<!doctype html>
<html lang="en-AU">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="{html.escape(description, quote=True)}">
<meta name="color-scheme" content="light">
<title>{html.escape(title)} · Red LED Panel 8×2</title>
<link rel="stylesheet" href="{css}">
</head>
<body id="top">
<a class="skip" href="#main">Skip to document</a>
<aside class="sidebar">
<a class="brand" href="{home}"><div class="led-mark" aria-hidden="true">{led_dots}</div><strong>RED / 8×2</strong><span>ESP32 · HUB08 · 64×16</span></a>
<div class="nav-label">Documentation</div>
<nav class="nav-links" aria-label="Documentation">{''.join(nav)}</nav>
<div class="sidebar-note"><strong>64×16 pixels · eleven scenes</strong><br>1–9 / g / c select · p pauses<br>+ / − adjusts brightness<br>Offline edition · no remote assets.</div>
</aside>
<main class="page" id="main">
<div class="toolbar"><span class="eyebrow">Workshop reference / {html.escape(label)}</span><div class="tools"><a href="{markdown}">Markdown source</a><button type="button" onclick="window.print()">Print</button></div></div>
<div class="doc-grid"><article>
<div class="status-line"><span class="tag resolved">ESP32 showcase</span><span class="tag">Text · pixels · animation</span></div>
{mobile_toc}
{article}
<footer class="footer">Red LED Panel 8×2 · Setup, scenes, and drawing.<br>Offline documentation generated with <code>tools/build_docs.py</code>. <a href="#top">Back to top</a></footer>
</article>{toc}</div>
</main>
</body>
</html>
'''


class References(HTMLParser):
    def __init__(self):
        super().__init__()
        self.ids = set()
        self.refs = []
    def handle_starttag(self, tag, attributes):
        values = dict(attributes)
        if "id" in values:
            if values["id"] in self.ids:
                raise ValueError(f"Duplicate HTML id: {values['id']}")
            self.ids.add(values["id"])
        for key in ("href", "src"):
            if values.get(key):
                self.refs.append(values[key])


def check_links(pages: list[Path]):
    parsed = {}
    def read(path):
        if path not in parsed:
            parser = References()
            parser.feed(path.read_text())
            parsed[path] = parser
        return parsed[path]
    count = 0
    for page in pages:
        for ref in read(page).refs:
            url = urlsplit(ref)
            if url.scheme or url.netloc:
                continue
            path = (page.parent / unquote(url.path)).resolve() if url.path else page
            if not path.is_relative_to(ROOT):
                raise ValueError(f"Link escapes documentation root: {page}: {ref}")
            if not path.is_file():
                raise ValueError(f"Missing local target: {page.relative_to(ROOT)} -> {ref}")
            if url.fragment and path.suffix == ".html" and unquote(url.fragment) not in read(path).ids:
                raise ValueError(f"Missing anchor: {page.relative_to(ROOT)} -> {ref}")
            count += 1
    return count


def main():
    for path, info in IMAGE_INFO.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != info["sha256"]:
            raise SystemExit(f"Photo checksum mismatch: {path.relative_to(ROOT)}")
    pages = []
    for filename, label, description in PAGES:
        source = ROOT / filename
        output = OUTPUTS[source]
        output.write_text(render_page(source, output, label, description))
        pages.append(output)
    index = ROOT / "index.html"
    index.write_text((ROOT / "README.html").read_text())
    pages.append(index)
    count = check_links(pages)
    bundled = set(pages) | set(OUTPUTS) | set(IMAGE_INFO)
    bundled |= {
        ROOT / "images/manifest.json", ROOT / "docs/assets/docs.css",
        ROOT / "tools/build_docs.py", ROOT / "tools/docs-requirements.txt",
        ROOT / "src/main.cpp", ROOT / "src/Hub08Panel.cpp", ROOT / "src/Hub08Panel.h",
        ROOT / "include/config.h", ROOT / "include/debug.h", ROOT / "platformio.ini",
    }
    with zipfile.ZipFile(ARCHIVE, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(bundled):
            archive.write(path, "Red_LED_Panel_8x2_Documentation/" + path.relative_to(ROOT).as_posix())
    print(f"Built {len(pages)} HTML pages; verified {len(IMAGE_INFO)} photos and {count} local references.")
    print(f"Share bundle: {ARCHIVE.name} ({ARCHIVE.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
