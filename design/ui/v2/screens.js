/* LOCUS page geometry only — no product theme or gameplay animations. */
(function(){
'use strict';
const M=window.LocusWire,s=M.state,H=M.escape;
function button(text,action,cls,attrs){
 return '<button type="button" class="'+(cls||'')+'" data-act="'+H(action)+'" '+(attrs||'')+'>'+H(text)+'</button>';
}
function panel(title,content,extra){
 return '<section class="box '+(extra||'')+'"><div class="box-head">'+title+'</div>'+content+'</section>';
}
function bar(name,extra){
 return '<header class="backbar">'+button('← BACK','home','back')+
 '<h1>'+H(name)+'</h1>'+(extra||'')+'</header>';
}
function field(label,key,value,type,extra){
 return '<label class="field">'+H(label)+'<input data-field="'+H(key)+'" type="'+(type||'text')+'" value="'+H(value)+'" '+(extra||'')+'></label>';
}
function flag(label,pressed,action){
 return '<button type="button" data-act="'+H(action)+'" aria-pressed="'+pressed+'">'+H(label)+'</button>';
}
function home(){
 return '<div class="home-page"><div class="home-logo">locus.</div><div class="home-actions">'+
 button('PLAY','library')+button('EDITOR','editor-new')+button('SKINS','skins')+
 button('SETTINGS','settings')+'</div>'+button('EXIT','quit','home-exit')+'</div>';
}
function library(){
 const song=M.song(),chart=M.chart(),visible=M.songs.filter(x=>(x.title+' '+x.artist+' '+x.charts.map(c=>c.title).join(' ')).toLowerCase().includes(s.query.toLowerCase()));
 const tree=visible.map(function(sg){
  const expanded=sg.id===s.expanded;
  return '<button type="button" class="song-row '+(sg.id===s.song?'current':'')+'" data-song="'+H(sg.id)+'">'+
    (expanded?'▾':'▸')+' <span class="clip">'+H(sg.title)+'</span></button>'+
    (expanded?sg.charts.map(function(c){
      return '<button type="button" class="song-row chart '+(sg.id===s.song&&c.id===s.chart?'current':'')+
      '" data-chart="'+H(c.id)+'" data-set="'+H(sg.id)+'"><span class="clip">'+H(c.title)+'</span><span class="rate">'+c.difficulty.toFixed(1)+'</span></button>';
    }).join(''):'');
 }).join('');
 const history=M.runs();
 const results=history.length?history.map(function(run){
  return '<button class="result-row" type="button" data-run="'+H(run.id)+'"><strong>'+M.accuracy(run).toFixed(2)+'%</strong>'+
   '<time>'+H(M.datetime(run.datetime))+'</time><span class="spacer"></span>›</button>';
 }).join(''):'<div class="empty" style="font-size:14px">No results</div>';
 const left='<div class="library-left">'+panel('SONGS', '<div class="library-songs">'+
  '<input class="library-search" type="search" aria-label="Search songs" placeholder="Search" value="'+H(s.query)+'" data-field="query">'+
  '<div class="song-tree vertical-scroll">'+(tree||'<div class="empty">No songs</div>')+'</div></div>')+
  panel('RESULTS <span class="badge">'+history.length+'</span>',
  '<div class="results-list vertical-scroll">'+results+'</div>')+'</div>';
 const isVisible=visible.some(x=>x.id===s.song)&&!!chart;
 const center=isVisible?'<div class="box library-details">'+
  '<div class="artwork-placeholder">ARTWORK</div>'+
  '<div class="library-name">'+H(song.title)+'</div>'+
  '<div class="muted clip">'+H(song.artist)+' · '+song.bpm+' BPM · '+M.clock(song.duration)+'</div>'+
  '<div class="library-mini-row"><span class="library-chart-name">'+H(chart.title)+'</span><span class="rating-total">'+chart.difficulty.toFixed(1)+'</span></div>'+
  '<div class="library-main-actions">'+button('PLAY','play','primary')+button('EDIT','editor-open')+'</div>'+
  '<div class="library-sub-actions">'+button('Song Set','editor-set')+'</div>'+
  '<div class="spacer"></div>'+
  '<div class="library-mini-row"><span>Best</span><strong>'+((history.length)?Math.max.apply(null,history.map(M.accuracy)).toFixed(2)+'%':'—')+'</strong></div>'+
  '<div class="library-mini-row"><span>Notes</span><strong>'+chart.notes+'</strong></div>'+
  '</div>':'<div class="box"><div class="empty">No chart selected</div></div>';
 const labels=['Speed','Rhythm','Coordination','Reading','Stamina','Peak'];
 const breakdown=isVisible?panel('DIFFICULTY','<div class="library-breakdown">'+
  labels.map((label,i)=>'<div class="rating-line"><span>'+label+'</span><span class="rating-bar"><b style="width:'+Math.min(100,chart.dims[i]*10)+'%"></b></span><b>'+chart.dims[i].toFixed(1)+'</b></div>').join('')+
  '<div class="summary-kv"><span>Song offset</span><b>'+song.offset+' ms</b><span>Charts</span><b>'+song.charts.length+'</b></div></div>'):
  '<div class="box"></div>';
 return '<div class="view">'+bar('SONG LIBRARY',button('IMPORT','file')+button('RESCAN','rescan'))+
  '<div class="view-content no-scroll"><div class="library-layout">'+left+center+breakdown+'</div></div></div>';
}
function routePreview(seed,name,selected){
 const points=[
  '35,164 76,164 76,101 138,101 138,39 205,39 205,105 250,105',
  '38,167 38,115 99,115 144,74 207,74 207,129 254,129',
  '38,63 106,63 106,120 149,164 210,164 251,119'
 ];
 return '<button type="button" class="route-pane '+(selected?'active':'')+'" data-pane="'+seed+'" aria-label="Focus '+H(name)+'">'+
  '<span class="route-pane-title">'+H(name)+'</span>'+
  '<svg viewBox="0 0 290 200" aria-hidden="true"><path d="M0 50H290M0 100H290M0 150H290M50 0V200M100 0V200M150 0V200M200 0V200M250 0V200" fill="none" stroke="#dedede" stroke-width="1"/>'+
  '<polyline points="'+points[seed%3]+'" fill="none" stroke="#555" stroke-width="6" stroke-linejoin="round"/>'+
  '<circle cx="76" cy="164" r="10" fill="#333" stroke="#000" stroke-width="2"/></svg></button>';
}
const noteDirections=['↖','↑','↗','←','DEL','→','↙','↓','↘'];
function pressCount(direction){
 const combos={'↖':['↑','←'],'↗':['↑','→'],'↙':['↓','←'],'↘':['↓','→']};
 return s.notes.filter(n=>n.dir===direction||(combos[n.dir]||[]).includes(direction)).length;
}
function noteRuler(){
 const markers=s.notes.map(n=>'<span class="ruler-beat" title="'+H(n.dir)+'" style="left:'+(n.beat/12*94+3)+'%"></span>').join('');
 return '<div class="note-ruler"><div class="ruler-track" data-seek="note">'+markers+
  '<div class="ruler-cursor" style="left:'+(s.beat/12*94+3)+'%"></div></div>'+
  '<div class="ruler-controls">'+button('SNAP 1/'+s.snap,'snap')+
  button('ZOOM '+s.zoom+'%','zoom')+'</div></div>';
}
function transport(){
 const rates=s.editorTab==='route'?[100,75,50,25,10].map(n=>button(n+'%', 'rate-'+n,
  'small '+(s.practice===n?'selected':''),'aria-pressed="'+(s.practice===n)+'"')).join(''):'';
 return '<div class="editor-transport">'+button(s.editorPlaying?'PAUSE':'PLAY','transport-play')+
  button('RESTART','transport-restart')+(rates?'<div class="practice">'+rates+'</div>':'')+
  '<div class="clock">'+M.clock(s.songPosition*M.song().duration)+' / '+M.clock(M.song().duration)+'</div></div>';
}
function overview(){
 const x=Math.round(s.songPosition*100);
 const bpm=s.tempo.map(t=>{
   const pos=Math.min(100,Math.max(0,t.time/1000/Math.max(1,M.song().duration)*100));
   return '<span class="overview-bpm" style="left:'+pos+'%">'+t.bpm+' BPM</span>';
 });
 return '<div class="song-overview"><div class="song-overview-bar" data-seek="song">'+
  '<span class="overview-progress" style="width:'+x+'%"></span>'+
  bpm.join('')+'<span class="overview-cursor" style="left:'+x+'%"></span>'+
  '<span class="overview-end">'+M.clock(M.song().duration)+'</span></div></div>';
}
function route(){
 const tools='<div class="route-toolbars"><div class="toolrow">'+
  ['Copy','Cut','Paste','Undo','Redo'].map(x=>button(x.toUpperCase(),'tool-'+x.toLowerCase())).join('')+
  '</div><div class="toolrow">'+
  [['FLIP H','flip-h'],['FLIP V','flip-v'],['ROT −90','rot-ccw'],['ROT +90','rot-cw'],['COMPARE','compare'],['TEST','play']].map(x=>button(x[0],x[1])).join('')+'</div></div>';
 const previewNames=[s.editorLoaded?M.chart().title:'Untitled'].concat(s.compared.map(id=>
   M.song().charts.find(x=>x.id===id)).filter(Boolean).map(c=>c.title));
 const preview='<div class="route-preview">'+previewNames.map((name,i)=>
   routePreview(i,name,s.focusedPane===i)).join('')+'</div>';
 const pad='<div class="pad-3">'+noteDirections.map(dir=>
  '<button type="button" data-note="'+H(dir)+'">'+H(dir)+
   (dir==='DEL'?'':'<span class="count">'+pressCount(dir)+'</span>')+'</button>').join('')+'</div>';
 return '<div class="route-grid">'+
  '<div class="route-left">'+tools+preview+'</div>'+
  '<aside class="route-side"><div class="route-ms">'+button('TIME  '+Math.round(s.songPosition*M.song().duration*1000)+' ms','copy-time','small')+'</div>'+pad+
  '<div class="pair-control"><span class="pair-name">SPEED<b>'+s.editorSpeed+' px/s</b></span>'+
  button('−','speed-down','icon')+button('+','speed-up','icon')+'</div>'+
  '<div class="pair-control"><span class="pair-name">PATH<b>'+s.editorPath+' ms</b></span>'+
  button('−','path-down','icon')+button('+','path-up','icon')+'</div></aside></div>';
}
function setPage(){
 const song=M.song();
 const managed=s.editorLoaded;
 const actions=[
  ['OPEN FOLDER','file'],['BACKGROUND','background'],['REMOVE BG','remove-bg'],
  ['IMPORT','file'],['EXPORT','file'],['RESTORE','restore']
 ];
 const left='<section class="box set-meta"><h2>SONG SET</h2>'+
  '<div class="set-name">'+(managed?H(song.title):'New Song Set')+'</div>'+
  '<div class="clip">'+(managed?H(song.artist)+' · '+song.bpm+' BPM':'No audio')+'</div>'+
  '<div class="line"></div><div class="set-action-grid">'+
  actions.map(x=>button(x[0],x[1])).join('')+'</div>'+
  '<div class="set-footer-actions">'+button('DELETE CHART','delete-chart','danger')+
  button('DELETE SET','delete-set','danger')+'</div></section>';
 const items=managed?song.charts.map(c=>
  '<button type="button" class="difficulty-row '+(s.chart===c.id?'selected':'')+
  '" data-difficulty="'+H(c.id)+'"><span class="clip">'+H(c.title)+'</span><span class="badge">'+c.difficulty.toFixed(1)+'</span></button>').join(''):'';
 const right='<section class="box set-diffs"><div class="box-head">DIFFICULTIES</div>'+
  '<div class="diff-actions">'+button('NEW','difficulty-new')+button('DUPLICATE','difficulty-copy')+'</div>'+
  '<div class="difficulty-list">'+(items||'<div class="empty">No charts</div>')+'</div></section>';
 return '<div class="set-grid">'+left+right+'</div>';
}
function audioPage(){
 const left='<section class="box audio-meta"><h2>CHART</h2>'+
  field('Difficulty title','editorTitle',s.editorTitle,'text')+
  field('Song preview start · ms','previewStart',s.previewStart,'number')+
  '<div class="line"></div>'+
  '<div class="row"><strong>Difficulty</strong><span class="spacer"></span><strong>'+(s.editorLoaded?M.chart().difficulty.toFixed(1):'—')+'</strong></div></section>';
 const rows=s.tempo.map((t,i)=>'<div class="tempo-row">'+
  '<input aria-label="Offset '+(i+1)+'" type="number" data-tempo-time="'+i+'" value="'+t.time+'">'+
  '<input aria-label="BPM '+(i+1)+'" type="number" min="1" step=".1" data-tempo-bpm="'+i+'" value="'+t.bpm+'">'+
  (i?button('DELETE','tempo-delete-'+i,'small'):'<span class="badge">START</span>')+'</div>').join('');
 const right='<section class="box tempo-table"><div class="box-head">SONG TEMPO MAP</div>'+
  '<div class="tempo-header"><span>OFFSET · ms</span><span>BPM</span><span></span></div>'+
  '<div class="tempo-rows">'+rows+'</div>'+
  '<div class="tempo-footer">'+button('+ ADD BPM MARKER','tempo-add')+'</div></section>';
 return '<div class="audio-grid">'+left+right+'</div>';
}
function fieldPage(){
 const head=['↑','↗','→','↘','↓','↙','←','↖'][s.headingIndex];
 return '<div class="field-grid"><section class="box field-controls">'+
  '<h2>INITIAL HEADING</h2>'+
  '<div class="heading-picker">'+button('‹','heading-prev','icon')+
  button(head,'heading-next','heading-value')+button('›','heading-next','icon')+'</div>'+
  '<div class="line"></div>'+
  '<div class="row"><span>SKIN SPEED</span><span class="spacer"></span><strong>'+s.skin.baseSpeed+' px/s</strong></div>'+
  '<div class="row"><span>SKIN PREVIEW</span><span class="spacer"></span><strong>'+s.skin.lookahead+' ms</strong></div>'+
  '</section>'+
  '<section class="box field-stage"><div class="field-diagram"><svg viewBox="0 0 220 220" aria-label="Field preview">'+
  '<rect x="1" y="1" width="218" height="218" stroke="#999" fill="none" stroke-width="2"/>'+
  '<circle cx="110" cy="110" r="16" fill="#555"/>'+
  '<text x="110" y="64" font-size="33" font-family="Segoe UI Symbol, sans-serif" text-anchor="middle" fill="#333">'+head+'</text>'+
  '</svg></div><div class="field-stats"><strong>FIELD</strong><div>Initial direction</div><div style="font-size:28px">'+head+'</div></div></section></div>';
}
function editor(){
 const tabs=[['set','SET'],['route','ROUTE'],['audio','AUDIO'],['field','FIELD']];
 const content=s.editorTab==='set'?setPage():s.editorTab==='route'?route():s.editorTab==='audio'?audioPage():fieldPage();
 const title=s.editorTitle||'Untitled';
 return '<div class="editor"><header class="editor-head">'+button('← BACK','home','back')+
  '<div class="editor-title" title="'+H(title)+'">'+H(title)+'</div>'+
  '<div class="editor-tabs">'+tabs.map(t=>flag(t[1],s.editorTab===t[0],'editor-'+t[0])).join('')+'</div>'+
  '<span class="editor-save-flag">'+(s.editorDirty?'UNSAVED':'')+'</span></header>'+
  noteRuler()+
  '<div class="editor-work">'+content+'</div>'+
  overview()+transport()+'</div>';
}
function settingRow(name,control){
 return '<div class="settings-row"><span class="clip">'+name+'</span>'+control+'</div>';
}
function settingsLeft(){
 if(s.settingsTab==='game')return settingRow('Hit flash',flag(s.hitFlash?'ON':'OFF',s.hitFlash,'hitFlash'))+
   settingRow('Master volume','<div class="row"><input type="range" aria-label="Master volume" data-range="volume" min="0" max="100" value="'+s.volume+'"><strong id="volLabel">'+s.volume+'%</strong></div>');
 if(s.settingsTab==='display')return settingRow('VSync',flag(s.vsync?'ON':'OFF',s.vsync,'vsync'))+
  settingRow('FPS cap','<select data-field="fps">'+['30','60','90','120','144','165','240','Unlimited'].map(x=>
   '<option '+((s.fps===x)?'selected':'')+'>'+x+'</option>').join('')+'</select>')+
  settingRow('Show FPS',flag(s.showFps?'ON':'OFF',s.showFps,'showFps'))+
  settingRow('Fullscreen',flag(s.fullscreen?'ON':'OFF',s.fullscreen,'fullscreen'));
 return settingRow('Master volume','<div class="row"><input type="range" data-range="volume" min="0" max="100" value="'+s.volume+'"><strong>'+s.volume+'%</strong></div>')+
  settingRow('Global offset','<div class="row"><input aria-label="Global offset" data-field="globalOffset" type="number" value="'+s.globalOffset+'"><strong>ms</strong></div>')+
  settingRow('Calibration',button('CALIBRATE','calibrate'));
}
function settings(){
 const bindings=M.actions[s.bindGroup].map((a,i)=>
  '<div class="bind-row"><span class="clip">'+H(a[0])+'</span>'+
  '<button type="button" data-bind="'+i+'" class="'+(s.bindCapture===i?'capturing':'')+'">'+
   (s.bindCapture===i?'PRESS KEY…':H(a[1]))+'</button></div>').join('');
 return '<div class="view">'+bar('SETTINGS')+
  '<div class="view-content no-scroll"><div class="settings-layout">'+
  '<section class="box settings-col"><div class="settings-tabs">'+
  [['game','GAME'],['display','DISPLAY'],['audio','AUDIO']].map(t=>flag(t[1],s.settingsTab===t[0],'settings-'+t[0])).join('')+
  '</div><div class="settings-panel-body">'+settingsLeft()+'</div></section>'+
  '<section class="box settings-col"><div class="bind-groups">'+
  [['play','PLAY'],['menus','MENUS'],['editor','EDITOR']].map(t=>flag(t[1],s.bindGroup===t[0],'bind-'+t[0])).join('')+
  '</div><div class="bind-list">'+bindings+'</div></section>'+
  '</div></div></div>';
}
function skinParams(data){
 return '<div class="box skin-params">'+data.map(function(p){
  const control=p.type==='toggle'?flag(s.skin[p.key]?'ON':'OFF',s.skin[p.key],'skin-toggle-'+p.key):
   '<input type="number" min="'+p.min+'" max="'+p.max+'" step="'+p.step+'" data-skin-param="'+p.key+'" value="'+s.skin[p.key]+'">';
  return '<div class="skin-param"><span class="clip">'+H(p.title)+'</span>'+
   (p.type==='toggle'?'<span></span>':'<input type="range" min="'+p.min+'" max="'+p.max+
    '" step="'+p.step+'" data-skin-range="'+p.key+'" value="'+s.skin[p.key]+'">')+
   control+'</div>';
 }).join('')+'</div>';
}
function skinColors(){
 return '<div class="box skin-swatches">'+M.colors.map(([key,label])=>
  '<button class="skin-swatch '+(s.selectedColor===key?'selected':'')+'" type="button" data-color="'+key+'">'+
  '<span class="swatch" style="background:'+H(s.skin.colors[key])+'"></span>'+H(label)+'</button>').join('')+'</div>';
}
function skinProfiles(){
 return '<div class="box skin-params"><div class="skin-profiles">'+s.profiles.map(name=>
  '<button type="button" data-profile="'+H(name)+'" class="'+(s.selectedProfile===name?'selected':'')+'">'+H(name)+'</button>').join('')+
  '</div><div class="row" style="padding:10px">'+button('ACTIVATE','skin-activate')+
  button('STORE','skin-save')+button('DELETE','skin-delete','danger')+'</div></div>';
}
function skinPreview(){
 const isColor=s.skinTab==='colors',colorKey=s.selectedColor;
 return '<section class="skin-right">'+
  (isColor?'<div class="box"><div class="box-head">'+H(M.colors.find(x=>x[0]===colorKey)[1])+'</div>'+
   '<div class="skin-color-entry"><input type="color" data-color-input aria-label="Color" value="'+H(s.skin.colors[colorKey])+'">'+
   '<input type="text" data-field="skinHex" aria-label="Hex color" value="'+H(s.skin.colors[colorKey])+'"></div></div>':'')+
  '<div class="skin-preview">'+routeDiagram()+'</div>'+
  '<div class="skin-actions">'+button('LOAD','skin-load')+button('SAVE','skin-save')+
   button('SAVE AS','skin-saveas')+button('RESET','skin-reset')+'</div></section>';
}
function routeDiagram(){
 return '<svg viewBox="0 0 300 220" aria-hidden="true"><path d="M20 20H280V200H20Z" fill="none" stroke="#888" stroke-width="2"/>'+
  '<path d="M35 162H83V94H155V40H240V125" fill="none" stroke="#555" stroke-width="6" stroke-linejoin="round"/>'+
  '<circle cx="83" cy="162" r="9" fill="#444"/><circle cx="155" cy="94" r="5" fill="#777"/></svg>';
}
function skins(){
 const tabs=[['path','PATH'],['player','PLAYER'],['colors','COLORS'],['profiles','MY SKINS']];
 const body=s.skinTab==='path'?skinParams(M.skinPath):s.skinTab==='player'?skinParams(M.skinPlayer):
  s.skinTab==='colors'?skinColors():skinProfiles();
 return '<div class="view"><header class="skin-top">'+button('← BACK','home','back')+
  '<h1>SKINS</h1><input class="skin-name" aria-label="Skin name" data-field="skinName" value="'+H(s.skinName)+'"></header>'+
  '<nav class="skin-tabs">'+tabs.map(t=>flag(t[1],s.skinTab===t[0],'skin-'+t[0])).join('')+'</nav>'+
  '<div class="skin-main">'+body+skinPreview()+'</div></div>';
}
function results(){
 const run=M.result();
 const chart=M.chart(),song=M.song();
 if(!run)return '<div class="view">'+bar('RESULTS')+'<div class="empty">No result selected</div></div>';
 const rows=[['PERFECT',run.perfect],['GOOD',run.good],['BAD',run.bad],['MISS',run.miss]];
 return '<div class="view">'+bar('RESULTS')+
  '<div class="view-content no-scroll"><div class="results-layout">'+
  '<div class="box results-primary"><div><h2>'+H(song.title)+'</h2><div>'+H(chart.title)+'</div>'+
  '<div class="muted" style="margin-top:7px">'+H(M.datetime(run.datetime))+'</div></div>'+
  '<div><div class="result-accuracy">'+M.accuracy(run).toFixed(2)+'%</div><h2>ACCURACY</h2></div>'+
  '<div><h2>NOTES JUDGED</h2><strong style="font-size:28px">'+run.count+' / '+run.count+'</strong></div>'+
  '<div class="row">'+button('RETRY','play','primary')+button('EDIT CHART','editor-open')+'</div></div>'+
  '<div class="box results-judgments"><h2 style="margin-bottom:12px">JUDGMENTS</h2>'+
  '<div class="judgment-grid">'+rows.map(([label,num])=>
   '<div class="judgment-cell">'+label+'<strong>'+num+'</strong></div>').join('')+'</div>'+
  '<div class="line"></div><h3>OTHER RESULTS</h3>'+
  '<div class="result-history-mini">'+M.runs().filter(r=>r.id!==run.id).map(r=>
   '<button type="button" data-run="'+H(r.id)+'"><strong>'+M.accuracy(r).toFixed(2)+'%</strong>'+
   '<time>'+H(M.datetime(r.datetime))+'</time></button>').join('')+'</div></div></div></div></div>';
}
function play(){
 return '<div class="play-reference"><div class="play-field">'+routeDiagram()+'</div>'+
  '<div class="play-hud"><div>ACCURACY<br><strong>97.27%</strong></div>'+
  '<div>PERFECT<br><strong>445</strong></div><div>GOOD<br><strong>37</strong></div>'+
  '<div>BAD<br><strong>0</strong></div><div>MISS<br><strong>4</strong></div></div>'+
  (s.paused?'<div class="play-paused"><div class="play-paused-inner"><h1>PAUSED</h1>'+
   '<div class="row">'+button('RESUME','pause')+button('RETRY','retry')+
    button('LEAVE','leave')+'</div></div></div>':'')+'</div>';
}
function dialogs(){
 const dialogs=[
  ['QUIT','quit'],['UNSAVED CHART','unsaved'],['DELETE CHART','delete-chart'],
  ['DELETE SONG SET','delete-set'],['DELETE SKIN','skin-delete'],
  ['LEAVE LEVEL','leave'],['RETRY','retry'],['IMPORT','file'],['SAVE AS','saveas']
 ];
 return '<div class="view">'+bar('DIALOGS')+
  '<div class="dialog-gallery">'+dialogs.map(([label,kind])=>button(label,'dialog-'+kind)).join('')+'</div></div>';
}
function page(){
 if(s.view==='home')return home();
 if(s.view==='library')return library();
 if(s.view==='editor')return editor();
 if(s.view==='skins')return skins();
 if(s.view==='settings')return settings();
 if(s.view==='results')return results();
 if(s.view==='play')return play();
 if(s.view==='dialogs')return dialogs();
 return home();
}
window.LocusWirePages={page,home,library,editor,route,setPage,audioPage,fieldPage,
 noteRuler,overview,transport,settings,skins,results,play,dialogs,button};
})();
