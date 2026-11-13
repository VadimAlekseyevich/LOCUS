/* LOCUS 960×540 wireframe: sample state and actual feature inventory. */
(function () {
  'use strict';
  const songs = [
    { id:'demo',title:'Built-in demo',artist:'LOCUS',mapper:'LOCUS',tags:['demo'],duration:32,bpm:120,offset:0,
      charts:[{id:'original',title:'Original',difficulty:2.5,notes:32,
        dims:[1.4,2.8,2.0,2.2,1.6,3.1]}]},
    { id:'aurora',title:'Aurora Drive',artist:'NOVA',mapper:'mapper_01',tags:['electronic','fast'],duration:192,bpm:174,offset:-15,
      charts:[
        {id:'horizon',title:'Horizon',difficulty:5.8,notes:486,
          dims:[5.4,5.9,6.1,5.7,5.3,6.6]},
        {id:'parallax',title:'Parallax',difficulty:8.3,notes:912,
          dims:[8.2,8.6,8.4,8.1,7.9,9.2]}
      ]},
    { id:'afterglow',title:'Afterglow',artist:'ECHO / WAV',mapper:'mapper_02',tags:['ambient','rhythm'],duration:246,bpm:156,offset:24,
      charts:[
        {id:'glimmer',title:'Glimmer',difficulty:3.9,notes:320,
          dims:[3.8,4.1,3.6,4.4,3.2,4.8]},
        {id:'refraction',title:'Refraction',difficulty:6.6,notes:655,
          dims:[6.3,7.0,6.4,6.8,6.1,7.4]}
      ]}
  ];
  const actions = {
    play:[
      ['Move up','J'],['Move right','K'],['Move down','F'],['Move left','D'],
      ['Pause / menu','Esc'],['Retry','R'],['Home','H'],
      ['Practice 100%','1'],['Practice 75%','2'],['Practice 50%','3'],
      ['Practice 25%','4'],['Practice 10%','5'],
      ['Speed up',']'],['Speed down','['],['Preview up','.'],['Preview down',',']
    ],
    menus:[
      ['Back','Esc'],['Open library','P'],['Open editor','E'],
      ['Open skins','K'],['Open settings','S'],['Fullscreen','F11'],
      ['Rescan','F5'],['Choose music','O'],['Choose chart','C'],
      ['Preview / pause','Space'],['Replay','R']
    ],
    editor:[
      ['Save','Ctrl+S'],['Save as','Ctrl+Shift+S'],['Open','Ctrl+O'],
      ['New','Ctrl+N'],['Test','T'],['Preview / pause','Space'],
      ['Restart','R'],['Copy','Ctrl+C'],['Cut','Ctrl+X'],
      ['Paste','Ctrl+V'],['Undo','Ctrl+Z'],['Redo','Ctrl+Y'],
      ['Mirror H','I'],['Mirror V','Shift+I'],
      ['Zoom +','+'],['Zoom −','-'],['Snap +','.'],['Snap −',',']
    ]
  };
  const skinPath=[
    {key:'lookahead',title:'Preview time',value:800,min:50,max:10000,step:50,unit:'ms'},
    {key:'pathWidth',title:'Path width',value:5,min:.25,max:40,step:.25,unit:'px'},
    {key:'outlineScale',title:'Outline scale',value:2,min:1,max:8,step:.25,unit:''},
    {key:'fadePower',title:'Fade curve',value:1.5,min:.1,max:8,step:.1,unit:''},
    {key:'pathOpacity',title:'Path opacity',value:1,min:0,max:1,step:.05,unit:''},
    {key:'junctionRadius',title:'Turn size',value:10,min:1,max:64,step:1,unit:'px'},
    {key:'junctionGlow',title:'Turn glow',value:true,type:'toggle'},
    {key:'reverseCapScale',title:'Reverse cap',value:2,min:0,max:12,step:.25,unit:''}
  ];
  const skinPlayer=[
    {key:'playerRadius',title:'Player size',value:11,min:1,max:80,step:1,unit:'px'},
    {key:'baseSpeed',title:'Speed',value:460,min:20,max:2000,step:10,unit:'px/s'},
    {key:'playerGlow',title:'Player glow',value:true,type:'toggle'},
    {key:'hitFlashScale',title:'Hit flash size',value:1,min:.1,max:8,step:.1,unit:''},
    {key:'showHud',title:'Show HUD',value:true,type:'toggle'},
    {key:'cameraLagMs',title:'Camera lag',value:100,min:0,max:1500,step:25,unit:'ms'}
  ];
  const colors=[
    ['background','Background'],['trajectory','Trajectory'],['outline','Outline'],
    ['junction','Junction'],['player','Player'],['playerCore','Player core'],
    ['hudBackground','HUD background'],['hudText','HUD text'],
    ['hitFlash','Hit flash'],['fieldBorder','Field border']
  ];
  const seedRuns={
    'aurora/horizon':[
      {id:'r1',datetime:'2026-11-12T14:42:00',count:486,perfect:445,good:37,bad:0,miss:4},
      {id:'r2',datetime:'2026-11-12T13:18:00',count:486,perfect:430,good:40,bad:8,miss:8}
    ],
    'aurora/parallax':[
      {id:'r3',datetime:'2026-11-12T14:03:00',count:912,perfect:812,good:68,bad:20,miss:12}
    ]
  };
  const seedNotes=[
    {beat:1,dir:'↑'}, {beat:2,dir:'↗'}, {beat:3,dir:'→'}, {beat:4,dir:'↓'},
    {beat:5,dir:'↙'}, {beat:6,dir:'←'}, {beat:7,dir:'↖'}, {beat:8,dir:'↑'},
    {beat:9,dir:'↘'}
  ];
  const deep=(x)=>JSON.parse(JSON.stringify(x));
  function initial(){
    return {
      view:'home',song:'aurora',chart:'horizon',expanded:'aurora',query:'',
      editorTab:'route',editorLoaded:false,editorTitle:'Untitled',
      setMeta:{title:'',artist:'',mapper:'',tags:''},
      editorDirty:false,beat:4,notes:deep(seedNotes),undo:[],redo:[],clipboard:null,
      zoom:100,snap:4,practice:100,editorPlaying:false,compared:[],
      focusedPane:0,editorSpeed:135,editorPath:800,songPosition:.32,
      background:false,tempo:[{time:-15,bpm:174}],previewStart:12500,
      headingIndex:2,
      settingsTab:'display',bindGroup:'play',bindCapture:null,
      volume:70,vsync:true,fps:'240',showFps:false,
      fullscreen:false,globalOffset:0,calibration:0,
      skinTab:'path',skinName:'Default',skinDirty:false,
      skin:{...Object.fromEntries(skinPath.concat(skinPlayer).map(x=>[x.key,x.value])),
        colors:Object.fromEntries(colors.map((x,i)=>[x[0],['#1e2538','#a9d8ef','#b4a3f2','#c3b6f6','#b4a3f2','#edf2fa','#292f45','#eaf0ff','#c3b6f6','#798ba9'][i]]))},
      selectedColor:'trajectory',selectedProfile:'Default',
      profiles:['Default','Nightglass'],
      results:deep(seedRuns),resultId:'r1',runCounter:4,
      paused:false,dialog:null,notice:''
    };
  }
  const state=initial();
  function escape(input){
    return String(input==null?'':input).replace(/[&<>"']/g,c=>({'&':'&amp;','<':'&lt;','>':'&gt;','"':'&quot;',"'":'&#39;'}[c]));
  }
  const song=()=>songs.find(x=>x.id===state.song)||songs[0];
  const chart=()=>song().charts.find(x=>x.id===state.chart)||song().charts[0]||null;
  const chartKey=()=>state.song+'/'+state.chart;
  const runs=()=>state.results[chartKey()]||[];
  const result=()=>runs().find(x=>x.id===state.resultId)||null;
  const accuracy=(r)=>100*(r.perfect+.75*r.good+.5*r.bad)/Math.max(1,r.count);
  const datetime=(iso)=>new Date(iso).toLocaleString('en-GB',{day:'2-digit',month:'short',year:'numeric',hour:'2-digit',minute:'2-digit',hour12:false});
  const clock=(sec)=>Math.floor(sec/60)+':'+String(Math.floor(sec%60)).padStart(2,'0');
  const selectedSkinSetting=(key)=>skinPath.concat(skinPlayer).find(x=>x.key===key);
  window.LocusWire={songs,actions,skinPath,skinPlayer,colors,state,initial,deep,escape,
    song,chart,chartKey,runs,result,accuracy,datetime,clock,selectedSkinSetting};
})();
