/* LOCUS UI v2 — screen composition. Semantically representative UI, not game code. */
(function(){
'use strict';
const L=window.LocusDemo, s=L.state, H=L.H;
function pill(text,type){return '<span class="tag '+(type||'')+'">'+H(text)+'</span>';}
function head(title,subtitle,actions){
 return '<div class="page-heading"><div><h1>'+H(title)+'</h1></div><div class="page-actions">'+(actions||'')+'</div></div>';
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
 return '<div class="check-row"><strong>'+H(name)+'</strong><button type="button" class="switch" role="switch" aria-label="'+H(name)+'" aria-checked="'+s[key]+'" data-toggle="'+H(key)+'"></button></div>';
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

  '<div class="home-actions">'+
  '<button class="home-action play" data-go="library">PLAY</button>'+
  '<button class="home-action editor" data-go="editor">EDITOR</button>'+
  '<button class="home-action skins" data-go="skins">SKINS</button>'+
  '<button class="home-action settings" data-go="settings">SETTINGS</button>'+
  '</div>'+
  '<button class="home-exit" data-action="quit">EXIT</button></div>';
}
function library(){
 const selectedSet=L.selectedSong(), chart=L.selectedChart();
 const sets=s.libraryEmpty?[]:L.library.filter(function(song){
  return (song.title+' '+song.artist+' '+song.charts.map(c=>c.title).join(' ')).toLowerCase().includes(s.search.toLowerCase());
 });
 const tree=sets.length?sets.map(function(song){
  const open=s.expanded.includes(song.id);
  return '<div class="tree-group"><button class="tree-parent '+(s.song===song.id?'selected':'')+'" data-song="'+H(song.id)+'"><span class="chevron">'+(open?'▾':'▸')+'</span><span class="grow">'+H(song.title)+'</span><span class="count">'+song.charts.length+'</span></button>'+
  (open?song.charts.map(function(c){
    return '<button class="tree-child '+(s.song===song.id&&s.chart===c.id?'selected':'')+'" data-chart="'+H(c.id)+'" data-song-id="'+H(song.id)+'"><span class="difficulty-dot"></span><span class="title">'+H(c.title)+'</span><span class="mono">'+c.rating.toFixed(1)+'</span></button>';
  }).join(''):'')+'</div>';
 }).join(''):
 '<div class="empty"><h3>No songs</h3>'+button('Restore','library-reset','button slim')+'</div>';
 const actions=button('Import','library-import','ghost')+button('Rescan','library-rescan','ghost');
 const hasSelection=sets.some(function(song){return song.id===s.song;})&&!!chart;
 const runs=hasSelection?L.runsForChart():[];
 const history=hasSelection?
  '<section class="run-history"><div class="row space-between" style="margin-bottom:8px"><h3 style="margin:0">Results</h3><span class="tag">'+runs.length+'</span></div>'+
  (runs.length?'<div class="library-run-list">'+runs.map(function(run){
    return '<button type="button" class="run-entry" data-run-id="'+H(run.id)+'"><span class="run-main"><strong>'+run.accuracy.toFixed(2)+'%</strong><span class="run-date">'+H(run.time)+'</span></span><span class="run-open" aria-hidden="true">›</span></button>';
  }).join('')+'</div>':'<div class="run-empty">No results</div>')+'</section>':'';
 const left='<section class="panel library-list-panel"><div class="panel-head"><h2>Songs</h2><span class="tag">'+sets.length+'</span></div>'+
  '<input aria-label="Search song library" type="search" placeholder="Search…" data-field="library-search" value="'+H(s.search)+'">'+
  '<div class="library-list">'+tree+'</div>'+history+'</section>';
 if(!hasSelection){
  return '<div class="page">'+head('Song library','',actions)+
   '<div class="workspace-area library-layout">'+left+
   '<section class="panel"><div class="empty"><h3>No chart selected</h3></div></section>'+
   '<section class="panel library-inspector"></section></div></div>';
 }
 const rating=chart.rating.toFixed(1);
 const center='<section class="panel selection-detail">'+
  '<div class="artwork"><span class="art-center">◌</span></div>'+
  '<h2>'+H(selectedSet.title)+'</h2><div class="meta">'+H(selectedSet.artist)+' · '+selectedSet.bpm+' BPM · '+H(selectedSet.duration)+'</div>'+
  '<div class="row space-between" style="align-items:end;margin:17px 0"><div><h3 style="font-size:16px;margin:0">'+H(chart.title)+'</h3></div>'+
  '<div class="rating" title="Difficulty rating">'+rating+'</div></div>'+
  '<div class="selection-actions">'+button('▶  Play','library-play','button primary')+button('Edit','library-edit','button')+
  button('Song Set','library-manage','ghost')+'</div>'+
  '<div class="stat-grid" style="margin-top:20px">'+stat('Best',runs.length?Math.max.apply(null,runs.map(r=>r.accuracy)).toFixed(2)+'%':'—')+
   stat('Attempts',runs.length)+stat('Offset',selectedSet.offset+' ms')+'</div></section>';
 const dimensions=[['Speed','speed'],['Rhythm','rhythm'],['Coordination','keys'],['Reading','reading'],['Stamina','stamina'],['Peak','peak']];
 const right='<section class="panel library-inspector"><div class="panel-head"><h2>Difficulty</h2></div>'+
  '<div class="rating-breakdown">'+dimensions.map(function(d){const value=chart[d[1]];return '<div class="rating-line"><span>'+d[0]+'</span><div class="progress"><span style="width:'+(value*10)+'%"></span></div><b>'+value.toFixed(1)+'</b></div>';}).join('')+
  '</div><div class="divider"></div><dl class="small-dl"><dt>Artist</dt><dd>'+H(selectedSet.artist)+'</dd><dt>Song offset</dt><dd>'+selectedSet.offset+' ms</dd><dt>Difficulties</dt><dd>'+selectedSet.charts.length+'</dd></dl></section>';
 return '<div class="page">'+head('Song library','',actions)+
  '<div class="workspace-area library-layout">'+left+center+right+'</div></div>';
}
function settings(){
 const menu=[['game','Game'],['display','Display'],['bindings','Key bindings'],['audio','Audio & offset']];
 let content='';
 if(s.settingsTab==='game'){
  content=panel('Gameplay preferences',
   checkbox('Hit flash','Display the existing on-hit flash feedback.','hitFlash')+
   '')+
   panel('Navigation','<div class="row wrap">'+button('Key bindings','settings-bindings','button')+button('Skins','settings-skins','ghost')+'</div>');
 } else if(s.settingsTab==='display'){
  content=panel('Display',
   checkbox('VSync','Synchronize display updates with the monitor.','vsync')+
   '<div class="setting-row"><div><div class="setting-name">Frame rate limit</div><div class="setting-desc"></div></div><div class="setting-control">'+selector('',s.fps,'fps',['30','60','90','120','144','165','240',['0','Unlimited']])+'</div></div>'+
   checkbox('Show FPS','Show a small performance counter.','showFps')+
   checkbox('Borderless fullscreen','Use the desktop resolution.','fullscreen')+
   '');
 } else if(s.settingsTab==='audio'){
  content=panel('Audio',
   '<div class="setting-row"><div><div class="setting-name">Master volume</div><div class="setting-desc"></div></div><div class="setting-control"><input aria-label="Master volume" type="range" min="0" max="100" value="'+s.volume+'" data-range="volume"><span class="mono" id="volumeLabel">'+s.volume+'%</span></div></div>'+
   '<div class="setting-row"><div><div class="setting-name">Global output offset</div><div class="setting-desc">Positive = audio is late, negative = audio is early.</div></div><div class="setting-control"><input style="width:105px" aria-label="Global offset ms" type="number" min="-1000" max="1000" step="1" data-field="offset" value="'+s.offset+'"><span class="mono muted">ms</span></div></div>'+
   '<div class="row space-between" style="margin-top:15px"><div><strong>Metronome calibration</strong></div>'+button('Open calibrator','calibration-open','button blue')+'</div>')+
   (s.calibrationOpen?calibrator(): '');
 } else {
  const groupNames=[['play','PLAY'],['menus','MENUS'],['editor','EDITOR']];
  const list=L.bindings[s.bindingGroup].filter(function(row){return row[0].toLowerCase().includes(s.bindingSearch.toLowerCase());});
  content=panel('Key bindings',
   '<div class="tab-row" style="margin-bottom:17px"><div class="segmented">'+groupNames.map(function(x){return L.segment(x[0],s.bindingGroup,x[1],'data-bind-group');}).join('')+'</div><input style="max-width:210px" type="search" placeholder="Find action…" value="'+H(s.bindingSearch)+'" data-field="binding-search"></div>'+
   '<div class="keymap-grid">'+list.map(function(x){
    return '<div>'+H(x[0])+'</div><div><button class="key-bind'+(s.capture===x[0]?' listening':'')+'" data-bind="'+H(x[0])+'" type="button">'+(s.capture===x[0]?'Press key…':H(x[1]))+'</button></div>';
   }).join('')+'</div>'+
   (!list.length?'<div class="empty"><h3>No bindings match</h3><p>Clear the filter to see all actions.</p></div>':'')+
   '');
 }
 return '<div class="page">'+head('Settings','System behavior, key mapping and calibration.',button('Reset settings','settings-reset','ghost'))+
  '<div class="workspace-area settings-layout"><section class="panel"><div class="eyebrow" style="margin-bottom:12px">Preferences</div><nav class="side-list">'+
  menu.map(function(x){return '<button class="side-link '+(s.settingsTab===x[0]?'active':'')+'" data-settings-tab="'+x[0]+'">'+x[1]+'<span>›</span></button>';}).join('')+
  '</nav></section><div class="settings-content">'+content+'</div></div></div>';
}
function calibrator(){
 const count=s.calibrationCount;
 return panel('Calibration exercise',
  '<div class="calibration-pad"><div class="beat-circle '+(count?'hit':'')+'">'+(s.calibrationComplete?'+24 ms':count+'/ 18')+'</div>'+
  '<div class="cal-ticks">'+Array.from({length:18},function(_,i){return '<span class="'+(i<count?'active':'')+'"></span>';}).join('')+'</div>'+
  ''+
  (s.calibrationComplete?'<div class="metric">+24 ms</div>':'')+
  '<div class="row" style="justify-content:center">'+button('Tap','calibration-tap','button primary',s.calibrationComplete?'disabled':'')+button('Retry','calibration-retry','ghost')+
  (s.calibrationComplete?button('Apply sample','calibration-apply','button blue'):'')+'</div></div>');
}
function skinPreview(){
 return '<div class="skin-preview"><section class="panel"><div class="panel-head"><h2>Live preview</h2>'+pill('Static sample','blue')+'</div><div class="preview-window"><div class="route-demo">'+L.routeSvg(1)+'</div><div class="skin-hud">ACC 97.27%<br>COMBO 042</div></div><div class="row space-between" style="margin-top:14px"><span class="mono">'+s.skin.baseSpeed+' px/s</span></div></section>'+
 panel('Profile','<h3 style="margin-top:6px">'+H(s.skinName)+(s.skinDirty?' <span class="rose-text">•</span>':'')+'</h3>'+
 '<div class="row wrap">'+button('Save','skin-save','button blue')+button('Save as…','skin-saveas','ghost')+button('Revert','skin-revert','ghost')+'</div>')+'</div>';
}
function skin(){
 const tabs=[['path','PATH'],['player','PLAYER'],['colors','COLORS'],['profiles','MY SKINS']];
 let body='';
 if(s.skinTab==='path'){
  const vals=[['Preview time','lookahead',50,2000,50,'ms'],['Path width','pathWidth',1,12,.25,'px'],['Outline scale','outlineScale',1,3,.25,'×'],['Fade curve','fadePower',.5,2.5,.1,'×'],['Path opacity','pathOpacity',0,1,.05,''],['Turn size','junctionRadius',3,18,1,'px'],['Reverse cap','reverseCapScale',0,6,.5,'×']];
  body=panel('Path & field',vals.map(function(v){return '<div class="control-line"><span>'+v[0]+'</span><input type="range" data-skin-range="'+v[1]+'" min="'+v[2]+'" max="'+v[3]+'" step="'+v[4]+'" value="'+s.skin[v[1]]+'"><span class="number">'+s.skin[v[1]]+' '+v[5]+'</span></div>';}).join('')+
   '<div class="check-row"><div><strong>Turn glow</strong></div><button type="button" class="switch" role="switch" aria-checked="'+s.skin.junctionGlow+'" data-skin-toggle="junctionGlow"></button></div>');
 } else if(s.skinTab==='player'){
  const vals=[['Player size','playerRadius',6,22,1,'px'],['Base speed','baseSpeed',60,2000,10,'px/s'],['Hit flash size','hitFlashScale',.5,2,.1,'×'],['Camera lag','cameraLagMs',0,500,25,'ms']];
  body=panel('Player & feedback',
   vals.map(function(v){return '<div class="control-line"><span>'+v[0]+'</span><input type="range" data-skin-range="'+v[1]+'" min="'+v[2]+'" max="'+v[3]+'" step="'+v[4]+'" value="'+s.skin[v[1]]+'"><span class="number">'+s.skin[v[1]]+' '+v[5]+'</span></div>';}).join('')+
   '<div class="check-row"><div><strong>Player glow</strong></div><button type="button" class="switch" role="switch" aria-checked="'+s.skin.playerGlow+'" data-skin-toggle="playerGlow"></button></div>'+
   '<div class="check-row"><div><strong>Show HUD</strong></div><button type="button" class="switch" role="switch" aria-checked="'+s.skin.showHud+'" data-skin-toggle="showHud"></button></div>');
 } else if(s.skinTab==='colors'){
  const names=[['background','Background'],['trajectory','Trajectory'],['outline','Outline'],['junction','Junction'],['player','Player'],['playerCore','Player core'],['hudBackground','HUD background'],['hudText','HUD text'],['hitFlash','Hit flash'],['fieldBorder','Field border']];
  body=panel('Element colors',''+
    '<div class="color-matrix">'+names.map(function(v){return '<button type="button" class="color-row '+(s.selectedColor===v[0]?'active':'')+'" data-skin-color="'+v[0]+'"><span class="color-swatch" style="background:'+H(s.skin.colors[v[0]])+'"></span><span>'+v[1]+'</span></button>';}).join('')+'</div>'+
    '<div class="skin-color-editor"><input aria-label="Selected color" type="color" data-color-picker value="'+H(s.skin.colors[s.selectedColor||'trajectory'])+'"><div class="grow">'+
    input('Hex color',s.skin.colors[s.selectedColor||'trajectory'],'skin-hex')+'</div></div>');
 } else {
  body=panel('Saved profiles',
   '<div class="profiles-grid">'+s.profiles.map(function(p){return '<button class="profile-row '+(s.skinProfile===p.id?'selected':'')+'" data-profile="'+H(p.id)+'"><span class="profile-avatar"></span><span class="grow" style="text-align:left"><strong>'+H(p.name)+'</strong><span class="row-sub" style="display:block">'+H(p.subtitle)+'</span></span>'+(s.skinProfile===p.id?pill('Active','pink'):'')+'</button>';}).join('')+'</div><div class="row wrap" style="margin-top:16px">'+
   button('Activate','skin-activate','button blue')+button('Store','skin-save','button')+button('Duplicate','skin-saveas','ghost')+button('Delete','skin-delete','ghost danger')+'</div>');
 }
 return '<div class="page">'+head('Skins','Customize the playfield without changing the application UI.',button('Folder','skin-folder','ghost'))+
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
  '</div><div class="row"><span class="mono">00:01.438 / 03:12</span></div></div></div>';
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
  '<div class="metric-label">Beat</div><div class="metric" style="font-size:34px;margin:8px 0 18px">'+s.selectedBeat+'.00</div>'+
  '<div class="input-label">Selected direction<select data-select="selectedDirection">'+['↑','↗','→','↘','↓','↙','←','↖'].map(d=>'<option '+(s.selectedDirection===d?'selected':'')+'>'+d+'</option>').join('')+'</select></div>'+
  '<div class="divider"></div><div class="eyebrow">Editing tools</div><div class="row wrap" style="margin-top:10px">'+
  button('Previous beat','beat-prev','ghost slim')+button('Next beat','beat-next','ghost slim')+'</div>'+
  '<div class="divider"></div><div class="small-dl" style="display:grid;grid-template-columns:1fr 1fr;gap:8px;color:var(--sub);font-size:11px">'+
  '<span>Selected tick</span><b>'+s.selectedBeat+'/12</b><span>Notes in chart</span><b>'+s.notes.length+'</b><span>Clipboard</span><b>'+(s.clipboard?'1 note':'Empty')+'</b></div>'+
  '</section>';
 const previews='<section class="editor-stage"><div class="editor-stage-header"><div class="row"><strong>Route preview</strong>'+''+'</div></div><div class="editor-stage-body">'+
  panes.map(function(p,i){return '<div class="preview-pane '+(s.focusedPane===i?'selected':'')+'" role="button" tabindex="0" data-pane="'+i+'">'+
   '<div class="preview-label"><span class="pulse-dot"></span>'+H(p?p.title:'Draft')+'</div><span class="preview-rating">DIFF '+(p?p.rating.toFixed(1):'—')+'</span>'+L.routeSvg(i)+'</div>';}).join('')+'</div></section>';
 const directions=['↖','↑','↗','←','DEL','→','↙','↓','↘'];
 const pad='<div class="direction-grid">'+directions.map(function(d){return '<button type="button" class="direction '+(d==='DEL'?'delete':'')+' '+(s.selectedDirection===d?'active':'')+'" data-note="'+H(d)+'" title="'+(d==='DEL'?'Remove note':'Place '+d)+'">'+(d==='DEL'?'⌫':H(d))+'<span style="font-size:10px;margin-left:6px">'+(d==='DEL'?'':directionCount(d))+'</span></button>';}).join('')+'</div>';
 const right='<section class="panel compact editor-inspector"><div class="panel-head"><h3>Inspector</h3>'+pill('Route')+'</div>'+
  '<div class="inspector-group"><div class="eyebrow">Direction pad</div>'+pad+'</div>'+
  '<div class="inspector-group"><div class="eyebrow">Preview speed</div><div class="row" style="margin-top:8px">'+
  '<input type="number" min="20" max="2000" step="5" data-field="previewSpeed" value="'+s.previewSpeed+'"><span class="mono muted">px/s</span></div></div>'+
  '<div class="inspector-group"><div class="eyebrow">Visible path</div><div class="row" style="margin-top:8px"><input type="number" min="50" max="10000" step="50" data-field="previewLength" value="'+s.previewLength+'"><span class="mono muted">ms</span></div></div>'+
  '</section>';
 return '<div class="editor-main">'+left+previews+right+'</div>';
}
function editorSet(){
 const song=L.selectedSong();
 const left=panel('Song Set','<h2 style="margin:8px 0 6px">'+H(song.title)+'</h2>'+
 ''+
 '<div class="divider"></div><div class="row wrap">'+pill(song.id==='demo'?'Built-in':'Managed Set','blue')+pill('Shared timing')+'</div>'+
 '<div class="divider"></div><div class="eyebrow">Files & artwork</div><div class="set-actions">'+
 button('Open folder','set-open-folder','ghost slim')+button('Add / replace BG','set-background','ghost slim')+
 button('Remove BG','set-remove-bg','ghost slim')+button('Import chart','set-import','ghost slim')+
 button('Export package','set-export','ghost slim')+button('Restore last','set-restore','ghost slim')+'</div>'+
 '<div class="divider"></div><div class="row wrap">'+button('Delete difficulty','set-delete-chart','ghost danger slim')+button('Delete Song Set','set-delete-set','ghost danger slim')+'</div>');
 const center='<section class="panel"><div class="panel-head"><h2>Difficulties</h2><div class="row">'+button('New','set-new','button blue slim')+button('Duplicate','set-duplicate','ghost slim')+'</div></div>'+
  ''+
  '<div class="list">'+song.charts.map(function(c){return itemInfo(c.title,c.notes+' notes  ·  Difficulty '+c.rating.toFixed(1),s.chart===c.id,'set-select-chart','data-chart-id="'+H(c.id)+'"');}).join('')+'</div></section>';
 const right=panel('Set details','<dl class="small-dl" style="display:grid;grid-template-columns:1fr auto;gap:13px 8px;font-size:12px"><dt class="muted">Artist</dt><dd style="margin:0">'+H(song.artist)+'</dd><dt class="muted">BPM</dt><dd style="margin:0">'+song.bpm+'</dd><dt class="muted">Offset</dt><dd style="margin:0">'+song.offset+' ms</dd><dt class="muted">Charts</dt><dd style="margin:0">'+song.charts.length+'</dd></dl>'+
  '<div class="divider"></div><h3>Artwork</h3><div class="artwork" style="min-height:116px"><span class="art-center" style="font-size:24px">◌</span><span class="art-caption">BACKGROUND SAMPLE</span></div>');
 return '<div class="editor-main">'+left+center+right+'</div>';
}
function editorAudio(){
 const left=panel('Difficulty metadata',
  '<div class="column">'+input('Chart title',s.chartName,'chartName')+input('Song preview start (ms)',s.previewMs,'previewMs','number')+'</div>'+
  '<div class="divider"></div>');
 const center=panel('Song tempo map','<div class="row space-between">'+button('Add BPM marker','tempo-add','button blue slim')+'</div>'+
  '<table class="tempo-table"><thead><tr><th>Offset (ms)</th><th>BPM</th><th></th></tr></thead><tbody>'+
  s.tempo.map(function(t,i){return '<tr><td><input aria-label="Marker '+(i+1)+' offset" type="number" value="'+t.time+'" data-tempo-time="'+i+'"></td><td><input aria-label="Marker '+(i+1)+' BPM" type="number" min="1" max="10000" step=".1" value="'+t.bpm+'" data-tempo-bpm="'+i+'"></td><td>'+(!i?'<span class="tag">START</span>':'<button class="ghost slim danger" data-tempo-delete="'+i+'">Delete</button>')+'</td></tr>';}).join('')+'</tbody></table>');
 const right=panel('Timing reference','<div class="metric-label">Track duration</div><div class="metric" style="font-size:26px;margin:10px 0">03:12.000</div>'+
  '<div class="divider"></div><div class="metric-label">Shared offset</div><div class="value mono" style="font-size:20px;margin:10px 0">'+(s.tempo[0]?s.tempo[0].time:0)+' ms</div><div class="divider"></div>'+
  '');
 return '<div class="editor-main">'+left+center+right+'</div>';
}
function editorField(){
 const directions=['↑','↗','→','↘','↓','↙','←','↖'];
 const left=panel('Initial orientation',
  selector('Initial heading',s.heading,'heading',directions)+
  '<div class="divider"></div>'+
  '<div class="divider"></div><div class="eyebrow">Chart geometry</div><div class="row space-between" style="margin-top:9px"><span>Field grid</span><span class="mono">'+s.fieldWidth+' × '+s.fieldHeight+'</span></div>'+
  '<div class="row space-between" style="margin-top:9px"><span>Start coordinate</span><span class="mono">'+s.startX+', '+s.startY+'</span></div>');
 const cells=Array.from({length:s.fieldWidth*s.fieldHeight},function(_,i){const x=i%s.fieldWidth,y=Math.floor(i/s.fieldWidth),isStart=x===s.startX&&y===s.startY;
  return '<div class="grid-cell '+(isStart?'origin':'')+'">'+(isStart?H(s.heading):'')+'</div>';}).join('');
 const center='<section class="editor-stage"><div class="editor-stage-header"><strong>Field preview</strong>'+''+'</div><div class="editor-stage-body"><div class="field-preview"><div class="grid-editor" style="grid-template-columns:repeat('+s.fieldWidth+',1fr)">'+cells+'</div></div></div></section>';
 const right=panel('Skin context','<div class="eyebrow">Gameplay speed</div><div class="metric" style="font-size:26px;margin:9px 0">'+s.skin.baseSpeed+' <span style="font-size:12px;color:var(--muted)">px/s</span></div>'+
 '<div class="eyebrow" style="margin-top:18px">Preview window</div><div class="metric" style="font-size:26px;margin:9px 0">'+s.skin.lookahead+' <span style="font-size:12px;color:var(--muted)">ms</span></div>'+
 '<div class="divider"></div>');
 return '<div class="editor-main">'+left+center+right+'</div>';
}
function editor(){
 const tabs=[['set','SET'],['route','ROUTE'],['audio','AUDIO'],['field','FIELD']];
 let body=s.editorTab==='set'?editorSet():s.editorTab==='route'?editorRoute():s.editorTab==='audio'?editorAudio():editorField();
 return '<div class="page editor-page"><div class="editor-heading"><div class="title-group"><h1>Chart editor<span class="dirty" title="Demo changes" '+(!s.editorDirty?'style="opacity:.25"':'')+'></span></h1><span class="muted">/</span><span style="font-weight:600;overflow:hidden;text-overflow:ellipsis;white-space:nowrap">'+H(s.chartName)+'</span>'+pill(s.editorDirty?'Unsaved':'Saved preview',s.editorDirty?'pink':'green')+'</div><div class="page-actions">'+button('Save · Ctrl+S','editor-save','button slim')+button('Save as…','editor-saveas','ghost slim')+button('Exit editor','editor-exit','ghost slim')+'</div></div>'+
 '<div class="tab-row">'+tabset(tabs,s.editorTab,'data-editor-tab')+'</div>'+
 (s.editorTab==='route'?routeTools():'')+body+timeline()+'</div>';
}
function play(){
 return '<div class="play-demo-page">'+
 '<div class="play-controls">'+button('Finish chart','play-complete','button slim')+'</div>'+
 '<div class="play-demo-field">'+L.routeSvg(2)+'</div>'+
 '<div class="play-hud"><div class="eyebrow">Accuracy</div><div class="metric">97.27%</div>'+
 '<div class="eyebrow">Perfect</div><div class="mono">445</div>'+
 '<div class="eyebrow">Good</div><div class="mono">37</div>'+
 '<div class="eyebrow">Bad</div><div class="mono">0</div>'+
 '<div class="eyebrow">Miss</div><div class="mono">4</div></div>'+
 '<div style="position:absolute;left:20px;bottom:20px" class="row">'+
 button('Pause / Esc','play-pause','button')+'</div>'+
 (s.playPaused?'<div class="play-paused"><section class="panel modal-card"><h1>Paused</h1>'+
 '<div class="row wrap" style="justify-content:center">'+button('Resume','play-pause','button primary')+
 button('Retry','play-retry','button')+button('Leave','play-leave','ghost danger')+
 '</div></section></div>':'')+'</div>';
}
function results(){
 const run=L.selectedRun();
 if(!run)return '<div class="page">'+head('Results','',button('Back to chart','results-library','ghost'))+
 '<div class="empty"><h3>No result selected</h3></div></div>';
 const chart=L.selectedChart(),song=L.selectedSong(),runs=L.runsForChart();
 const pct=run.accuracy.toFixed(2)+'%';
 const cells=[['Perfect',run.perfect],['Good',run.good],['Bad',run.bad],['Miss',run.miss]];
 const history=runs.length>1?'<div class="divider"></div><h3>Other results</h3><div class="library-run-list">'+
 runs.filter(x=>x.id!==run.id).map(function(r){
   return '<button class="run-entry" type="button" data-run-id="'+H(r.id)+'"><span class="run-main"><strong>'+r.accuracy.toFixed(2)+'%</strong>'+
    '<span class="run-date">'+H(r.time)+'</span></span><span class="run-open">›</span></button>';
 }).join('')+'</div>':'';
 return '<div class="page">'+head('Results','',button('Back to chart','results-library','ghost'))+
 '<div class="results-layout">'+
 '<section class="panel results-hero"><div class="eyebrow">'+H(chart?chart.title:'Chart')+'</div>'+
 '<div class="grade" style="font-size:65px;margin:20px 0">'+pct+'</div>'+
 '<div class="grade-caption">Accuracy</div>'+
 '<h2 style="margin:24px 0 5px">'+H(song.title)+'</h2>'+
 '<div class="muted">'+H(run.time)+'</div>'+
 '<div class="divider"></div><div class="metric-label">Notes judged</div>'+
 '<div class="results-score" style="margin:10px 0 20px">'+run.total+' / '+run.total+'</div>'+
 '<div class="row wrap" style="justify-content:center">'+button('Retry','results-retry','button primary')+
 button('Edit chart','results-edit','button')+'</div></section>'+
 '<section class="panel"><div class="panel-head"><h2>Judgments</h2></div>'+
 '<div class="results-stat-list">'+cells.map(function(c){return '<div class="results-stat"><div class="metric-label">'+c[0]+'</div>'+
 '<div class="value">'+c[1]+'</div></div>';}).join('')+'</div>'+
 history+'</section></div></div>';
}
function system(){
 const colors=[['Background','#202129'],['Surface','#2b2e38'],['Soft blue','#bbd2e3'],['Rose','#e95a99'],['Text','#f0eced']];
 const cases=[['quit','Exit LOCUS'],['unsaved','Unsaved chart'],['delete-chart','Delete difficulty'],
 ['delete-set','Delete Song Set'],['leave','Leave level'],['retry','Retry chart'],
 ['import','Import source'],['export','Export package'],['saveas','Save as…'],['error','File error'],
 ['success','Success state'],['compare','Compare charts']];
 return '<div class="page">'+head('Component library','The application design system, modal coverage and interaction states.',button('Back home','system-home','ghost'))+
 '<div class="system-layout">'+
 panel('Color & typography','<div class="swatches">'+colors.map(function(x){return '<div><div class="swatch" style="background:'+x[1]+'"></div><span class="mono muted">'+x[1]+'</span></div>';}).join('')+'</div><div class="divider"></div><h1 style="margin-bottom:8px">Primary heading</h1><h2 style="margin-bottom:8px">Section heading</h2>')+
 panel('Button states','<div class="kit-row">'+button('Primary','sample-toast','button primary')+button('Secondary','sample-toast','button blue')+button('Neutral','sample-toast','button')+button('Ghost','sample-toast','ghost')+'</div><div class="kit-row">'+button('Destructive','sample-error','danger-button')+'<button class="button" disabled>Disabled</button><button class="button" style="outline:2px solid var(--rose);outline-offset:2px">Focus</button></div>')+
 panel('Fields, selection & state','<div class="form-grid">'+input('Text input','Example value','kitText')+selector('Select','two','kitSelect',[['one','First'],['two','Second'],['three','Third']])+'</div><div class="divider"></div>'+checkbox('On / off control','Semantic control with visible state.','hitFlash')+'<div class="divider"></div><div class="row wrap">'+pill('Selected','pink')+pill('Ready','green')+pill('Read only','blue')+pill('Needs attention','warning')+'</div>')+
 panel('Dialogs & recovery','<div class="kit-row">'+cases.map(function(x){return '<button class="ghost slim" data-dialog-demo="'+x[0]+'">'+x[1]+'</button>';}).join('')+'</div>')+
 '</div></div>';
}
window.LocusScreens={home:home,library:library,settings:settings,skins:skin,editor:editor,play:play,results:results,system:system};
})();
