#!/usr/bin/env python3
"""Search engine registration helper: audit a site, plan registrations per language, and call the
engines' documented submission APIs. Standard library only.

  searchreg.py audit   SITE                      readiness report (read-only)
  searchreg.py plan    SITE                      engines per language, what's already done, in priority order
  searchreg.py env     SITE                      create ~/.config/search-engines/<host>.env (chmod 600) if missing
  searchreg.py indexnow key SITE [--write DIR]    new key: stored in the env file, and <key>.txt written to DIR
  searchreg.py indexnow submit SITE [URL ... | --urls-from-sitemap [--since YYYY-MM-DD]] [--dry-run]
  searchreg.py google  token|verify SITE [--method META|FILE] | sites | add | sitemap | sitemaps | inspect URL ...
  searchreg.py bing    sites | add | verify | sitemap | quota | submit URL ...
  searchreg.py yandex  hosts | add | verification | verify [--type ...] | sitemap | quota | recrawl URL ...
  searchreg.py baidu   push SITE URL ... [--dry-run]

SITE is the canonical origin, e.g. https://www.example.com. Secrets come from the environment or from
~/.config/search-engines/<host>.env (host = the canonical host, e.g. www.example.com). They are never
printed. API calls follow each engine's official reference; see references/engines.md.
"""
import argparse, json, os, re, secrets as pysecrets, sys, urllib.error, urllib.parse, urllib.request
import urllib.robotparser, xml.etree.ElementTree as ET
from html.parser import HTMLParser

# engine -> (robots.txt token, real crawler user-agent)
CRAWLERS = {
    'google': ('Googlebot', 'Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)'),
    'bing': ('bingbot', 'Mozilla/5.0 (compatible; bingbot/2.0; +http://www.bing.com/bingbot.htm)'),
    'yandex': ('YandexBot', 'Mozilla/5.0 (compatible; YandexBot/3.0; +http://yandex.com/bots)'),
    'naver': ('Yeti', 'Mozilla/5.0 (compatible; Yeti/1.1; +https://naver.me/spd)'),
    'daum': ('Daum', 'Mozilla/5.0 (compatible; Daum/4.1; +http://cs.daum.net/faq/15/4118.html?faqId=28966)'),
    'baidu': ('Baiduspider', 'Mozilla/5.0 (compatible; Baiduspider/2.0; +http://www.baidu.com/search/spider.html)'),
    '360': ('360Spider', 'Mozilla/5.0 (compatible; 360Spider; +http://webscan.360.cn)'),
    'seznam': ('SeznamBot', 'Mozilla/5.0 (compatible; SeznamBot/4.0; +https://o-seznam.cz/napoveda/vyhledavani/en/seznambot-crawler/)'),
}
# Engines per language (base or full subtag); 'all' applies to every site. See references/markets.md.
PLAN = {
    'all': ['google', 'bing', 'indexnow', 'brave?'],
    'ru': ['yandex'], 'be': ['yandex'], 'kk': ['yandex'], 'tr': ['yandex'],
    'ko': ['naver', 'daum'],
    'zh-hans': ['baidu*', '360?'], 'zh-cn': ['baidu*', '360?'], 'zh': ['baidu*', '360?'],
    'cs': ['seznam?'],
}
# Order to work in: biggest reach first; Google before Bing (Bing imports from Google).
PRIORITY = ['google', 'bing', 'indexnow', 'yandex', 'naver', 'daum', 'baidu*', '360?', 'seznam?', 'brave?']
NOTES = {
    'baidu*': 'baidu (only if someone with mainland-China real-name ID owns the account; otherwise skip, Baidu still crawls)',
    '360?': '360 (optional: desktop share in China)',
    'seznam?': 'seznam (Czech; not researched in depth, see markets.md)',
    'brave?': 'brave (optional: one-off form, search.brave.com/submit-url)',
}
META_VERIFY = {  # verification meta tags on the home page <head>
    'google': 'google-site-verification', 'bing': 'msvalidate.01', 'yandex': 'yandex-verification',
    'naver': 'naver-site-verification', 'baidu': 'baidu-site-verification', '360': '360-site-verification',
}
DNS_VERIFY = {'google': 'google-site-verification=', 'yandex': 'yandex-verification:', '360': '360-site-verification'}
INDEXNOW = 'https://api.indexnow.org/indexnow'   # shared with Bing, Yandex, Naver, Seznam, Yep, ... by protocol
UA = 'searchreg/1.1 (+site owner audit)'
ENV_KEYS = ['INDEXNOW_KEY', 'GOOGLE_ACCESS_TOKEN', 'BING_API_KEY', 'YANDEX_OAUTH_TOKEN', 'BAIDU_TOKEN', 'DAUM_PIN']


# ---------------------------------------------------------------- utilities
def env_path(host):
    return os.path.expanduser(f'~/.config/search-engines/{host}.env')


def load_env(host):
    f = env_path(host)
    if os.path.exists(f):
        for line in open(f):
            m = re.match(r'\s*([A-Z0-9_]+)\s*=\s*(.*?)\s*$', line)
            if m and m[2] and m[1] not in os.environ:
                os.environ[m[1]] = m[2].strip('\'"')


def make_env(host):
    f = env_path(host)
    if not os.path.exists(f):
        os.makedirs(os.path.dirname(f), mode=0o700, exist_ok=True)
        old = os.umask(0o077)
        with open(f, 'w') as fh:
            fh.write(f'# search engine secrets for {host}; never commit, echo or paste these values\n'
                     + ''.join(f'{k}=\n' for k in ENV_KEYS))
        os.umask(old)
    os.chmod(f, 0o600)
    return f


def set_env(host, key, value):
    f = make_env(host)
    lines = [l for l in open(f).read().splitlines() if not l.startswith(key + '=')]
    lines.append(f'{key}={value}')
    with open(f, 'w') as fh: fh.write('\n'.join(lines) + '\n')
    os.chmod(f, 0o600)


def need(name, allow_missing=False):
    v = os.environ.get(name)
    if not v and not allow_missing:
        sys.exit(f'{name} is not set (environment or ~/.config/search-engines/<host>.env; create it with "searchreg.py env SITE").')
    return v


def http(url, method='GET', data=None, headers=None, ua=UA, follow=True, timeout=30):
    """(status, headers, body bytes, final url). Never raises; status 0 means a network error."""
    h = {'User-Agent': ua, **(headers or {})}
    req = urllib.request.Request(url, data=data, headers=h, method=method)
    opener = urllib.request.build_opener() if follow else urllib.request.build_opener(NoRedirect)
    try:
        with opener.open(req, timeout=timeout) as r:
            return r.status, dict(r.headers), r.read(), r.geturl()
    except urllib.error.HTTPError as e:
        return e.code, dict(e.headers or {}), e.read() if e.fp else b'', url
    except Exception as e:
        return 0, {}, str(e).encode(), url


class NoRedirect(urllib.request.HTTPRedirectHandler):
    def redirect_request(self, *a, **k):
        return None


def origin_of(url):
    u = urllib.parse.urlsplit(url if '://' in url else 'https://' + url)
    return f'{u.scheme}://{u.netloc}'


def jcall(url, method='GET', body=None, headers=None):
    data = json.dumps(body).encode() if body is not None else None
    h = {'Content-Type': 'application/json; charset=utf-8', **(headers or {})}
    st, _, raw, _ = http(url, method, data, h)
    try:
        out = json.loads(raw) if raw else None
    except ValueError:
        out = raw.decode(errors='replace')[:500]
    return st, out


class Page(HTMLParser):
    def __init__(self):
        super().__init__(); self.lang = None; self.canonical = None; self.alts = {}; self.meta = {}; self.title = ''
        self._t = False

    def handle_starttag(self, tag, a):
        a = dict(a)
        if tag == 'html': self.lang = a.get('lang')
        elif tag == 'link' and a.get('rel') == 'canonical': self.canonical = a.get('href')
        elif tag == 'link' and a.get('rel') == 'alternate' and a.get('hreflang'): self.alts[a['hreflang'].lower()] = a.get('href')
        elif tag == 'meta':
            k = (a.get('name') or a.get('property') or a.get('http-equiv') or '').lower()
            if k: self.meta[k] = a.get('content', '')
        elif tag == 'title': self._t = True

    def handle_endtag(self, tag):
        if tag == 'title': self._t = False

    def handle_data(self, d):
        if self._t: self.title += d


def parse(html_bytes):
    p = Page(); p.feed(html_bytes.decode('utf-8', errors='replace')); return p


def dns_txt(name):
    """TXT records via DNS-over-HTTPS (Google Public DNS JSON API); [] on failure."""
    st, _, raw, _ = http('https://dns.google/resolve?type=TXT&name=' + urllib.parse.quote(name))
    try:
        return [a['data'].strip('"') for a in json.loads(raw).get('Answer', []) if a.get('type') == 16] if st == 200 else []
    except ValueError:
        return []


def robots(origin):
    st, h, raw, _ = http(origin + '/robots.txt')
    return st, h, raw.decode(errors='replace') if st == 200 else ''


def sitemap_locations(origin, robots_txt):
    locs = [l.split(':', 1)[1].strip() for l in robots_txt.splitlines() if l.lower().startswith('sitemap:')]
    return locs or [origin + '/sitemap.xml']


def sitemap_urls(origin, robots_txt=None, warn=print):
    """(urls, lastmod{url: 'YYYY-MM-DD'}) from the sitemaps in robots.txt (or /sitemap.xml); follows sitemap indexes."""
    ns = {'s': 'http://www.sitemaps.org/schemas/sitemap/0.9'}
    if robots_txt is None: robots_txt = robots(origin)[2]
    urls, lastmod, todo, seen = [], {}, sitemap_locations(origin, robots_txt), set()
    while todo:
        loc = todo.pop(0)
        if loc in seen: continue
        seen.add(loc)
        st, _, raw, _ = http(loc)
        if st != 200: warn(f'warning: sitemap {loc} answered HTTP {st}'); continue
        try:
            root = ET.fromstring(raw)
        except ET.ParseError as e:
            warn(f'warning: sitemap {loc} is not valid XML ({e})'); continue
        if root.tag.endswith('sitemapindex'):
            todo += [l.text.strip() for l in root.findall('s:sitemap/s:loc', ns) if l.text]
            continue
        for u in root.findall('s:url', ns):
            l = u.find('s:loc', ns)
            if l is None or not l.text: continue
            urls.append(l.text.strip())
            lm = u.find('s:lastmod', ns)
            if lm is not None and lm.text: lastmod[urls[-1]] = lm.text.strip()[:10]
    return urls, lastmod


def fetch_pages(origin, max_pages, robots_txt):
    urls, _ = sitemap_urls(origin, robots_txt, warn=lambda m: None)
    pages = {}
    for u in (urls[:max_pages] or [origin + '/']):
        st, h, b, _ = http(u)
        pages[u] = (st, h, parse(b) if st == 200 else None)
    return pages


def page_lang(p):
    return ((p.lang if p else '') or 'und').lower()


def plan_for(lang):
    return PLAN.get(lang, PLAN.get(lang.split('-')[0], []))


def engines_for(langs):
    want = list(PLAN['all'])
    for l in langs:
        for e in plan_for(l):
            if e not in want: want.append(e)
    return sorted(want, key=lambda e: PRIORITY.index(e) if e in PRIORITY else 99)


# ---------------------------------------------------------------- audit
def audit(args):
    origin = origin_of(args.site); host = urllib.parse.urlsplit(origin).netloc
    rows, fails = [], 0

    def row(ok, what, detail=''):
        nonlocal fails
        fails += ok is False
        mark = {True: 'ok  ', False: 'FAIL', None: 'warn'}[ok]
        rows.append(f'{mark}  {what}' + (f'  ({detail})' if detail else ''))

    st, h, _, _ = http(origin + '/', follow=False)
    row(st == 200, 'home page answers 200 without a redirect', f'HTTP {st}' + (f' -> {h.get("Location")}' if 300 <= st < 400 else ''))
    if host.startswith('www.'):
        s2, h2, _, _ = http(f'https://{host[4:]}/', follow=False)
        row(s2 in (301, 308) and h2.get('Location', '').rstrip('/').startswith(origin), f'{host[4:]} permanently redirects to {host}', f'HTTP {s2} -> {h2.get("Location")}')
    s3, _, _, _ = http(f'http://{host}/', follow=False)
    row(s3 in (301, 308), 'http:// permanently redirects to https://', f'HTTP {s3}')

    rst, rh, rtxt = robots(origin)
    row(rst == 200 and 'text/plain' in rh.get('Content-Type', ''), 'robots.txt is served as text/plain', f'HTTP {rst}, {rh.get("Content-Type")}')
    row('sitemap:' in rtxt.lower(), 'robots.txt declares the sitemap')
    rp = urllib.robotparser.RobotFileParser(); rp.parse(rtxt.splitlines())

    urls, _ = sitemap_urls(origin, rtxt, warn=lambda m: row(False, m))
    row(bool(urls), 'sitemap lists URLs', f'{len(urls)} URLs from {", ".join(sitemap_locations(origin, rtxt))}')
    pages = fetch_pages(origin, args.max_pages, rtxt)
    langs = {page_lang(p) for _, _, p in pages.values() if p}
    multilingual = len(langs) > 1
    heads = {u: p.alts for u, (_, _, p) in pages.items() if p}
    for u, (st, h, p) in pages.items():
        if not p:
            row(False, f'{u} answers 200', f'HTTP {st}'); continue
        row(bool(p.lang), f'{u}: <html lang>', p.lang or 'missing')
        row(p.canonical == u, f'{u}: canonical points to itself', p.canonical or 'missing')
        row(bool(p.title.strip()) and bool(p.meta.get('description')), f'{u}: title and meta description')
        if multilingual:
            if not p.alts:
                row(False, f'{u}: hreflang alternates in <head>', 'missing')
            else:
                problems = []
                if u not in p.alts.values(): problems.append('does not list itself')
                if 'x-default' not in p.alts: problems.append('no x-default')
                missing = langs - set(p.alts) - {'und'}
                if missing: problems.append('no entry for ' + ', '.join(sorted(missing)))
                for v in p.alts.values():   # every alternate we fetched must point back
                    if v in heads and v != u and u not in heads[v].values(): problems.append(f'{v} does not link back')
                row(not problems, f'{u}: hreflang in <head> ({len(p.alts)}: self, x-default, every language, reciprocal)', '; '.join(problems))
        cl = p.meta.get('content-language') or h.get('Content-Language')
        row(True if cl else None, f'{u}: content-language (meta or HTTP header; Bing and Baidu read it)', cl or 'missing: add it')
        if page_lang(p).startswith('zh'):
            row(True if p.meta.get('applicable-device') else None, f'{u}: <meta name="applicable-device"> (Baidu)', p.meta.get('applicable-device') or 'missing: add "pc,mobile" if responsive')

    # each crawler: robots.txt rules for every page, and a fetch with its user-agent of a page in its language
    by_lang = {}
    for u, (_, _, p) in pages.items():
        if p: by_lang.setdefault(page_lang(p), u)
    for e in engines_for(langs):
        eng = e.strip('*?')
        if eng not in CRAWLERS: continue
        token, ua = CRAWLERS[eng]
        target = next((by_lang[l] for l in by_lang if e in plan_for(l)), origin + '/')
        blocked = [u for u in pages if not rp.can_fetch(token, u)]
        s, _, _, _ = http(target, ua=ua)
        row(not blocked and s == 200, f'{eng}: robots.txt allows {token}; fetch of {target} as {token}',
            f'HTTP {s}' + (f'; robots.txt blocks {len(blocked)} page(s), e.g. {blocked[0]}' if blocked else ''))

    home = parse(http(origin + '/')[2])
    for eng, name in META_VERIFY.items():
        if name in home.meta: row(True, f'{eng}: verification meta tag on the home page')
    if 'DaumWebMasterTool' in rtxt: row(True, 'daum: DaumWebMasterTool line in robots.txt')
    key = os.environ.get('INDEXNOW_KEY')
    if key:
        s, _, b, _ = http(f'{origin}/{key}.txt', follow=False)
        row(s == 200 and b.strip() == key.encode(), 'indexnow: key file served at the root with exactly the key', f'HTTP {s}')
    else:
        row(None, 'indexnow: no INDEXNOW_KEY yet', 'create one with "searchreg.py indexnow key SITE --write <site root folder>"')

    print(f'Audit of {origin} ({len(pages)} pages; languages: {", ".join(sorted(langs))})\n' + '\n'.join(rows))
    print('\nNote: fetches come from this machine. They catch robots.txt and bot-protection blocks, not country blocks;'
          '\nfor zh and ru, check reachability from inside the country (references/engines.md: Baidu, Yandex).')
    print(f'\n{fails} failing check(s).' if fails else '\nNo failing checks.')
    return 1 if fails else 0


# ---------------------------------------------------------------- plan
def plan(args):
    origin = origin_of(args.site); host = urllib.parse.urlsplit(origin).netloc
    _, _, rtxt = robots(origin)
    pages = fetch_pages(origin, args.max_pages, rtxt)
    langs = {}
    for u, (_, _, p) in pages.items():
        if p: langs.setdefault(page_lang(p), []).append(u)
    home = parse(http(origin + '/')[2])
    apex = host[4:] if host.startswith('www.') else host
    txt = dns_txt(apex) + (dns_txt(host) if host != apex else [])
    in_dns = {e for e, pre in DNS_VERIFY.items() if any(t.startswith(pre) for t in txt)}
    bing_file = http(origin + '/BingSiteAuth.xml', follow=False)[0] == 200
    key = os.environ.get('INDEXNOW_KEY')
    key_live = bool(key) and http(f'{origin}/{key}.txt', follow=False)[0] == 200
    print(f'# Search engine plan for {origin}\n\nLanguages: ' + ', '.join(f'{l} ({len(v)})' for l, v in langs.items()))
    print('Work in this order (biggest reach first; Bing after Google because it imports from it):\n')
    for e in engines_for(langs):
        base = e.strip('*?')
        used = ['all'] if e in PLAN['all'] else [l for l in langs if e in plan_for(l)]
        mark, state = ' ', 'to do'
        if META_VERIFY.get(base) in home.meta: mark, state = 'x', 'verification tag on the home page'
        elif base == 'bing' and bing_file: mark, state = 'x', 'BingSiteAuth.xml present'
        elif base == 'daum' and 'DaumWebMasterTool' in rtxt: mark, state = 'x', 'robots.txt line present'
        elif base == 'indexnow': mark, state = ('x', 'key file live') if key_live else (' ', 'scripted: key file, then submit')
        elif base in in_dns:
            mark, state = '~', ('DNS verification record exists: someone may already own this property'
                                + ('; for Google this is often the Google Workspace admin. Get added as owner instead of creating another' if base == 'google' else ''))
        if base == 'bing' and mark == ' ': state += ' (Import from Google Search Console once Google is done)'
        if base == 'yandex': state += '; sanctions check first (engines.md)'
        if base == 'baidu': state += '; check mainland-China reachability first (engines.md)'
        print(f'- [{mark}] **{NOTES.get(e, e)}**: for {", ".join(used)}; {state}')
    print('\nThen follow SKILL.md step 3 (collect every token, one deploy) and step 4.')
    return 0


# ---------------------------------------------------------------- env
def env_cmd(args):
    host = urllib.parse.urlsplit(origin_of(args.site)).netloc
    print(f'{make_env(host)} (chmod 600). Fill in values in an editor; never paste them into chat.')
    return 0


# ---------------------------------------------------------------- IndexNow
def indexnow(args):
    origin = origin_of(args.site); host = urllib.parse.urlsplit(origin).netloc
    if args.cmd == 'key':
        # The key is public by design (served at /<key>.txt); it lives in the env file for the scripts.
        key = pysecrets.token_hex(16); set_env(host, 'INDEXNOW_KEY', key)
        msg = f'IndexNow key stored in {env_path(host)}.'
        if args.write:
            path = os.path.join(args.write, key + '.txt')
            with open(path, 'w', newline='') as fh: fh.write(key)
            msg += f' Key file written: {path}. Commit it; it must be served at {origin}/{key}.txt.'
        print(msg); return 0
    key = need('INDEXNOW_KEY', allow_missing=args.dry_run) or '<key>'
    if args.urls:
        urls = args.urls
    elif args.urls_from_sitemap:
        urls, lastmod = sitemap_urls(origin)
        if args.since: urls = [u for u in urls if lastmod.get(u, '9999') >= args.since]
    else:
        sys.exit('No URLs: pass URLs, or --urls-from-sitemap [--since YYYY-MM-DD].')
    if not urls: print('Nothing to submit.'); return 0
    bad = [u for u in urls if urllib.parse.urlsplit(u).netloc != host]
    if bad: sys.exit(f'URLs outside {host}: {bad[:3]}')
    s, _, b, _ = http(f'{origin}/{key}.txt', follow=False)
    if s != 200 or b.strip() != key.encode():
        msg = f'Key file {origin}/<key>.txt is not live with the exact key (HTTP {s}). Deploy it first.'
        if not args.dry_run: sys.exit(msg)
        print('warning: ' + msg)
    body = {'host': host, 'key': key, 'keyLocation': f'{origin}/{key}.txt', 'urlList': urls[:10000]}
    print(f'IndexNow: {len(body["urlList"])} URLs -> {args.endpoint}')
    if args.dry_run:
        print(json.dumps(body, indent=1)); return 0
    st, out = jcall(args.endpoint, 'POST', body)
    meaning = {200: 'accepted', 202: 'accepted, key validation pending (normal the first time)', 400: 'bad request',
               403: 'key not valid (file missing or different)', 422: 'URLs do not match host, or key format invalid',
               429: 'too many requests; slow down'}.get(st, '')
    print(f'HTTP {st} {meaning}' + (f'\n{out}' if out else ''))
    return 0 if st in (200, 202) else 1


# ---------------------------------------------------------------- Google (Search Console + Site Verification APIs)
GSC = 'https://www.googleapis.com/webmasters/v3'
GSV = 'https://www.googleapis.com/siteVerification/v1'


def google(args):
    H = {'Authorization': 'Bearer ' + need('GOOGLE_ACCESS_TOKEN')}
    origin = origin_of(args.site)
    prop = args.property or origin + '/'
    P = urllib.parse.quote(prop, safe='')
    site = ({'type': 'INET_DOMAIN', 'identifier': prop[len('sc-domain:'):]} if prop.startswith('sc-domain:')
            else {'type': 'SITE', 'identifier': prop})
    if args.cmd == 'token':      # what to publish: META tag/content, FILE name, or DNS record
        st, out = jcall(f'{GSV}/token', 'POST', {'site': site, 'verificationMethod': args.method}, H)
    elif args.cmd == 'verify':   # after the token is live; the caller becomes an owner
        st, out = jcall(f'{GSV}/webResource?verificationMethod={args.method}', 'POST', {'site': site}, H)
    elif args.cmd == 'sites':
        st, out = jcall(f'{GSC}/sites', headers=H)
    elif args.cmd == 'add':
        st, out = jcall(f'{GSC}/sites/{P}', 'PUT', headers=H)
    elif args.cmd == 'sitemap':
        sm = urllib.parse.quote(args.sitemap or origin + '/sitemap.xml', safe='')
        st, out = jcall(f'{GSC}/sites/{P}/sitemaps/{sm}', 'PUT', headers=H)
    elif args.cmd == 'sitemaps':
        st, out = jcall(f'{GSC}/sites/{P}/sitemaps', headers=H)
    elif args.cmd == 'inspect':
        res = []
        for u in args.urls or [origin + '/']:
            st, out = jcall('https://searchconsole.googleapis.com/v1/urlInspection/index:inspect', 'POST',
                            {'inspectionUrl': u, 'siteUrl': prop, 'languageCode': 'en-US'}, H)
            r = out.get('inspectionResult', {}).get('indexStatusResult', {}) if isinstance(out, dict) else {}
            res.append({'url': u, 'verdict': r.get('verdict'), 'coverage': r.get('coverageState'),
                        'googleCanonical': r.get('googleCanonical'), 'lastCrawl': r.get('lastCrawlTime')}
                       if st == 200 else {'url': u, 'http': st, 'error': out})
        print(json.dumps(res, indent=1, ensure_ascii=False)); return 0
    print(f'HTTP {st}'); out and print(json.dumps(out, indent=1, ensure_ascii=False))
    return 0 if 200 <= st < 300 else 1


# ---------------------------------------------------------------- Bing Webmaster API (JSON)
BING = 'https://ssl.bing.com/webmaster/api.svc/json'


def bing(args):
    key = need('BING_API_KEY'); site = origin_of(args.site)
    q = lambda m, extra='': f'{BING}/{m}?apikey={urllib.parse.quote(key)}{extra}'
    if args.cmd == 'sites':
        st, out = jcall(q('GetUserSites'))
        if isinstance(out, dict):   # verification codes end up public anyway; the key is never printed
            out = [{k: s.get(k) for k in ('Url', 'IsVerified', 'AuthenticationCode', 'DnsVerificationCode')} for s in out.get('d') or []]
    elif args.cmd == 'add':
        st, out = jcall(q('AddSite'), 'POST', {'siteUrl': site})
    elif args.cmd == 'verify':
        st, out = jcall(q('VerifySite'), 'POST', {'siteUrl': site})
    elif args.cmd == 'sitemap':
        st, out = jcall(q('SubmitFeed'), 'POST', {'siteUrl': site, 'feedUrl': args.sitemap or site + '/sitemap.xml'})
    elif args.cmd == 'quota':
        st, out = jcall(q('GetUrlSubmissionQuota', '&siteUrl=' + urllib.parse.quote(site, safe='')))
    elif args.cmd == 'submit':
        st, out = jcall(q('SubmitUrlBatch'), 'POST', {'siteUrl': site, 'urlList': args.urls[:500]})
    print(f'HTTP {st}'); out is not None and print(json.dumps(out, indent=1, ensure_ascii=False))
    return 0 if st == 200 else 1


# ---------------------------------------------------------------- Yandex Webmaster API v4
YA = 'https://api.webmaster.yandex.net/v4'


def yandex(args):
    H = {'Authorization': 'OAuth ' + need('YANDEX_OAUTH_TOKEN')}
    st, me = jcall(f'{YA}/user', headers=H)
    if st != 200: print(f'HTTP {st} on /user (token expired? tokens last 6 months)', me); return 1
    uid = me['user_id']; site = origin_of(args.site)
    st, hs = jcall(f'{YA}/user/{uid}/hosts', headers=H)
    host = next((h for h in (hs or {}).get('hosts', []) if h.get('ascii_host_url', '').rstrip('/') == site), None)
    if args.cmd == 'hosts':
        print(json.dumps(hs, indent=1, ensure_ascii=False)); return 0
    if args.cmd == 'add':
        st, out = jcall(f'{YA}/user/{uid}/hosts', 'POST', {'host_url': site}, H)
        print(f'HTTP {st}', json.dumps(out, ensure_ascii=False)); return 0 if st in (201, 409) else 1
    if not host: sys.exit(f'{site} is not in Yandex Webmaster yet: run "yandex add" first.')
    base = f'{YA}/user/{uid}/hosts/{urllib.parse.quote(host["host_id"], safe="")}'
    if args.cmd == 'verification':
        st, out = jcall(f'{base}/verification', headers=H)
        uin = out.get('verification_uin') if isinstance(out, dict) else None
        if uin:
            print(f'Meta tag for the home page <head>:  <meta name="yandex-verification" content="{uin}" />')
            print(f'(or the file yandex_{uin}.html with the exact contents Webmaster shows, or DNS TXT "yandex-verification: {uin}")')
    elif args.cmd == 'verify':
        st, out = jcall(f'{base}/verification?verification_type={args.type}', 'POST', headers=H)
    elif args.cmd == 'sitemap':
        st, out = jcall(f'{base}/user-added-sitemaps', 'POST', {'url': args.sitemap or site + '/sitemap.xml'}, H)
    elif args.cmd == 'quota':
        st, out = jcall(f'{base}/recrawl/quota', headers=H)
    elif args.cmd == 'recrawl':
        out = []
        for u in args.urls:
            s, o = jcall(f'{base}/recrawl/queue', 'POST', {'url': u}, H)
            out.append({'url': u, 'http': s, **(o if isinstance(o, dict) else {})})
        st = 202 if all(o['http'] in (202, 409) for o in out) else 400
    print(f'HTTP {st}'); out is not None and print(json.dumps(out, indent=1, ensure_ascii=False))
    return 0 if 200 <= st < 300 or st == 409 else 1


# ---------------------------------------------------------------- Baidu push (普通收录 API)
def baidu(args):
    site = origin_of(args.site); token = need('BAIDU_TOKEN', allow_missing=args.dry_run)
    urls = [u for u in args.urls if u == site or u.startswith(site + '/')]
    if len(urls) != len(args.urls): sys.exit('Every URL must belong to ' + site)
    print(f'Baidu push: {len(urls)} URLs (daily quota is small, often 10; push only new or changed pages)')
    if args.dry_run: print('\n'.join(urls)); return 0
    # plain http: the https endpoint's certificate does not match (checked 2026-09-25)
    endpoint = f'http://data.zz.baidu.com/urls?site={urllib.parse.quote(site, safe=":/")}&token={urllib.parse.quote(token)}'
    st, _, raw, _ = http(endpoint, 'POST', '\n'.join(urls[:2000]).encode(), {'Content-Type': 'text/plain'})
    print(f'HTTP {st}', raw.decode(errors='replace'))
    return 0 if st == 200 else 1


# ---------------------------------------------------------------- CLI
def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest='engine', required=True)
    for name in ('audit', 'plan'):
        s = sub.add_parser(name); s.add_argument('site'); s.add_argument('--max-pages', type=int, default=50)
    sub.add_parser('env').add_argument('site')
    i = sub.add_parser('indexnow'); i.add_argument('cmd', choices=['key', 'submit']); i.add_argument('site'); i.add_argument('urls', nargs='*')
    i.add_argument('--write', metavar='DIR', help='with "key": write <key>.txt into this site-root folder')
    i.add_argument('--urls-from-sitemap', action='store_true'); i.add_argument('--since', metavar='YYYY-MM-DD')
    i.add_argument('--endpoint', default=INDEXNOW); i.add_argument('--dry-run', action='store_true')
    g = sub.add_parser('google'); g.add_argument('cmd', choices=['token', 'verify', 'sites', 'add', 'sitemap', 'sitemaps', 'inspect'])
    g.add_argument('site'); g.add_argument('urls', nargs='*')
    g.add_argument('--property', help='existing property: sc-domain:example.com or https://www.example.com/ (default: SITE/)')
    g.add_argument('--method', default='META', choices=['META', 'FILE', 'DNS_TXT', 'DNS_CNAME']); g.add_argument('--sitemap')
    b = sub.add_parser('bing'); b.add_argument('cmd', choices=['sites', 'add', 'verify', 'sitemap', 'quota', 'submit'])
    b.add_argument('site'); b.add_argument('urls', nargs='*'); b.add_argument('--sitemap')
    y = sub.add_parser('yandex'); y.add_argument('cmd', choices=['hosts', 'add', 'verification', 'verify', 'sitemap', 'quota', 'recrawl'])
    y.add_argument('site'); y.add_argument('urls', nargs='*'); y.add_argument('--type', default='META_TAG', choices=['META_TAG', 'HTML_FILE', 'DNS'])
    y.add_argument('--sitemap')
    d = sub.add_parser('baidu'); d.add_argument('cmd', choices=['push']); d.add_argument('site'); d.add_argument('urls', nargs='+')
    d.add_argument('--dry-run', action='store_true')
    args = ap.parse_args()
    load_env(urllib.parse.urlsplit(origin_of(args.site)).netloc)
    sys.exit({'audit': audit, 'plan': plan, 'env': env_cmd, 'indexnow': indexnow, 'google': google, 'bing': bing,
              'yandex': yandex, 'baidu': baidu}[args.engine](args))


if __name__ == '__main__':
    main()
