import {test} from 'node:test';
import assert from 'node:assert/strict';
import {TouchGesture} from '../interaction.js';

test('short release requests one call',()=>{
  const gesture=new TouchGesture();gesture.press(0);gesture.release(120);
  assert.equal(gesture.tick(121).shortPress,true);
  assert.equal(gesture.tick(122).shortPress,false);
});
test('two-second hold hangs up without calling on release',()=>{
  const gesture=new TouchGesture();gesture.press(0);
  assert.equal(gesture.tick(1999).longPress,false);
  assert.equal(gesture.tick(2000).longPress,true);
  assert.equal(gesture.tick(2100).longPress,false);
  gesture.release(2200);assert.equal(gesture.tick(2201).shortPress,false);
});
test('canceled pointer never calls',()=>{
  const gesture=new TouchGesture();gesture.press(0);gesture.cancel();
  assert.equal(gesture.tick(100).shortPress,false);
});
