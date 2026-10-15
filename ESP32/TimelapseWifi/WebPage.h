#pragma once
#include <Arduino.h>
static const char Web_Page[] PROGMEM = R"HTML(<!doctype html>
<html lang="ru">
<meta charset="utf-8"><meta name="viewport" content="width=device-width,initial-scale=1">
<title>Таймлапс-камера</title>
<style>
*{box-sizing:border-box}body{margin:0;background:#11191f;color:#e8eef1;font:16px system-ui,sans-serif}main{max-width:620px;margin:8vh auto;padding:28px}h1{font-size:30px;margin:8px 0 28px}label{display:block;margin:22px 0 8px}input,button{width:100%;padding:14px;border-radius:9px;border:1px solid #52616b;font:inherit}input{background:#1d2932;color:inherit}button{background:#9cdbc1;color:#10231c;font-weight:600;cursor:pointer;margin-top:18px}button.secondary{background:transparent;color:#9cdbc1}button:disabled{opacity:.5;cursor:wait}p{line-height:1.5;color:#afbec8}#status{min-height:48px;color:#e8eef1}.eyebrow{font-size:12px;letter-spacing:2px;color:#9cdbc1}small{display:block;margin-top:10px;color:#afbec8}
</style>
<main><div class="eyebrow">TIMELAPSE CAMERA</div><h1>Дата и время записи</h1>
<p>Дата и время появятся в левом нижнем углу записываемого видео.</p>
<button id="sync" type="button">Установить время телефона / компьютера</button>
<label for="date">Или задать вручную</label><input id="date" type="datetime-local" step="1" min="2000-01-01T00:00" max="2099-12-31T23:59:59">
<small id="zone"></small><button id="manual" class="secondary" type="button">Сохранить выбранное время</button>
<p id="status" role="status" aria-live="polite"></p></main>
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
</script></html>
)HTML";
