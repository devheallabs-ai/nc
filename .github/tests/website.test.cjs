const {test}=require('node:test');
const assert=require('node:assert/strict');
const fs=require('node:fs');
const path=require('node:path');
const root=path.resolve(__dirname,'../..');
test('published NC pages use current domains and valid links',()=>{
 for(const name of ['index.html','404.html']) {
  const html=fs.readFileSync(path.join(root,name),'utf8');
  assert.doesNotMatch(html,/devheallabs\.in|<a\s+https?:|badge-beta|badge-research|no reference customers/i);
  for(const m of html.matchAll(/(?:href|src)="([^"]+)"/g)) {
   if(/^(https?:|mailto:|#)/.test(m[1]))continue;
   const file=m[1].split(/[?#]/)[0].replace(/^\//,'');
   assert.ok(fs.existsSync(path.join(root,file)),name+': '+file);
  }
 }
 const html=fs.readFileSync(path.join(root,'index.html'),'utf8');
 assert.match(html,/<link rel="canonical" href="https:\/\/nc\.devheallabs\.com\/">/);
 assert.match(html,/<meta property="og:url" content="https:\/\/nc\.devheallabs\.com\/">/);
 assert.equal(fs.readFileSync(path.join(root,'CNAME'),'utf8').trim(),'nc.devheallabs.com');
});
