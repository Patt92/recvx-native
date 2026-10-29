// LOAD GAME im Browser, deterministisch wie tools/verify_typewriter_save.py: Testspielstand (Kopie, nur gelesen)
// und Replay-Datei werden in das OPFS dieses eigenen Testprofils gelegt; die Runtime laeuft mit
// --replay /user/r.txt --frames 700 --state /user/s.json und beendet sich danach. Der Zustand wird aus dem OPFS
// gelesen und mit dem nativen Lauf verglichen.
const fs = require('fs');
const [browserName = 'chromium', save = 'saves/save-7.rcvxsav', url = 'http://127.0.0.1:8722/'] = process.argv.slice(2);
const ROOT = require('path').resolve(__dirname, '../..') + '/';
const bytes = [...fs.readFileSync(ROOT + save)];
const replay = '60 confirm\n200 confirm\n420 confirm\n480 confirm\n';
require('./launch').run(browserName, 240000, async (browser) => {
  const page = await browser.newPage({ viewport: { width: 1280, height: 1000 } });
  const t0 = Date.now(), ts = () => ((Date.now() - t0) / 1000).toFixed(1);
  let finished = false;
  page.on('console', m => { const t = m.text(); if (/Typewriter|RM_|error|Error|Presentation: \d/.test(t)) console.log(ts(), browserName, t.slice(0, 200)); if (/game has ended|Presentation: \d/.test(t)) finished = true; });
  page.on('pageerror', e => console.log(ts(), browserName, 'PAGEERROR', e.message));
  await page.goto(url + '?args=--saves,/user/saves,--ss,1,--frames,700,--headless,--replay,/user/r.txt,--state,/user/s.json', { waitUntil: 'load' });
  await page.evaluate(async ({ data, replay }) => {
    const root = await navigator.storage.getDirectory();
    const put = async (dir, name, content) => { const f = await dir.getFileHandle(name, { create: true }); const w = await f.createWritable(); await w.write(content); await w.close(); };
    await put(await root.getDirectoryHandle('saves', { create: true }), 'save-1.rcvxsav', new Uint8Array(data));
    await put(root, 'r.txt', replay);
  }, { data: bytes, replay });
  await page.click('#start');
  for (let i = 0; i < 180 && !finished; i++) await page.waitForTimeout(1000);
  await page.waitForTimeout(1500);
  const state = await page.evaluate(async () => { try { const f = await (await navigator.storage.getDirectory()).getFileHandle('s.json'); return await (await f.getFile()).text(); } catch (e) { return ''; } });
  if (!state) { console.log(ts(), browserName, 'RESULT no state file'); process.exitCode = 1; return; }
  const s = JSON.parse(state);
  console.log(ts(), browserName, 'RESULT', JSON.stringify({ room: s.room, title_active: s.title_active, actor: s.actor, yaw: s.yaw, hp: s.hp, weapon: s.weapon, items: s.items }));
});
