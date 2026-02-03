#include "web_server.h"
#include "config.h"
#include "state.h"
#include "storage.h"
#include "process.h"
#include "outputs.h"
#include <WiFi.h>
#include <Update.h>
#include "FS.h"
#include <SD.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>

// =================================================================
// Szablon HTML dla GŁÓWNEJ STRONY "/"
// =================================================================
static const char HTML_TEMPLATE_MAIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><title>Wedzarnia</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style> 
    body{font-family:sans-serif;background:#111;color:#eee;padding:10px;} 
    h2,h3{margin-top:20px; border-bottom: 1px solid #333; padding-bottom: 5px;} 
    button{margin:4px;padding:8px 12px;background:#333;color:#fff;border:1px solid #555;border-radius:4px; cursor: pointer;} button:hover{background:#555;} 
    input,select{padding:6px;margin:4px;background:#222;color:#eee;border:1px solid #444;border-radius:4px; width: 95%; box-sizing: border-box;} 
    .status, .timer, .section{background:#222;padding:10px;border-radius:8px;margin-bottom:10px;} 
    .btn-start{background:#1a5f1a;} .btn-start:hover{background:#2a8f2a;} 
    .btn-stop{background:#8b0000;} .btn-stop:hover{background:#cd0000;} 
    #timer-section { display: none; } 
    #timer-display { font-size: 1.5em; color: #4CAF50; font-weight: bold; } 
    #countdown-timer-display { font-size: 1.5em; color: #FF9800; font-weight: bold; } 
    #step-name-display { color: #aaa; margin-bottom: 5px; } 
    #active-profile-info {font-size: 0.8em; color: #888; margin-top: 5px;} 
    #debug-output { margin-top: 15px; border: 1px dashed red; padding: 10px; font-family: monospace; color: red; } 
</style>
</head><body>
<h2>🔥 Wedzarnia v3.3 by wojtek </h2>
<div class="status" id="status">Ładowanie...</div>
<div class="timer" id="timer-section"> <div id="step-name-display"></div> <div> <span title="Czas, który upłynął">🕒 Upłynęło: </span><span id="timer-display">--:--:--</span> <span id="countdown-timer-section" style="margin-left: 15px;"> <span title="Czas pozostały do końca kroku">⏳ Pozostało: </span><span id="countdown-timer-display">--:--:--</span> </span> <button onclick="resetTimer()" style="float: right;">Resetuj Czas</button> </div> </div>
<div class="section"> <h3>Sterowanie Główne</h3> <div> <button class="btn-start" onclick="fetch('/mode/manual').then(fetchStatus)">MANUAL</button> <button class="btn-start" onclick="fetch('/auto/start').then(fetchStatus)">AUTO START</button> <button class="btn-stop" onclick="fetch('/auto/stop').then(fetchStatus)">STOP</button> <button id="nextStepBtn" style="display:none;" onclick="nextStep()">Następny krok &raquo;</button> </div> <div id="active-profile-info">Aktywny profil: <span id="active-profile-name">Brak</span></div> </div>
<div class="section"> <h3>Wybór profilu</h3> <div> Źródło profili: <select id="profileSource" onchange="sourceChanged()"><option value="sd" selected>Karta SD</option><option value="github">GitHub</option></select> </div> <div> <select id="profileList"></select> <button onclick="selectProfile()">Ustaw jako aktywny</button> <button onclick="editProfile()">Edytuj wybrany</button> <button id="reloadSdBtn" onclick="reloadProfiles()" style="float: right;">Odczytaj z karty</button> </div> </div>
<div class="section"> <h3>Ustawienia manualne</h3> <div> Temp: <input id="tSet" type="number" value="70" min="20" max="130" style="width:60px"> °C <button onclick="setT()">Ustaw</button> </div> <div> Moc: <select id="power"><option value="1">1 grzałka</option><option value="2" selected>2 grzałki</option><option value="3">3 grzałki</option></select> <button onclick="setP()">Ustaw</button> </div> <div> Dym PWM: <input id="smoke" type="range" min="0" max="255" value="0" style="width:100px"><span id="smokeVal">0</span> <button onclick="setS()">Ustaw</button> </div> <div> Termoobieg: <select id="fan"><option value="0">OFF</option><option value="1" selected>ON</option><option value="2">CYKL</option></select> ON: <input id="fon" type="number" value="10" style="width:50px">s OFF: <input id="foff" type="number" value="60" style="width:50px">s <button onclick="setF()">Ustaw</button> </div> </div>
<hr> <a href="/creator">📝 Nowy Profil</a> | <a href="/wifi">⚙️ WiFi</a> | <a href="/update">📦 OTA Update</a>
<script>
let currentProfileSource = 'sd';
document.getElementById('smoke').oninput=function(){document.getElementById('smokeVal').textContent=this.value;};
function formatHHMMSS(s){if(isNaN(s)||s<0)return"--:--:--";const h=Math.floor(s/3600);const m=Math.floor((s%3600)/60);const sec=s%60;return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(sec).padStart(2,'0')}
function fetchStatus(){
    fetch('/status').then(r=>r.json()).then(j=>{
        let s=j.mode.includes('PAUSE')?'PAUSE':j.mode.includes('ERROR')?'ERROR':j.mode;
        let p=Math.round((j.smokePwm/255)*100);
        document.getElementById('status').innerHTML=`<div style="font-size:1.5em;margin-bottom:8px;"><span style="color:orange;">🌡️ ${j.tChamber.toFixed(1)}°C</span> | <span style="color:yellow;">🍖 ${j.tMeat.toFixed(1)}°C</span> | <span style="color:cyan;">🎯 ${j.tSet.toFixed(1)}°C</span></div><span style="color:${{IDLE:'#888',AUTO:'#0f0',MANUAL:'#0ff',PAUSE:'#ff0',ERROR:'#f00'}[s]||'#fff'};">⚡ ${j.mode}</span> | Moc: ${j.powerModeText} | Fan: ${j.fanModeText} | 💨 Dym: ${p}%`;
        const t=document.getElementById('timer-section'),d=document.getElementById('timer-display'),n=document.getElementById('step-name-display'),c=document.getElementById('countdown-timer-section'),o=document.getElementById('countdown-timer-display'),b=document.getElementById('nextStepBtn');
        if(j.mode==='AUTO'||j.mode==='MANUAL'){
            t.style.display='block';
            d.textContent=formatHHMMSS(j.elapsedTimeSec);
            if(j.mode==='AUTO'){
                n.textContent='Krok: '+j.stepName;
                c.style.display='inline';
                b.style.display='inline-block';
                let r=Math.max(0,j.stepTotalTimeSec-j.elapsedTimeSec);
                o.textContent=formatHHMMSS(r);
            } else {
                n.textContent='Tryb Manualny';
                c.style.display='none';
                b.style.display='none';
            }
        } else {
            t.style.display='none';
            b.style.display='none';
        }
        let profileName = j.activeProfile.replace('/profiles/','').replace('github:','[GitHub] ');
        document.getElementById('active-profile-name').textContent = profileName;
    }).catch(e=>console.error(e));
}
function resetTimer(){fetch('/timer/reset').then(r=>r.text()).then(t=>{console.log(t);fetchStatus()})}
function nextStep(){if(confirm("Czy na pewno chcesz pominąć bieżący krok?")){fetch('/auto/next_step').then(r=>r.text()).then(t=>{console.log(t);fetchStatus()})}}
setInterval(fetchStatus,1000);fetchStatus();
function setT(){fetch('/manual/set?tSet='+document.getElementById('tSet').value).then(fetchStatus)}
function setP(){fetch('/manual/power?val='+document.getElementById('power').value).then(fetchStatus)}
function setS(){fetch('/manual/smoke?val='+document.getElementById('smoke').value).then(fetchStatus)}
function setF(){var t=document.getElementById('fan').value,e=document.getElementById('fon').value,n=document.getElementById('foff').value;fetch(`/manual/fan?mode=${t}&on=${e}&off=${n}`).then(fetchStatus)}
function sourceChanged(){currentProfileSource=document.getElementById('profileSource').value;document.getElementById('reloadSdBtn').style.display="sd"===currentProfileSource?"inline-block":"none";loadProfiles()}
function loadProfiles(){let t="sd"===currentProfileSource?"/api/profiles":"/api/github_profiles";fetch(t).then(r=>r.json()).then(p=>{let t=document.getElementById("profileList");t.innerHTML="";p.forEach(p=>{let e=p.replace("/profiles/",""),n=document.createElement("option");n.value=e,n.textContent=e,t.appendChild(n)})})}
function selectProfile(){let t=document.getElementById('profileList').value;t&&fetch(`/profile/select?name=${t}&source=${currentProfileSource}`).then(r=>r.text()).then(t=>{alert(t),fetchStatus()})}
function editProfile(){const t=document.getElementById('profileList').value;if(!t)return;let e=currentProfileSource;window.location.href=`/creator?edit=${t}&source=${e}`}
function reloadProfiles(){"sd"===currentProfileSource&&fetch("/profile/reload").then(r=>{r.ok||alert("Błąd odczytu karty SD!"),loadProfiles(),fetchStatus()})}
loadProfiles();
</script>
</body></html>
)rawliteral";

// Szablon strony kreatora
static const char HTML_TEMPLATE_CREATOR[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><title>Kreator/Edytor Profili</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style> body{font-family:sans-serif;background:#111;color:#eee;padding:10px;} h2,h3{margin-top:20px; border-bottom: 1px solid #333; padding-bottom: 5px;} button{margin:4px;padding:8px 12px;background:#333;color:#fff;border:1px solid #555;border-radius:4px; cursor: pointer;} button:hover{background:#555;} input,select{padding:6px;margin:4px;background:#222;color:#eee;border:1px solid #444;border-radius:4px; width: 95%; box-sizing: border-box;} .section{background:#222;padding:10px;border-radius:8px;margin-bottom:10px;} .btn-start{background:#1a5f1a;} .btn-start:hover{background:#2a8f2a;} .btn-stop{background:#8b0000;} .btn-stop:hover{background:#cd0000;} fieldset{border: 1px solid #444; border-radius: 5px; margin-top: 10px;} legend{font-weight: bold; color: #bbb;} .step-preview{background: #333; border-radius: 4px; padding: 5px; margin-bottom: 5px; font-size: 0.9em; cursor: pointer;} </style>
</head><body>
<h2 id="creator-title"><span style="vertical-align: middle;"><img src="data:image/png;base64,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" alt="Logo" style="height: 60px; background-color: white; border-radius: 50%; margin: 10px 0;"></div>
<div class="status" id="status">Ładowanie...</div>
<div class="timer" id="timer-section"> <div id="step-name-display"></div> <div> <span title="Czas, który upłynął">🕒 Upłynęło: </span><span id="timer-display">--:--:--</span> <span id="countdown-timer-section" style="margin-left: 15px;"> <span title="Czas pozostały do końca kroku">⏳ Pozostało: </span><span id="countdown-timer-display">--:--:--</span> </span> <button onclick="resetTimer()" style="float: right;">Resetuj Czas</button> </div> </div>
<div class="section"> <h3>Sterowanie Główne</h3> <div> <button class="btn-start" onclick="fetch('/mode/manual').then(fetchStatus)">MANUAL</button> <button class="btn-start" onclick="fetch('/auto/start').then(fetchStatus)">AUTO START</button> <button class="btn-stop" onclick="fetch('/auto/stop').then(fetchStatus)">STOP</button> <button id="nextStepBtn" style="display:none;" onclick="nextStep()">Następny krok &raquo;</button> </div> <div id="active-profile-info">Aktywny profil: <span id="active-profile-name">Brak</span></div> </div>
<div class="section"> <h3>Wybór profilu</h3> <div> Źródło profili: <select id="profileSource" onchange="sourceChanged()"><option value="sd" selected>Karta SD</option><option value="github">GitHub</option></select> </div> <div> <select id="profileList"></select> <button onclick="selectProfile()">Ustaw jako aktywny</button> <button onclick="editProfile()">Edytuj wybrany</button> <button id="reloadSdBtn" onclick="reloadProfiles()" style="float: right;">Odczytaj z karty</button> </div> </div>
<div class="section"> <h3>Ustawienia manualne</h3> <div> Temp: <input id="tSet" type="number" value="70" min="20" max="130" style="width:60px"> °C <button onclick="setT()">Ustaw</button> </div> <div> Moc: <select id="power"><option value="1">1 grzałka</option><option value="2" selected>2 grzałki</option><option value="3">3 grzałki</option></select> <button onclick="setP()">Ustaw</button> </div> <div> Dym PWM: <input id="smoke" type="range" min="0" max="255" value="0" style="width:100px"><span id="smokeVal">0</span> <button onclick="setS()">Ustaw</button> </div> <div> Termoobieg: <select id="fan"><option value="0">OFF</option><option value="1" selected>ON</option><option value="2">CYKL</option></select> ON: <input id="fon" type="number" value="10" style="width:50px">s OFF: <input id="foff" type="number" value="60" style="width:50px">s <button onclick="setF()">Ustaw</button> </div> </div>
<hr> <a href="/creator">📝 Nowy Profil</a> | <a href="/wifi">⚙️ WiFi</a> | <a href="/update">📦 OTA Update</a>
<script>
let currentProfileSource = 'sd';
document.getElementById('smoke').oninput=function(){document.getElementById('smokeVal').textContent=this.value;};
function formatHHMMSS(s){if(isNaN(s)||s<0)return"--:--:--";const h=Math.floor(s/3600);const m=Math.floor((s%3600)/60);const sec=s%60;return String(h).padStart(2,'0')+':'+String(m).padStart(2,'0')+':'+String(sec).padStart(2,'0')}
function fetchStatus(){
    fetch('/status').then(r=>r.json()).then(j=>{
        let s=j.mode.includes('PAUSE')?'PAUSE':j.mode.includes('ERROR')?'ERROR':j.mode;
        let p=Math.round((j.smokePwm/255)*100);
        document.getElementById('status').innerHTML=`<div style="font-size:1.5em;margin-bottom:8px;"><span style="color:orange;">🌡️ ${j.tChamber.toFixed(1)}°C</span> | <span style="color:yellow;">🍖 ${j.tMeat.toFixed(1)}°C</span> | <span style="color:cyan;">🎯 ${j.tSet.toFixed(1)}°C</span></div><span style="color:${{IDLE:'#888',AUTO:'#0f0',MANUAL:'#0ff',PAUSE:'#ff0',ERROR:'#f00'}[s]||'#fff'};">⚡ ${j.mode}</span> | Moc: ${j.powerModeText} | Fan: ${j.fanModeText} | 💨 Dym: ${p}%`;
        const t=document.getElementById('timer-section'),d=document.getElementById('timer-display'),n=document.getElementById('step-name-display'),c=document.getElementById('countdown-timer-section'),o=document.getElementById('countdown-timer-display'),b=document.getElementById('nextStepBtn');
        if(j.mode==='AUTO'||j.mode==='MANUAL'){
            t.style.display='block';
            d.textContent=formatHHMMSS(j.elapsedTimeSec);
            if(j.mode==='AUTO'){
                n.textContent='Krok: '+j.stepName;
                c.style.display='inline';
                b.style.display='inline-block';
                let r=Math.max(0,j.stepTotalTimeSec-j.elapsedTimeSec);
                o.textContent=formatHHMMSS(r);
            } else {
                n.textContent='Tryb Manualny';
                c.style.display='none';
                b.style.display='none';
            }
        } else {
            t.style.display='none';
            b.style.display='none';
        }
        let profileName = j.activeProfile.replace('/profiles/','').replace('github:','[GitHub] ');
        document.getElementById('active-profile-name').textContent = profileName;
    }).catch(e=>console.error(e));
}
function resetTimer(){fetch('/timer/reset').then(r=>r.text()).then(t=>{console.log(t);fetchStatus()})}
function nextStep(){if(confirm("Czy na pewno chcesz pominąć bieżący krok?")){fetch('/auto/next_step').then(r=>r.text()).then(t=>{console.log(t);fetchStatus()})}}
setInterval(fetchStatus,1000);fetchStatus();
function setT(){fetch('/manual/set?tSet='+document.getElementById('tSet').value).then(fetchStatus)}
function setP(){fetch('/manual/power?val='+document.getElementById('power').value).then(fetchStatus)}
function setS(){fetch('/manual/smoke?val='+document.getElementById('smoke').value).then(fetchStatus)}
function setF(){var t=document.getElementById('fan').value,e=document.getElementById('fon').value,n=document.getElementById('foff').value;fetch(`/manual/fan?mode=${t}&on=${e}&off=${n}`).then(fetchStatus)}
function sourceChanged(){currentProfileSource=document.getElementById('profileSource').value;document.getElementById('reloadSdBtn').style.display="sd"===currentProfileSource?"inline-block":"none";loadProfiles()}
function loadProfiles(){let t="sd"===currentProfileSource?"/api/profiles":"/api/github_profiles";fetch(t).then(r=>r.json()).then(p=>{let t=document.getElementById("profileList");t.innerHTML="";p.forEach(p=>{let e=p.replace("/profiles/",""),n=document.createElement("option");n.value=e,n.textContent=e,t.appendChild(n)})})}
function selectProfile(){let t=document.getElementById('profileList').value;t&&fetch(`/profile/select?name=${t}&source=${currentProfileSource}`).then(r=>r.text()).then(t=>{alert(t),fetchStatus()})}
function editProfile(){const t=document.getElementById('profileList').value;if(!t)return;let e=currentProfileSource;window.location.href=`/creator?edit=${t}&source=${e}`}
function reloadProfiles(){"sd"===currentProfileSource&&fetch("/profile/reload").then(r=>{r.ok||alert("Błąd odczytu karty SD!"),loadProfiles(),fetchStatus()})}
loadProfiles();
</script>
</body></html>
)rawliteral";

// Szablon strony aktualizacji OTA
static const char HTML_TEMPLATE_OTA[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><title>Aktualizacja OTA</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style>
body{font-family:sans-serif;background:#111;color:#eee;padding:10px; text-align: center;}
.section{background:#222;padding:20px;border-radius:8px;margin-bottom:10px; display: inline-block;}
button{margin:4px;padding:8px 12px;background:#333;color:#fff;border:1px solid #555;border-radius:4px; cursor: pointer;}
button:hover{background:#555;}
progress{width: 100%; height: 25px; margin-top: 10px;}
#status{margin-top: 10px; font-weight: bold;}
</style>
</head><body>
<h2>📦 Aktualizacja oprogramowania OTA</h2>
<div class="section">
    <form id="upload_form" method="POST" action="/update" enctype="multipart/form-data">
        <input type="file" name="update" id="file" accept=".bin"><br><br>
        <button type="submit">Rozpocznij aktualizację</button>
    </form>
    <progress id="progress" value="0" max="100" style="display:none;"></progress>
    <div id="status"></div>
    <div id="pr" style="display:none;">0%</div>
</div>
<hr><a href="/">⬅️ Wróć do strony głównej</a>
<script>
var upload_form = document.getElementById("upload_form");
var file_input = document.getElementById("file");
var pr = document.getElementById("pr");
var progress_bar = document.getElementById("progress");
var status_div = document.getElementById("status");

upload_form.addEventListener("submit", function(event) {
    event.preventDefault();
    var file = file_input.files[0];
    if (!file) {
        status_div.innerHTML = "Nie wybrano pliku!";
        return;
    }
    
    var xhr = new XMLHttpRequest();
    xhr.open("POST", "/update");
    
    xhr.upload.addEventListener("progress", function(event) {
        if (event.lengthComputable) {
            var percentComplete = Math.round((event.loaded / event.total) * 100);
            progress_bar.style.display = 'block';
            pr.style.display = 'block';
            progress_bar.value = percentComplete;
            pr.innerHTML = percentComplete + "%";
        }
    });

    // ZMIANA LOGIKI OBSŁUGI ZAKOŃCZENIA
    xhr.onloadend = function() {
        // To zdarzenie jest wywoływane zawsze, nawet przy błędzie sieciowym.
        // Sprawdzamy, czy pasek postępu doszedł do 100%.
        if (progress_bar.value === 100) {
            // Jeśli tak, zakładamy, że aktualizacja się udała, a błąd sieciowy
            // był spowodowany restartem urządzenia.
            status_div.innerHTML = "<span style='color: lime;'>Aktualizacja zakończona!</span> Urządzenie uruchomi się ponownie...";
            setTimeout(function(){ 
                status_div.innerHTML = "Próba ponownego połączenia...";
                window.location.href = '/'; 
            }, 5000);
        } else if (xhr.status !== 200) {
            // Jeśli pasek nie doszedł do 100% i mamy błąd, to jest to prawdziwy błąd.
            status_div.innerHTML = "<span style='color: red;'>Błąd aktualizacji:</span> " + xhr.responseText;
        } else {
            // Inne, rzadkie przypadki.
             status_div.innerHTML = "<span style='color: red;'>Nieznany błąd.</span>";
        }
    };
    
    var formData = new FormData();
    formData.append("update", file, "firmware.bin");
    status_div.innerHTML = "Wysyłanie pliku...";
    xhr.send(formData);
});
</script>
</body></html>
)rawliteral";

static const char* getStateString(ProcessState st) {
  switch (st) {
    case ProcessState::IDLE: return "IDLE";
    case ProcessState::RUNNING_AUTO: return "AUTO";
    case ProcessState::RUNNING_MANUAL: return "MANUAL";
    case ProcessState::PAUSE_DOOR: return "PAUZA: DRZWI";
    case ProcessState::PAUSE_SENSOR: return "PAUZA: CZUJNIK";
    case ProcessState::PAUSE_OVERHEAT: return "PAUZA: PRZEGRZANIE";
    case ProcessState::PAUSE_USER: return "PAUZA UZYTK.";
    case ProcessState::ERROR_PROFILE: return "ERROR_PROFILE";
    case ProcessState::SOFT_RESUME: return "Wznawianie...";
    default: return "UNKNOWN";
  }
}

static const char* getStatusJSON() {
    static char jsonBuffer[512]; // Bufor jest wystarczająco duży
    
    // Zmienne do wysłania
    double tc, tm, ts;
    int pm, fm, sm;
    ProcessState st;
    unsigned long elapsedSec = 0;
    unsigned long stepTotalSec = 0;
    const char* stepName = "";
    char activeProfile[64] = "Brak"; // Bufor na nazwę aktywnego profilu

    state_lock();
    st = g_currentState;
    tc = g_tChamber;
    tm = g_tMeat;
    ts = g_tSet;
    pm = g_powerMode;
    fm = g_fanMode;
    sm = g_manualSmokePwm;

    // Bezpieczne skopiowanie ścieżki
    strncpy(activeProfile, storage_get_profile_path(), sizeof(activeProfile) - 1);
    activeProfile[sizeof(activeProfile) - 1] = '\0'; // Upewnij się, że string jest zakończony

    if (st == ProcessState::RUNNING_MANUAL) {
        elapsedSec = (millis() - g_processStartTime) / 1000;
    } else if (st == ProcessState::RUNNING_AUTO) {
        elapsedSec = (millis() - g_stepStartTime) / 1000;
        if (g_currentStep < g_stepCount) {
            stepName = g_profile[g_currentStep].name;
            stepTotalSec = g_profile[g_currentStep].minTimeMs / 1000;
        }
    }
    state_unlock();
    
    const char* powerModeStr;
    switch (pm) {
        case 1: powerModeStr = "1-grzalka"; break;
        case 2: powerModeStr = "2-grzalki"; break;
        case 3: powerModeStr = "3-grzalki"; break;
        default: powerModeStr = "Brak"; break;
    }
    const char* fanModeStr;
    switch (fm) {
        case 0: fanModeStr = "OFF"; break;
        case 1: fanModeStr = "ON"; break;
        case 2: fanModeStr = "Cyklicznie"; break;
        default: fanModeStr = "Brak"; break;
    }

    // Usunięcie prefixów ze ścieżki do wyświetlenia
    char cleanProfileName[64];
    strncpy(cleanProfileName, activeProfile, sizeof(cleanProfileName));
    if (strstr(cleanProfileName, "/profiles/") != NULL) {
        strcpy(cleanProfileName, strstr(cleanProfileName, "/profiles/") + 10);
    } else if (strstr(cleanProfileName, "github:") != NULL) {
        memmove(cleanProfileName, cleanProfileName + 7, strlen(cleanProfileName) - 6);
    }
    
    // POPRAWIONA LINIJKA snprintf
    snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{\"tChamber\":%.1f,\"tMeat\":%.1f,\"tSet\":%.1f,\"powerMode\":%d,\"fanMode\":%d,\"smokePwm\":%d,\"mode\":\"%s\",\"state\":%d,\"powerModeText\":\"%s\",\"fanModeText\":\"%s\",\"elapsedTimeSec\":%lu,\"stepName\":\"%s\",\"stepTotalTimeSec\":%lu,\"activeProfile\":\"%s\"}",
        tc, tm, ts, pm, fm, sm, getStateString(st), (int)st, powerModeStr, fanModeStr, elapsedSec, stepName, stepTotalSec, cleanProfileName);
    
    return jsonBuffer;
}


void web_server_init() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.softAP(CFG_AP_SSID, CFG_AP_PASS);
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  const char* ssid = storage_get_wifi_ssid();
  if (strlen(ssid) > 0) {
    WiFi.begin(ssid, storage_get_wifi_pass());
    Serial.printf("Connecting STA to %s...\n", ssid);
  }

  server.on("/", HTTP_GET, []() {
    server.send_P(200, "text/html", HTML_TEMPLATE_MAIN);
  });
  server.on("/creator", HTTP_GET, []() {
    server.send_P(200, "text/html", HTML_TEMPLATE_CREATOR);
  });
  server.on("/update", HTTP_GET, []() {
    server.send_P(200, "text/html", HTML_TEMPLATE_OTA);
  });

  server.on("/api/profiles", HTTP_GET, []() {
    char jsonBuf[2048];
    storage_list_profiles_json(jsonBuf, sizeof(jsonBuf));
    server.send(200, "application/json", jsonBuf);
  });
  server.on("/api/github_profiles", HTTP_GET, []() {
    char jsonBuf[2048];
    storage_list_github_profiles_json(jsonBuf, sizeof(jsonBuf));
    server.send(200, "application/json", jsonBuf);
  });

  server.on("/profile/get", HTTP_GET, []() {
    if (!server.hasArg("name") || !server.hasArg("source")) {
      server.send(400, "text/plain", "Brak nazwy profilu lub źródła");
      return;
    }
    const char* profileName = server.arg("name").c_str();
    const char* source = server.arg("source").c_str();
    
    if (strcmp(source, "sd") == 0) {
      char jsonBuf[2048];
      storage_get_profile_as_json(profileName, jsonBuf, sizeof(jsonBuf));
      server.send(200, "application/json", jsonBuf);
    } else if (strcmp(source, "github") == 0) {
      char jsonBuf[2048];
      storage_get_profile_as_json(profileName, jsonBuf, sizeof(jsonBuf));
      server.send(200, "application/json", jsonBuf);
    } else {
      server.send(400, "text/plain", "Nieznane źródło");
    }
  });

  server.on("/profile/select", HTTP_GET, []() {
    if (server.hasArg("name") && server.hasArg("source")) {
      const char* profileName = server.arg("name").c_str();
      const char* source = server.arg("source").c_str();
      bool success = false;
      char fullPath[128];
      
      if (strcmp(source, "sd") == 0) {
        snprintf(fullPath, sizeof(fullPath), "/profiles/%s", profileName);
        storage_save_profile_path_nvs(fullPath);
        success = storage_load_profile();
      } else if (strcmp(source, "github") == 0) {
        snprintf(fullPath, sizeof(fullPath), "github:%s", profileName);
        storage_save_profile_path_nvs(fullPath);
        success = storage_load_github_profile(profileName);
      }
      
      if (success) {
        char msg[128];
        snprintf(msg, sizeof(msg), "OK, profil %s załadowany.", profileName);
        server.send(200, "text/plain", msg);
      } else {
        server.send(500, "text/plain", "Błąd ładowania profilu.");
      }
    } else {
      server.send(400, "text/plain", "Brak nazwy profilu lub źródła");
    }
  });

  server.on("/auto/next_step", HTTP_GET, []() {
    state_lock();
    if (g_currentState == ProcessState::RUNNING_AUTO && g_currentStep < g_stepCount) {
      g_profile[g_currentStep].minTimeMs = 0;
    }
    state_unlock();
    server.send(200, "text/plain", "OK");
  });

  server.on("/timer/reset", HTTP_GET, []() {
    state_lock();
    if (g_currentState == ProcessState::RUNNING_MANUAL) {
      g_processStartTime = millis();
    } else if (g_currentState == ProcessState::RUNNING_AUTO) {
      g_stepStartTime = millis();
    }
    state_unlock();
    server.send(200, "text/plain", "Timer zresetowany");
  });
  server.on("/status", HTTP_GET, []() {
    server.send(200, "application/json", getStatusJSON());
  });
  server.on("/mode/manual", HTTP_GET, []() {
    process_start_manual();
    server.send(200, "text/plain", "OK");
  });
  server.on("/auto/start", HTTP_GET, []() {
    if (storage_load_profile()) {
      process_start_auto();
      server.send(200, "text/plain", "OK");
    } else {
      server.send(500, "text/plain", "Profile error");
    }
  });
  server.on("/auto/stop", HTTP_GET, []() {
    allOutputsOff();
    state_lock();
    g_currentState = ProcessState::IDLE;
    state_unlock();
    server.send(200, "text/plain", "OK");
  });
  server.on("/profile/reload", HTTP_GET, []() {
    if (storage_reinit_sd()) {
      storage_load_profile();
      server.send(200, "text/plain", "Karta SD odświeżona.");
    } else {
      server.send(500, "text/plain", "Błąd ponownej inicjalizacji karty SD!");
    }
  });

  server.on("/profile/create", HTTP_POST, []() {
    if (!server.hasArg("filename") || !server.hasArg("data")) {
      server.send(400, "text/plain", "Brak nazwy pliku lub danych.");
      return;
    }
    const char* filenameArg = server.arg("filename").c_str();
    const char* data = server.arg("data").c_str();
    
    char filename[128];
    strncpy(filename, filenameArg, sizeof(filename) - 6);
    filename[sizeof(filename) - 6] = '\0';
    
    if (strlen(filename) == 0) {
      server.send(400, "text/plain", "Nazwa pliku nie może być pusta.");
      return;
    }
    
    // Add .prof extension if not present
    size_t len = strlen(filename);
    if (len < 5 || strcmp(filename + len - 5, ".prof") != 0) {
      strncat(filename, ".prof", sizeof(filename) - len - 1);
    }
    
    char path[150];
    snprintf(path, sizeof(path), "/profiles/%s", filename);
    
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
      server.send(500, "text/plain", "Nie można otworzyć pliku do zapisu.");
      return;
    }
    file.print(data);
    file.close();
    
    char msg[200];
    snprintf(msg, sizeof(msg), "Profil '%s' został pomyślnie zapisany!", filename);
    server.send(200, "text/plain", msg);
  });

server.on("/update", HTTP_POST, 
    []() { // On success
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "BŁĄD" : "OK");

        // --- POPRAWIONA INICJALIZACJA WDT ---
        Serial.println("Re-initializing WDT...");
        // Stwórz strukturę konfiguracyjną
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = WDT_TIMEOUT * 1000,
            .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
            .trigger_panic = true,
        };
        // Zainicjuj WDT ze strukturą
        esp_task_wdt_init(&wdt_config);
        // Dodaj główne zadanie do WDT (jeśli to konieczne, ale zwykle tak)
        esp_task_wdt_add(NULL); 
        // ------------------------------------

        delay(1000);
        ESP.restart();
    }, 
    []() { // On upload
        HTTPUpload& upload = server.upload();
        if (upload.status == UPLOAD_FILE_START) {
            Serial.printf("Update: %s\n", upload.filename.c_str());
            
            // Wyłącz Watchdoga PRZED rozpoczęciem zapisu do flasha
            Serial.println("De-initializing WDT for OTA");
            esp_task_wdt_deinit();
            
            if (!Update.begin(UPDATE_SIZE_UNKNOWN)) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_WRITE) {
            if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
                Update.printError(Serial);
            }
        } else if (upload.status == UPLOAD_FILE_END) {
            if (Update.end()) { 
                Serial.printf("Update Success: %u\n", upload.totalSize);
            } else {
                Update.printError(Serial);
            }
        }
        // yield() nie jest potrzebny, gdy WDT jest wyłączony, ale nie zaszkodzi
        yield(); 
    }
);

  server.on("/manual/set", HTTP_GET, []() {
    if (server.hasArg("tSet")) {
      double val = constrain(server.arg("tSet").toFloat(), CFG_T_MIN_SET, CFG_T_MAX_SET);
      state_lock();
      g_tSet = val;
      state_unlock();
      storage_save_manual_settings_nvs();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/manual/power", HTTP_GET, []() {
    if (server.hasArg("val")) {
      int val = constrain(server.arg("val").toInt(), CFG_POWERMODE_MIN, CFG_POWERMODE_MAX);
      state_lock();
      g_powerMode = val;
      state_unlock();
      storage_save_manual_settings_nvs();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/manual/smoke", HTTP_GET, []() {
    if (server.hasArg("val")) {
      int val = constrain(server.arg("val").toInt(), CFG_SMOKE_PWM_MIN, CFG_SMOKE_PWM_MAX);
      state_lock();
      g_manualSmokePwm = val;
      state_unlock();
      storage_save_manual_settings_nvs();
    }
    server.send(200, "text/plain", "OK");
  });
  server.on("/manual/fan", HTTP_GET, []() {
    if (server.hasArg("mode")) {
      state_lock();
      g_fanMode = constrain(server.arg("mode").toInt(), 0, 2);
      state_unlock();
    }
    if (server.hasArg("on")) {
      state_lock();
      g_fanOnTime = max(1000UL, (unsigned long)server.arg("on").toInt() * 1000UL);
      state_unlock();
    }
    if (server.hasArg("off")) {
      state_lock();
      g_fanOffTime = max(1000UL, (unsigned long)server.arg("off").toInt() * 1000UL);
      state_unlock();
    }
    storage_save_manual_settings_nvs();
    server.send(200, "text/plain", "OK");
  });
  server.on("/wifi", HTTP_GET, []() {
    char html[512];
    char ssid[32];
    strncpy(ssid, storage_get_wifi_ssid(), sizeof(ssid) - 1);
    ssid[sizeof(ssid) - 1] = '\0';
    
    snprintf(html, sizeof(html),
      "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>"
      "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:20px;}input{padding:8px;margin:5px 0;width:200px;}</style>"
      "</head><body><h2>WiFi STA Configuration</h2>"
      "<form method='POST' action='/wifi/save'>"
      "SSID: <input name='ssid' value='%s'><br>"
      "Password: <input name='pass' type='password'><br>"
      "<input type='submit' value='Connect'>"
      "</form><br><a href='/'>← Back</a></body></html>",
      ssid);
    server.send(200, "text/html", html);
  });
  server.on("/wifi/save", HTTP_POST, []() {
    if (server.hasArg("ssid") && server.hasArg("pass")) {
      storage_save_wifi_nvs(server.arg("ssid").c_str(), server.arg("pass").c_str());
      WiFi.begin(storage_get_wifi_ssid(), storage_get_wifi_pass());
    }
    server.send(200, "text/html", "<html><body style='background:#111;color:#eee;padding:20px;'>Connecting... <a href='/'>Back</a></body></html>");
  });

  server.begin();
  Serial.println("Web server started");
}

void web_server_handle_client() {
  server.handleClient();
}
