/* LOCUS UI v2 — browser interactions. Purely demonstrative, no network or persistence. */
(function(){
'use strict';
const L=window.LocusDemo,S=window.LocusScreens,s=L.state,H=L.H;
const screen=document.getElementById('screen'),overlay=document.getElementById('overlay'),dialog=document.getElementById('dialog');
const topbar=document.getElementById('topbar'),navrail=document.getElementById('navrail');
const titles={home:'Home',library:'Song library',editor:'Chart editor',skins:'Skins',settings:'Settings',
 play:'Play',results:'Results',system:'Components'};
const navigationPages=['home','library','editor','skins','settings','system'];
const startingLibrary=JSON.stringify(L.library),startingBindings=JSON.stringify(L.bindings);
const modalTypes={
 quit:{icon:'↗',title:'Exit LOCUS?',message:'Close LOCUS?',buttons:[['Stay','cancel','ghost'],['Exit preview','exit-confirm','danger-button']]},
 unsaved:{icon:'!',title:'Unsaved chart changes',message:'Save changes before leaving?',buttons:[['Cancel','cancel','ghost'],['Discard demo changes','discard-changes','danger-button'],['Save demo','save-changes','button blue']]},
 'delete-chart':{icon:'−',title:'Move difficulty to Trash?',message:'This difficulty will be moved to Trash.',buttons:[['Cancel','cancel','ghost'],['Move to Trash','delete-chart-confirm','danger-button']]},
 'delete-set':{icon:'−',title:'Move entire Song Set to Trash?',message:'Song, audio and all difficulties will move to Trash.',buttons:[['Cancel','cancel','ghost'],['Move Song Set','delete-set-confirm','danger-button']]},
 leave:{icon:'←',title:'Leave this level?',message:'The current attempt will be lost.',buttons:[['Keep playing','cancel','ghost'],['Leave level','leave-confirm','danger-button']]},
 retry:{icon:'↺',title:'Restart chart?',message:'The current attempt will be lost.',buttons:[['Cancel','cancel','ghost'],['Restart demo','retry-confirm','button primary']]},
 import:{icon:'+',title:'Import to Song Library',message:'Chart or .locuspack package.',buttons:[['Cancel','cancel','ghost'],['Preview import','import-confirm','button blue']]},
 export:{icon:'↗',title:'Export Song Set',message:'Export song, audio and difficulties.',buttons:[['Cancel','cancel','ghost'],['Preview export','export-confirm','button blue']]},
 saveas:{icon:'▣',title:'Save as…',message:'New difficulty name.',input:'Horizon Copy',buttons:[['Cancel','cancel','ghost'],['Save demo copy','saveas-confirm','button blue']]},
 error:{icon:'!',title:'Cannot open chart',message:'Audio file is missing or inaccessible.',details:'Example: audioPath is missing, unreadable or outside the Song Set folder.',buttons:[['Close','cancel','button']]},
 success:{icon:'✓',title:'Operation complete',message:'Completed.',buttons:[['Close','cancel','button blue']]},
 compare:{icon:'◫',title:'Compare charts',message:'Choose up to two difficulties.',buttons:[['Close','cancel','button blue']]},
 commands:{icon:'⌕',title:'Quick navigation',message:'Go to',buttons:[['Close','cancel','ghost']]},
 'skin-saveas':{icon:'◇',title:'Save skin profile as…',message:'Profile name.',input:'My skin',buttons:[['Cancel','cancel','ghost'],['Create profile','skin-saveas-confirm','button blue']]},
 'skin-delete':{icon:'−',title:'Delete skin profile?',message:'Move this profile to Trash?',buttons:[['Cancel','cancel','ghost'],['Delete demo profile','skin-delete-confirm','danger-button']]},
 'remove-bg':{icon:'−',title:'Remove background?',message:'Remove Song Set background?',buttons:[['Cancel','cancel','ghost'],['Remove preview BG','remove-bg-confirm','danger-button']]},
 'choose-audio':{icon:'♫',title:'Choose music',message:'Select an audio file.',buttons:[['Cancel','cancel','ghost'],['Use sample title','audio-confirm','button blue']]}
};
let restoreFocus=null;
let renderedPage=null;
let savedSkin=JSON.stringify(s.skin);
function toast(message,warning){
 const box=document.getElementById('toasts');
 const el=document.createElement('div');
 el.className='toast'+(warning?' warning':'');
 el.textContent=message;box.appendChild(el);
 window.setTimeout(function(){el.remove();},3900);
}
function loadHash(){
 const bits=location.hash.replace(/^#/,'').split('/');
 const page=bits[0];
 if(S[page] && page!=='results')s.page=page;
 else if(page==='results'&&L.selectedRun())s.page='results';
 else if(page==='results'||page==='listen')s.page='library';
 if(page==='editor'&&['set','route','audio','field'].includes(bits[1]))s.editorTab=bits[1];
 if(page==='skins'&&['path','player','colors','profiles'].includes(bits[1]))s.skinTab=bits[1];
 if(page==='settings'&&['game','display','bindings','audio'].includes(bits[1]))s.settingsTab=bits[1];
}
function setHash(){
 const suffix=s.page==='editor'?'/'+s.editorTab:s.page==='settings'?'/'+s.settingsTab:s.page==='skins'?'/'+s.skinTab:'';
 const hash='#'+s.page+suffix;
 if(location.hash!==hash)history.replaceState(null,'',hash);
}
function render(){
 const fullScreen=s.page==='home'||s.page==='play';
 topbar.hidden=fullScreen;navrail.hidden=fullScreen;
 document.getElementById('pageTitle').textContent=titles[s.page]||'LOCUS';
 document.querySelectorAll('[data-go]').forEach(function(button){
  button.classList.toggle('active',button.dataset.go===s.page);
  if(button.dataset.go===s.page)button.setAttribute('aria-current','page');
  else button.removeAttribute('aria-current');
 });
 const scrollY=renderedPage===s.page?screen.scrollTop:0;
 screen.innerHTML=S[s.page]?S[s.page]():S.home();
 screen.scrollTop=scrollY;renderedPage=s.page;
 setHash();renderModal();
}
function navigate(page,sub,allowUnsaved){
 if(!S[page] || (page==='results'&&!L.selectedRun()))return;
 if(!allowUnsaved&&s.page==='editor'&&page!=='editor'&&s.editorDirty){s.modalReturn={page:page,sub:sub};openModal('unsaved');return;}
 s.lastPage=s.page;s.page=page;
 if(page==='editor'&&sub)s.editorTab=sub;
 if(page==='settings'&&sub)s.settingsTab=sub;
 if(page==='skins'&&sub)s.skinTab=sub;
 if(page==='editor'){
  const chart=L.selectedChart();if(chart&&!s.editorDirty)s.chartName=chart.title;
 }
 s.modal=null;s.capture=null;
 render();
}
function openModal(kind){
 if(!modalTypes[kind])return;
 restoreFocus=document.activeElement;
 s.modal=kind;renderModal();
 const preferred=dialog.querySelector('input')||dialog.querySelector('button');
 if(preferred)preferred.focus({preventScroll:true});
}
function closeModal(){
 s.modal=null;renderModal();
 if(restoreFocus&&restoreFocus.isConnected)restoreFocus.focus({preventScroll:true});
 restoreFocus=null;
}
function renderModal(){
 if(!s.modal){overlay.hidden=true;dialog.innerHTML='';return;}
 overlay.hidden=false;
 const d=modalTypes[s.modal],kind=s.modal;
 const buttons=(d.buttons||[]).map(function(x){
  return '<button type="button" class="'+x[2]+'" data-modal-action="'+x[1]+'">'+H(x[0])+'</button>';
 }).join('');
 let extra='';
 if(d.input)extra='<label class="input-label" style="margin-bottom:12px">Name<input type="text" id="modalInput" value="'+H(d.input)+'" maxlength="120"></label>';
 if(kind==='compare'){
  const chart=L.selectedChart();
  const options=L.selectedSong().charts.filter(function(c){return !chart||c.id!==chart.id;});
  extra='<div class="list" style="max-height:180px;overflow:auto">'+options.map(function(c){
   return '<button type="button" class="list-row '+(s.compare.includes(c.id)?'selected':'')+'" data-compare="'+H(c.id)+'"><span class="row-main"><strong>'+H(c.title)+'</strong><span class="row-sub" style="display:block">DIFF '+c.rating.toFixed(1)+'</span></span><span>'+ (s.compare.includes(c.id)?'✓':'+')+'</span></button>';
  }).join('')+'</div><div class="caption" style="margin-top:10px">'+s.compare.length+' of 2 reference charts selected</div>';
 }
 if(kind==='commands'){
  extra='<div class="list">'+navigationPages.map(function(key){
   return '<button class="list-row" type="button" data-modal-go="'+key+'"><span class="row-main"><span class="row-title">'+titles[key]+'</span></span><span>↗</span></button>';
  }).join('')+'</div>';
 }
 dialog.innerHTML='<div class="dialog-icon">'+H(d.icon||'•')+'</div>'+
  '<h2 id="dialogTitle">'+H(d.title)+'</h2><p>'+H(d.message)+'</p>'+
  (d.details?'<div class="details">'+H(d.details)+'</div>':'')+extra+
  '<div class="dialog-actions">'+buttons+'</div>';
}
function remember(){
 s.undo.push(JSON.stringify(s.notes));if(s.undo.length>30)s.undo.shift();s.redo=[];
 s.editorDirty=true;
}
function markDirty(){s.editorDirty=true;}
function updateNote(direction){
 remember();
 s.notes=s.notes.filter(function(n){return n.beat!==s.selectedBeat;});
 if(direction!=='DEL')s.notes.push({beat:s.selectedBeat,direction:direction});
 s.notes.sort(function(a,b){return a.beat-b.beat;});
 s.selectedDirection=direction==='DEL'?s.selectedDirection:direction;
 render();
}
function transformNote(mode){
 const note=s.notes.find(function(n){return n.beat===s.selectedBeat;});
 if(!note){toast('Select a beat with a note first.',true);return;}
 const dirs=['↑','↗','→','↘','↓','↙','←','↖'];
 let idx=dirs.indexOf(note.direction);
 if(idx<0)return;
 remember();
 if(mode==='Rot +90°')idx=(idx+2)%8;
 if(mode==='Rot −90°')idx=(idx+6)%8;
 if(mode==='Flip H')idx=[0,7,6,5,4,3,2,1][idx];
 if(mode==='Flip V')idx=[4,3,2,1,0,7,6,5][idx];
 note.direction=dirs[idx];s.selectedDirection=note.direction;
 render();
}
function performEdit(mode){
 if(mode==='copy'||mode==='cut'){
  const note=s.notes.find(function(n){return n.beat===s.selectedBeat;});
  if(!note){toast('No note on this beat.',true);return;}
  s.clipboard={direction:note.direction};if(mode==='cut')updateNote('DEL');else render();
  toast(mode==='cut'?'Note cut.':'Note copied.');return;
 }
 if(mode==='paste'){
  if(!s.clipboard){toast('Clipboard is empty.',true);return;}
  updateNote(s.clipboard.direction);return;
 }
 if(mode==='undo'||mode==='redo'){
  const from=mode==='undo'?s.undo:s.redo,to=mode==='undo'?s.redo:s.undo;
  if(!from.length){toast('Nothing to '+mode+'.');return;}
  to.push(JSON.stringify(s.notes));s.notes=JSON.parse(from.pop());markDirty();render();return;
 }
 transformNote(mode);
}
function finishAttempt(){
 const chart=L.selectedChart();
 if(!chart||chart.notes<=0){toast('This chart has no notes.',true);return;}
 const count=chart.notes,sequence=++s.runSequence;
 const miss=Math.floor(count*(.002+(sequence%4)*.004));
 const bad=Math.floor(count*(.003+(sequence%3)*.004));
 const good=Math.floor(count*(.06+(sequence%3)*.015));
 const perfect=count-miss-bad-good;
 const accuracy=Number(((perfect+.75*good+.5*bad)/count*100).toFixed(2));
 const timestamp=new Date().toLocaleString(undefined,{
   year:'numeric',month:'short',day:'numeric',hour:'2-digit',minute:'2-digit'
 });
 const run={id:'run-'+sequence,time:timestamp,total:count,perfect:perfect,
   good:good,bad:bad,miss:miss,accuracy:accuracy};
 const key=L.chartKey();
 if(!s.resultsByChart[key])s.resultsByChart[key]=[];
 s.resultsByChart[key].unshift(run);
 s.selectedRunId=run.id;
 s.playPaused=false;
 navigate('results');
}
function sampleSaved(){
 s.editorDirty=false;toast('Saved in prototype only.');render();
}
function handleModalAction(action){
 const kind=s.modal;
 if(action==='cancel'){closeModal();return;}
 if(action==='exit-confirm'){closeModal();navigate('home');toast('Preview mode');return;}
 if(action==='save-changes'||action==='discard-changes'){
  if(action==='save-changes')s.editorDirty=false;
  if(action==='discard-changes')s.editorDirty=false;
  const pendingChart=s.pendingChartSelection;s.pendingChartSelection=null;
  const destination=s.modalReturn;s.modalReturn=null;closeModal();
  if(pendingChart){
    s.chart=pendingChart;
    s.chartName=(L.selectedChart()||{}).title||'Draft';
    s.editorTab='route';
    render();return;
  }
  if(destination)navigate(destination.page,destination.sub);
  else render();return;
 }
 if(action==='delete-chart-confirm'){
  const song=L.selectedSong();
  if(song.id==='demo'){closeModal();toast('Built-in demo cannot be removed.',true);return;}
  const idx=song.charts.findIndex(x=>x.id===s.chart);
  if(idx>=0){s.trashedChart=song.charts.splice(idx,1)[0];s.chart=song.charts[0]?song.charts[0].id:'';}
  closeModal();render();toast('Difficulty moved to demonstration Trash.');return;
 }
 if(action==='delete-set-confirm'){
  const idx=L.library.findIndex(x=>x.id===s.song);
  if(idx>=0&&s.song!=='demo'){
   s.trashedSet=L.library.splice(idx,1)[0];s.song='demo';s.chart='original';
  }
  closeModal();navigate('library');toast('Song Set moved to demonstration Trash.');return;
 }
 if(action==='leave-confirm'){s.playPaused=false;closeModal();navigate('library');return;}
 if(action==='retry-confirm'){s.playPaused=false;closeModal();render();toast('Static PLAY preview reset.');return;}
 if(action==='import-confirm'){closeModal();toast('Import preview: no real files were installed.');return;}
 if(action==='export-confirm'){closeModal();toast('Export preview: no package was created.');return;}
 if(action==='saveas-confirm'){
  const value=(dialog.querySelector('#modalInput')||{}).value||'Untitled';
  s.chartName=value.trim().slice(0,120)||'Untitled';s.editorDirty=false;closeModal();render();toast('Demo chart renamed. No file created.');return;
 }
 if(action==='skin-saveas-confirm'){
  const value=(dialog.querySelector('#modalInput')||{}).value||'My skin';
  const id='profile-'+Date.now();
  s.profiles.push({id:id,name:value.trim().slice(0,100)||'My skin',subtitle:'Local preview profile'});
  s.skinProfile=id;s.skinName=value.trim()||'My skin';s.skinDirty=false;savedSkin=JSON.stringify(s.skin);
  closeModal();render();toast('Demo skin profile created.');return;
 }
 if(action==='skin-delete-confirm'){
  if(s.skinProfile==='default'){closeModal();toast('Default profile is protected.',true);return;}
  s.profiles=s.profiles.filter(x=>x.id!==s.skinProfile);
  s.skinProfile='default';s.skinName='Default';closeModal();render();toast('Demo profile removed.');return;
 }
 if(action==='remove-bg-confirm'){s.changedSet=true;closeModal();toast('Background removed from sample state.');render();return;}
 if(action==='audio-confirm'){closeModal();toast('Sample audio title selected. No file access.');return;}
 closeModal();
}
function action(name,el){
 switch(name){
 case 'reset':
  Object.assign(s,L.defaults());savedSkin=JSON.stringify(s.skin);
  L.library.splice(0,L.library.length,...JSON.parse(startingLibrary));
  Object.keys(L.bindings).forEach(k=>{L.bindings[k]=JSON.parse(startingBindings)[k];});
  navigate('home');toast('Demo reset.');return;
 case 'back':if(s.page==='home')openModal('quit');else navigate(s.page==='results'?'library':'home');return;
 case 'quit':openModal('quit');return;
 case 'open-commands':openModal('commands');return;
 case 'library-refresh':toast('Rescan preview complete. Sample library unchanged.');return;
 case 'library-empty':s.libraryEmpty=true;render();return;
 case 'library-reset':s.libraryEmpty=false;s.search='';render();return;
 case 'library-import':openModal('import');return;
 case 'library-export':openModal('export');return;
 case 'library-more':navigate('editor','set');return;
 case 'library-manage':navigate('editor','set');return;
 case 'library-play':s.playPaused=false;navigate('play');return;
 case 'library-edit':navigate('editor','route');return;

 case 'settings-reset':s.volume=70;s.vsync=true;s.fps='240';s.showFps=false;s.fullscreen=false;s.offset=0;s.hitFlash=true;render();toast('Demo settings reset.');return;
 case 'settings-bindings':navigate('settings','bindings');return;
 case 'settings-skins':navigate('skins','path');return;
 case 'calibration-open':s.settingsTab='audio';s.calibrationOpen=true;s.calibrationCount=0;s.calibrationComplete=false;render();toast('Calibrator is a visual state demo; no metronome audio.');return;
 case 'calibration-tap':s.calibrationCount=Math.min(18,s.calibrationCount+1);s.calibrationComplete=s.calibrationCount===18;render();return;
 case 'calibration-retry':s.calibrationCount=0;s.calibrationComplete=false;render();return;
 case 'calibration-apply':s.offset=24;s.calibrationOpen=false;s.calibrationCount=0;s.calibrationComplete=false;render();toast('Illustrative +24 ms sample applied to demo settings.');return;
 case 'skin-folder':toast('Native folder picker is not available in the HTML prototype.');return;
 case 'skin-save':savedSkin=JSON.stringify(s.skin);s.skinDirty=false;render();toast('Skin preview state saved (in memory).');return;
 case 'skin-saveas':openModal('skin-saveas');return;
 case 'skin-revert':s.skin=JSON.parse(savedSkin);s.skinDirty=false;render();toast('Unsaved skin changes reverted to last sample save.');return;
 case 'skin-activate':s.skinDirty=false;render();toast('Sample skin activated.');return;
 case 'skin-delete':openModal('skin-delete');return;
 case 'set-open-folder':toast('Open-folder action uses native Windows explorer in the game.');return;
 case 'set-background':s.changedSet=true;toast('Background picker preview: no file imported.');return;
 case 'set-remove-bg':openModal('remove-bg');return;
 case 'set-import':openModal('import');return;
 case 'set-export':openModal('export');return;
 case 'set-restore':
  if(s.trashedChart){L.selectedSong().charts.push(s.trashedChart);s.chart=s.trashedChart.id;s.trashedChart=null;toast('Demo difficulty restored.');render();}
  else if(s.trashedSet){L.library.push(s.trashedSet);s.song=s.trashedSet.id;s.chart=s.trashedSet.charts[0].id;s.trashedSet=null;toast('Demo Song Set restored.');render();}
  else toast('Nothing in demonstration Trash.');return;
 case 'set-delete-chart':openModal('delete-chart');return;
 case 'set-delete-set':openModal('delete-set');return;
 case 'set-new':{
  const song=L.selectedSong(),index=song.charts.length+1,id='new-'+Date.now();
  song.charts.push({id:id,title:'New difficulty '+index,rating:0,speed:0,rhythm:0,keys:0,reading:0,stamina:0,peak:0,notes:0});
  s.chart=id;s.chartName='New difficulty '+index;s.editorDirty=true;render();toast('New sample difficulty drafted.');return;
 }
 case 'set-duplicate':{
  const src=L.selectedChart(),song=L.selectedSong();if(!src)return;
  const copy=Object.assign({},src,{id:'copy-'+Date.now(),title:src.title+' Copy'});
  song.charts.push(copy);s.chart=copy.id;s.chartName=copy.title;s.editorDirty=true;render();toast('Sample difficulty duplicated.');return;
 }
 case 'tempo-add':if(s.tempo.length>=6){toast('Demo view shows up to six BPM markers.',true);return;}s.tempo.push({time:s.tempo[s.tempo.length-1].time+15000,bpm:174});markDirty();render();return;
 case 'editor-save':sampleSaved();return;
 case 'editor-saveas':openModal('saveas');return;
 case 'editor-exit':navigate('home');return;
 case 'editor-test':s.playPaused=false;navigate('play',null,true);return;
 case 'preview-toggle':s.previewPlaying=!s.previewPlaying;render();toast('Preview toggled.');return;
 case 'preview-restart':s.previewPlaying=false;s.selectedBeat=0;render();return;
 case 'zoom-in':s.zoom=Math.min(300,s.zoom+25);render();return;
 case 'zoom-out':s.zoom=Math.max(50,s.zoom-25);render();return;
 case 'snap-more':s.snap=s.snap==='1/4'?'1/8':s.snap==='1/8'?'1/16':'1/32';render();return;
 case 'snap-less':s.snap=s.snap==='1/32'?'1/16':s.snap==='1/16'?'1/8':'1/4';render();return;
 case 'beat-prev':s.selectedBeat=Math.max(0,s.selectedBeat-1);render();return;
 case 'beat-next':s.selectedBeat=Math.min(12,s.selectedBeat+1);render();return;
 case 'compare-open':openModal('compare');return;
 case 'play-pause':s.playPaused=!s.playPaused;render();return;
 case 'play-retry':openModal('retry');return;
 case 'play-leave':openModal('leave');return;
 case 'play-complete':finishAttempt();return;
 case 'results-library':navigate('library');return;
 case 'results-retry':s.playPaused=false;navigate('play');return;
 case 'results-edit':navigate('editor','route');return;
 case 'system-home':navigate('home');return;
 case 'sample-toast':toast('Done');return;
 case 'sample-error':openModal('error');return;
 }
 toast('This control is represented in the design, but its native action requires LOCUS.',true);
}
function handleClick(event){
 if(event.target===overlay){closeModal();return;}
 const modalAction=event.target.closest('[data-modal-action]');
 if(modalAction){handleModalAction(modalAction.dataset.modalAction);return;}
 const modalGo=event.target.closest('[data-modal-go]');
 if(modalGo){closeModal();navigate(modalGo.dataset.modalGo);return;}
 const compare=event.target.closest('[data-compare]');
 if(compare){
  const id=compare.dataset.compare;
  if(s.compare.includes(id))s.compare=s.compare.filter(x=>x!==id);
  else if(s.compare.length>=2){toast('Maximum of two additional panes.',true);return;}
  else s.compare.push(id);
  renderModal();return;
 }
 if(s.modal)return;
 const demoDialog=event.target.closest('[data-dialog-demo]');
 if(demoDialog){openModal(demoDialog.dataset.dialogDemo);return;}
 const go=event.target.closest('[data-go]');
 if(go){navigate(go.dataset.go);return;}
 const tab=event.target.closest('[data-editor-tab],[data-settings-tab],[data-skin-tab],[data-bind-group]');
 if(tab){
  if(tab.hasAttribute('data-editor-tab'))s.editorTab=tab.dataset.editorTab;
  if(tab.hasAttribute('data-settings-tab'))s.settingsTab=tab.dataset.settingsTab;
  if(tab.hasAttribute('data-skin-tab'))s.skinTab=tab.dataset.skinTab;
  if(tab.hasAttribute('data-bind-group')){s.bindingGroup=tab.dataset.bindGroup;s.bindingSearch='';s.capture=null;}
  render();return;
 }
 const song=event.target.closest('[data-song]:not([data-song-id])');
 if(song){
  const id=song.dataset.song;
  if(s.song===id){s.expanded=s.expanded.includes(id)?s.expanded.filter(x=>x!==id):s.expanded.concat(id);}
  else{s.song=id;s.chart=(L.selectedSong().charts[0]||{}).id||'';s.expanded=[id];}
  render();return;
 }
 const chart=event.target.closest('[data-chart][data-song-id]');
 if(chart){s.song=chart.dataset.songId;s.chart=chart.dataset.chart;render();return;}
 const runRow=event.target.closest('[data-run-id]');
 if(runRow){
   const run=L.runsForChart().find(function(x){return x.id===runRow.dataset.runId;});
   if(run){s.selectedRunId=run.id;navigate('results');}
   return;
 }
 const selected=event.target.closest('[data-chart-id]');
 if(selected){
  if(s.editorDirty){s.pendingChartSelection=selected.dataset.chartId;openModal('unsaved');return;}
  s.chart=selected.dataset.chartId;s.chartName=(L.selectedChart()||{}).title||'Draft';
  s.editorTab='route';s.editorDirty=false;render();return;
 }
 const profile=event.target.closest('[data-profile]');
 if(profile){s.skinProfile=profile.dataset.profile;s.skinName=(s.profiles.find(x=>x.id===s.skinProfile)||{}).name||'Default';s.skinDirty=false;render();return;}
 const color=event.target.closest('[data-skin-color]');
 if(color){s.selectedColor=color.dataset.skinColor;render();return;}
 const note=event.target.closest('[data-note]');
 if(note){updateNote(note.dataset.note);return;}
 const edit=event.target.closest('[data-edit-action]');
 if(edit){performEdit(edit.dataset.editAction);return;}
 const cell=event.target.closest('[data-field-cell]');
 if(cell){const coords=cell.dataset.fieldCell.split(',').map(Number);s.startX=coords[0];s.startY=coords[1];markDirty();render();return;}
 const speed=event.target.closest('[data-speed]');
 if(speed){s.practice=Number(speed.dataset.speed);render();return;}
 const pane=event.target.closest('[data-pane]');
 if(pane){s.focusedPane=Number(pane.dataset.pane);render();return;}
 const keybind=event.target.closest('[data-bind]');
 if(keybind){s.capture=keybind.dataset.bind;render();const active=screen.querySelector('.key-bind.listening');if(active)active.focus();return;}
 const toggle=event.target.closest('[data-toggle]');
 if(toggle){const key=toggle.dataset.toggle;s[key]=!s[key];render();return;}
 const skinToggle=event.target.closest('[data-skin-toggle]');
 if(skinToggle){const key=skinToggle.dataset.skinToggle;s.skin[key]=!s.skin[key];s.skinDirty=true;render();return;}
 const tempoDelete=event.target.closest('[data-tempo-delete]');
 if(tempoDelete){const index=Number(tempoDelete.dataset.tempoDelete);if(index>0){s.tempo.splice(index,1);markDirty();render();}return;}
 const seek=event.target.closest('[data-seek]');
 if(seek){
  const rect=seek.getBoundingClientRect(),x=(event.clientX-rect.left)/rect.width;
  s.selectedBeat=Math.round(L.pointMax(x,0,1)*12);render();return;
 }
 const a=event.target.closest('[data-action]');
 if(a){action(a.dataset.action,a);return;}
}
function handleChange(event){
 const node=event.target;
 if(node.hasAttribute('data-tempo-time')||node.hasAttribute('data-tempo-bpm')){
  const field=node.hasAttribute('data-tempo-time')?'time':'bpm';
  const index=Number(node.dataset.tempoTime||node.dataset.tempoBpm);
  const num=Number(node.value);
  if(Number.isFinite(num)&&s.tempo[index]){
   if(field==='bpm')s.tempo[index].bpm=L.pointMax(num,1,10000);
   else s.tempo[index].time=Math.round(num);
   markDirty();render();
  }return;
 }
 if(node.hasAttribute('data-range')){
  const field=node.dataset.range;
  s[field]=Number(node.value);
  render();return;
 }
 if(node.hasAttribute('data-skin-range')){
  const key=node.dataset.skinRange;s.skin[key]=Number(node.value);s.skinDirty=true;render();return;
 }
 if(node.hasAttribute('data-color-picker')){
  const key=s.selectedColor||'path';s.skin.colors[key]=node.value;s.skinDirty=true;render();return;
 }
 if(node.hasAttribute('data-select')){
  const key=node.dataset.select;
  if(key==='fps')s.fps=node.value;
  else if(key==='selectedDirection')s.selectedDirection=node.value;
  else if(key==='heading'){s.heading=node.value;markDirty();}
  render();return;
 }
 if(node.hasAttribute('data-field')){
  const key=node.dataset.field,value=node.value;
  if(key==='library-search'||key==='binding-search')return;
  if(key==='offset')s.offset=L.pointMax(Math.round(Number(value)||0),-1000,1000);
  else if(key==='skin-hex'){
   const color=value.trim();
   if(!/^#[\da-f]{6}$/i.test(color)){toast('Use a six-digit hexadecimal color.',true);return;}
   s.skin.colors[s.selectedColor||'path']=color;s.skinDirty=true;
  } else if(['fieldWidth','fieldHeight'].includes(key)){
   s[key]=L.pointMax(Math.round(Number(value)||4),2,8);
   s.startX=Math.min(s.startX,s.fieldWidth-1);s.startY=Math.min(s.startY,s.fieldHeight-1);markDirty();
  } else if(['startX','startY'].includes(key)){
   s[key]=L.pointMax(Math.round(Number(value)||0),0,key==='startX'?s.fieldWidth-1:s.fieldHeight-1);markDirty();
  } else if(['previewSpeed','previewLength'].includes(key)){
   s[key]=L.pointMax(Number(value)||0,key==='previewSpeed'?20:50,key==='previewSpeed'?2000:10000);
  } else if(key==='previewMs'){s.previewMs=Math.max(0,Number(value)||0);markDirty();}
  else if(key==='chartName'){s.chartName=value.slice(0,120);markDirty();}
  render();return;
 }
}
function handleInput(event){
 const node=event.target;
 if(node.dataset.field==='library-search'||node.dataset.field==='binding-search'){
  const key=node.dataset.field==='library-search'?'search':'bindingSearch';
  const caret=node.selectionStart;s[key]=node.value;render();
  const fresh=screen.querySelector('[data-field="'+node.dataset.field+'"]');
  if(fresh){fresh.focus({preventScroll:true});fresh.setSelectionRange(caret,caret);}
  return;
 }
 if(node.dataset.range==='volume'){
  document.getElementById('volumeLabel').textContent=node.value+'%';
 }
}
function handleKey(event){
 if(s.capture){
  if(event.key==='Escape'){s.capture=null;render();return;}
  if(['Shift','Control','Alt','Meta'].includes(event.key))return;
  event.preventDefault();
  const group=L.bindings[s.bindingGroup],row=group.find(x=>x[0]===s.capture);
  if(row)row[1]=(event.ctrlKey?'Ctrl+':'')+(event.shiftKey?'Shift+':'')+(event.altKey?'Alt+':'')+(event.key.length===1?event.key.toUpperCase():event.key);
  s.capture=null;render();toast('Demonstration binding updated.');return;
 }
 if(event.key==='Escape'){
  if(s.modal){event.preventDefault();closeModal();return;}
  if(s.page==='results'){event.preventDefault();navigate('library');return;}
  if(s.page==='home'){openModal('quit');return;}
  if(s.page==='play'){s.playPaused=!s.playPaused;render();return;}
  if(!event.target.closest('input,textarea,select')){navigate('home');return;}
  event.target.blur();return;
 }
 if(event.ctrlKey&&event.key.toLowerCase()==='k'){event.preventDefault();openModal('commands');return;}
 if(event.ctrlKey&&event.key.toLowerCase()==='s'&&s.page==='editor'&&!s.modal){
  event.preventDefault();if(event.shiftKey)openModal('saveas');else sampleSaved();return;
 }
 if(event.target.closest('input,textarea,select'))return;
 if(event.key==='Enter'||event.key===' '){
  const pane=document.activeElement.closest('[data-pane]');
  if(pane){event.preventDefault();s.focusedPane=Number(pane.dataset.pane);render();}
 }
}
document.addEventListener('click',handleClick);
document.addEventListener('change',handleChange);
document.addEventListener('input',handleInput);
document.addEventListener('keydown',handleKey);
window.addEventListener('hashchange',function(){if(!s.modal){loadHash();render();}});
loadHash();render();
})();
