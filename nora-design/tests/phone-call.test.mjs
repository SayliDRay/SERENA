import {test} from 'node:test';
import assert from 'node:assert/strict';
import {PhoneCall} from '../phone-call.js';

test('concurrent gestures create one request; hang-up goes to backend',async()=>{
  const requests=[];
  const phone=new PhoneCall({fetcher:async path=>{
    requests.push(path);
    return {ok:true,json:async()=>({configured:true,token:'test',status:path==='/api/arm'?'countdown':'idle'})};
  }});
  await phone.refresh();await Promise.all([phone.arm(),phone.arm()]);
  assert.equal(requests.filter(path=>path==='/api/arm').length,1);
  await phone.hangup();assert.equal(requests.at(-1),'/api/hangup');
});
test('unconfigured or offline backend cannot dial',async()=>{
  const requests=[];
  const phone=new PhoneCall({fetcher:async path=>{requests.push(path);throw Error('offline');}});
  await phone.refresh();await assert.rejects(phone.arm());
  assert.deepEqual(requests,['/api/status']);
});
