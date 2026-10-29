// WebAudio-Pruefung: der ScriptProcessor von SDL (SDL2.audio.scriptProcessorNode) wird zusaetzlich an einen
// Analyser gehaengt; gemessen wird der Spitzenpegel der tatsaechlich ausgegebenen Abtastwerte.
const pw=require('playwright');const [browserName='chromium',url='http://127.0.0.1:8722/',plan='']=process.argv.slice(2);
require('./launch').run(browserName,90000,async(browser)=>{
 const page=await browser.newPage();
 await page.goto(url,{waitUntil:'commit'});await page.waitForSelector('#start:not([hidden])',{timeout:30000});await page.click('#start');
 const probe=()=>page.evaluate(()=>{const s=Module.SDL2;if(!s||!s.audio||!s.audio.scriptProcessorNode)return 'no SDL audio node';
   if(!window.__an){window.__an=s.audioContext.createAnalyser();window.__an.fftSize=4096;s.audio.scriptProcessorNode.connect(window.__an);window.__peak=0;
     const d=new Float32Array(4096);setInterval(()=>{window.__an.getFloatTimeDomainData(d);for(const v of d)window.__peak=Math.max(window.__peak,Math.abs(v));},20);}
   const p=window.__peak;window.__peak=0;return s.audioContext.state+' peak '+p.toFixed(4);});
 const steps=plan?plan.split(','):[];
 for(let i=0;i<10;i++){await page.waitForTimeout(1500);if(steps[i])await page.keyboard.press(steps[i]);console.log(browserName,((i+1)*1.5).toFixed(1)+'s',steps[i]||'',await probe());}
 });
