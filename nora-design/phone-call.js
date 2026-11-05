// Shared by the two 3D views. Only an explicit completed SOS gesture arms a call.
const allowed = new Set(['idle','completed','canceled','busy','failed','no-answer']);
export const callLabels = {idle:'Prêt',countdown:'Annulation possible',sending:'Connexion…',queued:'Demande acceptée',initiated:'Numérotation',ringing:'Votre téléphone sonne', 'in-progress':'Ligne décrochée',completed:'Appel terminé',canceled:'Annulé',busy:'Téléphone occupé',failed:'Appel refusé', 'no-answer':'Sans réponse',unknown:'Résultat à vérifier'};
export class PhoneCall {
  constructor({fetcher=globalThis.fetch.bind(globalThis),onUpdate=()=>{}}={}){
    this.fetcher=fetcher;this.onUpdate=onUpdate;this.state={status:'offline',configured:false};
    this.token='';this.pending=null;this.cancelRequested=false;
  }
  async refresh(){
    try{const response=await this.fetcher('/api/status');if(!response.ok)throw Error('Serveur téléphonique indisponible');
      const state=await response.json();if(typeof state.token!=='string'||typeof state.configured!=='boolean')throw Error('Backend absent');
      this.token=state.token;this.state=state;
    }catch{this.state={status:'offline',configured:false,message:'Le serveur téléphonique est indisponible. Ouvrez la page 3D depuis localhost:5176/bracelet/.'};}
    this.onUpdate(this.state);return this.state;
  }
  async post(name){
    const response=await this.fetcher('/api/'+name,{method:'POST',headers:{'X-Nora-Token':this.token}});
    const body=await response.json();if(!response.ok)throw Error(body.error||'Demande refusée');
    this.state=body;this.onUpdate(body);return body;
  }
  async arm(){
    if(this.pending)return this.pending;
    if(!this.state.configured||!allowed.has(this.state.status))throw Error('Appel réel indisponible ou déjà en cours. Consultez son état.');
    this.cancelRequested=false;
    this.pending=this.post('arm');
    try{const result=await this.pending;if(this.cancelRequested)await this.post('cancel');return result;}
    finally{this.pending=null;}
  }
  async hangup(){if(this.pending)await this.pending;return this.post('hangup');}
  async cancel(){
    if(this.pending){this.cancelRequested=true;return;}
    if(this.state.status==='countdown')return this.post('cancel');
  }
}

export function mountPhoneCall(parent){
  const panel=document.createElement('section');panel.className='real-call-panel';
  panel.innerHTML='<strong>MON CONTACT</strong><p class="real-call-state" aria-live="polite">Connexion au serveur…</p><p class="real-call-detail"></p><button type="button" class="real-call-cancel" disabled>Raccrocher</button>';
  parent.prepend(panel);
  const status=panel.querySelector('.real-call-state'),detail=panel.querySelector('.real-call-detail'),cancel=panel.querySelector('button');
  const phone=new PhoneCall({onUpdate(state){
    status.textContent=!state.configured&&state.status!=='offline'?'Démonstration 3D · appels non configurés':state.status==='countdown'?'Connexion…':(callLabels[state.status]||'Connexion indisponible');
    detail.textContent=state.message?.startsWith('Raccrochage non confirmé')?'Raccrochez sur le téléphone : la fin d’appel n’est pas confirmée.':state.errorCode?'Connexion refusée. Vérifiez la configuration après la présentation.':state.status==='idle'?'Appui bref : appeler. Appui long 2 s : raccrocher.':state.status==='in-progress'?'Votre contact a décroché.':state.status==='ringing'?'En attente de votre contact…':'';
    cancel.disabled=['idle','completed','canceled','failed','busy','no-answer','offline'].includes(state.status);
  }});
  cancel.addEventListener('click',()=>phone.hangup().catch(error=>{detail.textContent=error.message;}));
  void phone.refresh();const timer=setInterval(()=>phone.refresh(),1000);
  window.addEventListener('pagehide',()=>clearInterval(timer),{once:true});
  return phone;
}
