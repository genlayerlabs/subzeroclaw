// Share images (og.png, og-<lang>.png) rendered from each built page's headline and stats.
//
//   NODE_PATH=<dir with playwright-core> CHROME=<chrome binary> node tools/og.cjs
//
// Run after `tools/i18n.py build`. Writes i18n/og.lock.json: for each language, a hash of the headline and
// stats the image shows (the same hash tools/i18n.py computes) and the image's own hash. `tools/i18n.py
// check` refuses to deploy when an image shows an old headline or was edited by hand.
const { chromium } = require('playwright-core');
const crypto = require('crypto');
const fs = require('fs');
const path = require('path');

const ROOT = path.resolve(__dirname, '..');
const SITE = path.join(ROOT, 'site');
const LANGS = ['en', 'es', 'ko', 'zh', 'ru', 'tr'];
// scripts the display font lacks use the system face, as on the page
const FONTS = {
  ru: '-apple-system,BlinkMacSystemFont,"Segoe UI",Roboto,"Noto Sans",sans-serif',
  ko: '"Apple SD Gothic Neo","Noto Sans KR","Malgun Gothic",sans-serif',
  zh: '"PingFang SC","Hiragino Sans GB","Noto Sans SC","Microsoft YaHei",sans-serif',
};
const sha256 = b => crypto.createHash('sha256').update(b).digest('hex');

(async () => {
  const browser = await chromium.launch({ executablePath: process.env.CHROME || '/Applications/Google Chrome.app/Contents/MacOS/Google Chrome' });
  const lock = {};
  for (const key of LANGS) {
    const src = fs.readFileSync(path.join(SITE, key === 'en' ? '' : key, 'index.html'), 'utf8');
    const lang = src.match(/<html lang="([^"]+)"/)[1];
    const h1 = src.match(/<h1>([\s\S]*?)<\/h1>/)[1];
    const stats = src.match(/<p class="stats">([\s\S]*?)<\/p>/)[1];
    const sys = FONTS[key];
    const tight = key === 'ko' || key === 'zh';
    const html = `<!DOCTYPE html><html lang="${lang}"><head><meta charset="utf-8">
<link href="https://fonts.googleapis.com/css2?family=Bricolage+Grotesque:opsz,wght@12..96,800&family=JetBrains+Mono:wght@400&display=block" rel="stylesheet">
<style>*{margin:0;padding:0;box-sizing:border-box}
body{width:1200px;height:630px;background:#E9E0D2;color:#33301f;position:relative;overflow:hidden}
.brand{position:absolute;left:92px;top:90px;display:flex;align-items:center;gap:22px;font-family:"Bricolage Grotesque";font-weight:800;font-size:31px;letter-spacing:-.02em}
.brand svg{width:48px;height:48px}
h1{position:absolute;left:90px;top:0;bottom:130px;display:flex;align-items:center;padding-top:150px;width:700px;
  font-family:${sys || '"Bricolage Grotesque"'};font-weight:${sys ? 700 : 800};line-height:${sys ? 1.1 : 1};letter-spacing:${sys ? '-.01em' : '-.03em'};
  text-transform:lowercase;${tight ? 'word-break:keep-all;' : ''}}
h1 em{font-style:normal;color:#A84E36}
.stats{position:absolute;left:92px;top:516px;white-space:nowrap;font-family:"JetBrains Mono",${sys || 'monospace'};font-size:24px;color:#6b6450;letter-spacing:.01em}
.sun{position:absolute;left:940px;top:0;width:600px;height:600px;border-radius:50%;background:radial-gradient(circle at 38% 33%,#C9633F 0%,#A84E36 60%,#7E3A22 100%)}
.hole{position:absolute;left:889px;top:399px;width:252px;height:252px;border-radius:50%;background:#26241F}
</style></head><body>
<div class="brand"><svg viewBox="0 0 40 40"><circle cx="20" cy="20" r="10" fill="#A84E36"/><circle cx="12.5" cy="27.5" r="4.2" fill="#26241F"/></svg>SubZeroClaw</div>
<h1><span>${h1}</span></h1><p class="stats">${stats}</p><div class="sun"></div><div class="hole"></div></body></html>`;
    const page = await browser.newPage({ viewport: { width: 1200, height: 630 } });
    await page.setContent(html, { waitUntil: 'networkidle' });
    await page.evaluate(() => document.fonts.ready);
    // shrink the headline until it fits between the brand and the stats; keep the stats clear of the circle
    await page.evaluate(() => {
      const h = document.querySelector('h1'), inner = h.firstElementChild;
      let s = 84; h.style.fontSize = s + 'px';
      while (s > 40 && (inner.getBoundingClientRect().height > 300 || inner.scrollWidth > h.clientWidth)) { s -= 2; h.style.fontSize = s + 'px'; }
      const st = document.querySelector('.stats');
      let f = 24;
      while (f > 16 && st.getBoundingClientRect().right > 860) { f -= 1; st.style.fontSize = f + 'px'; }
    });
    const out = path.join(SITE, key === 'en' ? 'og.png' : `og-${key}.png`);
    fs.writeFileSync(out, await page.screenshot());
    await page.close();
    lock[key] = { source: sha256(h1 + '|' + stats), png: sha256(fs.readFileSync(out)) };
    console.log(path.relative(ROOT, out));
  }
  await browser.close();
  fs.writeFileSync(path.join(ROOT, 'i18n', 'og.lock.json'), JSON.stringify(lock, null, 1) + '\n');
  console.log('i18n/og.lock.json');
})();
