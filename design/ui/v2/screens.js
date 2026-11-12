/* LOCUS UI v2 — screen composition. Semantically representative UI, not game code. */
(function(){
'use strict';
const L=window.LocusDemo, s=L.state, H=L.H;
function pill(text,type){return '<span class="tag '+(type||'')+'">'+H(text)+'</span>';}
function head(title,subtitle,actions){
 return '<div class="page-heading"><div><div class="eyebrow">LOCUS / '+H(title)+'</div><h1>'+H(title)+'</h1>'+(subtitle?'<p>'+subtitle+'</p>':'')+'</div><div class="page-actions">'+(actions||'')+'</div></div>';
}
function panel(title,body,actions,cls){
 return '<section class="panel '+(cls||'')+'"><div class="panel-head"><h2>'+title+'</h2>'+(actions||'')+'</div>'+body+'</section>';
}
function tabset(items,current,attr){
 return '<div class="tabs" role="tablist">'+items.map(function(item){
  return '<button class="tab '+(current===item[0]?'active':'')+'" type="button" role="tab" aria-selected="'+(current===item[0])+'" '+attr+'="'+item[0]+'">'+H(item[1])+'</button>';
 }).join('')+'</div>';
}
function button(text,action,cls,extra){
 return '<button type="button" class="'+(cls||'button')+'" data-action="'+H(action)+'" '+(extra||'')+'>'+H(text)+'</button>';
}
function input(name,value,key,type,hint){
 return '<label class="input-label">'+H(name)+'<input type="'+(type||'text')+'" data-field="'+H(key)+'" value="'+H(value)+'"'+(hint?' placeholder="'+H(hint)+'"':'')+'></label>';
}
function selector(name,value,key,items){
 return '<label class="input-label">'+H(name)+'<select data-select="'+H(key)+'">'+items.map(function(x){
  const key=typeof x==='string'?x:x[0],txt=typeof x==='string'?x:x[1];
  return '<option value="'+H(key)+'" '+(String(key)===String(value)?'selected':'')+'>'+H(txt)+'</option>';
 }).join('')+'</select></label>';
}
function checkbox(name,detail,key){
 return '<div class="check-row"><div><strong>'+H(name)+'</strong>'+(detail?'<p>'+H(detail)+'</p>':'')+'</div><button type="button" class="switch" role="switch" aria-label="'+H(name)+'" aria-checked="'+s[key]+'" data-toggle="'+H(key)+'"></button></div>';
}
function stat(name,val,extra){
 return '<div class="stat-card"><div class="metric-label">'+H(name)+'</div><div class="value">'+H(val)+'</div>'+(extra?'<small>'+extra+'</small>':'')+'</div>';
}
function itemInfo(title,subtitle,active,action,extra){
 return '<button type="button" class="list-row '+(active?'selected':'')+'" data-action="'+H(action)+'" '+(extra||'')+'><span class="row-main"><span class="row-title">'+H(title)+'</span><span class="row-sub">'+H(subtitle)+'</span></span><span class="count">›</span></button>';
}
function home(){
 return '<div class="home">'+
  '<img class="home-wordmark" src="../v1/wordmark.svg" alt="locus.">'+
  '<div class="home-subtitle">Follow the rhythm</div>'+
  '<div class="home-actions">'+
  '<button class="home-action play" data-go="library">PLAY</button>'+
  '<button class="home-action editor" data-go="editor">EDITOR</button>'+
  '<button class="home-action skins" data-go="skins">SKINS</button>'+
  '<button class="home-action settings" data-go="settings">SETTINGS</button>'+
  '</div>'+
  '<button class="home-exit" data-action="quit">EXIT</button></div>';
}
function library(){
 const selectedSet=L.selectedSong(), selected=L.selectedChart();
 const sets=s.libraryEmpty?[]:L.library.filter(function(x){
  return (x.title+' '+x.artist+' '+x.charts.map(c=>c.title).join(' ')).toLowerCase().includes(s.search.toLowerCase());
 });
 const tree=sets.length?sets.map(function(song){
  const open=s.expanded.includes(song.id);
  return '<div class="tree-group"><button class="tree-parent '+(s.song===song.id?'selected':'')+'" data-song="'+H(song.id)+'"><span class="chevron">'+(open?'▾':'▸')+'</span><span class="grow">'+H(song.title)+'</span><span class="count">'+song.charts.length+'</span></button>'+
  (open?song.charts.map(function(c){return '<button class="tree-child '+(s.song===song.id&&s.chart===c.id?'selected':'')+'" data-chart="'+H(c.id)+'" data-song-id="'+H(song.id)+'"><span class="difficulty-dot"></span><span class="title">'+H(c.title)+'</span><span class="mono">'+c.rating.toFixed(1)+'</span></button>';}).join(''):'')+'</div>';
 }).join(''):
 '<div class="empty"><div class="empty-glyph">≡</div><h3>No songs found</h3><p>'+(s.libraryEmpty?'Your library has no imported Song Sets in this demo state.':'Try a different title or artist.')+'</p>'+button('Restore samples','library-reset','button slim')+'</div>';
 const actions=button('Import','library-import','ghost')+button('Rescan','library-rescan','ghost')+button('Empty state','library-empty','ghost');
 const left='<section class="panel library-list-panel"><div class="panel-head"><h2>Song library</h2><span class="tag">'+(sets.length)+' sets</span></div><label class="input-label">Search library<input type="search" placeholder="Song, artist, difficulty…" data-field="library-search" value="'+H(s.search)+'"></label><div class="library-list">'+tree+'</div><div class="library-stats"><span class="caption">Song Sets / difficulties</span><span class="mono">'+(sets.length)+' / '+sets.reduce((a,x)=>a+x.charts.length,0)+'</span></div></section>';
 if(!sets.length||!sets.some(function(x){return x.id===s.song;})){
  return '<div class="page">'+head('Song library','Browse songs and their difficulties. Select a chart to inspect or launch it.',actions)+
  '<div class="workspace-area library-layout">'+left+
  '<section class="panel"><div class="empty"><div class="empty-glyph">◌</div><h3>No chart selected</h3><p>Choose a Song Set in the library or restore the example entries.</p></div></section>'+
  '<section class="panel library-inspector"><h2>Chart analysis</h2><p class="caption">Difficulty metrics appear after a selection.</p></section></div></div>';
 }
 const chart=selected||{title:'None',rating:0,speed:0,rhythm:0,keys:0,reading:0,stamina:0,peak:0,notes:0};
 const center='<section class="panel selection-detail"><div class="artwork"><span class="art-center">◌</span><span class="art-caption">SONG ARTWORK · PLACEHOLDER</span></div><div class="row wrap" style="margin-top:15px">'+pill('Song Set','blue')+pill(selectedSet.bpm+' BPM')+pill(selectedSet.duration)+'</div>'+
  '<h2>'+H(selectedSet.title)+'</h2><div class="meta">'+H(selectedSet.artist)+' · '+H(chart.title)+'</div>'+
  '<div class="selection-actions">'+button('▶  Play chart','library-play','button primary')+button('Edit chart','library-edit','button')+button('Listen','library-listen','ghost')+'</div>'+
  '<div class="divider"></div><div class="row space-between"><div><div class="eyebrow">Selected difficulty</div><h3 style="margin:5px 0 0">'+H(chart.title)+'</h3></div><div style="text-align:right"><div class="rating">'+chart.rating.toFixed(1)+'</div><small>Difficulty v2</small></div></div>'+
  '<div class="stat-grid" style="margin-top:16px">'+stat('Notes',chart.notes)+stat('BPM',selectedSet.bpm)+stat('Offset',selectedSet.offset+' ms')+'</div>'+
  '<div class="panel-bottom"><div class="eyebrow" style="margin-bottom:9px">Manage song</div><div class="inline-actions">'+button('Open SET','library-manage','ghost slim')+button('Export package','library-export','ghost slim')+button('More ···','library-more','ghost slim')+'</div></div></section>';
 const dimensions=[['Speed','speed'],['Rhythm','rhythm'],['Coordination','keys'],['Reading','reading'],['Stamina','stamina'],['Peak','peak']];
 const right='<section class="panel library-inspector"><div class="panel-head"><h2>Chart analysis</h2>'+pill('v2','blue')+'</div><p style="font-size:12px;margin-bottom:18px">A compact breakdown. Main actions stay with the selected chart.</p><div class="rating-breakdown">'+dimensions.map(function(d){const value=chart[d[1]];return '<div class="rating-line"><span>'+d[0]+'</span><div class="progress"><span style="width:'+(value*10)+'%"></span></div><b>'+value.toFixed(1)+'</b></div>';}).join('')+'</div><div class="divider"></div><div class="eyebrow" style="margin-bottom:12px">Song details</div><dl class="small-dl"><dt>Artist</dt><dd>'+H(selectedSet.artist)+'</dd><dt>Duration</dt><dd>'+H(selectedSet.duration)+'</dd><dt>Song timing</dt><dd>'+selectedSet.bpm+' BPM</dd><dt>Shared offset</dt><dd>'+selectedSet.offset+' ms</dd><dt>Difficulties</dt><dd>'+selectedSet.charts.length+'</dd></dl><div class="panel-bottom"><div class="callout">The set owns the audio, artwork and tempo map. Individual difficulties own their notes.</div></div></section>';
 return '<div class="page">'+head('Song library','Browse songs and their difficulties. Select a chart to inspect or launch it.',actions)+
 '<div class="workspace-area library-layout">'+left+center+right+'</div></div>';
}
function settings(){
 const menu=[['game','Game'],['display','Display'],['bindings','Key bindings'],['audio','Audio & offset']];
 let content='';
 if(s.settingsTab==='game'){
  content=panel('Gameplay preferences',
   checkbox('Hit flash','Display the existing on-hit flash feedback.','hitFlash')+
   '<div class="divider"></div><p class="caption">Gameplay speed, preview distance and SKIN appearance are controlled in their own contexts. No new in-game UI animation is introduced.</p>')+
   panel('Quick access','<div class="row wrap">'+button('Edit gameplay keys','settings-bindings','button')+button('Open skin controls','settings-skins','ghost')+'</div>');
 } else if(s.settingsTab==='display'){
  content=panel('Display',
   checkbox('VSync','Synchronize display updates with the monitor.','vsync')+
   '<div class="setting-row"><div><div class="setting-name">Frame rate limit</div><div class="setting-desc">The limit does not affect music or judgment timing.</div></div><div class="setting-control">'+selector('',s.fps,'fps',['30','60','90','120','144','165','240',['0','Unlimited']])+'</div></div>'+
   checkbox('Show FPS','Show a small performance counter.','showFps')+
   checkbox('Borderless fullscreen','Use the desktop resolution.','fullscreen')+
   '<div class="callout" style="margin-top:14px">Display values here demonstrate settings controls; they are not applied to your machine.</div>');
 } else if(s.settingsTab==='audio'){
  content=panel('Audio',
   '<div class="setting-row"><div><div class="setting-name">Master volume</div><div class="setting-desc">Applies to music and built-in preview.</div></div><div class="setting-control"><input aria-label="Master volume" type="range" min="0" max="100" value="'+s.volume+'" data-range="volume"><span class="mono" id="volumeLabel">'+s.volume+'%</span></div></div>'+
   '<div class="setting-row"><div><div class="setting-name">Global output offset</div><div class="setting-desc">Positive = audio is late, negative = audio is early.</div></div><div class="setting-control"><input style="width:105px" aria-label="Global offset ms" type="number" min="-1000" max="1000" step="1" data-field="offset" value="'+s.offset+'"><span class="mono muted">ms</span></div></div>'+
   '<div class="row space-between" style="margin-top:15px"><div><strong>Metronome calibration</strong><div class="caption">100 BPM · use a bound direction key on each click</div></div>'+button('Open calibrator','calibration-open','button blue')+'</div>')+
   (s.calibrationOpen?calibrator(): '');
 } else {
  const groupNames=[['play','PLAY'],['menus','MENUS / LISTEN'],['editor','EDITOR']];
  const list=L.bindings[s.bindingGroup].filter(function(row){return row[0].toLowerCase().includes(s.bindingSearch.toLowerCase());});
  content=panel('Key bindings',
   '<div class="tab-row" style="margin-bottom:17px"><div class="segmented">'+groupNames.map(function(x){return L.segment(x[0],s.bindingGroup,x[1],'data-bind-group');}).join('')+'</div><input style="max-width:210px" type="search" placeholder="Find action…" value="'+H(s.bindingSearch)+'" data-field="binding-search"></div>'+
   '<div class="keymap-grid">'+list.map(function(x){
    return '<div>'+H(x[0])+'</div><div><button class="key-bind'+(s.capture===x[0]?' listening':'')+'" data-bind="'+H(x[0])+'" type="button">'+(s.capture===x[0]?'Press key…':H(x[1]))+'</button></div>';
   }).join('')+'</div>'+
   (!list.length?'<div class="empty"><h3>No bindings match</h3><p>Clear the filter to see all actions.</p></div>':'')+
   '<div class="caption" style="margin-top:17px">Select a binding, then press a key. Escape cancels rebinding. Directions place notes only in PLAY, not in the editor.</div>');
 }
 return '<div class="page">'+head('Settings','System behavior, key mapping and calibration.',button('Reset demo settings','settings-reset','ghost'))+
  '<div class="workspace-area settings-layout"><section class="panel"><div class="eyebrow" style="margin-bottom:12px">Preferences</div><nav class="side-list">'+
  menu.map(function(x){return '<button class="side-link '+(s.settingsTab===x[0]?'active':'')+'" data-settings-tab="'+x[0]+'">'+x[1]+'<span>›</span></button>';}).join('')+
  '</nav></section><div class="settings-content">'+content+'</div></div></div>';
}
function calibrator(){
 const count=s.calibrationCount;
 return panel('Calibration exercise',
  '<div class="calibration-pad"><div class="beat-circle '+(count?'hit':'')+'">'+(s.calibrationComplete?'+24 ms':count+'/ 18')+'</div>'+
  '<div class="cal-ticks">'+Array.from({length:18},function(_,i){return '<span class="'+(i<count?'active':'')+'"></span>';}).join('')+'</div>'+
  '<div class="caption">Visual-only demonstration. No metronome sound is generated here.</div>'+
  (s.calibrationComplete?'<p><strong>Suggested output offset: +24 ms</strong><br><small>Illustrative result, not a measurement.</small></p>':'<p>Tap on 12 imagined beats to inspect the finished state.</p>')+
  '<div class="row" style="justify-content:center">'+button('Tap','calibration-tap','button primary',s.calibrationComplete?'disabled':'')+button('Retry','calibration-retry','ghost')+
  (s.calibrationComplete?button('Apply sample','calibration-apply','button blue'):'')+'</div></div>');
}
function skinPreview(){
 return '<div class="skin-preview"><section class="panel"><div class="panel-head"><h2>Live preview</h2>'+pill('Static sample','blue')+'</div><div class="preview-window"><div class="route-demo">'+L.routeSvg(1)+'</div><div class="skin-hud">ACC 97.27%<br>COMBO 042</div></div><div class="row space-between" style="margin-top:14px"><div class="caption">Preview uses local sample geometry only.</div><span class="mono">'+s.skin.baseSpeed+' px/s</span></div></section>'+
 panel('Profile','<div class="eyebrow">Editing</div><h3 style="margin-top:6px">'+H(s.skinName)+(s.skinDirty?' <span class="rose-text">•</span>':'')+'</h3><p class="caption">Gameplay styling is independent of the application theme.</p>'+
 '<div class="row wrap">'+button('Save','skin-save','button blue')+button('Save as…','skin-saveas','ghost')+button('Revert','skin-revert','ghost')+'</div>')+'</div>';
}
function skin(){
 const tabs=[['path','PATH'],['player','PLAYER'],['colors','COLORS'],['profiles','MY SKINS']];
 let body='';
 if(s.skinTab==='path'){
  const vals=[['Preview time','lookahead',50,2000,50,'ms'],['Path width','pathWidth',1,12,.25,'px'],['Outline scale','outlineScale',1,3,.25,'×'],['Fade curve','fadePower',.5,2.5,.1,'×'],['Path opacity','pathOpacity',0,1,.05,''],['Turn size','junctionRadius',3,18,1,'px'],['Reverse cap','reverseCapScale',0,6,.5,'×']];
  body=panel('Path & field',vals.map(function(v){return '<div class="control-line"><span>'+v[0]+'</span><input type="range" data-skin-range="'+v[1]+'" min="'+v[2]+'" max="'+v[3]+'" step="'+v[4]+'" value="'+s.skin[v[1]]+'"><span class="number">'+s.skin[v[1]]+' '+v[5]+'</span></div>';}).join('')+
   '<div class="check-row"><div><strong>Turn glow</strong><p>Existing path effect.</p></div><button type="button" class="switch" role="switch" aria-checked="'+s.skin.junctionGlow+'" data-skin-toggle="junctionGlow"></button></div>');
 } else if(s.skinTab==='player'){
  const vals=[['Player size','playerRadius',6,22,1,'px'],['Base speed','baseSpeed',60,2000,10,'px/s'],['Hit flash size','hitFlashScale',.5,2,.1,'×'],['Camera lag','cameraLagMs',0,500,25,'ms']];
  body=panel('Player & feedback',
   vals.map(function(v){return '<div class="control-line"><span>'+v[0]+'</span><input type="range" data-skin-range="'+v[1]+'" min="'+v[2]+'" max="'+v[3]+'" step="'+v[4]+'" value="'+s.skin[v[1]]+'"><span class="number">'+s.skin[v[1]]+' '+v[5]+'</span></div>';}).join('')+
   '<div class="check-row"><div><strong>Player glow</strong><p>Existing visual effect.</p></div><button type="button" class="switch" role="switch" aria-checked="'+s.skin.playerGlow+'" data-skin-toggle="playerGlow"></button></div>'+
   '<div class="check-row"><div><strong>Show HUD</strong><p>Accuracy and judgment counts.</p></div><button type="button" class="switch" role="switch" aria-checked="'+s.skin.showHud+'" data-skin-toggle="showHud"></button></div>');
 } else if(s.skinTab==='colors'){
  const names=[['background','Background'],['trajectory','Trajectory'],['outline','Outline'],['junction','Junction'],['player','Player'],['playerCore','Player core'],['hudBackground','HUD background'],['hudText','HUD text'],['hitFlash','Hit flash'],['fieldBorder','Field border']];
  body=panel('Element colors','<p class="caption">Choose an element. Changes update the static preview, not gameplay files.</p>'+
    '<div class="color-matrix">'+names.map(function(v){return '<button type="button" class="color-row '+(s.selectedColor===v[0]?'active':'')+'" data-skin-color="'+v[0]+'"><span class="color-swatch" style="background:'+H(s.skin.colors[v[0]])+'"></span><span>'+v[1]+'</span></button>';}).join('')+'</div>'+
    '<div class="skin-color-editor"><input aria-label="Selected color" type="color" data-color-picker value="'+H(s.skin.colors[s.selectedColor||'trajectory'])+'"><div class="grow">'+
    input('Hex color',s.skin.colors[s.selectedColor||'trajectory'],'skin-hex')+'</div></div>');
 } else {
  body=panel('Saved profiles',
   '<div class="profiles-grid">'+s.profiles.map(function(p){return '<button class="profile-row '+(s.skinProfile===p.id?'selected':'')+'" data-profile="'+H(p.id)+'"><span class="profile-avatar"></span><span class="grow" style="text-align:left"><strong>'+H(p.name)+'</strong><span class="row-sub" style="display:block">'+H(p.subtitle)+'</span></span>'+(s.skinProfile===p.id?pill('Active','pink'):'')+'</button>';}).join('')+'</div><div class="row wrap" style="margin-top:16px">'+
   button('Activate','skin-activate','button blue')+button('Store','skin-save','button')+button('Duplicate','skin-saveas','ghost')+button('Delete','skin-delete','ghost danger')+'</div><div class="caption" style="margin-top:12px">Profile operations affect only the demonstration state.</div>');
 }
 return '<div class="page">'+head('Skins','Customize the playfield without changing the application UI.',button('Open profiles folder','skin-folder','ghost'))+
  '<div class="tab-row">'+tabset(tabs,s.skinTab,'data-skin-tab')+'<span class="tag '+(s.skinDirty?'pink':'green')+'">'+(s.skinDirty?'Unsaved changes':'Profile ready')+'</span></div>'+
  '<div class="workspace-area skin-layout"><div class="skin-controls">'+body+'</div>'+skinPreview()+'</div></div>';
}
function directionCount(direction){
 const diagonals={ '↖':['↑','←'], '↗':['↑','→'],'↙':['↓','←'],'↘':['↓','→'] };
 return s.notes.filter(function(n){return n.direction===direction ||
   (diagonals[n.direction]&&diagonals[n.direction].includes(direction));}).length;
}
function timeline(){
 const notes=s.notes.map(function(n){return '<span class="note '+('↖↗↙↘'.includes(n.direction)?'diag':'')+'" style="left:'+(n.beat/12*100)+'%" title="Beat '+n.beat+' · '+H(n.direction)+'"></span>';}).join('');
 return '<div class="editor-bottom"><div class="row space-between wrap"><div class="row"><span class="eyebrow">Timeline</span><span class="mono">BEAT '+s.selectedBeat+'.00</span><span class="tag">SNAP '+H(s.snap)+'</span></div><div class="range-row">'+
  button('−','zoom-out','ghost slim')+'<span class="mono">'+s.zoom+'%</span>'+button('+','zoom-in','ghost slim')+
  button('Snap −','snap-less','ghost slim')+button('Snap +','snap-more','ghost slim')+'</div></div>'+
  '<div class="timeline" data-seek="timeline" aria-label="Timeline selection"><div class="ticks">'+Array.from({length:13},(_,i)=>'<span>'+i+'</span>').join('')+'</div>'+notes+'<span class="cursor" style="left:'+(s.selectedBeat/12*100)+'%"></span></div>'+
  '<div class="overview" data-seek="overview" aria-label="Whole song position"><span class="fill" style="width:'+(s.selectedBeat/12*100)+'%"></span><span class="cursor" style="left:'+(s.selectedBeat/12*100)+'%"></span></div>'+
  '<div class="timeline-status"><div class="transport">'+
  button(s.previewPlaying?'Ⅱ Pause':'▶ Play','preview-toggle','transport-button '+(s.previewPlaying?'active':''))+
  button('↺','preview-restart','transport-button')+
  (s.editorTab==='route'?[100,75,50,25,10].map(function(rate){return '<button class="transport-button '+(s.practice===rate?'active':'')+'" data-speed="'+rate+'">'+rate+'%</button>';}).join(''):'')+
  '</div><div class="row"><span class="mono">00:01.438 / 03:12</span><span class="caption">Click ruler to seek</span></div></div></div>';
}
function routeTools(){
 return '<div class="editor-toolbar"><div class="tool-group">'+
 ['Copy','Cut','Paste','Undo','Redo'].map(function(x){return '<button class="tool-btn" data-edit-action="'+x.toLowerCase()+'">'+x+'</button>';}).join('')+
 '<span class="tool-divider"></span>'+
 ['Flip H','Flip V','Rot −90°','Rot +90°'].map(function(x){return '<button class="tool-btn" data-edit-action="'+H(x)+'">'+x+'</button>';}).join('')+
 '</div><div class="tool-group">'+button('Compare +','compare-open','ghost slim')+button('Test chart','editor-test','button primary slim')+'</div></div>';
}
function editorRoute(){
 const available=L.selectedSong().charts.filter(c=>c.id!==s.chart);
 const compares=s.compare.map(function(id){return available.find(c=>c.id===id);}).filter(Boolean);
 const panes=[L.selectedChart()].concat(compares);
 const left='<section class="panel compact"><div class="panel-head"><h3>Selection</h3>'+pill('Pattern','blue')+'</div>'+
  '<div class="eyebrow">Current beat</div><div class="metric" style="font-size:34px;margin:8px 0 18px">'+s.selectedBeat+'.00</div>'+
  '<div class="input-label">Selected direction<select data-select="selectedDirection">'+['↑','↗','→','↘','↓','↙','←','↖'].map(d=>'<option '+(s.selectedDirection===d?'selected':'')+'>'+d+'</option>').join('')+'</select></div>'+
  '<div class="divider"></div><div class="eyebrow">Editing tools</div><div class="row wrap" style="margin-top:10px">'+
  button('Previous beat','beat-prev','ghost slim')+button('Next beat','beat-next','ghost slim')+'</div>'+
  '<div class="divider"></div><div class="small-dl" style="display:grid;grid-template-columns:1fr 1fr;gap:8px;color:var(--sub);font-size:11px">'+
  '<span>Selected tick</span><b>'+s.selectedBeat+'/12</b><span>Notes in chart</span><b>'+s.notes.length+'</b><span>Clipboard</span><b>'+(s.clipboard?'1 note':'Empty')+'</b></div>'+
  '<div class="callout" style="margin-top:18px">Direction buttons place notes. Keyboard directions are reserved for PLAY.</div></section>';
 const previews='<section class="editor-stage"><div class="editor-stage-header"><div class="row"><strong>Route preview</strong>'+pill('Song-time aligned','blue')+'</div><span class="caption">'+panes.length+' / 3 panes</span></div><div class="editor-stage-body">'+
  panes.map(function(p,i){return '<div class="preview-pane '+(s.focusedPane===i?'selected':'')+'" role="button" tabindex="0" data-pane="'+i+'">'+
   '<div class="preview-label"><span class="pulse-dot"></span>'+H(p?p.title:'Draft')+'</div><span class="preview-rating">DIFF '+(p?p.rating.toFixed(1):'—')+'</span>'+L.routeSvg(i)+'</div>';}).join('')+'</div></section>';
 const directions=['↖','↑','↗','←','DEL','→','↙','↓','↘'];
 const pad='<div class="direction-grid">'+directions.map(function(d){return '<button type="button" class="direction '+(d==='DEL'?'delete':'')+' '+(s.selectedDirection===d?'active':'')+'" data-note="'+H(d)+'" title="'+(d==='DEL'?'Remove note':'Place '+d)+'">'+(d==='DEL'?'⌫':H(d))+'<span style="font-size:10px;margin-left:6px">'+(d==='DEL'?'':directionCount(d))+'</span></button>';}).join('')+'</div>';
 const right='<section class="panel compact editor-inspector"><div class="panel-head"><h3>Inspector</h3>'+pill('Route')+'</div>'+
  '<div class="inspector-group"><div class="eyebrow">Direction pad</div>'+pad+'<div class="caption" style="margin-top:8px">Diagonal notes also count toward both cardinal directions.</div></div>'+
  '<div class="inspector-group"><div class="eyebrow">Preview speed</div><div class="row" style="margin-top:8px">'+
  '<input type="number" min="20" max="2000" step="5" data-field="previewSpeed" value="'+s.previewSpeed+'"><span class="mono muted">px/s</span></div></div>'+
  '<div class="inspector-group"><div class="eyebrow">Visible path</div><div class="row" style="margin-top:8px"><input type="number" min="50" max="10000" step="50" data-field="previewLength" value="'+s.previewLength+'"><span class="mono muted">ms</span></div></div>'+
  '<div class="caption">Viewport controls affect the editor preview only.</div></section>';
 return '<div class="editor-main">'+left+previews+right+'</div>';
}
function editorSet(){
 const song=L.selectedSong();
 const left=panel('Song Set','<div class="eyebrow">Installed collection</div><h2 style="margin:8px 0 6px">'+H(song.title)+'</h2>'+
 '<div class="caption">'+H(song.artist)+' · '+H(song.duration)+' · '+song.bpm+' BPM</div>'+
 '<div class="divider"></div><div class="row wrap">'+pill(song.id==='demo'?'Built-in':'Managed Set','blue')+pill('Shared timing')+'</div>'+
 '<div class="divider"></div><div class="eyebrow">Files & artwork</div><div class="set-actions">'+
 button('Open folder','set-open-folder','ghost slim')+button('Add / replace BG','set-background','ghost slim')+
 button('Remove BG','set-remove-bg','ghost slim')+button('Import chart','set-import','ghost slim')+
 button('Export package','set-export','ghost slim')+button('Restore last','set-restore','ghost slim')+'</div>'+
 '<div class="divider"></div><div class="row wrap">'+button('Delete difficulty','set-delete-chart','ghost danger slim')+button('Delete Song Set','set-delete-set','ghost danger slim')+'</div>');
 const center='<section class="panel"><div class="panel-head"><h2>Difficulties</h2><div class="row">'+button('New','set-new','button blue slim')+button('Duplicate','set-duplicate','ghost slim')+'</div></div>'+
  '<p class="caption">Click a chart to open it. No Open / Prev / Next buttons.</p>'+
  '<div class="list">'+song.charts.map(function(c){return itemInfo(c.title,c.notes+' notes  ·  Difficulty '+c.rating.toFixed(1),s.chart===c.id,'set-select-chart','data-chart-id="'+H(c.id)+'"');}).join('')+'</div></section>';
 const right=panel('Set details','<dl class="small-dl" style="display:grid;grid-template-columns:1fr auto;gap:13px 8px;font-size:12px"><dt class="muted">Artist</dt><dd style="margin:0">'+H(song.artist)+'</dd><dt class="muted">BPM</dt><dd style="margin:0">'+song.bpm+'</dd><dt class="muted">Offset</dt><dd style="margin:0">'+song.offset+' ms</dd><dt class="muted">Charts</dt><dd style="margin:0">'+song.charts.length+'</dd></dl>'+
  '<div class="divider"></div><h3>Artwork</h3><div class="artwork" style="min-height:116px"><span class="art-center" style="font-size:24px">◌</span><span class="art-caption">BACKGROUND SAMPLE</span></div>');
 return '<div class="editor-main">'+left+center+right+'</div>';
}
function editorAudio(){
 const left=panel('Difficulty metadata',
  '<div class="column">'+input('Chart title',s.chartName,'chartName')+input('Song preview start (ms)',s.previewMs,'previewMs','number')+'</div>'+
  '<div class="divider"></div><div class="caption">The Song Set tempo map is shared across all difficulties.</div>');
 const center=panel('Song tempo map','<div class="row space-between"><p class="caption" style="margin:0">Absolute milliseconds; first marker defines offset and BPM.</p>'+button('Add BPM marker','tempo-add','button blue slim')+'</div>'+
  '<table class="tempo-table"><thead><tr><th>Offset (ms)</th><th>BPM</th><th></th></tr></thead><tbody>'+
  s.tempo.map(function(t,i){return '<tr><td><input aria-label="Marker '+(i+1)+' offset" type="number" value="'+t.time+'" data-tempo-time="'+i+'"></td><td><input aria-label="Marker '+(i+1)+' BPM" type="number" min="1" max="10000" step=".1" value="'+t.bpm+'" data-tempo-bpm="'+i+'"></td><td>'+(!i?'<span class="tag">START</span>':'<button class="ghost slim danger" data-tempo-delete="'+i+'">Delete</button>')+'</td></tr>';}).join('')+'</tbody></table>');
 const right=panel('Timing reference','<div class="metric-label">Track duration</div><div class="metric" style="font-size:26px;margin:10px 0">03:12.000</div>'+
  '<div class="divider"></div><div class="metric-label">Shared offset</div><div class="value mono" style="font-size:20px;margin:10px 0">'+(s.tempo[0]?s.tempo[0].time:0)+' ms</div><div class="divider"></div>'+
  '<div class="callout">Editing the shared tempo map changes beat alignment for all charts in the Song Set.</div>');
 return '<div class="editor-main">'+left+center+right+'</div>';
}
function editorField(){
 const directions=['↑','↗','→','↘','↓','↙','←','↖'];
 const left=panel('Initial orientation',
  selector('Initial heading',s.heading,'heading',directions)+
  '<div class="divider"></div><div class="caption">The start heading is owned by FIELD. It is intentionally absent from ROUTE.</div>'+
  '<div class="divider"></div><div class="eyebrow">Chart geometry</div><div class="row space-between" style="margin-top:9px"><span>Field grid</span><span class="mono">'+s.fieldWidth+' × '+s.fieldHeight+'</span></div>'+
  '<div class="row space-between" style="margin-top:9px"><span>Start coordinate</span><span class="mono">'+s.startX+', '+s.startY+'</span></div>');
 const cells=Array.from({length:s.fieldWidth*s.fieldHeight},function(_,i){const x=i%s.fieldWidth,y=Math.floor(i/s.fieldWidth),isStart=x===s.startX&&y===s.startY;
  return '<div class="grid-cell '+(isStart?'origin':'')+'">'+(isStart?H(s.heading):'')+'</div>';}).join('');
 const center='<section class="editor-stage"><div class="editor-stage-header"><strong>Field preview</strong>'+pill('Read-only grid','blue')+'</div><div class="editor-stage-body"><div class="field-preview"><div class="grid-editor" style="grid-template-columns:repeat('+s.fieldWidth+',1fr)">'+cells+'</div><p class="caption" style="max-width:190px">Reference view of the authored field. Heading is the only directional parameter in this panel.</p></div></div></section>';
 const right=panel('Skin context','<div class="eyebrow">Gameplay speed</div><div class="metric" style="font-size:26px;margin:9px 0">'+s.skin.baseSpeed+' <span style="font-size:12px;color:var(--muted)">px/s</span></div>'+
 '<div class="eyebrow" style="margin-top:18px">Preview window</div><div class="metric" style="font-size:26px;margin:9px 0">'+s.skin.lookahead+' <span style="font-size:12px;color:var(--muted)">ms</span></div>'+
 '<div class="divider"></div><div class="caption">These gameplay values are edited in Skins, not duplicated here.</div>');
 return '<div class="editor-main">'+left+center+right+'</div>';
}
function editor(){
 const tabs=[['set','SET'],['route','ROUTE'],['audio','AUDIO'],['field','FIELD']];
 let body=s.editorTab==='set'?editorSet():s.editorTab==='route'?editorRoute():s.editorTab==='audio'?editorAudio():editorField();
 return '<div class="page editor-page"><div class="editor-heading"><div class="title-group"><h1>Chart editor<span class="dirty" title="Demo changes" '+(!s.editorDirty?'style="opacity:.25"':'')+'></span></h1><span class="muted">/</span><span style="font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">'+H(s.chartName)+'</span>'+pill(s.editorDirty?'Unsaved':'Saved preview',s.editorDirty?'pink':'green')+'</div><div class="page-actions">'+button('Save · Ctrl+S','editor-save','button slim')+button('Save as…','editor-saveas','ghost slim')+button('Exit editor','editor-exit','ghost slim')+'</div></div>'+
 '<div class="tab-row">'+tabset(tabs,s.editorTab,'data-editor-tab')+'<span class="caption">Audio time shared across all panes</span></div>'+
 (s.editorTab==='route'?routeTools():'')+body+timeline()+'</div>';
}
function waveform(){
 let bars='';
 for(let i=0;i<100;i++){
  const n=(Math.sin(i*1.37)*.5+.5)*25+(Math.sin(i*.31)*.5+.5)*28+7;
  bars+='<span class="'+(i<s.listenProgress?'played':'')+'" style="height:'+n+'px"></span>';
 }
 return '<div class="waveform" aria-hidden="true">'+bars+'</div>';
}
function listen(){
 const totalSeconds=192,currentSeconds=Math.round(totalSeconds*s.listenProgress/100);
 const listenTime=String(Math.floor(currentSeconds/60)).padStart(2,'0')+':'+String(currentSeconds%60).padStart(2,'0');
 return '<div class="page">'+head('Listen','Preview a local audio file without a chart.',button('Choose music','listen-choose','button'))+
 '<div class="listen-layout"><section class="panel"><div class="artwork listen-art"><span class="art-center">♫</span><span class="art-caption">LOCAL AUDIO · DEMO</span></div><div class="row space-between" style="margin-top:20px"><div><div class="eyebrow">Current track</div><h2 style="font-size:25px;margin-top:5px">Aurora Drive.wav</h2><p class="caption" style="margin:0">Sample file title · no audio is loaded</p></div>'+pill(s.listenPlaying?'Playing (UI)':'Paused (UI)','blue')+'</div>'+
 '<div style="margin-top:15px">'+waveform()+'</div><input aria-label="Audio position" type="range" min="0" max="100" value="'+s.listenProgress+'" data-range="listenProgress">'+
 '<div class="listen-time"><span>'+listenTime+'</span><span>03:12</span></div><div class="row wrap" style="justify-content:center;margin-top:19px">'+
 button(s.listenPlaying?'Ⅱ Pause':'▶ Preview','listen-toggle','button primary')+button('↺ Restart','listen-restart','button')+button('Home','listen-home','ghost')+
 '</div><div class="callout" style="margin-top:19px">UI prototype: waveform and transport show possible states. No music is played or selected.</div></section></div></div>';
}
function play(){
 return '<div class="play-demo-page"><div class="play-controls">'+pill('PLAY · STATIC COMPOSITION','blue')+button('View Results','play-results','ghost slim')+'</div>'+
 '<div class="play-demo-field">'+L.routeSvg(2)+'</div><div class="play-hud"><div class="eyebrow">Accuracy</div><div class="metric">97.27%</div><div class="eyebrow">Combo</div><div class="metric">042</div></div>'+
 '<div style="position:absolute;left:20px;bottom:20px" class="row">'+button('Pause / Esc','play-pause','button')+pill('Gameplay untouched')+'</div>'+
 (s.playPaused?'<div class="play-paused"><section class="panel modal-card"><div class="eyebrow">Paused</div><h1>Take a break</h1><p>Only pause controls are shown here. No new gameplay animation is proposed.</p>'+
 '<div class="row wrap" style="justify-content:center">'+button('Resume','play-pause','button primary')+button('Retry','play-retry','button')+button('Leave','play-leave','ghost danger')+'</div></section></div>':'')+'</div>';
}
function results(){
 return '<div class="page">'+head('Results','Representative accuracy and judgment summary.',button('Back to library','results-library','ghost'))+
 '<div class="results-layout"><section class="panel results-hero"><div class="eyebrow">Chart complete</div>'+
 '<div class="grade" style="font-size:65px;margin:20px 0;color:var(--blue)">98.54<span style="font-size:25px">%</span></div>'+
 '<div class="grade-caption">Accuracy</div><h2 style="margin:25px 0 5px">Horizon</h2><p class="caption">Aurora Drive / NOVA</p><div class="divider"></div>'+
 '<div class="eyebrow">Notes judged</div><div class="results-score" style="margin:10px 0 18px">486 / 486</div>'+
 '<div class="row wrap" style="justify-content:center">'+button('Play again','results-retry','button primary')+button('Edit chart','results-edit','button')+'</div></section>'+
 '<section class="panel"><div class="panel-head"><h2>Judgments</h2>'+pill('Example values')+'</div><div class="results-stat-list">'+
 [['Perfect','445'],['Good','37'],['Bad','0'],['Miss','4']].map(function(x){return '<div class="results-stat"><div class="metric-label">'+x[0]+'</div><div class="value">'+x[1]+'</div></div>';}).join('')+
 '</div><div class="divider"></div><div class="row space-between"><strong>No fail</strong><span class="caption">Authored route remains unchanged</span></div>'+
 '<div class="callout" style="margin-top:18px">Static demonstration of the actual accuracy and counts hierarchy. Not a recorded play.</div></section></div></div>';
}
function system(){
 const colors=[['Background','#202129'],['Surface','#2b2e38'],['Soft blue','#bbd2e3'],['Rose','#e95a99'],['Text','#f0eced']];
 const cases=[['quit','Exit LOCUS'],['unsaved','Unsaved chart'],['delete-chart','Delete difficulty'],
 ['delete-set','Delete Song Set'],['leave','Leave level'],['retry','Retry chart'],
 ['import','Import source'],['export','Export package'],['saveas','Save as…'],['error','File error'],
 ['success','Success state'],['compare','Compare charts']];
 return '<div class="page">'+head('Component library','The application design system, modal coverage and interaction states.',button('Back home','system-home','ghost'))+
 '<div class="system-layout">'+
 panel('Color & typography','<div class="swatches">'+colors.map(function(x){return '<div><div class="swatch" style="background:'+x[1]+'"></div><div class="caption">'+x[0]+'</div><span class="mono muted">'+x[1]+'</span></div>';}).join('')+'</div><div class="divider"></div><h1 style="margin-bottom:8px">Primary heading</h1><h2 style="margin-bottom:8px">Section heading</h2><p style="margin-bottom:0">A quiet hierarchy: only actions, active selection and critical status use strong contrast.</p>')+
 panel('Button states','<div class="kit-row">'+button('Primary','sample-toast','button primary')+button('Secondary','sample-toast','button blue')+button('Neutral','sample-toast','button')+button('Ghost','sample-toast','ghost')+'</div><div class="kit-row">'+button('Destructive','sample-error','danger-button')+'<button class="button" disabled>Disabled</button><button class="button" style="outline:2px solid var(--rose);outline-offset:2px">Focus</button></div><div class="caption">Hover 55ms on Home. No motion is added to gameplay.</div>')+
 panel('Fields, selection & state','<div class="form-grid">'+input('Text input','Example value','kitText')+selector('Select','two','kitSelect',[['one','First'],['two','Second'],['three','Third']])+'</div><div class="divider"></div>'+checkbox('On / off control','Semantic control with visible state.','hitFlash')+'<div class="divider"></div><div class="row wrap">'+pill('Selected','pink')+pill('Ready','green')+pill('Read only','blue')+pill('Needs attention','warning')+'</div>')+
 panel('Dialogs & recovery','<div class="caption" style="margin-bottom:14px">Each opens the same reusable modal component. Confirm and cancel are separate, and destructive actions stay explicit.</div><div class="kit-row">'+cases.map(function(x){return '<button class="ghost slim" data-dialog-demo="'+x[0]+'">'+x[1]+'</button>';}).join('')+'</div>')+
 '</div></div>';
}
window.LocusScreens={home:home,library:library,settings:settings,skins:skin,editor:editor,listen:listen,play:play,results:results,system:system};
})();
