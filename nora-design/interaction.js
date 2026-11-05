export class TouchGesture {
  constructor(duration=2000){this.duration=duration;this.reset();}
  reset(){this.started=null;this.confirmedAt=null;this.active=false;this.shortPending=false;}
  press(now){if(this.active)return;this.active=true;this.started=now;this.confirmedAt=null;}
  release(now){if(this.active&&this.confirmedAt===null&&now-this.started<this.duration)this.shortPending=true;this.active=false;this.started=null;}
  cancel(){this.active=false;this.started=null;this.shortPending=false;}
  tick(now){
    const progress=this.active?Math.min(1,Math.max(0,(now-this.started)/this.duration)):0;
    let longPress=false;if(progress===1&&this.confirmedAt===null){this.confirmedAt=now;longPress=true;}
    const shortPress=this.shortPending;this.shortPending=false;
    return {progress,confirming:this.confirmedAt!==null&&now-this.confirmedAt<500,shortPress,longPress,justConfirmed:shortPress};
  }
}
