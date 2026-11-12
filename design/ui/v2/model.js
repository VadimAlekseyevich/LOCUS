/* LOCUS UI v2 — deterministic in-browser demo state. No audio, files or game data. */
(function () {
  'use strict';
  const library = [
    {
      id:'demo', title:'Built-in demo', artist:'LOCUS', duration:'0:32', bpm:120, offset:0,
      charts:[{id:'original',title:'Original',rating:2.5,speed:1.4,rhythm:2.8,keys:2.0,reading:2.2,stamina:1.6,peak:3.1,notes:32}]
    },
    {
      id:'aurora', title:'Aurora Drive', artist:'NOVA', duration:'3:12', bpm:174, offset:-15,
      charts:[
        {id:'horizon',title:'Horizon',rating:5.8,speed:5.4,rhythm:5.9,keys:6.1,reading:5.7,stamina:5.3,peak:6.6,notes:486},
        {id:'parallax',title:'Parallax',rating:8.3,speed:8.2,rhythm:8.6,keys:8.4,reading:8.1,stamina:7.9,peak:9.2,notes:912}
      ]
    },
    {
      id:'afterglow', title:'Afterglow', artist:'ECHO / WAV', duration:'4:06', bpm:156, offset:24,
      charts:[
        {id:'glimmer',title:'Glimmer',rating:3.9,speed:3.8,rhythm:4.1,keys:3.6,reading:4.4,stamina:3.2,peak:4.8,notes:320},
        {id:'refraction',title:'Refraction',rating:6.6,speed:6.3,rhythm:7.0,keys:6.4,reading:6.8,stamina:6.1,peak:7.4,notes:655}
      ]
    }
  ];
  const bindings = {
    play:[
      ['Move up','J'],['Move right','K'],['Move down','F'],['Move left','D'],
      ['Pause / menu','Esc'],['Retry','R'],['Leave level','H'],
      ['Practice 100%','1'],['Practice 75%','2'],['Practice 50%','3'],
      ['Practice 25%','4'],['Practice 10%','5'],
      ['Speed up',']'],['Speed down','['],['Preview up','.'],['Preview down',',']
    ],
    menus:[
      ['Back','Esc'],['Open library','P'],['Open editor','E'],['Open skins','K'],
      ['Open settings','S'],['Fullscreen','F11'],['Refresh library','F5'],
      ['Choose music','O'],['Choose chart','C'],['Listen pause','Space'],
      ['Listen replay','R']
    ],
    editor:[
      ['Save','Ctrl+S'],['Save as','Ctrl+Shift+S'],['Open','Ctrl+O'],
      ['New chart','Ctrl+N'],['Test chart','T'],['Preview / pause','Space'],
      ['Restart preview','R'],['Copy','Ctrl+C'],['Cut','Ctrl+X'],['Paste','Ctrl+V'],
      ['Undo','Ctrl+Z'],['Redo','Ctrl+Y'],['Mirror H','I'],['Mirror V','Shift+I'],
      ['Zoom in','+'],['Zoom out','-'],['Snap finer','.'],['Snap coarser',',']
    ]
  };
  const initialNotes = [
    {beat:1,direction:'↑'},{beat:2,direction:'↗'},{beat:3,direction:'→'},
    {beat:4,direction:'↓'},{beat:5,direction:'↙'},{beat:6,direction:'←'},
    {beat:7,direction:'↖'},{beat:8,direction:'↑'},{beat:9,direction:'↘'}
  ];
  function defaults(){
    return {
      page:'home',lastPage:'home',song:'aurora',chart:'horizon',expanded:['aurora'],
      search:'',libraryEmpty:false,editorTab:'route',skinTab:'path',settingsTab:'game',
      bindingGroup:'play',bindingSearch:'',capture:null,
      editorDirty:false,chartName:'Horizon',tempo:[{time:-15,bpm:174}],previewMs:12500,
      selectedBeat:4,selectedDirection:'↓',notes:initialNotes.map(n=>({beat:n.beat,direction:n.direction})),
      clipboard:null,undo:[],redo:[],compare:[],focusedPane:0,zoom:100,snap:'1/4',
      practice:100,previewPlaying:false,previewSpeed:135,previewLength:800,
      fieldWidth:4,fieldHeight:4,startX:1,startY:1,heading:'→',
      volume:70,hitFlash:true,vsync:true,fps:'240',showFps:false,fullscreen:false,offset:0,
      listenProgress:33,listenPlaying:false,playPaused:false,
      modal:null,modalReturn:null,calibrationOpen:false,calibrationCount:0,calibrationComplete:false,selectedColor:'trajectory',
      skinName:'Default',skinDirty:false,skinProfile:'default',
      profiles:[{id:'default',name:'Default',subtitle:'Current design'},{id:'night',name:'Nightglass',subtitle:'Saved profile'}],
      skin:{baseSpeed:460,lookahead:800,pathWidth:5,outlineScale:2,fadePower:1.5,pathOpacity:1,
        junctionRadius:10,junctionGlow:false,reverseCapScale:2,playerRadius:11,playerGlow:true,
        hitFlashScale:1,cameraLagMs:100,showHud:true,colors:{
        background:'#151b24',trajectory:'#b3d9e7',outline:'#edf0f5',junction:'#a795dd',
        player:'#ef74ac',playerCore:'#ffffff',hudBackground:'#232932',hudText:'#f0eced',
        hitFlash:'#9adfcb',fieldBorder:'#667989'
      }},
      changedSet:false
    };
  }
  const state=defaults();
  function H(value){
    return String(value==null?'':value).replace(/[&<>"']/g,function(c){
      return {'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c];
    });
  }
  function selectedSong(){return library.find(x=>x.id===state.song)||library[1];}
  function selectedChart(){
    const song=selectedSong();
    return song.charts.find(x=>x.id===state.chart)||song.charts[0]||null;
  }
  function pointMax(n,min,max){return Math.max(min,Math.min(max,n));}
  function field(name,value,note){
    return '<label class="input-label">'+H(name)+'<input type="text" value="'+H(value)+'"'+(note?' placeholder="'+H(note)+'"':'')+'></label>';
  }
  function segment(id,selected,label,attribute){
    return '<button type="button" class="seg'+(selected===id?' active':'')+'" '+attribute+'="'+H(id)+'">'+H(label)+'</button>';
  }
  function shortButton(title,act,variant){
    return '<button type="button" class="'+(variant||'ghost')+' slim" data-action="'+H(act)+'">'+H(title)+'</button>';
  }
  function routeSvg(seed,color){
    const c=color||state.skin.colors;
    const choices=[
      'M40 170 L80 170 L80 105 L150 105 L150 55 L212 55 L212 113 L274 113',
      'M45 183 L45 125 L111 125 L158 80 L218 80 L218 133 L282 133',
      'M42 75 L110 75 L110 122 L155 168 L222 168 L260 120 L288 120'
    ];
    const d=choices[seed%choices.length],markers=seed%3===1
      ?[[45,125],[111,125],[158,80],[218,133]]
      :seed%3===2?[[110,75],[155,168],[260,120]]:[[80,170],[80,105],[150,105],[212,55],[212,113]];
    return '<svg viewBox="0 0 330 230" preserveAspectRatio="xMidYMid meet" role="img" aria-label="Representative route preview">'+
      '<defs><pattern id="g'+seed+'" width="22" height="22" patternUnits="userSpaceOnUse"><path d="M22 0H0V22" fill="none" stroke="'+H(c.fieldBorder)+'" stroke-width=".6" opacity=".47"/></pattern></defs>'+
      '<rect width="330" height="230" fill="'+H(c.background)+'"/>'+
      '<rect width="330" height="230" fill="url(#g'+seed+')"/>'+
      '<path d="'+d+'" stroke="'+H(c.trajectory)+'" stroke-width="'+Math.max(2,state.skin.pathWidth+2)+'" stroke-linejoin="round" stroke-linecap="round" fill="none"/>'+
      markers.map(function(p,i){return '<circle cx="'+p[0]+'" cy="'+p[1]+'" r="'+(i===0?'6':'4')+'" fill="'+(i===0?H(c.player):H(c.outline))+'" stroke="'+H(c.background)+'" stroke-width="2"/>';}).join('')+
      '<circle cx="80" cy="170" r="8" fill="'+H(c.player)+'" stroke="'+H(c.outline)+'" stroke-width="2"/>'+
      '</svg>';
  }
  window.LocusDemo={state:state,defaults:defaults,library:library,bindings:bindings,H:H,
    selectedSong:selectedSong,selectedChart:selectedChart,pointMax:pointMax,
    field:field,segment:segment,shortButton:shortButton,routeSvg:routeSvg};
})();
