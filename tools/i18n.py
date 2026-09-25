#!/usr/bin/env python3
"""Translated landing pages for site/.

  python3 tools/i18n.py extract   # English page -> i18n/en.json (the catalogue)
  python3 tools/i18n.py build     # i18n/<lang>.json -> site/<dir>/index.html, 404 strings, sitemap.xml

The English page is the template: every element with its own text becomes one catalogue entry
(its inner HTML, so inline markup such as <em> and <code> stays with the sentence), plus the
metadata and accessible names. Elements marked translate="no" (the quickstart, the formula) are
left alone; a descendant marked translate="yes" is still translated. Translations are keyed by
entry id; a missing id falls back to English and is reported. Same scheme as genlayerlabs.com.
Needs beautifulsoup4 (pip install beautifulsoup4).
"""
import hashlib, json, os, re, sys
from pathlib import Path
from bs4 import BeautifulSoup, NavigableString, Comment

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / 'site'
I18N = Path(os.environ.get('I18N_DIR', ROOT / 'i18n'))
ORIGIN = 'https://subzeroclaw.com/'
# code, directory, og:locale, name
LANGS = [('en', '', 'en_US', 'English'), ('es', 'es/', 'es_ES', 'Español'), ('ko', 'ko/', 'ko_KR', '한국어'),
         ('zh-Hans', 'zh/', 'zh_CN', '简体中文'), ('ru', 'ru/', 'ru_RU', 'Русский'), ('tr', 'tr/', 'tr_TR', 'Türkçe')]
# names that stay as they are in every language
KEEP = {'SubZeroClaw', 'GenLayer Labs', 'GitHub ↗', 'v0.1 · MIT'} | {n for *_, n in LANGS} \
    | {'EN', 'ES', 'KO', 'ZH', 'RU', 'TR'}
META = [('meta', 'name', 'description'), ('meta', 'property', 'og:title'), ('meta', 'property', 'og:description'),
        ('meta', 'property', 'og:image:alt'), ('meta', 'name', 'twitter:title'), ('meta', 'name', 'twitter:description'),
        ('meta', 'name', 'twitter:image:alt')]
ATTRS = ['aria-label', 'data-copied']
SKIP_TAGS = {'script', 'style', 'svg', 'select', 'option'}
# strings the 404 page swaps in by path prefix (it is one file for every locale)
NOT_FOUND = ["this path ran somewhere we can't reach."]


def sid(s):
    return hashlib.sha1(s.encode()).hexdigest()[:8]


def norm(s):
    return re.sub(r'\s+', ' ', s).strip()


def inner(el):
    return norm(el.decode_contents())


def plain(el):
    return norm(el.get_text())


def load(path):
    return BeautifulSoup(path.read_text(), 'html.parser')


def where(el):
    parts = []
    for p in [el, *el.parents]:
        if p.name in (None, '[document]', 'html', 'body'):
            continue
        parts.append(p.name + ('.' + '.'.join(p['class']) if p.get('class') else '') + ('#' + p['id'] if p.get('id') else ''))
        if p.get('id') and p is not el:
            break
    return ' < '.join(parts[:4])


def segments(soup):
    """Elements that own text, outermost first; each is translated as one unit."""
    out = []

    def walk(el, blocked):
        for c in el.children:
            if isinstance(c, NavigableString) or c.name in SKIP_TAGS:
                continue
            if c.get('translate') == 'yes':
                out.append(c)
            elif blocked or c.get('translate') == 'no':
                walk(c, True)
            elif any(isinstance(t, NavigableString) and not isinstance(t, Comment) and t.strip() for t in c.children):
                out.append(c)
            else:
                walk(c, False)
    walk(soup.body, False)
    return out


def translatable(text):
    return text and text not in KEEP and not re.fullmatch(r'[\d\s.,–$€%/+:~-]+', text)


def jsonld(soup):
    tag = soup.find('script', type='application/ld+json')
    return tag, json.loads(tag.string)


def extract():
    soup = load(SITE / 'index.html')
    items, seen = [], {}

    def add(en, ctx, kind):
        if en in seen:
            seen[en]['where'].append(ctx)
            return
        seen[en] = {'id': sid(en), 'kind': kind, 'en': en, 'where': [ctx]}
        items.append(seen[en])

    add(norm(soup.title.string), 'head <title>', 'meta')
    for tag, attr, val in META:
        add(norm(soup.find(tag, attrs={attr: val})['content']), f'head meta {val}', 'meta')
    _, ld = jsonld(soup)
    add(ld['description'], 'structured data: description', 'meta')
    for el in segments(soup):
        if translatable(plain(el)):
            add(inner(el), where(el), 'html')
    for at in ATTRS:
        for el in soup.body.find_all(attrs={at: True}):
            if translatable(el[at]):
                add(norm(el[at]), where(el) + f' [{at}]', 'attr')
    for s in NOT_FOUND:
        add(s, '404.html message', 'text')
    I18N.mkdir(exist_ok=True)
    (I18N / 'en.json').write_text(json.dumps(items, ensure_ascii=False, indent=1) + '\n')
    print(f'{len(items)} entries -> i18n/en.json')


def build():
    cat = json.loads((I18N / 'en.json').read_text())
    not_found = {}
    for code, d, og, _ in LANGS[1:]:
        src = I18N / f'{d.strip("/")}.json'
        if not src.exists():
            print(f'{code}: no {src.name}, skipped')
            continue
        tx = json.loads(src.read_text())
        T = {e['en']: tx.get(e['id'], '').strip() for e in cat}
        missing = [e['id'] for e in cat if not T[e['en']]]
        t = lambda en: T.get(en) or en
        soup = load(SITE / 'index.html')
        soup.html['lang'] = code
        url = ORIGIN + d
        soup.title.string = t(norm(soup.title.string))
        for tag, attr, val in META:
            el = soup.find(tag, attrs={attr: val}); el['content'] = t(norm(el['content']))
        soup.find('meta', property='og:locale')['content'] = og
        soup.find('meta', property='og:url')['content'] = url
        soup.find('link', rel='canonical')['href'] = url
        if (SITE / f'og-{d.strip("/")}.png').exists():
            for el in [soup.find('meta', property='og:image'), soup.find('meta', attrs={'name': 'twitter:image'})]:
                el['content'] = f'{ORIGIN}og-{d.strip("/")}.png'
        tag, ld = jsonld(soup)
        ld.update(description=t(ld['description']), inLanguage=code, url=url)
        tag.string = '\n' + json.dumps(ld, ensure_ascii=False, indent=2) + '\n'
        for el in segments(soup):
            if translatable(plain(el)):
                new = T.get(inner(el))
                if new:
                    el.clear(); el.append(BeautifulSoup(new, 'html.parser'))
        for at in ATTRS:
            for el in soup.body.find_all(attrs={at: True}):
                el[at] = t(norm(el[at]))
        # Russian typesetting: a dash never starts a line
        if code == 'ru':
            for s in soup.body.find_all(string=True):
                if ' —' in s and s.parent.name not in SKIP_TAGS:
                    s.replace_with(s.replace(' —', '\u00a0—'))
        # keep each stats separator on the line of the item before it
        for s in soup.select('p.stats')[0].find_all(string=True):
            s.replace_with(s.replace(' · ', '\u00a0· '))
        # this page sits one folder down: relative links to other locales go up a level
        for a in soup.select('.foot-langs a'):
            a['href'] = '../' + a['href'][2:]
            if a.has_attr('aria-current'):
                del a['aria-current']
            if a['hreflang'] == code:
                a['aria-current'] = 'page'
        for o in soup.select('select[data-lang] option'):
            o['value'] = '../' + o['value'][2:]
            if o['lang'] == code:
                o['selected'] = ''
            elif o.has_attr('selected'):
                del o['selected']
        out = SITE / d
        out.mkdir(exist_ok=True)
        (out / 'index.html').write_text(str(soup))
        not_found[d.strip('/')] = {'lang': code, 'home': '/' + d, 'msg': t(NOT_FOUND[0])}
        print(f'{code}: {d}index.html, {len(missing)} missing' + (f' {missing[:8]}' if missing else ''))
    # 404: one page for every path; it picks its language from the first path segment
    nf = SITE / '404.html'
    html = nf.read_text()
    block = ('<script id="i18n-404">\n/* generated by tools/i18n.py build */\n(function(){\n  var L='
             + json.dumps(not_found, ensure_ascii=False) + ';\n'
             "  var k=location.pathname.split('/')[1], s=L[k]; if(!s) return;\n"
             "  document.documentElement.lang=s.lang;\n"
             "  document.querySelector('p').textContent=s.msg;\n"
             "  document.querySelector('a').setAttribute('href', s.home);\n})();\n</script>")
    html = re.sub(r'<script id="i18n-404">.*?</script>', lambda m: block, html, flags=re.S) if 'id="i18n-404"' in html \
        else html.replace('</body>', block + '\n</body>')
    nf.write_text(html)
    # sitemap: every locale, each listing all alternates
    alts = ''.join(f'    <xhtml:link rel="alternate" hreflang="{c}" href="{ORIGIN}{d}"/>\n' for c, d, *_ in LANGS) \
        + f'    <xhtml:link rel="alternate" hreflang="x-default" href="{ORIGIN}"/>\n'
    lastmod = re.search(r'<lastmod>(.*?)</lastmod>', (SITE / 'sitemap.xml').read_text()).group(1)
    urls = ''.join(f'  <url>\n    <loc>{ORIGIN}{d}</loc>\n    <lastmod>{lastmod}</lastmod>\n{alts}  </url>\n' for _, d, *_ in LANGS)
    (SITE / 'sitemap.xml').write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" '
                                      'xmlns:xhtml="http://www.w3.org/1999/xhtml">\n' + urls + '</urlset>\n')


if __name__ == '__main__':
    {'extract': extract, 'build': build}[sys.argv[1] if len(sys.argv) > 1 else 'extract']()
