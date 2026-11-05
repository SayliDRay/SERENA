import * as THREE from 'three';
import { RoundedBoxGeometry } from 'three/addons/geometries/RoundedBoxGeometry.js';
import { mergeVertices } from 'three/addons/utils/BufferGeometryUtils.js';

// Exterior study and separate hardware references. Dimensions are not a manufacturing drawing.
export const FINISHES = {
  stone: { name: 'Pierre naturelle', color: 0xc9bfad, band: 0xbfb5a2 },
  sage: { name: 'Sauge minérale', color: 0xa6b09f, band: 0x9ba68f },
  clay: { name: 'Argile douce', color: 0xc6a58e, band: 0xb99b84 },
};

function random(seed) {
  return () => { seed = (Math.imul(seed, 1664525) + 1013904223) >>> 0; return seed / 4294967296; };
}

function mineralTexture() {
  const canvas = document.createElement('canvas'); canvas.width = canvas.height = 512;
  const ctx = canvas.getContext('2d'), rng = random(6179);
  ctx.fillStyle = '#eeeae1'; ctx.fillRect(0, 0, 512, 512);
  const pixels = ctx.getImageData(0, 0, 512, 512);
  for (let i = 0; i < pixels.data.length; i += 4) {
    const shade = Math.floor((rng() - .5) * 15);
    pixels.data[i] += shade; pixels.data[i + 1] += shade; pixels.data[i + 2] += shade;
  }
  ctx.putImageData(pixels, 0, 0);
  for (let i = 0; i < 1800; i++) {
    const x = rng() * 512, y = rng() * 512, r = .4 + rng() * 3.2;
    ctx.fillStyle = rng() > .3 ? `rgba(81,70,52,${.1 + rng() * .24})` : 'rgba(255,255,248,.32)';
    ctx.beginPath(); ctx.ellipse(x, y, r, r * (.3 + rng()), rng() * Math.PI, 0, Math.PI * 2); ctx.fill();
  }
  const map = new THREE.CanvasTexture(canvas);
  map.wrapS = map.wrapT = THREE.RepeatWrapping; map.colorSpace = THREE.SRGBColorSpace;
  map.anisotropy = 4;
  return map;
}

function tactileTexture() {
  const canvas = document.createElement('canvas'); canvas.width = canvas.height = 256;
  const ctx = canvas.getContext('2d'), rng = random(441);
  const pixels = ctx.createImageData(256, 256);
  for (let i = 0; i < pixels.data.length; i += 4) {
    const shade = 130 + Math.floor(rng() * 100);
    pixels.data[i] = pixels.data[i + 1] = pixels.data[i + 2] = shade; pixels.data[i + 3] = 255;
  }
  ctx.putImageData(pixels, 0, 0);
  const map = new THREE.CanvasTexture(canvas);
  map.wrapS = map.wrapT = THREE.RepeatWrapping;
  return map;
}

function contour(w, d, n = 2.55, path = new THREE.Shape(), reverse = false) {
  const count = 112;
  for (let i = 0; i <= count; i++) {
    const a = (reverse ? -1 : 1) * i / count * Math.PI * 2;
    const c = Math.cos(a), s = Math.sin(a);
    const x = w * .5 * Math.sign(c) * Math.abs(c) ** (2 / n);
    const y = d * .5 * Math.sign(s) * Math.abs(s) ** (2 / n);
    if (i === 0) path.moveTo(x, y); else path.lineTo(x, y);
  }
  path.closePath(); return path;
}

function exteriorUV(geometry) {
  const pos = geometry.attributes.position, uv = geometry.attributes.uv;
  for (let i = 0; i < pos.count; i++) uv.setXY(i, pos.getX(i) / 16, (pos.getZ(i) + pos.getY(i) * .7) / 16);
  uv.needsUpdate = true;
}

function ovalGeometry(w, d, h, bevel = .6, inner = null) {
  const shape = contour(w - bevel * 2, d - bevel * 2);
  if (inner) shape.holes.push(contour(inner[0], inner[1], 2.55, new THREE.Path(), true));
  const geo = new THREE.ExtrudeGeometry(shape, {
    depth: h, bevelEnabled: bevel > 0, bevelSegments: 5, steps: 1,
    bevelSize: bevel, bevelThickness: bevel, curveSegments: 48,
  });
  geo.translate(0, 0, -h / 2); geo.rotateX(-Math.PI / 2);
  exteriorUV(geo);
  geo.deleteAttribute('normal');
  const smooth=mergeVertices(geo,.0001);smooth.computeVertexNormals();geo.dispose();
  return smooth;
}

function mesh(geo, material, parent, x = 0, y = 0, z = 0) {
  const object = new THREE.Mesh(geo, material); object.position.set(x, y, z);
  object.castShadow = true; object.receiveShadow = true; parent.add(object); return object;
}

function roundedBox(w, h, d, radius, material, parent, x = 0, y = 0, z = 0) {
  const geo = new RoundedBoxGeometry(w, h, d, 4, radius); exteriorUV(geo);
  return mesh(geo, material, parent, x, y, z);
}

// Closed curved ribbon with actual through-holes, including inside walls.
// Independent quads avoid bending a single long, planar triangulation.
export function strapGeometry() {
  const nx = 64, ny = 250, start = .58, end = Math.PI * 2 - .58, thickness = 2.25;
  const positions = [], uvs = [], indices = [], filled = [];
  const isHole = (u, a) => {
    for (let k = 0; k < 7; k++) {
      const center = 2.03 + k * .247;
      const dx = Math.abs(u) - 2.2, dy = Math.abs((a - center) * 27.5) - .7;
      if (Math.hypot(Math.max(dx, 0), Math.max(dy, 0)) + Math.min(Math.max(dx, dy), 0) < .65) return true;
    }
    return false;
  };
  const point = (i, j, outside) => {
    const a = start + (end - start) * j / ny;
    const taper = 8.9 + 1.1 * Math.exp(-Math.min(a - start, end - a) * 6);
    const x = (i / nx * 2 - 1) * taper, r = outside ? thickness / 2 : -thickness / 2;
    return [x, -22 + (29 + r) * Math.cos(a), (26.5 + r) * Math.sin(a)];
  };
  const addQuad = (a, b, c, d, flip = false) => {
    const base = positions.length / 3;
    for (const p of [a,b,c,d]) { positions.push(...p); uvs.push(p[0] / 16, Math.atan2(p[2], p[1] + 24) * 2); }
    if (flip) indices.push(base,base+1,base+2,base,base+2,base+3);
    else indices.push(base,base+2,base+1,base,base+3,base+2);
  };
  for (let j = 0; j < ny; j++) {
    filled[j] = [];
    const a = start + (end - start) * (j + .5) / ny;
    for (let i = 0; i < nx; i++) filled[j][i] = !isHole(((i + .5) / nx * 2 - 1) * 8.9, a);
  }
  for (let j = 0; j < ny; j++) for (let i = 0; i < nx; i++) {
    if (!filled[j][i]) continue;
    const a = point(i,j,true), b = point(i+1,j,true), c = point(i+1,j+1,true), d = point(i,j+1,true);
    const e = point(i,j,false), f = point(i+1,j,false), g = point(i+1,j+1,false), h = point(i,j+1,false);
    addQuad(a,b,c,d); addQuad(e,f,g,h,true);
    if (i === 0 || !filled[j][i-1]) addQuad(a,d,h,e);
    if (i === nx-1 || !filled[j][i+1]) addQuad(b,f,g,c);
    if (j === 0 || !filled[j-1][i]) addQuad(a,e,f,b);
    if (j === ny-1 || !filled[j+1][i]) addQuad(d,c,g,h);
  }
  const geo = new THREE.BufferGeometry();
  geo.setAttribute('position', new THREE.Float32BufferAttribute(positions, 3));
  geo.setAttribute('uv', new THREE.Float32BufferAttribute(uvs, 2));
  geo.setIndex(indices);
  const smooth=mergeVertices(geo,.0001);smooth.computeVertexNormals();geo.dispose();
  return smooth;
}

function identificationTexture() {
  const canvas = document.createElement('canvas'); canvas.width = canvas.height = 768;
  const ctx = canvas.getContext('2d');
  ctx.fillStyle = '#eee9df'; ctx.fillRect(0, 0, 768, 768);
  ctx.fillStyle = '#4a4c43'; ctx.textAlign = 'center';
  ctx.font = 'bold 46px Georgia'; ctx.fillText('SÉRÉNA',384,91);
  // Decorative identification study, deliberately not a generated/encoded QR.
  const cells = 25, cell = 16, ox = 184, oy = 167, rng = random(531);
  const finder = (x,y) => {
    ctx.fillStyle='#343a33';ctx.fillRect(ox+x*cell,oy+y*cell,7*cell,7*cell);
    ctx.fillStyle='#eee9df';ctx.fillRect(ox+(x+1)*cell,oy+(y+1)*cell,5*cell,5*cell);
    ctx.fillStyle='#343a33';ctx.fillRect(ox+(x+2)*cell,oy+(y+2)*cell,3*cell,3*cell);
  };
  ctx.fillStyle = '#343a33';
  for(let y=0;y<cells;y++)for(let x=0;x<cells;x++) {
    if ((x<8&&y<8)||(x>16&&y<8)||(x<8&&y>16)) continue;
    if(rng()>.52)ctx.fillRect(ox+x*cell,oy+y*cell,cell,cell);
  }
  finder(0,0);finder(18,0);finder(0,18);
  ctx.fillStyle = '#8b8e80';ctx.font='20px Arial';ctx.fillText('Fiche d’urgence',384,657);
  ctx.font='17px Arial';ctx.fillText('Insert remplaçable · démonstration',384,693);
  const map = new THREE.CanvasTexture(canvas); map.colorSpace = THREE.SRGBColorSpace;
  map.anisotropy = 8; return map;
}

export function createBracelet() {
  const root = new THREE.Group(), housing = new THREE.Group(), identification = new THREE.Group();
  const head = new THREE.Group(); root.add(head); head.add(housing, identification);
  head.scale.set(.76,.60,.84); head.position.y=1.5;
  const map = mineralTexture(), bump = tactileTexture();
  const stone = new THREE.MeshStandardMaterial({color:FINISHES.stone.color,map,roughness:.88,metalness:0,bumpMap:bump,bumpScale:.045});
  const strapMaterial = stone.clone();strapMaterial.color.setHex(FINISHES.stone.band);strapMaterial.roughness=.96;
  const graphite = new THREE.MeshStandardMaterial({color:0x444b49,roughness:.82,metalness:.025,bumpMap:bump,bumpScale:.075});
  const dark = new THREE.MeshStandardMaterial({color:0x252c29,roughness:.8});
  const inner = new THREE.MeshStandardMaterial({color:0xd9d1c2,roughness:.87});
  const seam = new THREE.MeshStandardMaterial({color:0x938979,roughness:.9});
  mesh(strapGeometry(),strapMaterial,root);

  // The sleeve is a separate moulded keeper, wrapped around the curved strap.
  const keeper = new THREE.Group(), ka = 1.63;
  keeper.position.set(0,-22+29*Math.cos(ka),26.5*Math.sin(ka)); keeper.rotation.x=ka; root.add(keeper);
  roundedBox(22,3,6.4,1.2,stone,keeper,0,2.6,0);
  roundedBox(2.6,5.5,6.4,.9,stone,keeper,-10,-.9,0);
  roundedBox(2.6,5.5,6.4,.9,stone,keeper,10,-.9,0);
  roundedBox(21.8,2.1,6.2,.8,stone,keeper,0,-4,0);
  const stud = new THREE.Group(), sa=3.88;
  stud.position.set(0,-22+30.5*Math.cos(sa),28*Math.sin(sa));stud.rotation.x=sa;root.add(stud);
  mesh(new THREE.CylinderGeometry(2.2,2.35,1.3,40),stone,stud,0,0,0);
  mesh(new THREE.SphereGeometry(2.2,32,16),stone,stud,0,.5,0).scale.y=.25;

  // Continuous softly rounded body, separate seam and inset well.
  mesh(ovalGeometry(48,53,7.8,1.85),stone,housing,0,6.6,0);
  mesh(ovalGeometry(47.9,52.9,.13,.18),seam,housing,0,5.5,0);
  mesh(ovalGeometry(47.8,52.8,3.5,1.1),stone,housing,0,10.6,0);
  mesh(ovalGeometry(43.2,48.1,.45,.45,[38.4,43.3]),inner,housing,0,13.5,0);
  mesh(ovalGeometry(38.3,43.2,.6,.6),inner,identification,0,13.25,0);
  const labelMat = new THREE.MeshStandardMaterial({map:identificationTexture(),roughness:.94,side:THREE.FrontSide});
  const label = mesh(new THREE.PlaneGeometry(29,33),labelMat,identification,0,14.2,0);
  label.rotation.x=-Math.PI/2;

  // Keep a readable amber diffuser under studio exposure, rather than clipping
  // an emissive white highlight through the tone mapper.
  const haloOff=new THREE.Color(0xb0aaa0),haloAmber=new THREE.Color(0xffb350);
  const haloMaterial = new THREE.MeshBasicMaterial({color:0xffb350,toneMapped:false});
  const halo = mesh(ovalGeometry(44.6,49.4,.25,.12,[41.4,46.1]),haloMaterial,housing,0,14.2,0);
  halo.visible=false;
  const haloLight = new THREE.PointLight(0xffb454,10,30,2);haloLight.position.set(0,13.5,22);housing.add(haloLight);

  const lidPivot = new THREE.Group();lidPivot.position.set(0,14.55,-23.3);head.add(lidPivot);
  const lidFrame = mesh(ovalGeometry(42.8,47.2,1.05,.65),stone,lidPivot,0,0,23.3);
  mesh(ovalGeometry(38.2,42.5,.35,.5),inner,lidPivot,0,-.91,23.3);
  const tactileGroup = new THREE.Group();lidPivot.add(tactileGroup);
  const button = mesh(ovalGeometry(40.3,44.8,.7,1.25),graphite,tactileGroup,0,1.53,23.3);
  const bumpMesh = mesh(new THREE.SphereGeometry(1.65,32,20),graphite,tactileGroup,0,3.05,11);
  bumpMesh.scale.y=.58;
  // Fixed outer knuckles and moving central barrel around a true hinge axis.
  for (const x of [-8.5,8.5]) {
    const barrel=mesh(new THREE.CylinderGeometry(2.2,2.2,6.3,48),stone,housing,x,14.4,-23.3);barrel.rotation.z=Math.PI/2;
    roundedBox(5.5,2.6,5.8,.9,stone,housing,x,12.9,-22.5);
  }
  const movingBarrel=mesh(new THREE.CylinderGeometry(2.1,2.1,9.7,48),stone,lidPivot);movingBarrel.rotation.z=Math.PI/2;
  const hingePin=mesh(new THREE.CylinderGeometry(.72,.72,23.7,32),seam,housing,0,14.4,-23.3);hingePin.rotation.z=Math.PI/2;
  // Fingernail access at the front; a shallow darker well, not an extra button.
  mesh(ovalGeometry(10,4.5,.45,.4),inner,housing,0,13.2,24.25);

  // Physical surface detail, with recessed dark openings on the side wall.
  const caseSurfaceX=z=>24*Math.pow(1-Math.pow(Math.abs(z)/26.5,2.55),1/2.55);
  const surfaceOpening=(radius,y,z)=>{
    const x=caseSurfaceX(z),normalZ=Math.sign(z)*Math.pow(Math.abs(z)/26.5,1.55)*24/26.5/Math.pow(x/24,1.55);
    const opening=mesh(new THREE.CircleGeometry(radius,28),dark,housing,x+.035,y,z);
    opening.rotation.y=Math.atan2(1,normalZ);return opening;
  };
  for(let row=0;row<3;row++)for(let col=0;col<8;col++) {
    surfaceOpening(.37,6.5+row*1.2,-4.4+col*1.55);
  }
  surfaceOpening(.45,7.6,-11.2);
  // USB-C opening, metal shell, recessed tongue, and an attached open dust cover.
  const portX=caseSurfaceX(12.5);
  const portMetal=new THREE.MeshStandardMaterial({color:0xb7b4a9,metalness:.8,roughness:.35});
  roundedBox(1.4,4.8,10,.9,stone,housing,portX+.3,7.8,12.5);
  roundedBox(.6,3.5,8,.65,portMetal,housing,portX+1.05,7.8,12.5);
  const usb=roundedBox(.2,2.6,6.8,.55,dark,housing,portX+1.4,7.8,12.5);
  roundedBox(.25,.45,4.8,.18,portMetal,housing,portX+1.55,7.8,12.5);
  const dustCover=new THREE.Group();dustCover.position.set(portX+5,4.3,18);dustCover.rotation.x=-.35;housing.add(dustCover);
  roundedBox(1.2,5.4,8.7,1.1,stone,dustCover);
  const tether=new THREE.CatmullRomCurve3([new THREE.Vector3(portX+.7,5.5,16),new THREE.Vector3(portX+3,3.9,18),new THREE.Vector3(portX+5,3.2,18)]);
  mesh(new THREE.TubeGeometry(tether,18,.55,8,false),stone,housing);
  // Front-facing lift marker. It is a decorative QR symbol, not a medical link.
  const markerCanvas=document.createElement('canvas');markerCanvas.width=512;markerCanvas.height=192;
  const mc=markerCanvas.getContext('2d');mc.fillStyle='#36382f';
  for(let j=0;j<9;j++)for(let i=0;i<9;i++)if((i+j*3)%4!==1)mc.fillRect(45+i*14,30+j*14,12,12);
  mc.font='bold 150px Arial';mc.textAlign='center';mc.fillText('↑',325,157);
  const markerMap=new THREE.CanvasTexture(markerCanvas);markerMap.colorSpace=THREE.SRGBColorSpace;
  const liftMarker=mesh(new THREE.PlaneGeometry(10,3.8),new THREE.MeshBasicMaterial({map:markerMap,transparent:true,depthWrite:false}),housing,0,13.65,22.9);
  liftMarker.rotation.x=-Math.PI/2;
  // Small fastener heads on the underside are exterior form studies only.
  const metal = new THREE.MeshStandardMaterial({color:0x9b9c91,roughness:.4,metalness:.7});
  for (const x of [-13,13])for(const z of [-14,14]) {
    mesh(new THREE.CylinderGeometry(1.1,1.1,.22,28),metal,housing,x,.75,z);
    roundedBox(1.15,.08,.18,.05,dark,housing,x,.59,z);
  }
  const undersideCanvas=document.createElement('canvas');undersideCanvas.width=512;undersideCanvas.height=256;
  const uc=undersideCanvas.getContext('2d');uc.fillStyle='#77766b';uc.textAlign='center';uc.font='40px Georgia';uc.fillText('SÉRÉNA',256,115);uc.font='16px Arial';uc.fillText('ÉTUDE DE FORME',256,155);
  const undersideMap=new THREE.CanvasTexture(undersideCanvas);undersideMap.colorSpace=THREE.SRGBColorSpace;
  const underside=mesh(new THREE.PlaneGeometry(17,8.5),new THREE.MeshBasicMaterial({map:undersideMap,transparent:true,depthWrite:false}),housing,0,.65,0);
  underside.rotation.x=Math.PI/2;

  // Independent, unscaled component studies: the exterior is not a fit claim.
  const hardware = new THREE.Group(); root.add(hardware); hardware.visible=false;
  const pcbMat=new THREE.MeshStandardMaterial({color:0x245446,roughness:.75});
  const silver=new THREE.MeshStandardMaterial({color:0xaeb4b4,metalness:.65,roughness:.45});
  function caption(text,x,y,z,width=54){
    const c=document.createElement('canvas');c.width=1024;c.height=128;
    const ctx=c.getContext('2d');ctx.fillStyle='#f4f1eb';ctx.fillRect(0,0,1024,128);
    ctx.fillStyle='#29312d';ctx.font='34px Arial';ctx.textAlign='center';ctx.fillText(text,512,75);
    const map=new THREE.CanvasTexture(c);map.colorSpace=THREE.SRGBColorSpace;
    const sprite=new THREE.Sprite(new THREE.SpriteMaterial({map,depthTest:false}));
    sprite.position.set(x,y,z);sprite.scale.set(width,width/8,1);hardware.add(sprite);
  }
  roundedBox(20,4,30,.7,silver,hardware,-32,25,0);
  caption('Batterie · volume illustratif',-32,32,0);
  roundedBox(17.8,1.2,21,.4,pcbMat,hardware,0,44,0);
  roundedBox(12,2,13,.3,silver,hardware,0,46,0);
  caption('XIAO · 17,8 × 21 mm',0,54,0);
  roundedBox(19.9,2.4,23.6,.4,silver,hardware,32,67,0);
  caption('EG915U · candidat, non validé',32,77,0,60);
  // SIM7600 HAT remains outside the wearable, at its reference footprint.
  roundedBox(56.21,1.6,65.15,.5,pcbMat,hardware,84,-25,0);
  roundedBox(24,3,24,.5,silver,hardware,84,-22,0);
  roundedBox(15,3,16,.5,dark,hardware,84,-22,23);
  caption('SIM7600 HAT · banc externe',84,0,0,65);
  caption('56,21 × 65,15 mm · ne rentre pas',84,-6,0,66);

  function finish(key) {
    const value=FINISHES[key]||FINISHES.stone;
    stone.color.setHex(value.color);strapMaterial.color.setHex(value.band);
  }
  function animate({opening=0,construction=0,press=0,brightness=1,vibration=0,time=0}) {
    hardware.visible=construction>.05;
    hardware.scale.setScalar(Math.max(.001,construction));
    lidPivot.rotation.x=-opening;
    lidPivot.position.y=14.55+construction*26;
    identification.position.y=construction*13;
    tactileGroup.position.y=-press*.85;
    haloMaterial.color.lerpColors(haloOff,haloAmber,Math.min(1,brightness));
    haloLight.intensity=0;
    root.position.x=0;
    root.rotation.z=0;
  }
  const anchors={
    touch:{object:button,point:new THREE.Vector3(0,2,4)},
    lid:{object:lidPivot,point:new THREE.Vector3(8,2,0)},
    light:{object:usb,point:new THREE.Vector3(0,0,0)},
    audio:{object:housing,point:new THREE.Vector3(25,8,4)},
  };
  return {root,button,tactileGroup,lidPivot,finish,animate,anchors,materials:{stone,strap:strapMaterial,graphite},lidFrame};
}
