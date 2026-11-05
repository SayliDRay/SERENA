import { mountPhoneCall } from './phone-call.js';
import { TouchGesture } from './interaction.js';

const $=id=>document.getElementById(id);
const reducedMotion=matchMedia('(prefers-reduced-motion: reduce)').matches;
const gesture=new TouchGesture();
const phoneCall=mountPhoneCall(document.querySelector('.inspector'));
const state={finish:'stone',opening:0,construction:false,light:'off',hotspots:true,rotation:false,feature:null};
const current={opening:0,construction:0,press:0};
let THREE,renderer,camera,scene,controls,model,cameraMove=null,canvasPress=false;
let lastTime=0,toastTimer,resizeObserver,renderedFrames=0;
const cameraViews={
  perspective:{position:[134,139,164],target:[0,-15,0],name:'PERSPECTIVE',index:'01'},
  front:{position:[5,155,160],target:[0,-11,0],name:'DE FACE',index:'02'},
  profile:{position:[205,25,20],target:[0,-13,0],name:'PROFIL',index:'03'},
  back:{position:[103,-148,143],target:[0,-16,0],name:'DESSOUS',index:'04'},
};

function toast(message){$('toast').textContent=message;$('toast').classList.add('show');clearTimeout(toastTimer);toastTimer=setTimeout(()=>$('toast').classList.remove('show'),3200);}
function updateLid(degrees){
  state.opening=degrees*Math.PI/180;
  $('lid-angle').value=String(Math.round(degrees));$('angle-value').value=`${Math.round(degrees)}°`;
  $('lid-angle').setAttribute('aria-valuenow',String(Math.round(degrees)));
  $('lid-angle').setAttribute('aria-valuetext',`${Math.round(degrees)} degrés`);
  const open=degrees>15;
  $('lid-toggle').setAttribute('aria-checked',String(open));$('lid-toggle').setAttribute('aria-label',open?'Fermer le capot':'Ouvrir le capot');
  $('lid-state').textContent=open?'Un accès discret à l’identification.':'Un geste à portée de main.';
  $('lid-description').textContent=open?'Un motif illustratif se dévoile sous le capot.':'Le grand capot tactile protège la zone d’identification.';
  $('touch-test').disabled=degrees>5||state.construction;
  if(degrees>5)endPress();
}
function setConstruction(value){
  state.construction=value;
  $('hotspots').hidden=value||!state.hotspots;
  $('construction').setAttribute('aria-pressed',String(value));
  $('construction').querySelector('span').textContent=value?'Réassembler le bracelet':'Voir les composants et le matériel';
  $('construction').lastElementChild.textContent=value?'−':'+';
  if(value){updateLid(100);setCamera({position:[235,220,290],target:[26,12,0]});endPress();}
  else {updateLid(0);setView('perspective');}
  $('touch-test').disabled=value||state.opening>.09;
}
function setCamera(view){
  if(!camera)return;
  state.rotation=false;$('autorotate').setAttribute('aria-pressed','false');$('autorotate').classList.remove('selected');
  cameraMove={start:performance.now(),duration:reducedMotion?1:900,from:camera.position.clone(),to:new THREE.Vector3(...view.position),targetFrom:controls.target.clone(),targetTo:new THREE.Vector3(...view.target)};
}
function setView(key){
  const view=cameraViews[key];if(!view)return;
  document.querySelectorAll('[data-view]').forEach(button=>{const active=button.dataset.view===key;button.classList.toggle('active',active);button.setAttribute('aria-pressed',String(active));});
  $('view-name').textContent=view.name;$('view-number').textContent=view.index;setCamera(view);
}
function setLight(value){
  state.light=value;
  document.querySelectorAll('[data-light]').forEach(button=>{const selected=button.dataset.light===value;button.classList.toggle('active',selected);button.setAttribute('aria-pressed',String(selected));});
}
function setFeature(feature){
  state.feature=feature;
  document.querySelectorAll('.hotspot').forEach(button=>button.classList.toggle('active',button.dataset.feature===feature));
  if(state.construction)setConstruction(false);
  if(feature==='lid'){
    updateLid(110);setCamera({position:[108,127,143],target:[0,1,0]});
    toast('Le capot révèle l’identification · motif de démonstration');
  }else if(feature==='touch'){
    updateLid(0);setCamera({position:[98,133,126],target:[0,-2,0]});toast('Un repère en relief, une grande surface tactile');
  }else if(feature==='light'){
    updateLid(0);setCamera({position:[144,40,76],target:[9,5,10]});toast('USB-C latéral avec cache attaché');
  }else if(feature==='audio'){
    setCamera({position:[144,44,74],target:[0,-1,0]});toast('Grille et microphone : détails de forme uniquement');
  }
  if(window.scrollY>100)$('explorer').scrollIntoView({behavior:reducedMotion?'instant':'smooth'});
}
function beginPress(){
  if(state.opening>.09||state.construction)return;
  gesture.press(performance.now());$('touch-test').classList.add('holding');
}
function endPress(activate=false){if(activate===true)gesture.release(performance.now());else gesture.cancel();canvasPress=false;$('touch-test').classList.remove('holding');if(controls)controls.enabled=true;}

for(const button of document.querySelectorAll('[data-view]'))button.addEventListener('click',()=>setView(button.dataset.view));
for(const button of document.querySelectorAll('[data-finish]'))button.addEventListener('click',()=>{
  state.finish=button.dataset.finish;model?.finish(state.finish);
  const names={stone:'Pierre naturelle',sage:'Sauge minérale',clay:'Argile douce'};$('finish-name').textContent=names[state.finish];
  document.querySelectorAll('[data-finish]').forEach(item=>{const selected=item===button;item.classList.toggle('active',selected);item.setAttribute('aria-pressed',String(selected));});
});
for(const button of document.querySelectorAll('[data-light]'))button.addEventListener('click',()=>setLight(button.dataset.light));
for(const button of document.querySelectorAll('[data-feature]'))button.addEventListener('click',()=>setFeature(button.dataset.feature));
$('lid-toggle').addEventListener('click',()=>{if(state.construction)setConstruction(false);updateLid(state.opening>.3?0:110);});
$('lid-angle').addEventListener('input',event=>{if(state.construction){state.construction=false;$('hotspots').hidden=!state.hotspots;$('construction').setAttribute('aria-pressed','false');$('construction').querySelector('span').textContent='Voir les composants et le matériel';$('construction').lastElementChild.textContent='+';}updateLid(Number(event.target.value));});
$('construction').addEventListener('click',()=>setConstruction(!state.construction));
$('autorotate').addEventListener('click',()=>{state.rotation=!state.rotation;cameraMove=null;$('autorotate').setAttribute('aria-pressed',String(state.rotation));$('autorotate').classList.toggle('selected',state.rotation);$('autorotate').setAttribute('aria-label',state.rotation?'Arrêter la rotation automatique':'Activer la rotation automatique');});
$('toggle-hotspots').addEventListener('click',()=>{state.hotspots=!state.hotspots;$('hotspots').hidden=state.construction||!state.hotspots;$('toggle-hotspots').setAttribute('aria-pressed',String(state.hotspots));$('toggle-hotspots').classList.toggle('selected',state.hotspots);$('toggle-hotspots').setAttribute('aria-label',state.hotspots?'Masquer les repères':'Afficher les repères');});
$('touch-test').addEventListener('pointerdown',event=>{if(event.button!==0)return;event.preventDefault();$('touch-test').setPointerCapture(event.pointerId);beginPress();});
$('touch-test').addEventListener('pointerup',()=>endPress(true));
for(const name of ['pointercancel','lostpointercapture'])$('touch-test').addEventListener(name,()=>{if(gesture.active)gesture.cancel();canvasPress=false;});
$('touch-test').addEventListener('keydown',event=>{if((event.key===' '||event.key==='Enter')&&!event.repeat){event.preventDefault();beginPress();}});
$('touch-test').addEventListener('keyup',event=>{if(event.key===' '||event.key==='Enter'){event.preventDefault();endPress(true);}});
$('touch-test').addEventListener('blur',endPress);
window.addEventListener('blur',endPress);
document.addEventListener('visibilitychange',()=>{if(document.hidden)endPress();});
$('reference-open').addEventListener('click',()=>$('reference-dialog').showModal());
$('reference-close').addEventListener('click',()=>$('reference-dialog').close());
$('reference-dialog').addEventListener('click',event=>{if(event.target===$('reference-dialog'))$('reference-dialog').close();});
$('retry').addEventListener('click',()=>location.reload());
$('export').addEventListener('click',()=>{
  if(!renderer){toast('La vue 3D doit être chargée pour être enregistrée.');return;}
  const oldBackground=scene.background;scene.background=new THREE.Color(0xf4f1eb);renderer.render(scene,camera);
  renderer.domElement.toBlob(blob=>{if(!blob){toast('L’export n’a pas abouti.');return;}const url=URL.createObjectURL(blob),link=document.createElement('a');link.href=url;link.download=`NORA-design-${state.finish}.png`;link.click();setTimeout(()=>URL.revokeObjectURL(url),1000);toast('La vue du bracelet a été enregistrée.');},'image/png');
  scene.background=oldBackground;
});

async function init(){
  try{
    THREE=await import('three');
    const [{OrbitControls},{RoomEnvironment},{createBracelet}]=await Promise.all([
      import('three/addons/controls/OrbitControls.js'),import('three/addons/environments/RoomEnvironment.js'),import('./model.js'),
    ]);
    scene=new THREE.Scene();
    camera=new THREE.PerspectiveCamera(36,1,.1,1000);
    camera.position.set(...cameraViews.perspective.position);
    renderer=new THREE.WebGLRenderer({antialias:true,alpha:true,powerPreference:'high-performance'});
    renderer.setPixelRatio(Math.min(devicePixelRatio,1.8));
    renderer.outputColorSpace=THREE.SRGBColorSpace;renderer.toneMapping=THREE.ACESFilmicToneMapping;renderer.toneMappingExposure=.95;
    renderer.shadowMap.enabled=true;renderer.shadowMap.type=THREE.VSMShadowMap;
    $('scene').appendChild(renderer.domElement);
    const pmrem=new THREE.PMREMGenerator(renderer),room=new RoomEnvironment();
    const env=pmrem.fromScene(room,.04);scene.environment=env.texture;scene.environmentIntensity=.6;room.dispose();pmrem.dispose();
    scene.add(new THREE.HemisphereLight(0xfff9ec,0xb2afa0,.8));
    const key=new THREE.DirectionalLight(0xfffaf0,2.4);key.position.set(-65,150,95);key.castShadow=true;
    key.shadow.mapSize.set(2048,2048);Object.assign(key.shadow.camera,{left:-95,right:95,top:95,bottom:-95,near:1,far:380});key.shadow.normalBias=.06;key.shadow.bias=-.0002;key.shadow.radius=5;key.shadow.blurSamples=8;scene.add(key);
    const fill=new THREE.DirectionalLight(0xe9f0f3,.7);fill.position.set(90,35,-70);scene.add(fill);
    const shadow= new THREE.Mesh(new THREE.PlaneGeometry(1200,1200),new THREE.ShadowMaterial({opacity:.025}));shadow.rotation.x=-Math.PI/2;shadow.position.y=-52.3;shadow.receiveShadow=true;scene.add(shadow);
    const contactCanvas=document.createElement('canvas');contactCanvas.width=contactCanvas.height=256;
    const ctx=contactCanvas.getContext('2d'),gradient=ctx.createRadialGradient(128,128,4,128,128,120);gradient.addColorStop(0,'rgba(55,48,32,.24)');gradient.addColorStop(.45,'rgba(55,48,32,.12)');gradient.addColorStop(1,'rgba(55,48,32,0)');ctx.fillStyle=gradient;ctx.fillRect(0,0,256,256);
    const contact=new THREE.Mesh(new THREE.PlaneGeometry(77,68),new THREE.MeshBasicMaterial({map:new THREE.CanvasTexture(contactCanvas),transparent:true,depthWrite:false,opacity:.65}));contact.rotation.x=-Math.PI/2;contact.position.set(0,-52.15,0);scene.add(contact);
    model=createBracelet();scene.add(model.root);
    controls=new OrbitControls(camera,renderer.domElement);controls.target.set(...cameraViews.perspective.target);controls.enableDamping=true;controls.dampingFactor=.065;controls.minDistance=80;controls.maxDistance=350;controls.enablePan=false;controls.maxPolarAngle=Math.PI*.89;controls.minPolarAngle=.12;controls.autoRotateSpeed=.7;controls.update();
    controls.addEventListener('start',()=>{cameraMove=null;state.feature=null;document.querySelectorAll('.hotspot').forEach(item=>item.classList.remove('active'));});
    const resize=()=>{const box=$('scene').getBoundingClientRect();renderer.setSize(box.width,box.height);camera.aspect=box.width/box.height;camera.updateProjectionMatrix();
      // A vertical finger swipe can scroll the mobile page; horizontal motion
      // remains available for orbiting. Pointer cancellation also ends a press.
      renderer.domElement.style.touchAction=matchMedia('(max-width: 800px)').matches?'pan-y':'none';
      // Shift the composition away from the title at desktop sizes, and down
      // beneath the title on touch layouts. View offsets preserve orbit targets.
      if(box.width>800)camera.setViewOffset(box.width,box.height,-box.width*.075,0,box.width,box.height);
      else camera.setViewOffset(box.width,box.height,0,-box.height*.075,box.width,box.height);
    };
    resizeObserver=new ResizeObserver(resize);resizeObserver.observe($('scene'));resize();
    const ray=new THREE.Raycaster(),pointer=new THREE.Vector2();
    renderer.domElement.addEventListener('pointerdown',event=>{
      if(event.button!==0||state.opening>.09||state.construction)return;
      const box=renderer.domElement.getBoundingClientRect();pointer.set((event.clientX-box.left)/box.width*2-1,-(event.clientY-box.top)/box.height*2+1);ray.setFromCamera(pointer,camera);
      if(ray.intersectObject(model.button).length){canvasPress=true;controls.enabled=false;renderer.domElement.setPointerCapture(event.pointerId);beginPress();}
    });
    renderer.domElement.addEventListener('pointerup',()=>{if(canvasPress)endPress(true);});
    for(const name of ['pointercancel','lostpointercapture'])renderer.domElement.addEventListener(name,()=>{if(gesture.active)gesture.cancel();canvasPress=false;if(controls)controls.enabled=true;});
    renderer.domElement.addEventListener('webglcontextlost',event=>{event.preventDefault();endPress();$('scene-error').hidden=false;$('error-message').textContent='Le contexte graphique a été interrompu. Cliquez sur Réessayer.';});
    document.body.dataset.ready='true';$('loading').hidden=true;
    requestAnimationFrame(frame);
  }catch(error){
    console.error('NORA design:',error);$('loading').hidden=true;$('scene-error').hidden=false;$('hotspots').hidden=true;
    $('error-message').textContent=`Le navigateur n’a pas pu démarrer la 3D. Lancez le serveur local et vérifiez la disponibilité de WebGL. (${error.message})`;
    document.body.dataset.ready='error';
  }
}

function frame(now){
  const dt=Math.min((now-lastTime)/1000,.05)||.016;lastTime=now;
  const tick=gesture.tick(now),smoothing=reducedMotion?1:1-Math.exp(-dt*8);
  current.opening+=(state.opening-current.opening)*smoothing;
  current.construction+=((state.construction?1:0)-current.construction)*smoothing;
  current.press+=((gesture.active?1:0)-current.press)*Math.min(1,smoothing*2);
  let brightness=state.light==='off'?0:state.light==='breathe'?.4+(Math.sin(now*.0018)+1)*.4:1;
  if(gesture.active)brightness=.45+tick.progress*1.4;
  if(tick.confirming)brightness=1.2+(Math.sin(now*.018)+1)*.5;
  model.animate({opening:current.opening,construction:current.construction,press:current.press,brightness,vibration:0,time:now});
  if(tick.shortPress){toast('Connexion…');void phoneCall.arm().catch(()=>toast('Appel indisponible. Vérifiez la connexion.'));}
  if(tick.longPress){toast('Raccrochage…');void phoneCall.hangup().catch(()=>toast('Raccrochez sur le téléphone.'));}
  $('hold-progress').style.strokeDashoffset=String(100.53*(1-tick.progress));
  const completed=tick.confirming||tick.progress===1;
  $('touch-label').textContent=gesture.active?'Maintenir pour raccrocher':'Appeler mon contact';
  $('touch-hint').textContent=gesture.active?`${Math.max(0,2-(now-gesture.started)/1000).toFixed(1)} s`:'Appui bref : appeler · appui long 2 s : raccrocher';
  if(cameraMove){
    const t=Math.min((now-cameraMove.start)/cameraMove.duration,1),ease=t*t*(3-2*t);
    camera.position.lerpVectors(cameraMove.from,cameraMove.to,ease);controls.target.lerpVectors(cameraMove.targetFrom,cameraMove.targetTo,ease);
    if(t===1)cameraMove=null;
  }
  controls.autoRotate=state.rotation;controls.update();model.root.updateMatrixWorld(true);
  if(state.hotspots&&renderedFrames%2===0){
    const box=$('scene').getBoundingClientRect();
    for(const button of document.querySelectorAll('.hotspot')){
      const anchor=model.anchors[button.dataset.feature],point=anchor.object.localToWorld(anchor.point.clone()).project(camera);
      const x=(point.x*.5+.5)*box.width,y=(-point.y*.5+.5)*box.height;
      const visible=point.z>-1&&point.z<1&&x>18&&x<box.width-18&&y>35&&y<box.height-100;
      button.style.left=`${x}px`;button.style.top=`${y}px`;button.style.opacity=visible?'1':'0';button.style.pointerEvents=visible?'auto':'none';
    }
  }
  renderedFrames++;renderer.render(scene,camera);requestAnimationFrame(frame);
}

init();
