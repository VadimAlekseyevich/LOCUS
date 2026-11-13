/* LOCUS wireframe navigation and in-memory demo interactions. */
(function(){
'use strict';
const M=window.LocusWire,P=window.LocusWirePages,s=M.state,H=M.escape;
const screen=document.getElementById('screen');
const stage=document.getElementById('stage');
const holder=document.getElementById('stageHolder');
const overlay=document.getElementById('overlay');
const modal=document.getElementById('modal');
const view=document.getElementById('view');
const finish=document.getElementById('complete');
let pending=null,previousFocus=null,savedSkin=M.deep(s.skin);
const originalSongs=M.deep(M.songs),originalActions=M.deep(M.actions);
function fit(){
 const available=Math.max(300,document.documentElement.clientWidth-32);
 const scale=Math.min(1,available/960);
 holder.style.width=(960*scale)+'px';
 holder.style.height=(540*scale)+'px';
 stage.style.setProperty('--scale',String(scale));
}
function syncPicker(){
 const key=['editor','skins','settings'].includes(s.view)?
  s.view+'/'+({editor:s.editorTab,skins:s.skinTab,settings:s.settingsTab}[s.view]):s.view;
 if([...view.options].some(opt=>opt.value===key))view.value=key;
 finish.hidden=s.view!=='play';
}
function render(){
 const scroll=screen.scrollTop;
 screen.innerHTML=P.page();
 screen.scrollTop=scroll;
 syncPicker();
 paintModal();
}
function navigate(page){
 if(s.view==='editor'&&page!=='editor'&&s.editorDirty){
  pending=page;openDialog('unsaved');return;
 }
 s.view=page;s.dialog=null;render();
}
function enterEditor(loaded,tab){
 s.editorLoaded=loaded;
 const song=M.song();
 s.setMeta=loaded?{title:song.title,artist:song.artist,mapper:song.mapper||'',tags:(song.tags||[]).join(', ')}:
   {title:'',artist:'',mapper:'',tags:''};
 s.editorTab=tab||'route';
 s.editorTitle=loaded?(M.chart()?.title||'Untitled'):'Untitled';
 s.editorDirty=false;
 s.undo=[];s.redo=[];s.compared=[];s.focusedPane=0;
 s.notes=loaded?M.deep(M.initial().notes):[];
 s.tempo=[{time:loaded?M.song().offset:0,bpm:loaded?M.song().bpm:120}];
 navigate('editor');
}
function reviewPage(key){
 const bits=key.split('/');
 if(bits[0]==='editor'){
  s.editorLoaded=true;
  s.editorTitle=M.chart()?.title||'Draft';
  const song=M.song();
  s.setMeta={title:song.title,artist:song.artist,mapper:song.mapper||'',tags:(song.tags||[]).join(', ')};
  s.editorTab=['set','route','audio'].includes(bits[1])?bits[1]:'route';
 }else if(bits[0]==='skins')s.skinTab=bits[1]||'path';
 else if(bits[0]==='settings')s.settingsTab=bits[1]==='audio'?'audio':'display';
 else if(bits[0]==='results'&&!M.result()){
  s.resultId=M.runs()[0]?.id||null;
 }
 s.view=bits[0];s.editorDirty=false;s.dialog=null;render();
}
const dialogs={
 quit:{title:'EXIT LOCUS?',copy:'Close the application?',actions:[['CANCEL','cancel'],['EXIT','quit-confirm']]},
 unsaved:{title:'UNSAVED CHART',copy:'Save changes before leaving?',actions:[['CANCEL','cancel'],['DISCARD','unsaved-discard'],['SAVE','unsaved-save']]},
 'delete-chart':{title:'DELETE DIFFICULTY?',copy:'Move the selected difficulty to Trash?',actions:[['CANCEL','cancel'],['DELETE','delete-chart-confirm']]},
 'delete-set':{title:'DELETE SONG SET?',copy:'Move audio and all difficulties to Trash?',actions:[['CANCEL','cancel'],['DELETE','delete-set-confirm']]},
 'skin-delete':{title:'DELETE SKIN?',copy:'Delete the selected skin profile?',actions:[['CANCEL','cancel'],['DELETE','skin-delete-confirm']]},
 leave:{title:'LEAVE LEVEL?',copy:'The current attempt will be lost.',actions:[['RESUME','cancel'],['LEAVE','leave-confirm']]},
 retry:{title:'RESTART CHART?',copy:'The current attempt will be lost.',actions:[['CANCEL','cancel'],['RETRY','retry-confirm']]},
 file:{title:'FILE OPERATION',copy:'Native file picker is not connected in the wireframe.',actions:[['CLOSE','cancel']]},
 saveas:{title:'SAVE AS',copy:'Difficulty name',input:'Chart Copy',actions:[['CANCEL','cancel'],['SAVE','saveas-confirm']]},
 compare:{title:'COMPARE CHARTS',copy:'',actions:[['DONE','cancel']]},
 calibrate:{title:'GLOBAL OFFSET CALIBRATION',copy:'100 BPM · 18 taps',actions:[['CANCEL','cancel'],['RETRY','calibrate-reset'],['TAP','calibrate-tap']]},
 'skin-saveas':{title:'SAVE SKIN AS',copy:'Profile name',input:'New skin',actions:[['CANCEL','cancel'],['SAVE','skin-saveas-confirm']]}
};
function openDialog(kind){
 if(!dialogs[kind])return;
 previousFocus=document.activeElement;
 s.dialog=kind;paintModal();
 const focus=modal.querySelector('input')||modal.querySelector('button');
 if(focus)focus.focus();
}
function closeDialog(){
 pending=null;s.dialog=null;paintModal();
 if(previousFocus?.isConnected)previousFocus.focus();
 previousFocus=null;
}
function paintModal(){
 overlay.hidden=!s.dialog;
 if(!s.dialog){modal.innerHTML='';return;}
 const d=dialogs[s.dialog];
 let content='<h2 id="modalTitle">'+H(d.title)+'</h2>';
 if(d.copy)content+='<p>'+H(d.copy)+'</p>';
 if(d.input)content+='<input id="dialogInput" aria-label="Name" type="text" maxlength="100" value="'+H(d.input)+'">';
 if(s.dialog==='compare'){
  const other=M.song().charts.filter(c=>c.id!==M.chart()?.id);
  content+='<div class="column">'+other.map(c=>'<button data-compare="'+H(c.id)+'" type="button" aria-pressed="'+s.compared.includes(c.id)+'">'+
   H(c.title)+' · '+c.difficulty.toFixed(1)+'</button>').join('')+'</div>';
 }
 if(s.dialog==='calibrate'){
  content+='<div class="calibration-progress">'+s.calibration+' / 18</div>';
  if(s.calibration===18)content+='<div class="row"><button type="button" class="primary" data-modal-action="calibrate-apply">APPLY +24 ms</button></div>';
 }
 content+='<div class="modal-actions">'+d.actions.map(a=>
  '<button type="button" data-modal-action="'+H(a[1])+'" class="'+(a[1]==='cancel'?'':'primary')+'">'+H(a[0])+'</button>').join('')+'</div>';
 modal.innerHTML=content;
}
function pushUndo(){
 s.undo.push(M.deep(s.notes));if(s.undo.length>40)s.undo.shift();
 s.redo.length=0;s.editorDirty=true;
}
function transformNote(fn){
 const n=s.notes.find(x=>x.beat===s.beat);
 if(!n)return;
 const dirs=['↑','↗','→','↘','↓','↙','←','↖'];
 const old=dirs.indexOf(n.dir);if(old<0)return;
 pushUndo();n.dir=dirs[(fn(old)+8)%8];render();
}
function saveEditor(){
 const meta=s.setMeta;
 if(s.editorLoaded){
   const song=M.song();
   song.title=meta.title.trim()||song.title;
   song.artist=meta.artist.trim();
   song.mapper=meta.mapper.trim();
   song.tags=meta.tags.split(',').map(x=>x.trim()).filter(Boolean);
   const chart=M.chart();
   if(chart)chart.title=s.editorTitle.trim()||chart.title;
 }else if(meta.title.trim()){
   const id='set-'+M.songs.length+'-'+s.runCounter;
   const song={id,title:meta.title.trim(),artist:meta.artist.trim(),mapper:meta.mapper.trim(),
     tags:meta.tags.split(',').map(x=>x.trim()).filter(Boolean),
     bpm:s.tempo[0]?.bpm||120,offset:s.tempo[0]?.time||0,duration:192,
     charts:[{id:'draft',title:s.editorTitle.trim()||'Untitled',
       difficulty:0,notes:s.notes.length,dims:[0,0,0,0,0,0]}]};
   M.songs.push(song);s.song=id;s.chart='draft';s.editorLoaded=true;s.expanded=id;
 }
 s.editorDirty=false;
 render();
}
function editTool(name){
 const n=s.notes.find(x=>x.beat===s.beat);
 if(name==='undo'||name==='redo'){
  const from=name==='undo'?s.undo:s.redo;
  const to=name==='undo'?s.redo:s.undo;
  if(!from.length)return;
  to.push(M.deep(s.notes));s.notes=from.pop();s.editorDirty=true;render();return;
 }
 if(name==='copy'||name==='cut'){
  if(!n)return;s.clipboard=n.dir;
  if(name==='cut')note('DEL');return;
 }
 if(name==='paste'){if(s.clipboard)note(s.clipboard);return;}
 if(name==='flip-h')transformNote(i=>[0,7,6,5,4,3,2,1][i]);
 if(name==='flip-v')transformNote(i=>[4,3,2,1,0,7,6,5][i]);
 if(name==='rot-ccw')transformNote(i=>i-2);
 if(name==='rot-cw')transformNote(i=>i+2);
}
function note(dir){
 pushUndo();
 s.notes=s.notes.filter(x=>x.beat!==s.beat);
 if(dir!=='DEL')s.notes.push({beat:s.beat,dir});
 s.notes.sort((a,b)=>a.beat-b.beat);
 render();
}
function completeAttempt(){
 const song=M.song(),chart=M.chart();
 if(!chart)return;
 const count=chart.notes,number=++s.runCounter;
 const good=Math.floor(count*(.07+(number%3)*.01));
 const bad=Math.floor(count*.009);
 const miss=Math.floor(count*.005);
 const perfect=count-good-bad-miss;
 const run={id:'r'+number,datetime:new Date().toISOString(),count,perfect,good,bad,miss};
 const key=M.chartKey();
 if(!s.results[key])s.results[key]=[];
 s.results[key].unshift(run);
 s.resultId=run.id;
 s.paused=false;s.view='results';render();
}
function openResult(id){
 const found=M.runs().find(r=>r.id===id);
 if(!found)return;
 s.resultId=id;s.view='results';render();
}
function action(name){
 if(name==='home'){navigate('home');return;}
 if(name==='library'||name==='settings'||name==='skins'){navigate(name);return;}
 if(name==='editor-new'){enterEditor(false,'set');return;}
 if(name==='editor-open'||name==='editor-set'){enterEditor(true,name==='editor-set'?'set':'route');return;}
 if(name==='play'){
  s.paused=false;
  if(s.view==='editor'){s.view='play';render();}
  else navigate('play');
  return;
 }
 if(name==='pause'){s.paused=!s.paused;render();return;}
 if(name==='quit'){openDialog('quit');return;}
 if(name.startsWith('dialog-')){openDialog(name.slice(7));return;}
 if(['file','export','rescan','restore','background','remove-bg'].includes(name)){
  if(name==='rescan')return;
  if(name==='background'){s.background=true;render();return;}
  if(name==='remove-bg'){s.background=false;render();return;}
  openDialog('file');return;
 }
 if(name==='delete-chart'||name==='delete-set'){openDialog(name);return;}
 if(name==='skin-delete'){openDialog('skin-delete');return;}
 if(name==='leave'||name==='retry'){openDialog(name);return;}
 if(name==='compare'){openDialog('compare');return;}
 if(name==='calibrate'){s.calibration=0;openDialog('calibrate');return;}
 if(name.startsWith('editor-')){
  s.editorTab=name.slice(7);render();return;
 }
 if(name.startsWith('settings-')){s.settingsTab=name.slice(9);render();return;}
 if(name.startsWith('bind-')){s.bindGroup=name.slice(5);s.bindCapture=null;render();return;}
 if(name.startsWith('skin-toggle-')){
  const key=name.slice('skin-toggle-'.length);s.skin[key]=!s.skin[key];s.skinDirty=true;render();return;
 }
 if(name.startsWith('skin-')){s.skinTab=name.slice(5);render();return;}
 if(name.startsWith('rate-')){s.practice=Number(name.slice(5));render();return;}
 if(name.startsWith('tool-')){editTool(name.slice(5));return;}
 if(['flip-h','flip-v','rot-ccw','rot-cw'].includes(name)){editTool(name);return;}
 if(name==='heading-prev'||name==='heading-next'){
  s.headingIndex=(s.headingIndex+(name==='heading-next'?1:7))%8;s.editorDirty=true;render();return;
 }
 if(name==='snap-more'||name==='snap-less'){
  const values=[1,2,3,4,6,8,12,16,24];
  const current=values.indexOf(s.snap);
  const step=name==='snap-more'?1:-1;
  s.snap=values[Math.max(0,Math.min(values.length-1,current+step))];render();return;
 }
 if(name==='zoom-more'||name==='zoom-less'){
  const values=[50,75,100,125,150,175,200];
  const current=values.indexOf(s.zoom);
  const step=name==='zoom-more'?1:-1;
  s.zoom=values[Math.max(0,Math.min(values.length-1,current+step))];render();return;
 }
 if(name==='speed-up'||name==='speed-down'){
  s.editorSpeed=Math.max(20,Math.min(2000,s.editorSpeed+(name==='speed-up'?10:-10)));render();return;
 }
 if(name==='path-up'||name==='path-down'){
  s.editorPath=Math.max(50,Math.min(10000,s.editorPath+(name==='path-up'?50:-50)));render();return;
 }
 if(name==='transport-play'){s.editorPlaying=!s.editorPlaying;render();return;}
 if(name==='transport-restart'){s.editorPlaying=false;s.songPosition=0;s.beat=0;render();return;}
 if(name==='copy-time'){
  const ms=Math.round(s.songPosition*M.song().duration*1000);
  if(navigator.clipboard?.writeText)navigator.clipboard.writeText(String(ms)).catch(()=>{});
  return;
 }
 if(name==='vsync'||name==='showFps'||name==='fullscreen'){
  s[name]=!s[name];render();return;
 }
 if(name==='tempo-add'){
  s.tempo.push({time:Math.round(s.songPosition*M.song().duration*1000),bpm:s.tempo[s.tempo.length-1].bpm});s.editorDirty=true;render();return;
 }
 if(name.startsWith('tempo-delete-')){
  const i=Number(name.slice(13));if(i>0){s.tempo.splice(i,1);s.editorDirty=true;render();}return;
 }
 if(name==='difficulty-new'||name==='difficulty-copy'){
  const song=M.song(),base=M.chart();
  const next={id:'draft'+Date.now(),title:name==='difficulty-copy'?base.title+' Copy':'New difficulty',difficulty:0,notes:0,dims:[0,0,0,0,0,0]};
  song.charts.push(next);s.chart=next.id;s.editorLoaded=true;s.editorTitle=next.title;s.editorDirty=true;render();return;
 }
 if(name==='skin-save'){savedSkin=M.deep(s.skin);s.skinDirty=false;render();return;}
 if(name==='skin-load'||name==='skin-reset'){s.skin=M.deep(savedSkin);s.skinDirty=false;render();return;}
 if(name==='skin-saveas'){openDialog('skin-saveas');return;}
 if(name==='skin-activate'){s.skinName=s.selectedProfile;s.skinDirty=false;render();return;}
 if(name==='saveas'){openDialog('saveas');return;}
}
function modalAction(name){
 if(name==='calibrate-apply'){
  s.globalOffset=24;closeDialog();render();return;
 }
 if(name==='cancel'){pending=null;closeDialog();return;}
 if(name==='quit-confirm'){closeDialog();s.view='home';render();return;}
 if(name==='unsaved-discard'||name==='unsaved-save'){
  if(name==='unsaved-save')saveEditor();
  s.editorDirty=false;
  const next=pending;pending=null;closeDialog();
  if(next){
    if(typeof next==='string')s.view=next;
    else {s.view=next.view;s.chart=next.chartId;s.editorTitle=M.chart().title;s.editorLoaded=true;}
    render();
  }
  return;
 }
 if(name==='leave-confirm'){s.paused=false;closeDialog();s.view='library';render();return;}
 if(name==='retry-confirm'){s.paused=false;closeDialog();s.view='play';render();return;}
 if(name==='calibrate-reset'){s.calibration=0;paintModal();return;}
 if(name==='calibrate-tap'){s.calibration=Math.min(18,s.calibration+1);paintModal();return;}
 if(name==='skin-saveas-confirm'){
  const value=modal.querySelector('#dialogInput')?.value.trim()||'My skin';
  if(!s.profiles.includes(value))s.profiles.push(value);
  s.skinName=value;s.selectedProfile=value;s.skinDirty=false;savedSkin=M.deep(s.skin);
  closeDialog();render();return;
 }
 if(name==='saveas-confirm'){
  s.editorTitle=modal.querySelector('#dialogInput')?.value.trim()||'Untitled';
  s.editorDirty=false;closeDialog();saveEditor();return;
 }
 if(name==='skin-delete-confirm'){
  if(s.selectedProfile!=='Default')s.profiles=s.profiles.filter(p=>p!==s.selectedProfile);
  s.selectedProfile='Default';closeDialog();render();return;
 }
 if(name==='delete-chart-confirm'){
  if(s.editorLoaded&&M.song().charts.length>1){
   M.song().charts=M.song().charts.filter(c=>c.id!==s.chart);
   s.chart=M.song().charts[0].id;s.editorTitle=M.chart().title;s.editorDirty=false;
  }
  closeDialog();render();return;
 }
 if(name==='delete-set-confirm'){closeDialog();s.editorLoaded=false;s.editorTitle='Untitled';render();return;}
 closeDialog();render();
}
function onClick(event){
 if(event.target===overlay){closeDialog();return;}
 const m=event.target.closest('[data-modal-action]');
 if(m){modalAction(m.dataset.modalAction);return;}
 const comp=event.target.closest('[data-compare]');
 if(comp){
  const id=comp.dataset.compare;
  if(s.compared.includes(id))s.compared=s.compared.filter(x=>x!==id);
  else if(s.compared.length<2)s.compared.push(id);
  paintModal();return;
 }
 if(s.dialog)return;
 const run=event.target.closest('[data-run]');
 if(run){openResult(run.dataset.run);return;}
 const song=event.target.closest('[data-song]');
 if(song){
  const id=song.dataset.song;s.expanded=s.expanded===id?'':id;
  s.song=id;s.chart=M.song().charts[0]?.id||'';s.resultId=M.runs()[0]?.id||null;render();return;
 }
 const chart=event.target.closest('[data-chart]');
 if(chart){s.song=chart.dataset.set;s.chart=chart.dataset.chart;s.resultId=M.runs()[0]?.id||null;render();return;}
 const diff=event.target.closest('[data-difficulty]');
 if(diff){
  if(s.editorDirty){pending={view:'editor',chartId:diff.dataset.difficulty};openDialog('unsaved');return;}
  s.chart=diff.dataset.difficulty;s.editorTitle=M.chart().title;s.editorLoaded=true;render();return;
 }
 const skinColor=event.target.closest('[data-color]');
 if(skinColor){s.selectedColor=skinColor.dataset.color;render();return;}
 const profile=event.target.closest('[data-profile]');
 if(profile){s.selectedProfile=profile.dataset.profile;render();return;}
 const pane=event.target.closest('[data-pane]');
 if(pane){s.focusedPane=Number(pane.dataset.pane);render();return;}
 const noteButton=event.target.closest('[data-note]');
 if(noteButton){note(noteButton.dataset.note);return;}
 const bindButton=event.target.closest('[data-bind]');
 if(bindButton){
  s.bindCapture=Number(bindButton.dataset.bind);render();
  screen.querySelector('.capturing')?.focus();return;
 }
 const seek=event.target.closest('[data-seek]');
 if(seek){
  const rect=seek.getBoundingClientRect(),ratio=Math.max(0,Math.min(1,(event.clientX-rect.left)/rect.width));
  if(seek.dataset.seek==='note')s.beat=Math.round(ratio*12);
  else s.songPosition=ratio;render();return;
 }
 const btn=event.target.closest('[data-act]');
 if(btn)action(btn.dataset.act);
}
function updateField(input){
 const name=input.dataset.field,value=input.value;
 if(!name)return;
 if(name==='query'){
  const caret=input.selectionStart;
  s.query=value;render();const fresh=screen.querySelector('[data-field="query"]');
  fresh?.focus();fresh?.setSelectionRange(caret,caret);return;
 }
 if(name==='skinName'){s.skinName=value;s.skinDirty=true;return;}
 if(name==='editorTitle'){s.editorTitle=value;s.editorDirty=true;return;}
 if(name==='skinHex'){
  if(/^#[0-9a-f]{6}$/i.test(value)){s.skin.colors[s.selectedColor]=value;s.skinDirty=true;render();}return;
 }
 if(name==='fps'){s.fps=value;render();return;}
 if(name==='practice'){s.practice=Number(value);render();return;}
 const metaFields={setTitle:'title',setArtist:'artist',setMapper:'mapper',setTags:'tags'};
 if(metaFields[name]){
  s.setMeta[metaFields[name]]=value;
  s.editorDirty=true;return;
 }
 if(['previewStart','globalOffset'].includes(name)){
  s[name]=Number(value)||0;if(name==='previewStart')s.editorDirty=true;render();return;
 }
}
function onChange(event){
 const node=event.target;
 if(node.dataset.field){updateField(node);return;}
 if(node.dataset.range==='volume'){s.volume=Number(node.value);render();return;}
 if(node.dataset.skinRange||node.dataset.skinParam){
  const key=node.dataset.skinRange||node.dataset.skinParam;
  const def=M.selectedSkinSetting(key),v=Number(node.value);
  if(def&&Number.isFinite(v))s.skin[key]=Math.max(def.min,Math.min(def.max,v));
  s.skinDirty=true;render();return;
 }
 if(node.hasAttribute('data-color-input')){
  s.skin.colors[s.selectedColor]=node.value;s.skinDirty=true;render();return;
 }
 if(node.dataset.tempoTime!==undefined||node.dataset.tempoBpm!==undefined){
  const field=node.dataset.tempoTime!==undefined?'time':'bpm';
  const idx=Number(field==='time'?node.dataset.tempoTime:node.dataset.tempoBpm);
  const value=Number(node.value);
  if(s.tempo[idx]&&Number.isFinite(value)&&(!('bpm'===field)||value>0)){
   s.tempo[idx][field]=value;s.editorDirty=true;
  }render();return;
 }
}
document.addEventListener('click',onClick);
document.addEventListener('input',event=>{
 const field=event.target?.dataset?.field;
 if(field&&['setTitle','setArtist','setMapper','setTags','editorTitle'].includes(field))
   updateField(event.target);
});
document.addEventListener('change',onChange);
document.addEventListener('keydown',event=>{
 if(s.bindCapture!==null){
  if(event.key==='Escape'){s.bindCapture=null;render();return;}
  if(['Shift','Control','Alt','Meta'].includes(event.key))return;
  event.preventDefault();
  const row=M.actions[s.bindGroup][s.bindCapture];
  if(row)row[1]=(event.ctrlKey?'Ctrl+':'')+(event.shiftKey?'Shift+':'')+event.key;
  s.bindCapture=null;render();return;
 }
 if(event.key==='Escape'){
  if(s.dialog){closeDialog();return;}
  if(s.view==='play'){s.paused=!s.paused;render();return;}
  if(s.view==='results'){s.view='library';render();return;}
  if(!event.target.closest('input,select')){navigate('home');return;}
 }
 if(event.ctrlKey&&event.key.toLowerCase()==='s'&&s.view==='editor'){
  event.preventDefault();if(event.shiftKey)openDialog('saveas');else saveEditor();
  return;
 }
 if(s.view==='editor'&&s.editorTab==='route'&&!event.target.closest('input,select')){
  const key=event.key.toLowerCase();
  if(event.ctrlKey){
   const commands={c:'copy',x:'cut',v:'paste',z:'undo',y:'redo'};
   if(commands[key]){event.preventDefault();editTool(commands[key]);return;}
  }
  if(!event.ctrlKey&&key==='i'){
   event.preventDefault();editTool(event.shiftKey?'flip-v':'flip-h');return;
  }
  if(!event.ctrlKey&&!event.shiftKey&&key==='t'){
   event.preventDefault();action('play');return;
  }
 }
});
view.addEventListener('change',()=>reviewPage(view.value));
document.getElementById('reset').addEventListener('click',()=>{
 const original=M.initial();
 Object.keys(s).forEach(key=>delete s[key]);Object.assign(s,original);
 M.songs.splice(0,M.songs.length,...M.deep(originalSongs));
 Object.keys(M.actions).forEach(key=>{M.actions[key]=M.deep(originalActions[key]);});
 savedSkin=M.deep(s.skin);render();
});
finish.addEventListener('click',completeAttempt);
window.addEventListener('resize',fit);
fit();render();
window.LocusWireApp={reviewPage,completeAttempt,navigate,enterEditor,render};
})();
