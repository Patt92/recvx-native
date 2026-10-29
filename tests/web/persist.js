// OPFS: Lauf 1 erzeugt /user (INIs) und einen F5-Snapshot im Spiel; Lauf 2 im selben Browserprofil liest sie.
const pw=require('playwright');const fs=require('fs');const os=require('os');const path=require('path');const [browserName='chromium',url='http://127.0.0.1:8722/']=process.argv.slice(2);
(async()=>{const dir=fs.mkdtempSync(path.join(os.tmpdir(),'recvx-profile-'));
 const ctx=browserName==='firefox'?await pw.firefox.launchPersistentContext(dir,{channel:'moz-firefox',executablePath:'/Applications/Firefox.app/Contents/MacOS/firefox',headless:true,timeout:60000,firefoxUserPrefs:{'media.autoplay.default':0}})
   :await pw.chromium.launchPersistentContext(dir,{executablePath:'/Applications/Chromium.app/Contents/MacOS/Chromium',headless:true});
 const listing=async(page)=>page.evaluate(async()=>{const out=[];async function walk(d,p){for await(const [n,h] of d.entries()){if(h.kind==='directory')await walk(h,p+n+'/');else out.push(p+n+' '+(await h.getFile()).size);}}await walk(await navigator.storage.getDirectory(),'');return out;});
 for(const run of [1,2]){const page=ctx.pages()[0]||await ctx.newPage();const log=[];page.on('console',m=>{const t=m.text();if(/Snapshot|snapshot|saved|loaded|error|User data/i.test(t))log.push(t)});
   await page.goto(url,{waitUntil:'commit'});await page.waitForSelector('#start:not([hidden])',{timeout:30000});await page.click('#start');
   const w=ms=>page.waitForTimeout(ms);const key=k=>page.keyboard.press(k);
   await w(4000);await key('Enter');await w(2500);await key('Enter');await w(2500);await key('Enter');
   for(let i=0;i<10;i++){await w(3000);await key('Space');await w(1000);await key('P');}
   await w(5000);await key('Enter');await w(2500);await key('Escape');await w(800);await key('Escape');await w(3000);
   if(run===1){await page.keyboard.down('ArrowUp');await w(2000);await page.keyboard.up('ArrowUp');await w(500);await key('F5');await w(2500);}
   else{await key('F9');await w(3000);}
   console.log(browserName,'run',run,'log:',JSON.stringify(log));console.log(browserName,'run',run,'OPFS:',JSON.stringify(await listing(page)));
   await page.goto('about:blank');}
 await ctx.close();fs.rmSync(dir,{recursive:true,force:true});})().catch(e=>{console.log('HARNESS',e.message.slice(0,300));process.exit(1)});
