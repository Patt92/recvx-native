const pw=require('playwright');const crypto=require('crypto');
const [browserName='chromium',url='http://127.0.0.1:8722/']=process.argv.slice(2);const V=require('path').resolve(__dirname,'../../build/validation/web')+'/';
require('./launch').run(browserName,180000,async(browser)=>{
  const page=await browser.newPage({viewport:{width:1280,height:1000}});const t0=Date.now();const ts=()=>((Date.now()-t0)/1000).toFixed(1);
  page.on('console',m=>{const t=m.text();if(/error|Error|abort/i.test(t))console.log(ts(),t)});page.on('pageerror',e=>console.log(ts(),'PAGEERROR',e.message));
  await page.goto(url,{waitUntil:'commit'});await page.waitForSelector('#start:not([hidden])',{timeout:30000});await page.click('#start');
  const w=ms=>page.waitForTimeout(ms);const key=k=>page.keyboard.press(k);const shot=n=>page.screenshot({path:V+browserName+'_'+n+'.png'});
  await w(4000);await key('Enter');await w(2500);await key('Enter');await w(2500);await key('Enter');
  for(let i=0;i<10;i++){await w(3000);await key('Space');await w(1000);await key('P');}
  await w(5000);await key('Enter');await w(2500);await key('Escape');await w(800);await key('Escape');await w(3000);
  await shot('free');console.log(ts(),'free',await page.title());
  // Lange halten: Drehung links 1,5 s, dann vorwaerts 4 s; danach rennen (Shift) 3 s.
  await page.keyboard.down('ArrowLeft');await w(1500);await page.keyboard.up('ArrowLeft');await shot('turned');
  await page.keyboard.down('ArrowUp');await w(1000);console.log(ts(),'walking',await page.title());await w(3000);await page.keyboard.up('ArrowUp');await shot('walked');
  await page.keyboard.down('Shift');await page.keyboard.down('ArrowUp');await w(1000);console.log(ts(),'running',await page.title());await w(2000);await page.keyboard.up('ArrowUp');await page.keyboard.up('Shift');await shot('ran');
  console.log(ts(),'end',await page.title());
  });
