// Interaktives LOAD GAME wie ein Spieler (echte Zeit, Tastatur): Testspielstand ins OPFS des eigenen Testprofils,
// Titel -> LOAD GAME -> Slot 1 -> YES; danach gehen und Raumtitel pruefen.
const fs = require('fs');
const [browserName = 'chromium', save = 'saves/save-7.rcvxsav', url = 'http://127.0.0.1:8722/'] = process.argv.slice(2);
const ROOT = require('path').resolve(__dirname, '../..') + '/', V = ROOT + 'build/validation/web/';
const bytes = [...fs.readFileSync(ROOT + save)];
require('./launch').run(browserName, 180000, async (browser) => {
  const page = await browser.newPage({ viewport: { width: 1280, height: 1000 } });
  const t0 = Date.now(), ts = () => ((Date.now() - t0) / 1000).toFixed(1);
  page.on('console', m => { const t = m.text(); if (/Typewriter|RM_|error|Error/.test(t)) console.log(ts(), browserName, t.slice(0, 200)); });
  page.on('pageerror', e => console.log(ts(), browserName, 'PAGEERROR', e.message));
  await page.goto(url, { waitUntil: 'load' });
  await page.evaluate(async (data) => {
    const root = await navigator.storage.getDirectory();
    const f = await (await root.getDirectoryHandle('saves', { create: true })).getFileHandle('save-1.rcvxsav', { create: true });
    const w = await f.createWritable(); await w.write(new Uint8Array(data)); await w.close();
  }, bytes);
  await page.click('#start');
  const w = ms => page.waitForTimeout(ms), key = k => page.keyboard.press(k);
  // Wartezeiten ueber dem Vorlauf des Originals (Titel-Einblendung, Menue, Ladebildschirm, Rueckfrage).
  await w(5000); await key('Enter'); await w(5000); await key('Enter'); await w(8000);
  await page.screenshot({ path: V + browserName + '_live_load_screen.png' });
  await key('Enter'); await w(3000); await key('Enter'); await w(15000);
  console.log(ts(), browserName, 'TITLE', await page.title());
  await page.screenshot({ path: V + browserName + '_live_loaded.png' });
  await page.keyboard.down('ArrowUp'); await w(2500); await page.keyboard.up('ArrowUp'); await w(500);
  console.log(ts(), browserName, 'TITLE after walk', await page.title());
  await page.screenshot({ path: V + browserName + '_live_walk.png' });
});
