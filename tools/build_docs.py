#!/usr/bin/env python3
"""Build the single-page HTML documentation and ZIP from the tracked Markdown sources.

Requires Python 3.10+ and tools/docs-requirements.txt. Run from any directory.
Every Markdown document becomes one section of index.html, plus a release and
version log from CHANGELOG.md and git; the stylesheet is inlined and images stay local. The firmware is read for the share bundle, never modified.
"""
from __future__ import annotations

import hashlib
import html
import json
import os
import re
import subprocess
import unicodedata
from datetime import date
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
    ("docs/ESP32_TEST_CONFIG.md", "ESP32 test setup", "Complete bench wiring, Wi-Fi and acceptance checks"),
    ("docs/SCREEN_EXAMPLES.md", "Screen examples", "Rendered examples of every scene, boot and clock state"),
    ("docs/PHOTO_LOG.md", "Hardware photos", "Panel, connector, and component reference photos"),
    ("CHANGELOG.md", "Changelog", "Firmware and documentation changes"),
    ("CLAUDE.md", "Maintainer notes", "Architecture, constraints, and evidence limits"),
    ("images/README.md", "Image archive", "Image filenames and provenance"),
    ("docs/BRING_UP_REPORT.md", "Archived bring-up", "Historical investigation and panel replacement"),
]
OUTPUT = ROOT / "index.html"
STYLESHEET = ROOT / "docs/assets/docs.css"
MANIFEST = json.loads((ROOT / "images/manifest.json").read_text())
IMAGE_INFO = {ROOT / "images" / p["file"]: p for p in MANIFEST["photos"]}
EXAMPLES = json.loads((ROOT / "images/demos/manifest.json").read_text())
SCREEN_INFO = {ROOT / "images/demos" / item["file"]: item for item in EXAMPLES}
ARCHIVE = ROOT / "Red_LED_Panel_8x2_Documentation.zip"


def slug(text: str) -> str:
    text = unicodedata.normalize("NFKD", text).encode("ascii", "ignore").decode().lower()
    return re.sub(r"[^a-z0-9]+", "-", text).strip("-") or "section"


SECTION_IDS = {(ROOT / source).resolve(): slug(label) for source, label, _ in PAGES}


def rewrite_url(url: str, source: Path) -> str:
    """Point links at sections of the single page, or at files relative to the project root."""
    parts = urlsplit(url)
    if parts.scheme or parts.netloc:
        return url
    if not parts.path:
        # Same-document anchor: prefix it with this document's section id.
        return f"#{SECTION_IDS[source]}--{parts.fragment}" if parts.fragment else url
    target = (source.parent / unquote(parts.path)).resolve()
    if target in SECTION_IDS:
        section = SECTION_IDS[target]
        return f"#{section}--{parts.fragment}" if parts.fragment else f"#{section}"
    path = quote(Path(os.path.relpath(target, ROOT)).as_posix(), safe="/-._~")
    return urlunsplit(("", "", path, parts.query, parts.fragment))


def render_section(source: Path, label: str, description: str) -> tuple[str, str, list]:
    md = MarkdownIt("commonmark", {"html": False}).enable("table")
    tokens = md.parse(source.read_text())
    section = SECTION_IDS[source]
    headings = []
    used = {}
    title = label
    for i, token in enumerate(tokens):
        if token.type in ("heading_open", "heading_close"):
            # Demote one level: the page title is the only h1.
            level = int(token.tag[1])
            token.tag = f"h{min(level + 1, 6)}"
        if token.type == "heading_open":
            inline = tokens[i + 1]
            text = md.renderer.renderInlineAsText(inline.children or [], md.options, {})
            base = slug(text)
            used[base] = used.get(base, 0) + 1
            anchor = base if used[base] == 1 else f"{base}-{used[base]}"
            if token.tag == "h2":
                title = text
                token.attrSet("id", section)
                token.attrSet("class", "doc-title")
            else:
                token.attrSet("id", f"{section}--{anchor}")
                if token.tag == "h3":
                    headings.append((text, f"{section}--{anchor}"))
        if token.type == "inline":
            for child in token.children or []:
                key = "href" if child.type == "link_open" else "src" if child.type == "image" else None
                if key:
                    old = child.attrGet(key)
                    if old:
                        child.attrSet(key, rewrite_url(old, source))

    original_image = md.renderer.rules["image"]
    def image_rule(items, index, options, env):
        item = items[index]
        src = item.attrGet("src") or ""
        image_path = (ROOT / unquote(urlsplit(src).path)).resolve()
        info = IMAGE_INFO.get(image_path) or SCREEN_INFO.get(image_path)
        if info:
            item.attrSet("width", str(info["width"]))
            item.attrSet("height", str(info["height"]))
        item.attrSet("loading", "lazy")
        item.attrSet("decoding", "async")
        image = original_image(items, index, options, env)
        return f'<a class="image-link" href="{html.escape(src, quote=True)}" target="_blank" rel="noopener" aria-label="Open image at available full size">{image}</a>'
    md.renderer.rules["image"] = image_rule
    md.renderer.rules["table_open"] = lambda *_: '<div class="table-wrap"><table>\n'
    md.renderer.rules["table_close"] = lambda *_: '</table></div>\n'
    body = md.renderer.render(tokens, md.options, {})

    markdown = source.relative_to(ROOT).as_posix()
    meta = (f'<p class="doc-meta"><span class="eyebrow">{html.escape(label)}</span> · '
            f'{html.escape(description)} · <a href="{quote(markdown)}">{html.escape(markdown)}</a></p>')
    # Place the label/source line directly under the section title.
    body = re.sub(r"(</h2>\n)", r"\1" + meta.replace("\\", r"\\") + "\n", body, count=1)
    return f'<section class="doc-section" aria-labelledby="{section}">\n{body}</section>\n', title, headings


def git(*args: str) -> str | None:
    try:
        return subprocess.run(["git", *args], cwd=ROOT, capture_output=True, text=True,
                              check=True).stdout.strip()
    except (OSError, subprocess.CalledProcessError):
        return None


def render_release_log() -> str:
    """Release table from CHANGELOG.md headings plus git tags and commit history."""
    version = re.search(r'FIRMWARE_VERSION = "([^"]+)"', (ROOT / "include/config.h").read_text()).group(1)
    tags = {}
    for tag in (git("tag", "--list") or "").split():
        commit = git("rev-list", "-n", "1", tag)
        if commit:
            tags[tag.lstrip("v")] = (tag, commit[:7])
    rows = []
    changelog = (ROOT / "CHANGELOG.md").read_text()
    for match in re.finditer(r"^## \[([^\]]+)\][ -]*(\S*)\s*$", changelog, re.M):
        name, when = match.groups()
        body = changelog[match.end():].split("\n## ", 1)[0]
        groups = re.findall(r"^### (.+)$", body, re.M)
        tag, commit = tags.get(name, ("", ""))
        if tag:
            status = f'<span class="tag resolved">Tagged {html.escape(tag)}</span> <code>{commit}</code>'
        elif name == version:
            status = '<span class="tag">Current source, untagged</span>'
        else:
            status = "Pre-release"
        rows.append(f"<tr><td><strong>{html.escape(name)}</strong></td><td>{html.escape(when or '—')}</td>"
                    f"<td>{status}</td><td>{html.escape(', '.join(groups) or '—')}</td></tr>")
    head = git("rev-parse", "--short", "HEAD")
    dirty = bool(git("status", "--porcelain"))
    build = (f"source commit <code>{html.escape(head)}</code>{' plus uncommitted changes' if dirty else ''}"
             if head else "source outside a git checkout")
    log = git("log", "--date=short", "--pretty=%h%x1f%ad%x1f%s%x1f%D") or ""
    commits = []
    for line in log.splitlines():
        short, when, subject, *rest = line.split("\x1f")  # strip() can remove an empty refs field
        refs = rest[0] if rest else ""
        labels = " ".join(f'<span class="tag">{html.escape(r.removeprefix("tag: "))}</span>'
                          for r in refs.split(", ") if r.startswith("tag: "))
        commits.append(f"<tr><td><code>{short}</code></td><td>{when}</td>"
                       f"<td>{html.escape(subject)} {labels}</td></tr>")
    history = ("<div class=\"table-wrap\"><table><thead><tr><th>Commit</th><th>Date</th><th>Change</th></tr>"
               f"</thead><tbody>{''.join(commits)}</tbody></table></div>" if commits
               else "<p>No git history was available when this page was built.</p>")
    return f'''<section class="doc-section" aria-labelledby="release-log">
<h2 id="release-log" class="doc-title">Release and version log</h2>
<p class="doc-meta"><span class="eyebrow">Releases</span> · Generated from CHANGELOG.md and git · built {date.today().strftime("%d-%m-%Y")} from {build}</p>
<p>Firmware in this source tree reports <strong>v{html.escape(version)}</strong> (<code>FIRMWARE_VERSION</code> in <code>include/config.h</code>). Tagged versions are the releases flashed to the reference panel; details of each change are in the <a href="#changelog">changelog</a>.</p>
<h3 id="release-log--releases">Releases</h3>
<div class="table-wrap"><table><thead><tr><th>Version</th><th>Date</th><th>Status</th><th>Change groups</th></tr></thead>
<tbody>{''.join(rows)}</tbody></table></div>
<h3 id="release-log--commit-history">Commit history</h3>
{history}
</section>
'''


def render_page() -> str:
    sections = []
    nav = []
    contents = []
    for filename, label, description in PAGES:
        source = (ROOT / filename).resolve()
        section_html, title, headings = render_section(source, label, description)
        sections.append(section_html)
        section = SECTION_IDS[source]
        links = "".join(f'<a href="#{anchor}">{html.escape(text)}</a>' for text, anchor in headings)
        nav.append(f'<details class="nav-group"><summary><a href="#{section}">{html.escape(label)}</a></summary>'
                   f'<nav aria-label="{html.escape(label, quote=True)} sections">{links}</nav></details>'
                   if headings else f'<a class="nav-single" href="#{section}">{html.escape(label)}</a>')
        contents.append(f'<a href="#{section}"><strong>{html.escape(label)}</strong>'
                        f'<span>{html.escape(description)}</span></a>')
        if filename == "README.md":
            sections.append(render_release_log())
            nav.append('<details class="nav-group"><summary><a href="#release-log">Release log</a></summary>'
                       '<nav aria-label="Release log sections"><a href="#release-log--releases">Releases</a>'
                       '<a href="#release-log--commit-history">Commit history</a></nav></details>')
            contents.append('<a href="#release-log"><strong>Release log</strong>'
                            '<span>Versions, tags and commit history</span></a>')
    led_dots = '<i></i>' * 16
    css = STYLESHEET.read_text()
    return f'''<!doctype html>
<html lang="en-AU">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<meta name="description" content="Red LED Panel 8×2 — complete offline documentation">
<meta name="color-scheme" content="light">
<title>Red LED Panel 8×2 · Documentation</title>
<style>
{css}</style>
</head>
<body id="top">
<a class="skip" href="#main">Skip to document</a>
<aside class="sidebar">
<a class="brand" href="#top"><div class="led-mark" aria-hidden="true">{led_dots}</div><strong>RED / 8×2</strong><span>ESP32 · HUB08 · 64×16</span></a>
<div class="nav-label">Documentation</div>
<div class="nav-links">{''.join(nav)}</div>
<div class="sidebar-note"><strong>64×16 pixels · eleven scenes</strong><br>1–9 / g / c select · p pauses<br>+ / − adjusts brightness<br>Offline edition · no remote assets.</div>
</aside>
<main class="page" id="main">
<div class="toolbar"><span class="eyebrow">Workshop reference · single-page edition</span><div class="tools"><button type="button" onclick="window.print()">Print</button></div></div>
<article class="doc-body">
<header class="cover">
<div class="status-line"><span class="tag resolved">ESP32 showcase</span><span class="tag">Text · pixels · animation</span></div>
<h1>Red LED Panel 8×2 documentation</h1>
<p>Everything for the ESP32 and red 64×16 HUB08 panel in one page: setup, wiring, controls, examples, releases, history and maintainer notes.</p>
<nav class="contents" aria-label="Contents">{''.join(contents)}</nav>
</header>
{''.join(sections)}
<footer class="footer">Red LED Panel 8×2 · Setup, scenes, and drawing.<br>Generated from the Markdown sources with <code>tools/build_docs.py</code>. <a href="#top">Back to top</a></footer>
</article>
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


def check_links(page: Path) -> int:
    parser = References()
    parser.feed(page.read_text())
    count = 0
    for ref in parser.refs:
        url = urlsplit(ref)
        if url.scheme or url.netloc:
            continue
        if url.path:
            path = (page.parent / unquote(url.path)).resolve()
            if not path.is_relative_to(ROOT):
                raise ValueError(f"Link escapes documentation root: {ref}")
            if not path.is_file():
                raise ValueError(f"Missing local target: {ref}")
        elif url.fragment and unquote(url.fragment) not in parser.ids:
            raise ValueError(f"Missing anchor: {ref}")
        count += 1
    return count


def main():
    for path, info in IMAGE_INFO.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != info["sha256"]:
            raise SystemExit(f"Photo checksum mismatch: {path.relative_to(ROOT)}")
    for path, info in SCREEN_INFO.items():
        if hashlib.sha256(path.read_bytes()).hexdigest() != info["sha256"]:
            raise SystemExit(f"Screen example checksum mismatch: {path.relative_to(ROOT)}")
    OUTPUT.write_text(render_page())
    count = check_links(OUTPUT)
    bundled = {OUTPUT, STYLESHEET} | {ROOT / source for source, _, _ in PAGES} | set(IMAGE_INFO)
    bundled |= {
        ROOT / "images/manifest.json", ROOT / "tools/build_docs.py", ROOT / "tools/docs-requirements.txt",
        ROOT / "platformio.ini", ROOT / ".clang-format", ROOT / ".editorconfig",
    }
    for directory in ("src", "include", "web", "test_host", "tools", "images/demos"):
        bundled |= {path for path in (ROOT / directory).rglob("*")
                    if path.is_file() and "__pycache__" not in path.parts
                    and path.suffix not in (".pyc",) and path.name != ".DS_Store"}
    with zipfile.ZipFile(ARCHIVE, "w", zipfile.ZIP_DEFLATED) as archive:
        for path in sorted(bundled):
            archive.write(path, "Red_LED_Panel_8x2_Documentation/" + path.relative_to(ROOT).as_posix())
    print(f"Built {OUTPUT.name} from {len(PAGES)} documents; verified {len(IMAGE_INFO)} photos, "
          f"{len(SCREEN_INFO)} screen examples and {count} local references.")
    print(f"Share bundle: {ARCHIVE.name} ({ARCHIVE.stat().st_size:,} bytes)")


if __name__ == "__main__":
    main()
