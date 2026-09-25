// Layout and behaviour audit for every language version (playbook §10).
//
//   python3 -m http.server 8765 --directory site &
//   NODE_PATH=<dir with playwright-core> CHROME=<chrome binary> node tools/site-audit.cjs http://localhost:8765
//
// Per language and screen size: no sideways overflow, hero blocks don't overlap, no text under 12px,
// 44px tap targets on phones (links inside sentences are exempt), no console errors or failed requests.
// Then the suggestion bar: shown in the browser's language, never a redirect, remembered once dismissed,
// and never Simplified Chinese for zh-TW.
const { chromium } = require('playwright-core');

const BASE = (process.argv[2] || 'http://localhost:8765').replace(/\/$/, '');
const LANGS = { en: '/', es: '/es/', ko: '/ko/', 'zh-Hans': '/zh/', ru: '/ru/', tr: '/tr/' };
const SIZES = [320, 360, 375, 390, 414, 480, 600, 768, 820, 1024, 1280, 1440];
const UA = 'Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/140.0.0.0 Safari/537.36';

(async () => {
  const browser = await chromium.launch({ executablePath: process.env.CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' });
  const problems = [];
  for (const [lang, path] of Object.entries(LANGS)) {
    for (const w of SIZES) {
      const phone = w <= 820;
      const ctx = await browser.newContext({ viewport: { width: w, height: phone ? 800 : 900 }, isMobile: phone, hasTouch: phone });
      const page = await ctx.newPage();
      const errs = [];
      page.on('pageerror', e => errs.push(e.message));
      page.on('console', m => m.type() === 'error' && errs.push(m.text()));
      page.on('requestfailed', r => errs.push('failed ' + r.url()));
      page.on('response', r => r.status() >= 400 && errs.push(r.status() + ' ' + r.url()));
      await page.goto(BASE + path, { waitUntil: 'networkidle' });
      const r = await page.evaluate(phone => {
        const out = [];
        const de = document.documentElement;
        if (de.scrollWidth > innerWidth) out.push(`overflow ${de.scrollWidth}px > ${innerWidth}px`);
        const box = s => { const e = document.querySelector(s); return e && e.getBoundingClientRect(); };
        const hit = (a, b) => a && b && a.left < b.right && b.left < a.right && a.top < b.bottom && b.top < a.bottom;
        const hero = ['h1', '.copy', '.stats'];
        for (let i = 0; i < hero.length; i++) for (let j = i + 1; j < hero.length; j++)
          if (hit(box(hero[i]), box(hero[j]))) out.push(`${hero[i]} overlaps ${hero[j]}`);
        const navs = [...document.querySelectorAll('.brand, .nav-r > *')].filter(e => e.offsetParent).map(e => [e, e.getBoundingClientRect()]);
        for (let i = 0; i < navs.length; i++) for (let j = i + 1; j < navs.length; j++)
          if (hit(navs[i][1], navs[j][1])) out.push(`nav: ${navs[i][0].className || navs[i][0].tagName} overlaps ${navs[j][0].className || navs[j][0].tagName}`);
        const nav = box('.nav-in');
        for (const [e, b] of navs) if (b.right > nav.right + 1 || b.left < nav.left - 1) out.push(`nav item outside the bar: ${e.className || e.tagName}`);
        for (const e of document.querySelectorAll('.brand, .nav-r a')) { if (!e.offsetParent) continue; const lines = new Set(); for (const t of e.childNodes) { if (t.nodeType !== 3 || !t.textContent.trim()) continue; const r = document.createRange(); r.selectNodeContents(t); for (const x of r.getClientRects()) if (x.width > 1) lines.add(Math.round(x.top)); } if (lines.size > 1) out.push(`nav text wraps: ${e.textContent.trim().slice(0, 20)}`); }
        const walker = document.createTreeWalker(document.body, NodeFilter.SHOW_TEXT);
        const small = new Set();
        for (let n; (n = walker.nextNode());) {
          const el = n.parentElement;
          if (!n.textContent.trim() || !el.offsetParent || el.closest('.vh, script, style')) continue;
          const fs = parseFloat(getComputedStyle(el).fontSize);
          if (fs < 12) small.add(`${el.className || el.tagName} ${fs}px`);
        }
        small.forEach(s => out.push('text under 12px: ' + s));
        if (phone) for (const e of document.querySelectorAll('a, button, select')) {
          if (!e.offsetParent || e.closest('p:not(.links)')) continue;
          const b = e.getBoundingClientRect();
          if (b.height < 44 || b.width < 24) out.push(`tap target ${Math.round(b.width)}x${Math.round(b.height)}: ${(e.textContent || e.getAttribute('aria-label') || '').trim().slice(0, 30)}`);
        }
        if (document.documentElement.lang !== document.querySelector('select[data-lang] option[selected]').lang) out.push('picker selection does not match the page');
        return out;
      }, phone);
      for (const m of [...new Set([...r, ...errs])]) problems.push(`${lang} @${w}: ${m}`);
      await ctx.close();
    }
  }
  // the suggestion bar: offer, never redirect
  const bar = async (locales, path, expect) => {
    const ctx = await browser.newContext({ locale: locales[0], userAgent: UA, extraHTTPHeaders: { 'Accept-Language': locales.join(',') } });
    await ctx.addInitScript(l => Object.defineProperty(navigator, 'languages', { get: () => l }), locales);
    const page = await ctx.newPage();
    await page.goto(BASE + path, { waitUntil: 'networkidle' });
    const url = page.url();
    const got = await page.evaluate(() => { const b = document.querySelector('.langbar'); return b && { lang: b.lang, text: b.innerText, href: b.querySelector('a').getAttribute('href') }; });
    if (!url.endsWith(path)) problems.push(`bar ${locales} on ${path}: moved to ${url}`);
    if ((got && got.lang) !== expect) problems.push(`bar ${locales} on ${path}: expected ${expect}, got ${JSON.stringify(got)}`);
    else console.log(`bar ${locales.join(',')} on ${path}: ${got ? got.lang + ' | ' + got.text.replace(/\s+/g, ' ') + ' | ' + got.href : 'none'}`);
    if (got) {  // dismissal is remembered
      await page.click('.langbar button');
      await page.reload({ waitUntil: 'networkidle' });
      if (await page.$('.langbar')) problems.push(`bar ${locales} on ${path}: came back after dismissal`);
    }
    await ctx.close();
  };
  await bar(['ko-KR', 'ko'], '/', 'ko');
  await bar(['es-MX', 'en'], '/', 'es');
  await bar(['zh-CN'], '/ru/', 'zh-Hans');
  await bar(['en-US'], '/tr/', 'en');
  await bar(['tr-TR'], '/tr/', null);
  await bar(['zh-TW', 'zh'], '/', null);
  await bar(['de-DE'], '/es/', null);
  await browser.close();
  if (problems.length) { console.log(problems.join('\n')); process.exit(1); }
  console.log(`audit ok: ${Object.keys(LANGS).length} languages x ${SIZES.length} sizes, suggestion bar ok`);
})();
