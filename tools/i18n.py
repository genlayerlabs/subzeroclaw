#!/usr/bin/env python3
"""Translated landing pages for site/, following the GenLayer multilingual-site playbook.

  python3 tools/i18n.py extract        # English page + JS strings -> i18n/en.json (the catalogue)
  python3 tools/i18n.py build          # i18n/<lang>.json -> site/<dir>/index.html, langbar.js, 404 strings,
                                       #   sitemap.xml, then i18n/build.lock.json (the fingerprint)
  node tools/og.cjs                    # share images og.png / og-<lang>.png from each built page's headline
  python3 tools/i18n.py check          # deploy guard: fingerprint and share images still match the sources
  python3 tools/i18n.py verify <base>  # every version served right: 200s, slash redirects, lang/title/canonical

English (site/index.html) is the single source. Every element with its own text becomes one catalogue
entry (its inner HTML, so inline markup such as <em> and <code> stays with the sentence), plus the
metadata, structured data, accessible names and every string the scripts show. Elements marked
translate="no" (the quickstart, the formula) are left alone; a descendant marked translate="yes" is
still translated. Ids are a hash of the English text, so an edited sentence shows up as untranslated.

The build refuses to write anything if the catalogue is stale, an id is missing, markup or protected
names changed, a description is too long, or a translated page still contains English. A string that is
deliberately identical to English is listed under "_same_as_english" in that language's file.

extract and build need beautifulsoup4 (pip install beautifulsoup4); check and verify need nothing.
"""
import hashlib, json, os, re, sys, urllib.request, urllib.error
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SITE = ROOT / 'site'
I18N = Path(os.environ.get('I18N_DIR', ROOT / 'i18n'))
LOCK = I18N / 'build.lock.json'
OG_LOCK = I18N / 'og.lock.json'  # written by tools/og.cjs
ORIGIN = 'https://subzeroclaw.com/'
# code, directory, og:locale, name
LANGS = [('en', '', 'en_US', 'English'), ('es', 'es/', 'es_ES', 'Español'), ('ko', 'ko/', 'ko_KR', '한국어'),
         ('zh-Hans', 'zh/', 'zh_CN', '简体中文'), ('ru', 'ru/', 'ru_RU', 'Русский'), ('tr', 'tr/', 'tr_TR', 'Türkçe')]
CJK = {'ko', 'zh-Hans'}
# names that stay as they are in every language (never catalogued)
KEEP = {'SubZeroClaw', 'GenLayer Labs', 'GitHub ↗', 'v0.1 · MIT'} | {n for *_, n in LANGS} \
    | {'EN', 'ES', 'KO', 'ZH', 'RU', 'TR'}
# names a translation must carry over verbatim whenever the English has them
PROTECT = ['SubZeroClaw', 'GenLayer Labs', 'GitHub', 'README', 'unhardcoded', 'skill.md', 'MIT']
META = [('meta', 'name', 'description'), ('meta', 'property', 'og:title'), ('meta', 'property', 'og:description'),
        ('meta', 'property', 'og:image:alt'), ('meta', 'name', 'twitter:title'), ('meta', 'name', 'twitter:description'),
        ('meta', 'name', 'twitter:image:alt')]
# search snippets: at most 150 characters, 80 in Chinese and Korean
LIMITS = {'description', 'og:description'}
ATTRS = ['aria-label', 'data-copied']
SKIP_TAGS = {'script', 'style', 'svg', 'select', 'option'}
# strings the 404 page swaps in by path prefix (it is one file for every locale)
NOT_FOUND = ["this path ran somewhere we can't reach."]
# the suggestion bar (langbar.js): shown in the language it offers, so every version carries every language
LANGBAR = [('msg', 'This page is also available in English.'), ('go', 'Read it in English'), ('close', 'Dismiss')]


def sid(s):
    return hashlib.sha1(s.encode()).hexdigest()[:8]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def norm(s):
    return re.sub(r'\s+', ' ', s).strip()


def soup_of(path):
    from bs4 import BeautifulSoup
    return BeautifulSoup(Path(path).read_text(), 'html.parser')


def inner(el):
    return norm(el.decode_contents())


def plain(el):
    return norm(el.get_text())


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
    from bs4 import NavigableString, Comment
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


def catalogue(soup):
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
    for key, s in LANGBAR:
        add(s, f'langbar.js {key}: suggestion bar, shown to visitors whose browser prefers this language', 'js')
    return items


def extract():
    items = catalogue(soup_of(SITE / 'index.html'))
    I18N.mkdir(exist_ok=True)
    (I18N / 'en.json').write_text(json.dumps(items, ensure_ascii=False, indent=1) + '\n')
    print(f'{len(items)} entries -> i18n/en.json')


def tags(s):
    return sorted(re.findall(r'<[^>]+>', s))


def validate(code, cat, tx):
    """Every id present, markup and placeholders kept, protected names untouched, snippets short enough."""
    errs = []
    same = set(tx.get('_same_as_english', []))
    for e in cat:
        t = (tx.get(e['id']) or '').strip()
        if not t:
            errs.append(f"{e['id']} missing ({e['where'][0]}): {e['en'][:60]}")
            continue
        if tags(t) != tags(e['en']):
            errs.append(f"{e['id']} markup differs: {tags(e['en'])} vs {tags(t)}")
        if sorted(re.findall(r'<code>(.*?)</code>', t)) != sorted(re.findall(r'<code>(.*?)</code>', e['en'])):
            errs.append(f"{e['id']} <code> contents changed")
        if sorted(re.findall(r'\{\w+\}', t)) != sorted(re.findall(r'\{\w+\}', e['en'])):
            errs.append(f"{e['id']} placeholders changed")
        for name in PROTECT:
            if name in e['en'] and name not in t:
                errs.append(f"{e['id']} lost the protected name {name!r}")
        if t == e['en'] and e['id'] not in same:
            errs.append(f"{e['id']} is still English (list it under _same_as_english if that is deliberate): {t[:60]}")
        limit = 80 if code in CJK else 150
        if any(w.split()[-1] in LIMITS for w in e['where'] if w.startswith('head meta')) and len(t) > limit:
            errs.append(f"{e['id']} {len(t)} chars, over the {limit}-character snippet limit")
    stale = set(tx) - {e['id'] for e in cat} - {'_same_as_english'}
    if stale:
        print(f'{code}: note, ids no longer in the catalogue (safe to delete): {sorted(stale)}')
    return errs


def english_left(code, soup, en_soup, allowed):
    """Text runs of the built page that are identical to the English page (the 'no English left behind' check)."""
    def runs(s):
        out = {plain(el) for el in segments(s)}
        out |= {norm(el[at]) for at in ATTRS for el in s.body.find_all(attrs={at: True})}
        out |= {norm(s.title.string)} | {norm(s.find(t, attrs={a: v})['content']) for t, a, v in META}
        return {r for r in out if translatable(r)}
    return sorted(runs(soup) & runs(en_soup) - allowed)


def build():
    from bs4 import BeautifulSoup
    en_soup = soup_of(SITE / 'index.html')
    cat = json.loads((I18N / 'en.json').read_text())
    fresh = catalogue(en_soup)
    if [(e['id'], e['kind']) for e in fresh] != [(e['id'], e['kind']) for e in cat]:
        new = sorted({e['id'] for e in fresh} - {e['id'] for e in cat})
        sys.exit(f'The English page changed since the last extract (new or changed ids: {new}). '
                 'Run `tools/i18n.py extract`, translate the ids it reports, then build.')
    by_en = {e['en']: e for e in cat}
    txs, errors = {}, []
    for code, d, *_ in LANGS[1:]:
        src = I18N / f'{d.strip("/")}.json'
        if not src.exists():
            errors.append(f'{code}: {src.name} does not exist')
            continue
        txs[code] = json.loads(src.read_text())
        errors += [f'{code}: {m}' for m in validate(code, cat, txs[code])]
    if errors:
        sys.exit('Build refused, nothing written:\n  ' + '\n  '.join(errors))

    not_found, bar, outputs = {}, {}, {}
    en_bar = {k: s for k, s in LANGBAR}
    bar['en'] = dict(en_bar, href='/')
    for code, d, og, _ in LANGS[1:]:
        tx = txs[code]
        T = {e['en']: tx[e['id']].strip() for e in cat}
        t = lambda en: T[en] if en in T else en
        soup = soup_of(SITE / 'index.html')
        soup.html['lang'] = code
        url = ORIGIN + d
        soup.title.string = t(norm(soup.title.string))
        for tag, attr, val in META:
            el = soup.find(tag, attrs={attr: val}); el['content'] = t(norm(el['content']))
        soup.find('meta', property='og:locale')['content'] = og
        # Bing reads content-language as a strong language signal; Baidu reads it and applicable-device
        cl = soup.find('meta', attrs={'http-equiv': 'content-language'})
        cl['content'] = code
        if code == 'zh-Hans':
            dev = soup.new_tag('meta', attrs={'name': 'applicable-device', 'content': 'pc,mobile'})
            cl.insert_after(dev)
            cl.insert_after('\n')
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
                new = T[inner(el)]
                el.clear(); el.append(BeautifulSoup(new, 'html.parser'))
        for at in ATTRS:
            for el in soup.body.find_all(attrs={at: True}):
                el[at] = t(norm(el[at]))
        # Russian typesetting: a dash never starts a line
        if code == 'ru':
            for s in soup.body.find_all(string=True):
                if ' —' in s and s.parent.name not in SKIP_TAGS:
                    s.replace_with(s.replace(' —', ' —'))
        # keep each stats separator on the line of the item before it
        for s in soup.select('p.stats')[0].find_all(string=True):
            s.replace_with(s.replace(' · ', ' · '))
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
        allowed = {e['en'] for e in cat if e['id'] in set(tx.get('_same_as_english', []))}
        allowed |= {re.sub(r'<[^>]+>', '', a) for a in allowed}
        left = english_left(code, soup, en_soup, allowed)
        if left:
            sys.exit(f'Build refused: {code} still shows English text: {left}')
        out = SITE / d
        out.mkdir(exist_ok=True)
        (out / 'index.html').write_text(str(soup))
        outputs[f'site/{d}index.html'] = None
        not_found[d.strip('/')] = {'lang': code, 'home': '/' + d, 'msg': t(NOT_FOUND[0])}
        bar[code] = {k: t(s) for k, s in LANGBAR} | {'href': '/' + d}
        print(f'{code}: {d}index.html')

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

    # suggestion bar: offers the visitor's language, never redirects
    (SITE / 'langbar.js').write_text(LANGBAR_JS.replace('/*STRINGS*/', json.dumps(bar, ensure_ascii=False, indent=1)))

    # sitemap: every locale, each listing all alternates
    alts = ''.join(f'    <xhtml:link rel="alternate" hreflang="{c}" href="{ORIGIN}{d}"/>\n' for c, d, *_ in LANGS) \
        + f'    <xhtml:link rel="alternate" hreflang="x-default" href="{ORIGIN}"/>\n'
    lastmod = re.search(r'<lastmod>(.*?)</lastmod>', (SITE / 'sitemap.xml').read_text()).group(1)
    urls = ''.join(f'  <url>\n    <loc>{ORIGIN}{d}</loc>\n    <lastmod>{lastmod}</lastmod>\n{alts}  </url>\n' for _, d, *_ in LANGS)
    (SITE / 'sitemap.xml').write_text('<?xml version="1.0" encoding="UTF-8"?>\n<urlset xmlns="http://www.sitemaps.org/schemas/sitemap/0.9" '
                                      'xmlns:xhtml="http://www.w3.org/1999/xhtml">\n' + urls + '</urlset>\n')

    LOCK.write_text(json.dumps(fingerprint(), indent=1) + '\n')
    print(f'fingerprint -> {LOCK.relative_to(ROOT)}')


def fingerprint():
    """Hashes of the English template, the catalogue, every translation and every generated file."""
    files = ['site/index.html', 'i18n/en.json'] + [f'i18n/{d.strip("/")}.json' for _, d, *_ in LANGS[1:]] \
        + [f'site/{d}index.html' for _, d, *_ in LANGS[1:]] + ['site/404.html', 'site/langbar.js', 'site/sitemap.xml']
    return {'files': {f: sha(ROOT / f) for f in files},
            'og_sources': {(d.strip('/') or 'en'): og_source(SITE / d / 'index.html') for _, d, *_ in LANGS}}


def og_source(page):
    """What a share image shows (headline + stats), hashed; tools/og.cjs computes the same from the same page."""
    html = Path(page).read_text()
    h1 = re.search(r'<h1>([\s\S]*?)</h1>', html).group(1)
    stats = re.search(r'<p class="stats">([\s\S]*?)</p>', html).group(1)
    return hashlib.sha256((h1 + '|' + stats).encode()).hexdigest()


def check():
    """Deploy guard: stop if anything changed since the last successful build."""
    if not LOCK.exists():
        sys.exit('No i18n/build.lock.json: run `tools/i18n.py build` before deploying.')
    want = json.loads(LOCK.read_text())
    bad = []
    for f, h in want['files'].items():
        p = ROOT / f
        if not p.exists():
            bad.append(f'{f} is missing')
        elif sha(p) != h:
            bad.append(f'{f} changed since the last build')
    # share images: rendered from the current headline and stats, and not edited since
    og = json.loads(OG_LOCK.read_text()) if OG_LOCK.exists() else {}
    for key, src in want['og_sources'].items():
        png = SITE / ('og.png' if key == 'en' else f'og-{key}.png')
        got = og.get(key, {})
        if got.get('source') != src:
            bad.append(f'{png.name} shows an old headline or stats: run `node tools/og.cjs`')
        elif not png.exists() or sha(png) != got.get('png'):
            bad.append(f'{png.name} changed since tools/og.cjs rendered it')
    if bad:
        sys.exit('Stale translations, not deploying:\n  ' + '\n  '.join(bad)
                 + '\nEdit English, then: extract, translate the ids it reports, build, node tools/og.cjs, run the checks.')
    print(f"i18n fingerprint ok ({len(want['files'])} files, {len(want['og_sources'])} share images)")


def verify(base):
    """Every version served right: 200s, trailing-slash redirects, lang/title/canonical, reciprocal hreflang."""
    base = base.rstrip('/') + '/'
    bad = []

    def get(url):
        try:
            with urllib.request.urlopen(url, timeout=20) as r:
                return r.status, r.geturl(), r.read().decode('utf-8', 'replace')
        except urllib.error.HTTPError as e:
            return e.code, url, e.read().decode('utf-8', 'replace')

    want_alts = {(c, ORIGIN + d) for c, d, *_ in LANGS} | {('x-default', ORIGIN)}
    for code, d, *_ in LANGS:
        st, final, html = get(base + d)
        lang = re.search(r'<html[^>]*\blang="([^"]+)"', html)
        canon = re.search(r'<link[^>]*rel="canonical"[^>]*href="([^"]+)"|<link[^>]*href="([^"]+)"[^>]*rel="canonical"', html)
        canon = canon and (canon.group(1) or canon.group(2))
        alts = set(re.findall(r'<link[^>]*hreflang="([^"]+)"[^>]*href="([^"]+)"', html)) \
            | {(h, u) for u, h in re.findall(r'<link[^>]*href="([^"]+)"[^>]*hreflang="([^"]+)"', html)}
        title = re.search(r'<title>(.*?)</title>', html, re.S)
        cl = re.search(r'<meta[^>]*content="([^"]+)"[^>]*http-equiv="content-language"|<meta[^>]*http-equiv="content-language"[^>]*content="([^"]+)"', html)
        if not cl or (cl.group(1) or cl.group(2)) != code:
            bad.append(f'{d or "/"} content-language is not {code}')
        line = f'{base + d}: {st} lang={lang and lang.group(1)} canonical={canon} title={title and title.group(1)[:50]!r}'
        print(line)
        if st != 200: bad.append(f'{d or "/"} returned {st}')
        if not lang or lang.group(1) != code: bad.append(f'{d or "/"} lang is not {code}')
        if canon != ORIGIN + d: bad.append(f'{d or "/"} canonical is {canon}')
        if alts != want_alts: bad.append(f'{d or "/"} hreflang set differs: {sorted(want_alts ^ alts)}')
        if d:
            st2, final2, _ = get(base + d.rstrip('/'))
            if st2 != 200 or not final2.endswith('/' + d):
                bad.append(f'/{d.rstrip("/")} does not redirect to /{d} ({st2}, {final2})')
    for f in ['sitemap.xml', 'robots.txt', 'llms.txt', 'langbar.js', 'og.png'] + [f'og-{d.strip("/")}.png' for _, d, *_ in LANGS[1:]]:
        st, *_ = get(base + f)
        if st != 200: bad.append(f'/{f} returned {st}')
    if bad:
        sys.exit('verify failed:\n  ' + '\n  '.join(bad))
    print('verify ok')


# The suggestion bar. It only offers; it never redirects. An explicit choice (picker, footer links, the
# bar's own link) or a dismissal is remembered on this browser and the bar stays away after that.
LANGBAR_JS = r"""/* generated by tools/i18n.py build. Suggests the visitor's language; never redirects. */
(function(){
  var S = /*STRINGS*/;
  var KEY = 'szc-lang';
  function get(){ try { return localStorage.getItem(KEY); } catch (e) { return null; } }
  function set(v){ try { localStorage.setItem(KEY, v); } catch (e) {} }
  var here = document.documentElement.lang;

  // explicit choices are remembered
  document.addEventListener('click', function(e){
    var a = e.target.closest && e.target.closest('.foot-langs a[hreflang], .langbar a[hreflang]');
    if (a) set(a.getAttribute('hreflang'));
  });
  var pick = document.querySelector('select[data-lang]');
  if (pick) pick.addEventListener('change', function(){ set(pick.options[pick.selectedIndex].lang); });

  if (get() || /bot|crawl|spider|slurp|lighthouse|headless/i.test(navigator.userAgent)) return;

  // the first browser language we have a version for; Traditional Chinese is never offered Simplified
  var prefs = navigator.languages && navigator.languages.length ? navigator.languages : [navigator.language || ''];
  var want = null, hant = false;
  for (var i = 0; i < prefs.length && !want; i++) {
    var p = String(prefs[i]).toLowerCase();
    if (/^zh-(tw|hk|mo|hant)/.test(p)) { hant = true; continue; }
    if (/^zh/.test(p) && hant) continue;
    var c = /^zh/.test(p) ? 'zh-Hans' : p.split('-')[0];
    if (S[c]) want = c;
  }
  if (!want || want === here) return;

  var s = S[want], bar = document.createElement('div');
  bar.className = 'langbar';
  bar.lang = want;
  var p = document.createElement('p'), a = document.createElement('a'), x = document.createElement('button');
  p.appendChild(document.createTextNode(s.msg + ' '));
  a.href = s.href; a.hreflang = want; a.textContent = s.go;
  p.appendChild(a);
  x.type = 'button'; x.setAttribute('aria-label', s.close); x.textContent = '×';
  x.addEventListener('click', function(){ set(here); bar.remove(); });
  bar.appendChild(p); bar.appendChild(x);
  document.body.insertBefore(bar, document.body.firstChild);
})();
"""

if __name__ == '__main__':
    cmd = sys.argv[1] if len(sys.argv) > 1 else 'extract'
    if cmd == 'verify':
        verify(sys.argv[2] if len(sys.argv) > 2 else ORIGIN)
    else:
        {'extract': extract, 'build': build, 'check': check}[cmd]()
