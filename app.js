const GEOMETRIES={hs:{frontHash:28,backHash:56,backSideline:84},college:{frontHash:21.33,backHash:64,backSideline:85.33},nfl:{frontHash:23.56,backHash:61.78,backSideline:85.33}};
const EXCLUDED_PERFORMERS=new Set(["T07","T14","T29"]);
let source,results=[],instrumentStats=[],selected=null;
const $=s=>document.querySelector(s);
const fmt=n=>n.toLocaleString(undefined,{maximumFractionDigits:1,minimumFractionDigits:1});

function xy(set,geometry){
  const l=set.lateral,v=set.vertical;
  const base=(50-l.yardLine)*8/5*(l.side===1?-1:1);
  let x=base;
  if(l.relation!=="on"){
    const outward=l.relation==="outside"?-1:1;
    x+=l.offset*outward*(l.side===1?1:-1);
  }
  const landmarks={"front sideline":0,"front hash":geometry.frontHash,"back hash":geometry.backHash,"back sideline":geometry.backSideline};
  let y=landmarks[v.landmark];
  if(v.relation==="in front of")y-=v.offset;
  if(v.relation==="behind")y+=v.offset;
  return{x,y};
}

function calculate(performer,part,geometry){
  const all=performer.sets;
  const filtered=part==="all"?all:all.filter(s=>String(s.part)===part);
  const moves=[];
  for(let i=1;i<filtered.length;i++){
    const a=xy(filtered[i-1],geometry),b=xy(filtered[i],geometry);
    const steps=Math.hypot(b.x-a.x,b.y-a.y),yards=steps*5/8;
    moves.push({from:filtered[i-1],to:filtered[i],steps,yards});
  }
  const yards=moves.reduce((sum,m)=>sum+m.yards,0);
  const active=moves.filter(m=>m.yards>.01);
  return{...performer,filtered,moves,yards,steps:yards*8/5,activeMoves:active.length,longest:active.length?Math.max(...active.map(m=>m.yards)):0};
}

function recalculate(){
  const part=$("#partFilter").value,geometry=GEOMETRIES[$("#geometry").value];
  results=source.performers.map(p=>calculate(p,part,geometry)).sort((a,b)=>b.yards-a.yards);
  instrumentStats=instrumentAverages(results);
  selected=results.find(p=>p.label===selected?.label)||results[0];
  renderInstrumentChart();
  renderRoster();
  renderDetail(selected);
}

function instrumentAverages(performers){
  const groups=new Map();
  performers.filter(p=>p.filtered.length>0).forEach(p=>{const group=groups.get(p.instrument)||{instrument:p.instrument,total:0,count:0};group.total+=p.yards;group.count++;groups.set(p.instrument,group)});
  return [...groups.values()].map(g=>({...g,average:g.total/g.count})).sort((a,b)=>b.average-a.average);
}

function renderInstrumentChart(){
  const top=instrumentStats,max=top[0]?.average||1;
  $("#instrumentChart").innerHTML=top.map(g=>`<div class="instrument-column" title="${g.instrument}: ${fmt(g.average)} average yards across ${g.count} performers"><span class="instrument-value">${fmt(g.average)} yd</span><div class="instrument-bar-wrap"><div class="instrument-bar" style="height:${g.average/max*100}%"></div></div><div class="instrument-label">${g.instrument}<small class="instrument-members">${g.count} performer${g.count===1?"":"s"}</small></div></div>`).join("");
}

function renderRoster(){
  const query=$("#search").value.trim().toLowerCase();
  const shown=results.filter(p=>(p.label+" "+p.instrument+" "+p.symbol).toLowerCase().includes(query));
  $("#roster").innerHTML=shown.map((p,i)=>`<div class="roster-row ${p.label===selected?.label?"active":""}" data-label="${p.label}" role="option" tabindex="0"><div class="performer-id"><span class="rank">${String(i+1).padStart(2,"0")}</span><span class="avatar">${p.symbol}</span><span class="name"><strong>${p.label}</strong><small>${p.instrument}</small></span></div><div class="distance-cell"><strong>${fmt(p.yards)}</strong><small>YARDS · ${fmt(p.steps)} STEPS</small></div></div>`).join("")||'<div class="empty-state">No performers match that search.</div>';
  document.querySelectorAll(".roster-row").forEach(row=>{const choose=()=>{selected=results.find(p=>p.label===row.dataset.label);renderRoster();renderDetail(selected)};row.onclick=choose;row.onkeydown=e=>{if(e.key==="Enter"||e.key===" ")choose()}});
}

function renderDetail(p){
  if(!p)return;
  const parts=[1,2,3,4].map(part=>{
    const value=calculate(source.performers.find(x=>x.label===p.label),String(part),GEOMETRIES[$("#geometry").value]).yards;
    return{part,value};
  });
  const max=Math.max(...parts.map(x=>x.value),1);
  const topMoves=[...p.moves].sort((a,b)=>b.yards-a.yards).slice(0,5);
  const instrumentAverage=instrumentStats.find(g=>g.instrument===p.instrument)?.average||0;
  $("#detail").innerHTML=`<div class="detail-top"><span class="detail-badge">PERFORMER PROFILE</span><div class="detail-name"><span class="avatar">${p.symbol}</span><div><h2>${p.label}</h2><p>${p.instrument} · ${p.filtered.length} coordinates</p></div></div><div class="big-distance"><span>${fmt(p.yards)}</span><small>YARDS</small></div></div><div class="summary-grid"><div><strong>${fmt(p.steps)}</strong><span>8-TO-5 STEPS</span></div><div><strong>${p.activeMoves}</strong><span>ACTIVE MOVES</span></div><div><strong>${fmt(p.longest)}</strong><span>LONGEST YDS</span></div><div><strong>${fmt(instrumentAverage)}</strong><span>INSTRUMENT AVG YDS</span></div></div><div class="detail-body"><h3>Distance by production part</h3>${parts.map(x=>`<div class="part-bar"><div class="part-bar-head"><span>PART ${x.part}</span><strong>${fmt(x.value)} YD</strong></div><div class="track"><div class="fill" style="width:${x.value/max*100}%"></div></div></div>`).join("")}<div class="moves-title"><h3>Longest moves</h3><span>STRAIGHT LINE</span></div><div class="move-list">${topMoves.map(m=>`<div class="move"><span class="move-set">${m.from.set}→${m.to.set}</span><span class="move-info"><strong>${m.to.counts} counts · ${fmt(m.steps/m.to.counts)} step/count</strong><span>${m.to.coordinate}</span></span><span class="move-dist">${fmt(m.yards)} yd</span></div>`).join("")||'<div class="empty-state">No movement in this selection.</div>'}</div></div>`;
}

function exportCsv(){
  const rows=[["Rank","Label","Instrument","Symbol","Distance (yards)","8-to-5 steps","Active moves","Longest move (yards)"],...results.map((p,i)=>[i+1,p.label,p.instrument,p.symbol,p.yards.toFixed(2),p.steps.toFixed(2),p.activeMoves,p.longest.toFixed(2)])];
  const csv=rows.map(r=>r.map(v=>`"${String(v).replaceAll('"','""')}"`).join(",")).join("\n");
  const a=document.createElement("a");a.href=URL.createObjectURL(new Blob([csv],{type:"text/csv"}));a.download="stepwise-marching-distance.csv";a.click();URL.revokeObjectURL(a.href);
}

fetch("data/coordinates.json").then(r=>r.json()).then(data=>{source={...data,performers:data.performers.filter(p=>!EXCLUDED_PERFORMERS.has(p.label))};$("#showName").textContent=source.show;$("#performerCount").textContent=source.performers.length;recalculate()}).catch(()=>{$("#roster").innerHTML='<div class="empty-state">Could not load coordinate data. Serve this folder with a local web server.</div>'});
$("#search").addEventListener("input",()=>{const q=$("#search").value.trim().toLowerCase();const matches=results.filter(p=>(p.label+" "+p.instrument+" "+p.symbol).toLowerCase().includes(q));if(matches.length===1){selected=matches[0];renderDetail(selected)}renderRoster()});$("#partFilter").addEventListener("change",recalculate);$("#geometry").addEventListener("change",recalculate);$("#exportBtn").addEventListener("click",exportCsv);
