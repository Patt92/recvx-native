// Gemeinsamer Browserstart der Pruefskripte. Jeder Testbrowser ist ein eigener Prozess mit eigenem temporaeren
// Profil und wird ausschliesslich ueber das Playwright-Objekt geschlossen. Kein pkill/killall: der Firefox bzw.
// Chrome des Nutzers wird nie beruehrt. Haengt ein Lauf, beendet die eingebaute Frist nur diesen Testbrowser.
const pw = require('playwright');
async function launch(browserName, extra = {}) {
  if (browserName === 'firefox') {
    return pw.firefox.launch({ channel: 'moz-firefox', executablePath: '/Applications/Firefox.app/Contents/MacOS/firefox',
      headless: true, timeout: 60000, firefoxUserPrefs: { 'media.autoplay.default': 0, 'media.autoplay.blocking_policy': 0 }, ...extra });
  }
  return pw.chromium.launch({ executablePath: '/Applications/Chromium.app/Contents/MacOS/Chromium', headless: true,
    args: ['--autoplay-policy=no-user-gesture-required'], ...extra });
}
// Fuehrt body(browser) aus und schliesst den eigenen Testbrowser spaetestens nach limitMs.
async function run(browserName, limitMs, body) {
  const browser = await launch(browserName);
  let timer;
  const deadline = new Promise((_, reject) => { timer = setTimeout(() => reject(new Error(`time limit ${limitMs} ms`)), limitMs); });
  try { await Promise.race([body(browser), deadline]); }
  catch (e) { console.log('HARNESS', String(e.message || e).slice(0, 300)); process.exitCode = 1; }
  finally { clearTimeout(timer); await browser.close().catch(() => {}); }
}
module.exports = { launch, run };
