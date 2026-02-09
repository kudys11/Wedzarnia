// web_server.cpp - Kompletna implementacja z wszystkimi poprawkami
#include "web_server.h"
#include "config.h"
#include "state.h"
#include "storage.h"
#include "process.h"
#include "outputs.h"
#include "sensors.h"
#include <WiFi.h>
#include <Update.h>
#include "FS.h"
#include <SD.h>
#include <HTTPClient.h>
#include <esp_task_wdt.h>



// =================================================================
// ULEPSZONA WIZUALNIE GŁÓWNA STRONA "/"
// =================================================================
static const char HTML_TEMPLATE_MAIN[] PROGMEM = R"rawliteral(
<!DOCTYPE html>
<html lang="pl">
<head>
    <meta charset='utf-8'>
    <title>Wędzarnia</title>
    <meta name="viewport" content="width=device-width,initial-scale=1">
    <style>
        * { margin: 0; padding: 0; box-sizing: border-box; }
        
        body {
            font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
            background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
            color: #eee;
            padding: 15px;
            min-height: 100vh;
        }
        
        .container {
            max-width: 800px;
            margin: 0 auto;
        }
        
        /* Header */
        .header {
            background: linear-gradient(135deg, #d32f2f 0%, #c62828 100%);
            padding: 20px;
            border-radius: 15px;
            margin-bottom: 20px;
            box-shadow: 0 8px 20px rgba(211, 47, 47, 0.3);
            text-align: center;
        }
        
        .header h1 {
            font-size: 2em;
            margin: 0;
            text-shadow: 2px 2px 4px rgba(0,0,0,0.5);
        }
        
        .header .version {
            font-size: 0.8em;
            opacity: 0.9;
            margin-top: 5px;
        }
        
        /* Status Card */
        .status-card {
            background: rgba(255,255,255,0.05);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.1);
            padding: 20px;
            border-radius: 15px;
            margin-bottom: 15px;
            box-shadow: 0 8px 32px rgba(0,0,0,0.3);
        }
        
        .temp-display {
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
            gap: 15px;
            margin-bottom: 15px;
        }
        
        .temp-box {
            flex: 1;
            min-width: 120px;
            background: rgba(0,0,0,0.3);
            padding: 15px;
            border-radius: 12px;
            text-align: center;
            border: 2px solid transparent;
            transition: all 0.3s;
        }
        
        .temp-box:hover {
            transform: translateY(-3px);
            border-color: rgba(255,255,255,0.3);
        }
        
        .temp-box .label {
            font-size: 0.9em;
            opacity: 0.8;
            margin-bottom: 8px;
        }
        
        .temp-box .value {
            font-size: 2.2em;
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
        
        .temp-chamber { border-color: #ff9800; }
        .temp-chamber .value { color: #ff9800; }
        
        .temp-meat { border-color: #ffc107; }
        .temp-meat .value { color: #ffc107; }
        
        .temp-target { border-color: #00bcd4; }
        .temp-target .value { color: #00bcd4; }
        
        .status-info {
            display: flex;
            justify-content: space-between;
            flex-wrap: wrap;
            gap: 10px;
            font-size: 0.95em;
            padding: 10px;
            background: rgba(0,0,0,0.2);
            border-radius: 8px;
        }
        
        .status-badge {
            display: inline-block;
            padding: 5px 12px;
            border-radius: 20px;
            font-weight: 600;
            font-size: 0.9em;
        }
        
        .status-idle { background: #666; }
        .status-auto { background: #4caf50; }
        .status-manual { background: #00bcd4; }
        .status-pause { background: #ff9800; }
        .status-error { background: #f44336; }
        
        /* Timer Section */
        .timer-card {
            background: rgba(76,175,80,0.1);
            border: 2px solid #4caf50;
            padding: 15px;
            border-radius: 12px;
            margin-bottom: 15px;
            display: none;
        }
        
        .timer-card.active {
            display: block;
            animation: fadeIn 0.3s;
        }
        
        @keyframes fadeIn {
            from { opacity: 0; transform: translateY(-10px); }
            to { opacity: 1; transform: translateY(0); }
        }
        
        .timer-row {
            display: flex;
            justify-content: space-around;
            flex-wrap: wrap;
            gap: 10px;
            margin-top: 10px;
        }
        
        .timer-item {
            text-align: center;
        }
        
        .timer-item .label {
            font-size: 0.85em;
            opacity: 0.8;
            margin-bottom: 5px;
        }
        
        .timer-item .time {
            font-size: 1.8em;
            font-weight: bold;
            font-family: 'Courier New', monospace;
        }
        
        .time-elapsed { color: #4caf50; }
        .time-remaining { color: #ff9800; }
        
        /* Section Cards */
        .section {
            background: rgba(255,255,255,0.05);
            backdrop-filter: blur(10px);
            border: 1px solid rgba(255,255,255,0.1);
            padding: 20px;
            border-radius: 12px;
            margin-bottom: 15px;
            box-shadow: 0 4px 16px rgba(0,0,0,0.2);
        }
        
        .section h3 {
            color: #fff;
            margin-bottom: 15px;
            padding-bottom: 10px;
            border-bottom: 2px solid rgba(255,255,255,0.2);
            font-size: 1.3em;
        }
        
        /* Buttons */
        button {
            padding: 12px 20px;
            margin: 5px;
            border: none;
            border-radius: 8px;
            font-size: 1em;
            font-weight: 600;
            cursor: pointer;
            transition: all 0.3s;
            box-shadow: 0 4px 12px rgba(0,0,0,0.3);
        }
        
        button:hover {
            transform: translateY(-2px);
            box-shadow: 0 6px 20px rgba(0,0,0,0.4);
        }
        
        button:active {
            transform: translateY(0);
        }
        
        .btn-start {
            background: linear-gradient(135deg, #4caf50 0%, #45a049 100%);
            color: white;
        }
        
        .btn-start:hover {
            background: linear-gradient(135deg, #45a049 0%, #3d8b40 100%);
        }
        
        .btn-stop {
            background: linear-gradient(135deg, #f44336 0%, #e53935 100%);
            color: white;
        }
        
        .btn-stop:hover {
            background: linear-gradient(135deg, #e53935 0%, #d32f2f 100%);
        }
        
        .btn-action {
            background: linear-gradient(135deg, #2196f3 0%, #1976d2 100%);
            color: white;
        }
        
        .btn-action:hover {
            background: linear-gradient(135deg, #1976d2 0%, #1565c0 100%);
        }
        
        button:disabled {
            opacity: 0.5;
            cursor: not-allowed;
            transform: none !important;
        }
        
        /* Inputs and Selects */
        input, select {
            padding: 10px;
            margin: 5px;
            background: rgba(0,0,0,0.3);
            color: #fff;
            border: 2px solid rgba(255,255,255,0.2);
            border-radius: 8px;
            font-size: 1em;
        }
        
        input:focus, select:focus {
            outline: none;
            border-color: #2196f3;
            box-shadow: 0 0 10px rgba(33,150,243,0.3);
        }
        
        input[type="range"] {
            width: 100%;
            max-width: 200px;
        }
        
        input[type="number"] {
            width: 80px;
        }
        
        /* Control Groups */
        .control-group {
            display: flex;
            align-items: center;
            flex-wrap: wrap;
            gap: 10px;
            margin: 10px 0;
            padding: 12px;
            background: rgba(0,0,0,0.2);
            border-radius: 8px;
        }
        
        .control-group label {
            font-weight: 600;
            margin-right: 10px;
        }
        
        /* Footer Links */
        .footer {
            text-align: center;
            padding: 20px;
            margin-top: 20px;
            border-top: 1px solid rgba(255,255,255,0.1);
        }
        
        .footer a {
            color: #2196f3;
            text-decoration: none;
            margin: 0 15px;
            font-size: 1.1em;
            transition: all 0.3s;
        }
        
        .footer a:hover {
            color: #64b5f6;
            text-shadow: 0 0 10px rgba(33,150,243,0.5);
        }
        
        /* Profile Info */
        .profile-info {
            margin-top: 10px;
            padding: 10px;
            background: rgba(33,150,243,0.1);
            border-left: 4px solid #2196f3;
            border-radius: 4px;
            font-size: 0.9em;
        }
        
        /* Loading Animation */
        .loading {
            display: inline-block;
            width: 20px;
            height: 20px;
            border: 3px solid rgba(255,255,255,0.3);
            border-radius: 50%;
            border-top-color: #fff;
            animation: spin 1s linear infinite;
        }
        
        @keyframes spin {
            to { transform: rotate(360deg); }
        }
        
        /* Responsive */
        @media (max-width: 600px) {
            .header h1 { font-size: 1.5em; }
            .temp-box { min-width: 100px; }
            .temp-box .value { font-size: 1.8em; }
            button { padding: 10px 15px; font-size: 0.9em; }
        }
    </style>
</head>
<body>
    <div class="container">
        <!-- Header -->
        <div class="header">
            <h1>🔥 Wędzarnia IoT v3.3</h1>
            <div class="version">by Wojtek | Ikony i przejście do następnego kroku</div>
        </div>
        
        <!-- Status Card -->
        <div class="status-card">
            <div class="temp-display">
                <div class="temp-box temp-chamber">
                    <div class="label">🌡️ Komora</div>
                    <div class="value" id="temp-chamber">--°C</div>
                </div>
                <div class="temp-box temp-meat">
                    <div class="label">🍖 Mięso</div>
                    <div class="value" id="temp-meat">--°C</div>
                </div>
                <div class="temp-box temp-target">
                    <div class="label">🎯 Zadana</div>
                    <div class="value" id="temp-target">--°C</div>
                </div>
            </div>
            
            <div class="status-info" id="status-info">
                <div>Status: <span class="status-badge" id="status-badge">Ładowanie...</span></div>
                <div>Moc: <span id="power-mode">-</span></div>
                <div>Wentylator: <span id="fan-mode">-</span></div>
                <div>💨 Dym: <span id="smoke-level">0%</span></div>
            </div>
        </div>
        
        <!-- Timer Card -->
        <div class="timer-card" id="timer-section">
            <div style="text-align: center; margin-bottom: 10px;">
                <strong id="step-name">-</strong>
            </div>
            <div class="timer-row">
                <div class="timer-item">
                    <div class="label">🕒 Upłynęło</div>
                    <div class="time time-elapsed" id="timer-elapsed">00:00:00</div>
                </div>
                <div class="timer-item" id="countdown-section">
                    <div class="label">⏳ Pozostało</div>
                    <div class="time time-remaining" id="timer-remaining">00:00:00</div>
                </div>
                <div class="timer-item" id="process-total-section">
                    <div class="label">⏱️ Do końca</div>
                    <div class="time time-remaining" id="process-remaining">--:--:--</div>
                </div>
            </div>
            <div style="text-align: center; margin-top: 10px;">
                <button class="btn-action" onclick="resetTimer()">↻ Resetuj Czas</button>
                <button class="btn-action" id="nextStepBtn" style="display:none;" onclick="nextStep()">⏭️ Następny krok</button>
            </div>
        </div>
        
        <!-- Control Section -->
        <div class="section">
            <h3>⚡ Sterowanie</h3>
            <div style="text-align: center;">
                <button class="btn-action" onclick="startManual()">🎮 Tryb Manualny</button>
                <button class="btn-start" onclick="startAuto()">▶️ Start AUTO</button>
                <button class="btn-stop" onclick="stopProcess()">⏹️ Stop</button>
            </div>
            <div class="profile-info">
                Aktywny profil: <strong id="active-profile">Brak</strong>
            </div>
        </div>
        
        <!-- Profile Selection -->
        <div class="section">
            <h3>📋 Wybór Profilu</h3>
            <div class="control-group">
                <label>Źródło:</label>
                <select id="profileSource" onchange="sourceChanged()">
                    <option value="sd" selected>💾 Karta SD</option>
                    <option value="github">☁️ GitHub</option>
                </select>
            </div>
            <div class="control-group">
                <select id="profileList" style="flex: 1;"></select>
            </div>
            <div style="text-align: center;">
                <button class="btn-action" onclick="selectProfile()">✅ Ustaw aktywny</button>
                <button class="btn-action" onclick="editProfile()">✏️ Edytuj</button>
                <button class="btn-action" id="reloadSdBtn" onclick="reloadProfiles()">🔄 Odczytaj</button>
            </div>
        </div>
        
        <!-- Manual Settings -->
        <div class="section">
            <h3>🎛️ Ustawienia Manualne</h3>
            
            <div class="control-group">
                <label>Temperatura:</label>
                <input id="tSet" type="number" value="70" min="20" max="130">
                <span>°C</span>
                <button class="btn-action" onclick="setT()">✅ Ustaw</button>
            </div>
            
            <div class="control-group">
                <label>Moc grzałek:</label>
                <select id="power">
                    <option value="1">1 grzałka</option>
                    <option value="2" selected>2 grzałki</option>
                    <option value="3">3 grzałki</option>
                </select>
                <button class="btn-action" onclick="setP()">✅ Ustaw</button>
            </div>
            
            <div class="control-group">
                <label>Dym PWM:</label>
                <input id="smoke" type="range" min="0" max="255" value="0">
                <span id="smokeVal" style="min-width: 50px; font-weight: bold;">0</span>
                <button class="btn-action" onclick="setS()">✅ Ustaw</button>
            </div>
            
            <div class="control-group">
                <label>Termoobieg:</label>
                <select id="fan">
                    <option value="0">OFF</option>
                    <option value="1" selected>ON</option>
                    <option value="2">CYKL</option>
                </select>
                <span style="margin-left: 10px;">ON:</span>
                <input id="fon" type="number" value="10" style="width:60px;">
                <span>s</span>
                <span style="margin-left: 10px;">OFF:</span>
                <input id="foff" type="number" value="60" style="width:60px;">
                <span>s</span>
                <button class="btn-action" onclick="setF()">✅ Ustaw</button>
            </div>
        </div>
        
        <!-- Footer -->
        <div class="footer">
            <a href="/creator">📝 Nowy Profil</a>
            <a href="/sensors">🔧 Czujniki</a>
            <a href="/wifi">📶 WiFi</a>
            <a href="/update">📦 OTA Update</a>
        </div>
    </div>


    <script>
    let currentProfileSource = 'sd';
    
    // Update smoke value display
    document.getElementById('smoke').oninput = function() {
        document.getElementById('smokeVal').textContent = this.value;
    };
    
    // Format time helper
    function formatTime(seconds) {
        if(isNaN(seconds) || seconds < 0) return "--:--:--";
        const h = Math.floor(seconds / 3600);
        const m = Math.floor((seconds % 3600) / 60);
        const s = seconds % 60;
        return String(h).padStart(2,'0') + ':' + String(m).padStart(2,'0') + ':' + String(s).padStart(2,'0');
    }
    
    // Fetch status from server
    function fetchStatus() {
        fetch('/status')
            .then(r => r.json())
            .then(data => {
                // Update temperatures
                document.getElementById('temp-chamber').textContent = data.tChamber.toFixed(1) + '°C';
                document.getElementById('temp-meat').textContent = data.tMeat.toFixed(1) + '°C';
                document.getElementById('temp-target').textContent = data.tSet.toFixed(1) + '°C';
                
                // Update status badge
                let statusClass = 'status-idle';
                let statusText = data.mode;
                
                if(data.mode.includes('PAUSE')) {
                    statusClass = 'status-pause';
                    statusText = 'PAUZA';
                } else if(data.mode.includes('ERROR')) {
                    statusClass = 'status-error';
                    statusText = 'BŁĄD';
                } else if(data.mode === 'AUTO') {
                    statusClass = 'status-auto';
                } else if(data.mode === 'MANUAL') {
                    statusClass = 'status-manual';
                }
                
                const badge = document.getElementById('status-badge');
                badge.className = 'status-badge ' + statusClass;
                badge.textContent = statusText;
                
                // Update info
                document.getElementById('power-mode').textContent = data.powerModeText;
                document.getElementById('fan-mode').textContent = data.fanModeText;
                document.getElementById('smoke-level').textContent = Math.round((data.smokePwm/255)*100) + '%';
                
                // Update timer section
                const timerSection = document.getElementById('timer-section');
                if(data.mode === 'AUTO' || data.mode === 'MANUAL') {
                    timerSection.classList.add('active');
                    document.getElementById('timer-elapsed').textContent = formatTime(data.elapsedTimeSec);
                    
                    if(data.mode === 'AUTO') {
                        document.getElementById('step-name').textContent = 'Krok: ' + data.stepName;
                        document.getElementById('countdown-section').style.display = 'block';
                        document.getElementById('nextStepBtn').style.display = 'inline-block';
                        const remaining = Math.max(0, data.stepTotalTimeSec - data.elapsedTimeSec);
                        document.getElementById('timer-remaining').textContent = formatTime(remaining);
                        
                        // Pokaż pozostały czas całego procesu
                        const processSection = document.getElementById('process-total-section');
                        if(data.remainingProcessTimeSec && data.remainingProcessTimeSec > 0) {
                            processSection.style.display = 'block';
                            document.getElementById('process-remaining').textContent = formatTime(data.remainingProcessTimeSec);
                        } else {
                            processSection.style.display = 'none';
                        }
                    } else {
                        document.getElementById('step-name').textContent = 'Tryb Manualny';
                        document.getElementById('countdown-section').style.display = 'none';
                        document.getElementById('nextStepBtn').style.display = 'none';
                    }
                } else {
                    timerSection.classList.remove('active');
                }
                
                // Update active profile
                let profileName = data.activeProfile.replace('/profiles/','').replace('github:','[GitHub] ');
                document.getElementById('active-profile').textContent = profileName;
            })
            .catch(e => console.error('Status fetch error:', e));
    }
    
    // Control functions
    function startManual() {
        fetch('/mode/manual').then(fetchStatus);
    }
    
    function startAuto() {
        fetch('/auto/start').then(fetchStatus);
    }
    
    function stopProcess() {
        if(confirm('Czy na pewno zatrzymać proces?')) {
            fetch('/auto/stop').then(fetchStatus);
        }
    }
    
    function resetTimer() {
        fetch('/timer/reset').then(r => r.text()).then(() => fetchStatus());
    }
    
    function nextStep() {
        if(confirm('Czy na pewno pominąć bieżący krok?')) {
            fetch('/auto/next_step').then(r => r.text()).then(() => fetchStatus());
        }
    }
    
    // Manual settings
    function setT() {
        fetch('/manual/set?tSet=' + document.getElementById('tSet').value).then(fetchStatus);
    }
    
    function setP() {
        fetch('/manual/power?val=' + document.getElementById('power').value).then(fetchStatus);
    }
    
    function setS() {
        fetch('/manual/smoke?val=' + document.getElementById('smoke').value).then(fetchStatus);
    }
    
    function setF() {
        const mode = document.getElementById('fan').value;
        const on = document.getElementById('fon').value;
        const off = document.getElementById('foff').value;
        fetch(`/manual/fan?mode=${mode}&on=${on}&off=${off}`).then(fetchStatus);
    }
    
    // Profile management
    function sourceChanged() {
        currentProfileSource = document.getElementById('profileSource').value;
        document.getElementById('reloadSdBtn').style.display = 
            currentProfileSource === 'sd' ? 'inline-block' : 'none';
        loadProfiles();
    }
    
    function loadProfiles() {
        const url = currentProfileSource === 'sd' ? '/api/profiles' : '/api/github_profiles';
        fetch(url)
            .then(r => r.json())
            .then(profiles => {
                const list = document.getElementById('profileList');
                list.innerHTML = '';
                profiles.forEach(p => {
                    const name = p.replace('/profiles/','');
                    const option = document.createElement('option');
                    option.value = name;
                    option.textContent = name;
                    list.appendChild(option);
                });
            });
    }
    
    function selectProfile() {
        const name = document.getElementById('profileList').value;
        if(!name) return;
        fetch(`/profile/select?name=${name}&source=${currentProfileSource}`)
            .then(r => r.text())
            .then(msg => {
                alert(msg);
                fetchStatus();
            });
    }
    
    function editProfile() {
        const name = document.getElementById('profileList').value;
        if(!name) return;
        window.location.href = `/creator?edit=${name}&source=${currentProfileSource}`;
    }
    
    function reloadProfiles() {
        if(currentProfileSource === 'sd') {
            fetch('/profile/reload').then(r => {
                if(!r.ok) alert('Błąd odczytu karty SD!');
                loadProfiles();
                fetchStatus();
            });
        }
    }
    
    // Initialize
    loadProfiles();
    fetchStatus();
    setInterval(fetchStatus, 1000);
    </script>
</body>
</html>
)rawliteral";

// Szablon strony kreatora
static const char HTML_TEMPLATE_CREATOR[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head><meta charset='utf-8'><title>Kreator/Edytor Profili</title>
<meta name="viewport" content="width=device-width,initial-scale=1">
<style> body{font-family:sans-serif;background:#111;color:#eee;padding:10px;} h2,h3{margin-top:20px; border-bottom: 1px solid #333; padding-bottom: 5px;} button{margin:4px;padding:8px 12px;background:#333;color:#fff;border:1px solid #555;border-radius:4px; cursor: pointer;} button:hover{background:#555;} input,select{padding:6px;margin:4px;background:#222;color:#eee;border:1px solid #444;border-radius:4px; width: 95%; box-sizing: border-box;} .section{background:#222;padding:10px;border-radius:8px;margin-bottom:10px;} .btn-start{background:#1a5f1a;} .btn-start:hover{background:#2a8f2a;} .btn-stop{background:#8b0000;} .btn-stop:hover{background:#cd0000;} fieldset{border: 1px solid #444; border-radius: 5px; margin-top: 10px;} legend{font-weight: bold; color: #bbb;} .step-preview{background: #333; border-radius: 4px; padding: 5px; margin-bottom: 5px; font-size: 0.9em; cursor: pointer;} </style>
</head><body>
<h2 id="creator-title">📝 Kreator Profili</h2>
<div class="section">
    <fieldset>
        <legend>Krok <span id="step-counter">1</span></legend>
        Nazwa kroku: <input type="text" id="stepName" value="Krok 1"><br>
        Temp. komory (°C): <input type="number" id="stepTSet" value="60"><br>
        Temp. mięsa (°C): <input type="number" id="stepTMeat" value="0"><br>
        Min. czas (minuty): <input type="number" id="stepMinTime" value="60"><br>
        Tryb mocy (1-3): <input type="number" id="stepPowerMode" value="2" min="1" max="3"><br>
        Moc dymu (0-255): <input type="number" id="stepSmoke" value="150" min="0" max="255"><br>
        Tryb wentylatora: <select id="stepFanMode"><option value="0">OFF</option><option value="1" selected>ON</option><option value="2">CYKL</option></select><br>
        Czas ON went. (s): <input type="number" id="stepFanOn" value="10"><br>
        Czas OFF went. (s): <input type="number" id="stepFanOff" value="60"><br>
        Użyj temp. mięsa?: <input type="checkbox" id="stepUseMeatTemp"><br>
        <button id="addStepBtn" onclick="addStep()">Dodaj krok</button>
    </fieldset>
    <fieldset>
        <legend>Zbudowany profil</legend>
        <div id="steps-preview"></div>
        Nazwa pliku: <input type="text" id="profileFilename" placeholder="np. boczek.prof"><br>
        <button onclick="saveProfile()" class="btn-start">Zapisz profil na karcie SD</button>
        <button onclick="saveProfileToPC()">Zapisz na komputerze</button>
        <button onclick="clearCreator()" class="btn-stop" style="float: right;">Wyczyść</button>
    </fieldset>
</div>
<hr><a href="/">⬅️ Wróć do strony głównej</a>
<script>
let newProfileSteps = [];
let stepCounter = 1;
let editIndex = -1;
document.addEventListener('DOMContentLoaded', function() {
    const params = new URLSearchParams(window.location.search);
    const profileToEdit = params.get('edit');
    const source = params.get('source') || 'sd';
    if (profileToEdit) {
        document.getElementById('creator-title').textContent = '📝 Edytor Profilu: ' + profileToEdit;
        document.getElementById('profileFilename').value = profileToEdit;
        document.getElementById('profileFilename').readOnly = true;
        fetch(`/profile/get?name=${profileToEdit}&source=${source}`)
            .then(r => r.json())
            .then(data => {
                newProfileSteps = data;
                updatePreview();
                if (data.length > 0) {
                    stepCounter = data.length + 1;
                    document.getElementById('step-counter').textContent = stepCounter;
                }
            });
    }
});
function addStep(){const e={name:document.getElementById("stepName").value, tSet:document.getElementById("stepTSet").value, tMeat:document.getElementById("stepTMeat").value, minTime:document.getElementById("stepMinTime").value, powerMode:document.getElementById("stepPowerMode").value, smoke:document.getElementById("stepSmoke").value, fanMode:document.getElementById("stepFanMode").value, fanOn:document.getElementById("stepFanOn").value, fanOff:document.getElementById("stepFanOff").value, useMeatTemp:document.getElementById("stepUseMeatTemp").checked?1:0};if(editIndex===-1){newProfileSteps.push(e);stepCounter++}else{newProfileSteps[editIndex]=e;editIndex=-1}updatePreview();document.getElementById('step-counter').textContent=stepCounter;document.getElementById('stepName').value="Krok "+stepCounter;document.getElementById('addStepBtn').textContent='Dodaj krok';}
function updatePreview(){const e=document.getElementById("steps-preview");e.innerHTML="";newProfileSteps.forEach((t,n)=>{const o=document.createElement("div");o.className="step-preview";o.textContent=`Krok ${n+1}: ${t.name}; ${t.tSet}°C; ${t.minTime}min`;o.onclick=function(){loadStepForEdit(n)};e.appendChild(o)})}
function loadStepForEdit(e){const t=newProfileSteps[e];document.getElementById("stepName").value=t.name;document.getElementById("stepTSet").value=t.tSet;document.getElementById("stepTMeat").value=t.tMeat;document.getElementById("stepMinTime").value=t.minTime;document.getElementById("stepPowerMode").value=t.powerMode;document.getElementById("stepSmoke").value=t.smoke;document.getElementById("stepFanMode").value=t.fanMode;document.getElementById("stepFanOn").value=t.fanOn;document.getElementById("stepFanOff").value=t.fanOff;document.getElementById("stepUseMeatTemp").checked=1==t.useMeatTemp;editIndex=e;document.getElementById("step-counter").textContent=e+1;document.getElementById("addStepBtn").textContent="Aktualizuj krok";window.scrollTo(0,0)}
function clearCreator(){if(confirm("Czy na pewno chcesz wyczyścić cały kreator? Niezapisane zmiany zostaną utracone.")){newProfileSteps=[];stepCounter=1;editIndex=-1;document.getElementById("step-counter").textContent="1";document.getElementById("steps-preview").innerHTML="";document.getElementById("profileFilename").value="";document.getElementById("profileFilename").readOnly=false;document.getElementById("creator-title").textContent="📝 Kreator Profili"}}
function saveProfile(){const e=document.getElementById("profileFilename").value;if(!e)return alert("Wpisz nazwę pliku!");if(0===newProfileSteps.length)return alert("Dodaj przynajmniej jeden krok!");let t=`# Profil wygenerowany przez kreator WWW\n`;newProfileSteps.forEach(e=>{t+=`${e.name};${e.tSet};${e.tMeat};${e.minTime};${e.powerMode};${e.smoke};${e.fanMode};${e.fanOn};${e.fanOff};${e.useMeatTemp}\n`});const n=new URLSearchParams;n.append("filename",e),n.append("data",t),fetch("/profile/create",{method:"POST",body:n}).then(e=>e.text().then(t=>({ok:e.ok,text:t}))).then(({ok:e,text:t})=>{alert(t),e&&(window.location.href="/")})}
function saveProfileToPC(){const e=document.getElementById("profileFilename").value;if(!e)return alert("Wpisz nazwę pliku!");if(0===newProfileSteps.length)return alert("Dodaj przynajmniej jeden krok!");let t=`# Profil wygenerowany przez kreator WWW\n`;newProfileSteps.forEach(e=>{t+=`${e.name};${e.tSet};${e.tMeat};${e.minTime};${e.powerMode};${e.smoke};${e.fanMode};${e.fanOn};${e.fanOff};${e.useMeatTemp}\n`});const n=new Blob([t],{type:"text/plain;charset=utf-8"}),o=URL.createObjectURL(n),d=document.createElement("a");d.href=o;let l=e.endsWith(".prof")?e:e+".prof";d.download=l,document.body.appendChild(d),d.click(),document.body.removeChild(d),URL.revokeObjectURL(o)}
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
<hr>
<div style="margin-top: 20px;">
    <a href="/">🏠 Strona Główna</a> | 
    <a href="/sensors">🔧 Czujniki</a> | 
    <a href="/creator">📝 Kreator</a>
</div>
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

    xhr.onloadend = function() {
        if (progress_bar.value === 100) {
            status_div.innerHTML = "<span style='color: lime;'>Aktualizacja zakończona!</span> Urządzenie uruchomi się ponownie...";
            setTimeout(function(){ 
                status_div.innerHTML = "Próba ponownego połączenia...";
                window.location.href = '/'; 
            }, 5000);
        } else if (xhr.status !== 200) {
            status_div.innerHTML = "<span style='color: red;'>Błąd aktualizacji:</span> " + xhr.responseText;
        } else {
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

// =================================================================
// FUNKCJE POMOCNICZE
// =================================================================

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
    static char jsonBuffer[600];
    
    // Zmienne do wysłania
    double tc, tm, ts;
    int pm, fm, sm;
    ProcessState st;
    unsigned long elapsedSec = 0;
    unsigned long stepTotalSec = 0;
    const char* stepName = "";
    unsigned long totalProcessTimeSec = 0;
    unsigned long remainingProcessTimeSec = 0;
    char activeProfile[64] = "Brak";

    state_lock();
    st = g_currentState;
    tc = g_tChamber;
    tm = g_tMeat;
    ts = g_tSet;
    pm = g_powerMode;
    fm = g_fanMode;
    sm = g_manualSmokePwm;
    totalProcessTimeSec = g_processStats.totalProcessTimeSec;
    remainingProcessTimeSec = g_processStats.remainingProcessTimeSec;
    strncpy(activeProfile, storage_get_profile_path(), sizeof(activeProfile) - 1);
    activeProfile[sizeof(activeProfile) - 1] = '\0';

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
    
    snprintf(jsonBuffer, sizeof(jsonBuffer),
        "{\"tChamber\":%.1f,\"tMeat\":%.1f,\"tSet\":%.1f,\"powerMode\":%d,\"fanMode\":%d,\"smokePwm\":%d,\"mode\":\"%s\",\"state\":%d,\"powerModeText\":\"%s\",\"fanModeText\":\"%s\",\"elapsedTimeSec\":%lu,\"stepName\":\"%s\",\"stepTotalTimeSec\":%lu,\"activeProfile\":\"%s\",\"remainingProcessTimeSec\":%lu}",
        tc, tm, ts, pm, fm, sm, getStateString(st), (int)st, powerModeStr, fanModeStr, elapsedSec, stepName, stepTotalSec, cleanProfileName, remainingProcessTimeSec);
    
    return jsonBuffer;
}

// =================================================================
// ENDPOINTY DO ZARZĄDZANIA CZUJNIKAMI
// =================================================================

// Endpoint: Informacje o czujnikach
static void handleSensorInfo() {
    String json = "{";
    json += "\"chamber_index\":" + String(getChamberSensorIndex()) + ",";
    json += "\"meat_index\":" + String(getMeatSensorIndex()) + ",";
    json += "\"total_sensors\":" + String(sensors.getDeviceCount()) + ",";
    json += "\"identified\":" + String(areSensorsIdentified() ? "true" : "false");
    json += "}";
    server.send(200, "application/json", json);
}

// Endpoint: Zmiana przypisań czujników
static void handleSensorReassign() {
    if (server.hasArg("chamber") && server.hasArg("meat")) {
        int chamber = server.arg("chamber").toInt();
        int meat = server.arg("meat").toInt();
        
        if (chamber >= 0 && meat >= 0 && chamber != meat) {
            reassignSensors(chamber, meat);
            server.send(200, "application/json", "{\"status\":\"ok\"}");
        } else {
            server.send(400, "application/json", "{\"error\":\"Invalid indices\"}");
        }
    } else {
        server.send(400, "application/json", "{\"error\":\"Missing parameters\"}");
    }
}

// Endpoint: Auto-wykrywanie czujników
static void handleSensorAutoDetect() {
    identifyAndAssignSensors();
    if (areSensorsIdentified()) {
        server.send(200, "application/json", "{\"message\":\"Sensors auto-detected and assigned\"}");
    } else {
        server.send(500, "application/json", "{\"error\":\"Auto-detection failed\"}");
    }
}

// =================================================================
// STRONA ZARZĄDZANIA CZUJNIKAMI
// =================================================================

static void handleSensorsPage() {
    String html = R"rawliteral(
    <!DOCTYPE html>
    <html>
    <head>
        <meta charset='utf-8'>
        <title>Zarządzanie Czujnikami</title>
        <meta name="viewport" content="width=device-width,initial-scale=1">
        <style>
            * { margin: 0; padding: 0; box-sizing: border-box; }
            body { font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif;
                   background: linear-gradient(135deg, #1a1a2e 0%, #16213e 100%);
                   color: #eee; padding: 15px; min-height: 100vh; }
            .container { max-width: 800px; margin: 0 auto; }
            .header { background: linear-gradient(135deg, #2196f3 0%, #1976d2 100%);
                     padding: 20px; border-radius: 15px; margin-bottom: 20px;
                     box-shadow: 0 8px 20px rgba(33,150,243,0.3); text-align: center; }
            .header h1 { font-size: 2em; margin: 0; text-shadow: 2px 2px 4px rgba(0,0,0,0.5); }
            .section { background: rgba(255,255,255,0.05); backdrop-filter: blur(10px);
                      border: 1px solid rgba(255,255,255,0.1); padding: 20px;
                      border-radius: 12px; margin-bottom: 15px;
                      box-shadow: 0 4px 16px rgba(0,0,0,0.2); }
            .section h3 { color: #fff; margin-bottom: 15px; padding-bottom: 10px;
                         border-bottom: 2px solid rgba(255,255,255,0.2); font-size: 1.3em; }
            button { padding: 12px 20px; margin: 5px; border: none; border-radius: 8px;
                    font-size: 1em; font-weight: 600; cursor: pointer; transition: all 0.3s;
                    box-shadow: 0 4px 12px rgba(0,0,0,0.3); }
            button:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(0,0,0,0.4); }
            .btn-action { background: linear-gradient(135deg, #2196f3 0%, #1976d2 100%); color: white; }
            .btn-action:hover { background: linear-gradient(135deg, #1976d2 0%, #1565c0 100%); }
            .btn-success { background: linear-gradient(135deg, #4caf50 0%, #45a049 100%); color: white; }
            .btn-warning { background: linear-gradient(135deg, #ff9800 0%, #f57c00 100%); color: white; }
            select, input { padding: 10px; margin: 5px; background: rgba(0,0,0,0.3);
                           color: #fff; border: 2px solid rgba(255,255,255,0.2);
                           border-radius: 8px; font-size: 1em; }
            .control-group { display: flex; align-items: center; flex-wrap: wrap;
                            gap: 10px; margin: 10px 0; padding: 12px;
                            background: rgba(0,0,0,0.2); border-radius: 8px; }
            .status-box { padding: 10px; background: rgba(0,0,0,0.3); border-radius: 8px;
                         margin: 10px 0; font-family: monospace; }
            .footer { text-align: center; padding: 20px; margin-top: 20px;
                     border-top: 1px solid rgba(255,255,255,0.1); }
            .footer a { color: #2196f3; text-decoration: none; margin: 0 15px;
                       font-size: 1.1em; transition: all 0.3s; }
            .footer a:hover { color: #64b5f6; text-shadow: 0 0 10px rgba(33,150,243,0.5); }
            @media (max-width: 600px) { .header h1 { font-size: 1.5em; } }
        </style>
    </head>
    <body>
        <div class="container">
            <div class="header">
                <h1>🔧 Zarządzanie Czujników Temperatury</h1>
                <div class="version">Wędzarnia v3.3</div>
            </div>
            
            <div class="section">
                <h3>📊 Status Czujników</h3>
                <div id="sensorStatus" class="status-box">
                    Ładowanie informacji o czujnikach...
                </div>
            </div>
            
            <div class="section">
                <h3>⚙️ Przypisania Czujników</h3>
                <div class="control-group">
                    <label>Komora:</label>
                    <select id="chamberSelect"></select>
                    <label>Mięso:</label>
                    <select id="meatSelect"></select>
                </div>
                <div style="text-align: center;">
                    <button class="btn-action" onclick="saveAssignments()">💾 Zapisz Przypisania</button>
                    <button class="btn-warning" onclick="autoDetect()">🔄 Auto-Wykrywanie</button>
                    <button class="btn-success" onclick="resetToDefaults()">🔁 Przywróć Domyślne</button>
                </div>
            </div>
            
            <div class="section">
                <h3>📝 Instrukcja</h3>
                <p><strong>Domyślne przypisanie:</strong></p>
                <ul style="margin-left: 20px; margin-bottom: 15px;">
                    <li><strong>Czujnik 0</strong> → Temperatura komory</li>
                    <li><strong>Czujnik 1</strong> → Temperatura mięsa</li>
                </ul>
                <p><em>Uwaga:</em> Zmiana przypisań wymaga restartu procesu wędzenia.</p>
            </div>
            
            <div class="footer">
                <a href="/">🏠 Strona Główna</a>
                <a href="/creator">📝 Kreator Profili</a>
                <a href="/update">📦 Aktualizacja</a>
            </div>
        </div>
        
        <script>
        function loadSensorInfo() {
            fetch('/api/sensors')
                .then(r => r.json())
                .then(data => {
                    const status = document.getElementById('sensorStatus');
                    status.innerHTML = 
                        `<strong>Stan:</strong> ${data.identified ? '✅ Zidentyfikowano' : '⚠️ Niezidentyfikowano'}<br>
                         <strong>Liczba czujników:</strong> ${data.total_sensors}<br>
                         <strong>Komora:</strong> Czujnik ${data.chamber_index}<br>
                         <strong>Mięso:</strong> Czujnik ${data.meat_index}`;
                    
                    // Wypełnij selecty
                    const chamberSelect = document.getElementById('chamberSelect');
                    const meatSelect = document.getElementById('meatSelect');
                    chamberSelect.innerHTML = '';
                    meatSelect.innerHTML = '';
                    
                    for(let i = 0; i < data.total_sensors; i++) {
                        chamberSelect.innerHTML += `<option value="${i}" ${i==data.chamber_index?'selected':''}>Czujnik ${i}</option>`;
                        meatSelect.innerHTML += `<option value="${i}" ${i==data.meat_index?'selected':''}>Czujnik ${i}</option>`;
                    }
                })
                .catch(e => {
                    document.getElementById('sensorStatus').innerHTML = 
                        '❌ Błąd ładowania danych czujników';
                    console.error('Sensor info error:', e);
                });
        }
        
        function saveAssignments() {
            const chamber = document.getElementById('chamberSelect').value;
            const meat = document.getElementById('meatSelect').value;
            
            if(chamber == meat) {
                alert('❌ Nie można przypisać tego samego czujnika do komory i mięsa!');
                return;
            }
            
            fetch('/api/sensors/reassign', {
                method: 'POST',
                headers: {'Content-Type':'application/x-www-form-urlencoded'},
                body: `chamber=${chamber}&meat=${meat}`
            })
            .then(r => r.json())
            .then(data => {
                if(data.status == 'ok') {
                    alert('✅ Przypisania zapisane pomyślnie!');
                    loadSensorInfo();
                } else {
                    alert('❌ Błąd: ' + (data.error || 'Nieznany błąd'));
                }
            })
            .catch(e => {
                alert('❌ Błąd sieci: ' + e.message);
            });
        }
        
        function autoDetect() {
            if(confirm('Automatyczne wykrywanie zresetuje obecne przypisania. Kontynuować?')) {
                fetch('/api/sensors/autodetect', {method: 'POST'})
                    .then(r => r.json())
                    .then(data => {
                        if(data.message) {
                            alert('✅ ' + data.message);
                        } else {
                            alert('❌ ' + data.error);
                        }
                        loadSensorInfo();
                    })
                    .catch(e => {
                        alert('❌ Błąd: ' + e.message);
                    });
            }
        }
        
        function resetToDefaults() {
            if(confirm('Przywrócić domyślne przypisania (Czujnik 0 → Komora, Czujnik 1 → Mięso)?')) {
                fetch('/api/sensors/reassign', {
                    method: 'POST',
                    headers: {'Content-Type':'application/x-www-form-urlencoded'},
                    body: 'chamber=0&meat=1'
                })
                .then(r => r.json())
                .then(data => {
                    if(data.status == 'ok') {
                        alert('✅ Przywrócono domyślne przypisania!');
                        loadSensorInfo();
                    }
                });
            }
        }
        
        // Załaduj dane przy starcie
        loadSensorInfo();
        // Odświeżaj co 10 sekund
        setInterval(loadSensorInfo, 10000);
        </script>
    </body>
    </html>
    )rawliteral";
    
    server.send(200, "text/html", html);
}

// =================================================================
// HOME ASSISTANT COMMANDS ENDPOINT
// =================================================================

#ifdef ENABLE_HOME_ASSISTANT

// Endpoint do sterowania z Home Assistant
static void handleHACommand() {
    if (!server.hasArg("plain")) {
        server.send(400, "text/plain", "Missing JSON data");
        return;
    }
    
    String json = server.arg("plain");
    StaticJsonDocument<256> doc;
    DeserializationError error = deserializeJson(doc, json);
    
    if (error) {
        server.send(400, "text/plain", "Invalid JSON");
        return;
    }
    
    if (!doc.containsKey("command")) {
        server.send(400, "text/plain", "Missing command field");
        return;
    }
    
    String command = doc["command"].as<String>();
    String response = "OK";
    
    log_msg(LOG_LEVEL_INFO, "HA Command received: " + command);
    
    if (command == "start_auto") {
        process_start_auto();
        response = "Started AUTO mode";
        
    } else if (command == "start_manual") {
        process_start_manual();
        response = "Started MANUAL mode";
        
    } else if (command == "reset_ha") {
        #ifdef ENABLE_HOME_ASSISTANT
        ha_reset_connection();
        #endif
        response = "HA connection reset";

    } else if (command == "stop") {
        allOutputsOff();
        if (state_lock()) {
            g_currentState = ProcessState::IDLE;
            state_unlock();
        }
        response = "Process stopped";
        
    } else if (command == "set_temp") {
        if (doc.containsKey("value")) {
            float temp = doc["value"].as<float>();
            temp = constrain(temp, CFG_T_MIN_SET, CFG_T_MAX_SET);
            if (state_lock()) {
                g_tSet = temp;
                state_unlock();
            }
            storage_save_manual_settings_nvs();
            response = "Temperature set to " + String(temp, 1);
        } else {
            response = "Missing value parameter";
        }
        
    } else if (command == "set_power") {
        if (doc.containsKey("value")) {
            int power = doc["value"].as<int>();
            power = constrain(power, CFG_POWERMODE_MIN, CFG_POWERMODE_MAX);
            if (state_lock()) {
                g_powerMode = power;
                state_unlock();
            }
            storage_save_manual_settings_nvs();
            response = "Power mode set to " + String(power);
        }
        
    } else if (command == "get_status") {
        // Zwróć pełny status jako JSON
        server.send(200, "application/json", getStatusJSON());
        return;
        
    } else {
        response = "Unknown command: " + command;
        server.send(400, "text/plain", response);
        return;
    }
    
    server.send(200, "application/json", "{\"result\":\"" + response + "\"}");
}

// Endpoint do weryfikacji połączenia
static void handleHAPing() {
    server.send(200, "application/json", "{\"status\":\"online\",\"device\":\"wedzarnia\",\"uptime\":" + 
                String(millis() / 1000) + "}");
}

#endif // ENABLE_HOME_ASSISTANT

// =================================================================
// PODSTAWOWE ENDPOINTY
// =================================================================

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
    Serial.println("[WEB] /creator endpoint called");
    server.send_P(200, "text/html", HTML_TEMPLATE_CREATOR);
  });
  server.on("/update", HTTP_GET, []() {
    server.send_P(200, "text/html", HTML_TEMPLATE_OTA);
  });

  server.on("/api/profiles", HTTP_GET, []() {
    server.send(200, "application/json", storage_list_profiles_json());
  });
  server.on("/api/github_profiles", HTTP_GET, []() {
    server.send(200, "application/json", storage_list_github_profiles_json());
  });

  server.on("/profile/get", HTTP_GET, []() {
    if (!server.hasArg("name") || !server.hasArg("source")) {
      server.send(400, "text/plain", "Brak nazwy profilu lub źródła");
      return;
    }
    String profileName = server.arg("name");
    String source = server.arg("source");
    if (source == "sd") {
      server.send(200, "application/json", storage_get_profile_as_json(profileName.c_str()));
    } else if (source == "github") {
      // Edycja profili z GitHuba nie jest wspierana, ale możemy zwrócić jego zawartość
      server.send(200, "application/json", storage_get_profile_as_json(profileName.c_str()));
    } else {
      server.send(400, "text/plain", "Nieznane źródło");
    }
  });

  server.on("/profile/select", HTTP_GET, []() {
    if (server.hasArg("name") && server.hasArg("source")) {
      String profileName = server.arg("name");
      String source = server.arg("source");
      bool success = false;
      if (source == "sd") {
        String fullPath = "/profiles/" + profileName;
        storage_save_profile_path_nvs(fullPath.c_str());
        success = storage_load_profile();
      } else if (source == "github") {
        String githubPath = "github:" + profileName;
        storage_save_profile_path_nvs(githubPath.c_str());
        success = storage_load_github_profile(profileName.c_str());
      }
      if (success) {
        server.send(200, "text/plain", "OK, profil " + profileName + " załadowany.");
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
    String filename = server.arg("filename");
    String data = server.arg("data");
    if (filename.isEmpty()) {
      server.send(400, "text/plain", "Nazwa pliku nie może być pusta.");
      return;
    }
    if (!filename.endsWith(".prof")) { filename += ".prof"; }
    String path = "/profiles/" + filename;
    File file = SD.open(path, FILE_WRITE);
    if (!file) {
      server.send(500, "text/plain", "Nie można otworzyć pliku do zapisu.");
      return;
    }
    file.print(data);
    file.close();
    server.send(200, "text/plain", "Profil '" + filename + "' został pomyślnie zapisany!");
  });

  // Endpointy dla czujników
  server.on("/api/sensors", HTTP_GET, handleSensorInfo);
  server.on("/api/sensors/reassign", HTTP_POST, handleSensorReassign);
  server.on("/api/sensors/autodetect", HTTP_POST, handleSensorAutoDetect);
  server.on("/sensors", HTTP_GET, handleSensorsPage);
  
#ifdef ENABLE_HOME_ASSISTANT
  // Endpointy Home Assistant
  server.on("/ha/command", HTTP_POST, handleHACommand);
  server.on("/ha/ping", HTTP_GET, handleHAPing);
#endif

// =================================================================
// OTA UPDATE ENDPOINT (POPRAWIONY)
// =================================================================

server.on("/update", HTTP_POST, 
    []() { // On success
        server.sendHeader("Connection", "close");
        server.send(200, "text/plain", (Update.hasError()) ? "BŁĄD" : "OK");

        // POPRAWIONA INICJALIZACJA WDT
        Serial.println("Re-initializing WDT...");
        esp_task_wdt_config_t wdt_config = {
            .timeout_ms = WDT_TIMEOUT * 1000,
            .idle_core_mask = (1 << portNUM_PROCESSORS) - 1,
            .trigger_panic = true,
        };
        esp_task_wdt_init(&wdt_config);
        esp_task_wdt_add(NULL);

        delay(100);
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
        yield(); 
    }
);

// =================================================================
// MANUAL CONTROL ENDPOINTS
// =================================================================

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

// =================================================================
// WIFI CONFIGURATION ENDPOINTS
// =================================================================

  server.on("/wifi", HTTP_GET, []() {
    String html = "<html><head><meta charset='utf-8'><meta name='viewport' content='width=device-width,initial-scale=1'>";
    html += "<style>body{font-family:sans-serif;background:#111;color:#eee;padding:20px;}input{padding:8px;margin:5px 0;width:200px;}</style>";
    html += "</head><body><h2>WiFi STA Configuration</h2>";
    html += "<form method='POST' action='/wifi/save'>";
    html += "SSID: <input name='ssid' value='" + String(storage_get_wifi_ssid()) + "'><br>";
    html += "Password: <input name='pass' type='password'><br>";
    html += "<input type='submit' value='Connect'>";
    html += "</form><br><a href='/'>← Back</a></body></html>";
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