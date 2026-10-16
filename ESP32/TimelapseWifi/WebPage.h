#pragma once
#include <Arduino.h>
static const char Web_Page[] PROGMEM = R"HTML(<!doctype html>
<html lang="ru">
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Таймлапс-камера</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#11191f;color:#e8eef1;font:16px system-ui,sans-serif}main{max-width:620px;margin:8vh auto;padding:28px}h1{font-size:30px;margin:8px 0 28px}label{display:block;margin:22px 0 8px}input,button{width:100%;padding:14px;border-radius:9px;border:1px solid #52616b;font:inherit}input{background:#1d2932;color:inherit}button{background:#9cdbc1;color:#10231c;font-weight:600;cursor:pointer;margin-top:18px}button.secondary{background:transparent;color:#9cdbc1}button:disabled{opacity:.5;cursor:wait}p{line-height:1.5;color:#afbec8}#status{min-height:48px;color:#e8eef1}.eyebrow{font-size:12px;letter-spacing:2px;color:#9cdbc1}small{display:block;margin-top:10px;color:#afbec8}
section{margin-bottom:42px}.record{border:1px solid #52616b;border-radius:12px;padding:18px;margin:16px 0}.record h3{margin:0 0 8px}a.download{display:block;text-decoration:none;background:#9cdbc1;color:#10231c;padding:12px;border-radius:8px;margin-top:12px;font-weight:600}.parts a{background:#24352f;color:#c8e9db}.error{color:#ffb6a8}h2{font-size:23px}
</style>
<main><div class="eyebrow">TIMELAPSE CAMERA</div><h1>Записи камеры</h1>
<section><p>Скачайте готовый MP4 или объедините всю сессию в одно видео. Исходные фрагменты останутся на карте.</p>
<button id="refresh" type="button">Обновить список</button><p id="filesStatus" role="status"></p>
<div id="recordings"></div><button id="more" class="secondary" type="button" hidden>Следующие записи</button>
<small>Во время скачивания оставьте камеру в режиме Wi-Fi. Ход загрузки отображается в браузере.</small></section>
<section><h2>Дата и время записи</h2>
<p>Дата и время появятся в левом нижнем углу записываемого видео.</p>
<button id="sync" type="button">Установить время телефона / компьютера</button>
<label for="date">Или задать вручную</label><input id="date" type="datetime-local" step="1" min="2000-01-01T00:00" max="2099-12-31T23:59:59">
<small id="zone"></small><button id="manual" class="secondary" type="button">Сохранить выбранное время</button>
<p id="status" role="status" aria-live="polite"></p></section></main>
<script>
const input=document.getElementById('date'),status=document.getElementById('status');
const buttons=[document.getElementById('sync'),document.getElementById('manual')];
const pad=n=>String(n).padStart(2,'0');
const now=new Date();input.value=`${now.getFullYear()}-${pad(now.getMonth()+1)}-${pad(now.getDate())}T${pad(now.getHours())}:${pad(now.getMinutes())}:${pad(now.getSeconds())}`;
document.getElementById('zone').textContent='Часовой пояс: '+Intl.DateTimeFormat().resolvedOptions().timeZone;
async function save(date){
 if(!Number.isFinite(date.getTime())||date.getFullYear()<2000||date.getFullYear()>2099){status.textContent='Укажите дату с 2000 по 2099 год.';return;}
 buttons.forEach(b=>b.disabled=true);status.textContent='Передаю время в камеру…';
 const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),9000);
 try{const response=await fetch('/api/time',{method:'POST',body:new URLSearchParams({utc:Math.floor(date.getTime()/1000),offset:-date.getTimezoneOffset()}),signal:controller.signal});
 if(!response.ok)throw new Error();const result=await response.json();if(!result.ok)throw new Error();
 status.textContent='Камера подтвердила установку времени.';
 }catch{status.textContent='Нет подтверждения от камеры. Проверьте питание и USB-соединение со STM32.';}
 finally{clearTimeout(timer);buttons.forEach(b=>b.disabled=false);}
}
buttons[0].onclick=()=>save(new Date());buttons[1].onclick=()=>save(new Date(input.value));
const recordings=document.getElementById('recordings'),filesStatus=document.getElementById('filesStatus');
const refresh=document.getElementById('refresh'),more=document.getElementById('more');let cursor=0;
const bytes=n=>n>=1073741824?(n/1073741824).toFixed(2)+' ГиБ':(n/1048576).toFixed(1)+' МиБ';
const duration=n=>`${Math.floor(n/60)}:${pad(Math.floor(n%60))}`;
async function get(url){const controller=new AbortController(),timer=setTimeout(()=>controller.abort(),15000);try{const r=await fetch(url,{signal:controller.signal});if(!r.ok)throw new Error();return await r.json();}finally{clearTimeout(timer);}}
function downloadLink(session,part,label){const a=document.createElement('a');a.className='download';a.href=`/download?session=${session}&part=${part}`;a.textContent=label;a.setAttribute('download','');return a;}
function card(s){
 const box=document.createElement('article');box.className='record';const title=document.createElement('h3');title.textContent='Запись R'+String(s.id).padStart(7,'0');box.append(title);
 const info=document.createElement('p');info.textContent=s.error?'Не удалось объединить сессию. Проверьте доступные фрагменты.':`${s.width}×${s.height} · ${duration(s.frames/s.fps)} · ${s.fragments} фрагм. · ${bytes(s.size)}`;box.append(info);
 if(!s.error)box.append(downloadLink(s.id,0,'Объединить и скачать '+(s.format==='MP4'?'MP4':'AVI')));
 const parts=document.createElement('div');parts.className='parts';const button=document.createElement('button');button.className='secondary';button.textContent='Показать фрагменты';box.append(parts,button);
 let after=0;
 button.onclick=async()=>{button.disabled=true;try{for(let i=0;i<20;i++){const f=await get(`/api/fragments?session=${s.id}&after=${after}`);if(!f){button.hidden=true;return;}if(f.id<=after)throw new Error();after=f.id;parts.append(downloadLink(s.id,f.id,`Фрагмент ${f.id} · ${duration(f.frames/f.fps)} · ${bytes(f.size)}`));}button.textContent='Следующие фрагменты';}catch{button.textContent='Повторить загрузку фрагментов';}finally{button.disabled=false;}};
 recordings.append(box);
}
async function load(reset){
 refresh.disabled=more.disabled=true;filesStatus.textContent='Читаю карту камеры…';
 if(reset){cursor=0;recordings.replaceChildren();more.hidden=true;}
 try{for(let i=0;i<20;i++){const s=await get('/api/recordings?after='+cursor);if(!s){more.hidden=true;filesStatus.textContent=cursor?'':'Готовых записей пока нет.';return;}if(s.id<=cursor)throw new Error();cursor=s.id;card(s);}more.hidden=false;filesStatus.textContent='';}
 catch{filesStatus.textContent='Не удалось прочитать записи. Проверьте карту и USB-соединение с камерой.';more.hidden=!cursor;}
 finally{refresh.disabled=more.disabled=false;}
}
refresh.onclick=()=>load(true);more.onclick=()=>load(false);load(true);
</script></html>
)HTML";
