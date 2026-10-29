// Takt-/Praesentationsrate im Spiel: --frames N beendet die Runtime nach N Schleifendurchlaeufen und gibt die
// Praesentationsstatistik aus (wie nativ).
const pw=require('playwright');const [browserName='chromium',url='http://127.0.0.1:8722/']=process.argv.slice(2);
require('./launch').run(browserName,150000,async(browser)=>{
 const page=await browser.newPage();const t0=Date.now();page.on('console',m=>{const t=m.text();if(/Presentation:|Draw:|simulation|Raster|error/i.test(t))console.log(((Date.now()-t0)/1000).toFixed(1),browserName,t.slice(0,300))});
 await page.goto(url,{waitUntil:'commit'});await page.waitForSelector('#start:not([hidden])',{timeout:30000});await page.click('#start');
 const w=ms=>page.waitForTimeout(ms);const key=k=>page.keyboard.press(k);
 await w(4000);await key('Enter');await w(2500);await key('Enter');await w(2500);await key('Enter');
 for(let i=0;i<10;i++){await w(3000);await key('Space');await w(1000);await key('P');}
 await w(5000);await key('Enter');await w(2500);await key('Escape');await w(800);await key('Escape');await w(2000);
 // freie Steuerung: 20 s laufen und rennen im Wechsel, dann F10 (Quit) fuer die Abschlussstatistik
 const t1=Date.now();while(Date.now()-t1<20000){await page.keyboard.down('ArrowUp');await w(1500);await page.keyboard.down('Shift');await w(1500);await page.keyboard.up('Shift');await page.keyboard.up('ArrowUp');await page.keyboard.down('ArrowLeft');await w(700);await page.keyboard.up('ArrowLeft');}
 await key('F10');await w(4000);});
