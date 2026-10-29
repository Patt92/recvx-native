// Browserpruefung des Web-Builds gegen einen laufenden Server (Standard http://127.0.0.1:8722/, z. B. der
// Docker-Dienst "web"). Jede Pruefung startet einen eigenen Testbrowser mit eigenem Profil und schliesst nur
// diesen (launch.js); der Browser des Nutzers wird nie beruehrt.
//
//   node run.js [--browser chromium|firefox|all] [--url http://host:8722/] [--only walk,audio,...]
//
// Bewertet wird die Ausgabe jeder Pruefung (Spielzustand, Pegel, Takt), nicht nur ihr Ende.
const { spawnSync } = require('child_process');
const path = require('path');
const argv = process.argv.slice(2);
const option = (name, fallback) => { const i = argv.indexOf(name); return i >= 0 ? argv[i + 1] : fallback; };
const url = option('--url', 'http://127.0.0.1:8722/');
const browsers = option('--browser', 'all') === 'all' ? ['chromium', 'firefox'] : [option('--browser')];
const only = option('--only', '').split(',').filter(Boolean);

// Erwartungen: Spielzustaende wie die native Runtime (tools/verify_typewriter_save.py: Laden von save-7 nach
// RM_3110 an derselben Position, mit demselben Inventar). Takt: das Original laeuft mit 30 Hz.
const checks = {
  walk: { script: 'walk.js', args: (b) => [b, url], verify: (out) =>
    /walking .*Motion 0\b/.test(out) && /running .*Motion 4\b/.test(out) && /free .*RM_0000\.RDX/.test(out) },
  audio: { script: 'audio.js', args: (b) => [b, url, ',,Enter,ArrowDown,ArrowUp,Enter,,,,'], verify: (out) =>
    /running peak 0\.[1-9]/.test(out) },
  perf: { script: 'perf.js', args: (b) => [b, url], verify: (out) => {
    const m = /(\d+) simulation ticks.* over ([\d.]+) s/.exec(out); if (!m) return false;
    const hz = +m[1] / +m[2]; console.log(`    simulation ${hz.toFixed(2)} Hz`); return hz > 29 && hz < 30.5; } },
  persist: { script: 'persist.js', args: (b) => [b, url], verify: (out) =>
    /run 1 log:.*Snapshot 1 (gespeichert|saved)/.test(out) && /run 2 log:.*Snapshot 1 (geladen|loaded)/.test(out) },
  load_live: { script: 'load_live.js', args: (b) => [b, 'saves/save-7.rcvxsav', url], fixture: 'saves/save-7.rcvxsav', verify: (out) =>
    /TITLE RECVX native \| RM_3110\.RDX/.test(out) },
  load: { script: 'load.js', args: (b) => [b, 'saves/save-7.rcvxsav', url], fixture: 'saves/save-7.rcvxsav', verify: (out) =>
    out.includes('RESULT {"room":"RM_3110.RDX","title_active":false,"actor":[28.2641,0,43.4458],"yaw":63007,"hp":150,"weapon":4,"items":[3,3604481,2031624,589839,1179878,4063233,1638401,2359306,786523,0,0,0]}') },
};

const fs = require('fs');
let failed = 0;
for (const browser of browsers) {
  for (const [name, check] of Object.entries(checks)) {
    if (only.length && !only.includes(name)) continue;
    if (check.fixture && !fs.existsSync(path.resolve(__dirname, '../..', check.fixture))) { console.log(`SKIP ${browser} ${name}: fixture ${check.fixture} missing`); continue; }
    const started = Date.now();
    const result = spawnSync(process.execPath, [path.join(__dirname, check.script), ...check.args(browser)], { encoding: 'utf8', timeout: 300000 });
    const out = (result.stdout || '') + (result.stderr || '');
    const ok = result.status === 0 && !/PAGEERROR|HARNESS/.test(out) && check.verify(out);
    console.log(`${ok ? 'PASS' : 'FAIL'} ${browser} ${name} (${((Date.now() - started) / 1000).toFixed(0)} s)`);
    if (!ok) { failed++; console.log(out.split('\n').slice(-12).map((l) => '    ' + l).join('\n')); }
  }
}
process.exit(failed ? 1 : 0);
