// node turntable.js model.glb out_prefix  -> out_prefix_sheet.jpg (8 angles) + out_prefix.gif (spinning)
// Renders a GLB with three.js in headless Chromium (WebGL via SwiftShader) with studio lighting.
const { chromium } = require('playwright');
const fs = require('fs'), path = require('path');
const [,, glb, out] = process.argv;
const THREE_DIR = path.join(__dirname, 'node/node_modules/three');
(async () => {
  const browser = await chromium.launch({ args: ['--use-gl=angle', '--use-angle=swiftshader', '--enable-unsafe-swiftshader', '--ignore-gpu-blocklist'] });
  const page = await browser.newPage({ viewport: { width: 1024, height: 1024 } });
  await page.route('**/three/**', r => {
    const p = path.join(THREE_DIR, r.request().url().split('/three/')[1]);
    r.fulfill({ body: fs.readFileSync(p), contentType: 'application/javascript' });
  });
  await page.route('**/model.glb', r => r.fulfill({ body: fs.readFileSync(glb), contentType: 'model/gltf-binary' }));
  await page.setContent(`<html><body style="margin:0;background:#dcdcd8"><canvas id=c></canvas>
<script type="importmap">{"imports":{"three":"http://x/three/build/three.module.js","three/addons/":"http://x/three/examples/jsm/"}}</script>
<script type="module">
import * as THREE from 'three';
import { GLTFLoader } from 'three/addons/loaders/GLTFLoader.js';
import { RoomEnvironment } from 'three/addons/environments/RoomEnvironment.js';
const W=1024,H=1024, canvas=document.getElementById('c');
const r=new THREE.WebGLRenderer({canvas,antialias:true,preserveDrawingBuffer:true}); r.setSize(W,H); r.toneMapping=THREE.ACESFilmicToneMapping; r.outputColorSpace=THREE.SRGBColorSpace; r.shadowMap.enabled=true;
const scene=new THREE.Scene(); scene.background=new THREE.Color(0xdcdcd8);
scene.environment=new THREE.PMREMGenerator(r).fromScene(new RoomEnvironment(),0.04).texture;
const key=new THREE.DirectionalLight(0xffffff,2.2); key.position.set(3,6,4); key.castShadow=true; key.shadow.mapSize.set(2048,2048); scene.add(key);
scene.add(new THREE.DirectionalLight(0xbfd4ff,0.8).translateX(-4).translateY(2).translateZ(-3));
const cam=new THREE.PerspectiveCamera(32,1,0.01,100);
const g=await new GLTFLoader().loadAsync('http://x/model.glb'); const m=g.scene;
m.traverse(o=>{if(o.isMesh){o.castShadow=true;o.receiveShadow=true;}});
const box=new THREE.Box3().setFromObject(m); const size=box.getSize(new THREE.Vector3()); const c=box.getCenter(new THREE.Vector3());
m.position.sub(c); m.position.y+=size.y/2; const R=Math.max(size.x,size.y,size.z);
const floor=new THREE.Mesh(new THREE.CircleGeometry(R*3,64),new THREE.ShadowMaterial({opacity:0.35})); floor.rotation.x=-Math.PI/2; floor.receiveShadow=true; scene.add(floor);
const pivot=new THREE.Group(); pivot.add(m); scene.add(pivot);
cam.position.set(0,R*0.75,R*2.3); cam.lookAt(0,size.y*0.45,0);
window.frames={}; window.render=(deg)=>{pivot.rotation.y=deg*Math.PI/180; r.render(scene,cam); return canvas.toDataURL('image/png');};
window.ready=true; window.tris=0; m.traverse(o=>{if(o.isMesh){const i=o.geometry.index; window.tris+=(i?i.count:o.geometry.attributes.position.count)/3;}});
</script></body></html>`, { waitUntil: 'load' });
  await page.waitForFunction('window.ready===true', null, { timeout: 120000 });
  const tris = await page.evaluate('window.tris');
  const frames = [];
  for (let a = 0; a < 360; a += 15) {
    const d = await page.evaluate(`window.render(${a})`);
    frames.push(Buffer.from(d.split(',')[1], 'base64'));
  }
  const dir = out + '_frames'; fs.mkdirSync(dir, { recursive: true });
  frames.forEach((b, i) => fs.writeFileSync(`${dir}/f${String(i).padStart(2, '0')}.png`, b));
  console.log('triangles', tris, 'frames', frames.length, 'dir', dir);
  await browser.close();
})();
