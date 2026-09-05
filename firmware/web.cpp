/*
 * web.cpp - WiFi AP + dashboard web + endpoint /api/data (FASE 3)
 *
 * Access Point fissi:
 *   SSID: EngineSimulator2.0 | Pass: 1234567890 | IP: 192.168.254.1 (DHCP per i client)
 *
 * Route:
 *   /            -> dashboard motore (HTML statico, aggiornamento via /api/data)
 *   /setup       -> selezione ruota fonica/cam + salvataggio su NVS
 *   /api/data    -> JSON con stato motore + timing iniettori/candele
 *   /api/wheel   -> POST: salva config.wheel su NVS (chiamato da /setup)
 *
 * Nota: ESPAsyncWebServer gira sul core 0 (asincrono, non blocca la simulazione).
 */

#include "web.h"
#include "globals.h"
#include "timing.h"
#include "comms.h"
#include <WiFi.h>
#include <ESPAsyncWebServer.h>
#include <Preferences.h>

extern wheels Wheels[];

static AsyncWebServer server(80);
static Preferences prefs;

// --- HELPERS ---
static const char *engineStateName() {
  switch (engineState) {
    case ENGINE_OFF:       return "OFF";
    case ENGINE_CRANKING:  return "CRANKING";
    case ENGINE_RUNNING:   return "RUNNING";
    case ENGINE_STOPPING:  return "STOPPING";
    default:               return "UNKNOWN";
  }
}

static String wheelName(uint8_t idx) {
  char buf[60];
  strcpy_P(buf, Wheels[idx].decoder_name);
  return String(buf);
}

// --- HTML DASHBOARD (statico, JS popola via /api/data) ---
const char INDEX_HTML[] PROGMEM = R"HTML(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Engine Simulator 2.0</title>
<style>
:root{
  --bg:#0b0f14; --panel:#141b24; --panel2:#1a232e; --line:#263341;
  --txt:#e6edf3; --dim:#8ba0b4; --acc:#00d4ff; --ok:#22c55e; --warn:#f59e0b;
  --hot:#ef4444; --rpm:#00d4ff;
}
*{margin:0;padding:0;box-sizing:border-box}
body{
  background:radial-gradient(1200px 700px at 70% -10%,#12202e 0%,var(--bg) 55%);
  color:var(--txt); font-family:'Segoe UI',system-ui,sans-serif; min-height:100vh; padding:16px;
}
.wrap{max-width:1180px;margin:0 auto}
header{
  display:flex;align-items:center;justify-content:space-between;gap:12px;flex-wrap:wrap;
  padding:14px 18px;background:var(--panel);border:1px solid var(--line);border-radius:14px;
  box-shadow:0 4px 24px rgba(0,0,0,.35);
}
.brand{display:flex;align-items:center;gap:12px}
.brand .logo{
  width:42px;height:42px;border-radius:11px;display:grid;place-items:center;font-size:22px;
  background:linear-gradient(135deg,#0e2a3a,#123);border:1px solid var(--line);
}
.brand h1{font-size:1.15rem;font-weight:700;letter-spacing:.4px}
.brand small{display:block;color:var(--dim);font-size:.72rem;letter-spacing:1.4px;text-transform:uppercase}
.statebox{display:flex;align-items:center;gap:14px}
#state-badge{
  font-size:.78rem;font-weight:800;letter-spacing:1.6px;padding:7px 14px;border-radius:999px;
  background:#1a2430;border:1px solid var(--line);color:var(--dim);transition:all .3s;
}
#state-badge.on{background:rgba(34,197,94,.12);border-color:var(--ok);color:var(--ok);box-shadow:0 0 14px rgba(34,197,94,.35)}
#state-badge.crank{background:rgba(245,158,11,.12);border-color:var(--warn);color:var(--warn)}
#state-badge.stop{background:rgba(239,68,68,.12);border-color:var(--hot);color:var(--hot)}
.wifi-dot{width:11px;height:11px;border-radius:50%;background:var(--hot);transition:background .3s}
.wifi-dot.on{background:var(--ok);box-shadow:0 0 10px var(--ok)}
#client-count{font-size:.75rem;color:var(--dim)}
nav{display:flex;gap:8px;margin-top:14px}
nav a{
  text-decoration:none;color:var(--dim);font-size:.82rem;font-weight:600;letter-spacing:.5px;
  padding:8px 16px;border-radius:10px;border:1px solid transparent;transition:all .2s;
}
nav a:hover{color:var(--txt);border-color:var(--line);background:var(--panel2)}
nav a.active{color:var(--acc);border-color:var(--acc);background:rgba(0,212,255,.07)}
.grid{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-top:14px}
@media(max-width:900px){.grid{grid-template-columns:1fr}}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:16px;
  box-shadow:0 4px 20px rgba(0,0,0,.28)}
.card h2{font-size:.78rem;color:var(--dim);text-transform:uppercase;letter-spacing:1.4px;margin-bottom:10px}

/* Gauge RPM */
.gauge{position:relative;width:100%;max-width:340px;margin:0 auto}
.gauge svg{width:100%;display:block}
#rpm-num{font-size:2.6rem;font-weight:800;fill:var(--rpm);text-anchor:middle}
#rpm-label{font-size:.68rem;fill:var(--dim);text-anchor:middle;letter-spacing:1px}
#rpm-val{text-align:center;font-size:1.9rem;font-weight:800;color:var(--rpm);margin-top:8px;
  font-variant-numeric:tabular-nums;text-shadow:0 0 18px rgba(0,212,255,.45)}
#rpm-unit{font-size:.75rem;color:var(--dim);margin-left:5px;font-weight:600}

/* Barre */
.bar-band{display:flex;align-items:center;gap:12px;margin:10px 0}
.bar-l{width:72px;font-size:.8rem;color:var(--dim);font-weight:700}
.bar{flex:1;height:14px;background:#0d141c;border-radius:8px;border:1px solid var(--line);overflow:hidden}
.bar-f{height:100%;width:0%;border-radius:8px;transition:width .35s cubic-bezier(.22,1,.36,1)}
.bar-v{width:78px;text-align:right;font-size:.9rem;font-weight:700;font-variant-numeric:tabular-nums}
.mini{display:grid;grid-template-columns:1fr 1fr;gap:14px;margin-top:4px}
.mini .item{background:var(--panel2);border:1px solid var(--line);border-radius:10px;padding:10px 12px}
.mini .k{font-size:.68rem;color:var(--dim);text-transform:uppercase;letter-spacing:1px}
.mini .v{font-size:1.15rem;font-weight:700;margin-top:3px}

/* Timing tables */
table{width:100%;border-collapse:collapse;font-size:.82rem}
th{color:var(--dim);text-align:left;font-size:.68rem;text-transform:uppercase;letter-spacing:1px;padding:6px 8px;border-bottom:1px solid var(--line)}
td{padding:7px 8px;border-bottom:1px solid rgba(38,51,65,.5)}
td.ch{font-weight:700}
.val-in{color:var(--acc)}
.val-ig{color:var(--warn)}
.on{color:var(--ok)}
.off{color:var(--dim)}
.led{display:inline-block;width:9px;height:9px;border-radius:50%;margin-right:6px;vertical-align:middle}
.led.on{background:var(--ok);box-shadow:0 0 8px var(--ok)}
.led.off{background:#3a4a5a}

/* Setup */
select{
  width:100%;padding:11px 12px;border-radius:10px;background:var(--panel2);color:var(--txt);
  border:1px solid var(--line);font-size:.92rem;margin:10px 0;
}
button{
  width:100%;padding:12px;border:none;border-radius:10px;font-weight:800;font-size:.9rem;cursor:pointer;
  background:linear-gradient(135deg,#00a2c9,#0077b6);color:#fff;letter-spacing:.6px;transition:all .2s;
}
button:hover{filter:brightness(1.15);box-shadow:0 0 18px rgba(0,212,255,.35)}
.btn-run{margin-top:14px;background:linear-gradient(135deg,#16a34a,#15803d)}
.btn-run.stop{background:linear-gradient(135deg,#dc2626,#b91c1c)}
#wheel-current{color:var(--acc);font-weight:800}
.foot{text-align:center;color:var(--dim);font-size:.72rem;margin-top:16px;letter-spacing:.8px}
#save-msg{margin-top:10px;font-size:.82rem;color:var(--ok);min-height:16px;text-align:center}
</style>
</head>
<body>
<div class="wrap">
  <header>
    <div class="brand">
      <div class="logo">&#9881;</div>
      <div>
        <h1>Engine Simulator 2.0</h1>
        <small>Speeduino V0.4 test bench</small>
      </div>
    </div>
    <div class="statebox">
      <div class="wifi-dot" id="wifi-dot"></div>
      <span id="client-count">0 client</span>
      <span id="state-badge">OFF</span>
    </div>
  </header>

  <nav>
    <a href="/" class="active">Dashboard</a>
    <a href="/setup">Setup ruota fonica</a>
  </nav>

  <div class="grid">
    <!-- Gauge RPM -->
    <div class="card">
      <h2>Giri motore</h2>
      <div class="gauge">
        <svg viewBox="0 0 340 200">
          <defs>
            <linearGradient id="gRpm" x1="0" y1="0" x2="1" y2="0">
              <stop offset="0%" stop-color="#00d4ff"/><stop offset="100%" stop-color="#7c3aed"/>
            </linearGradient>
          </defs>
          <path d="M 40 176 A 130 130 0 0 1 300 176" fill="none" stroke="#0d141c" stroke-width="16" stroke-linecap="round"/>
      <path id="rpm-arc" d="M 40 176 A 130 130 0 0 1 300 176" fill="none" stroke="url(#gRpm)"
        stroke-width="16" stroke-linecap="round" stroke-dasharray="0 408.4" style="transition:stroke-dasharray .3s cubic-bezier(.22,1,.36,1)"/>
          <text id="rpm-num" x="170" y="118">0</text>
          <text id="rpm-label" x="170" y="146">RPM</text>
        </svg>
        <div id="rpm-val">0</div><span id="rpm-unit">RPM</span>
      </div>
    </div>

    <!-- Stato + parametri -->
    <div class="card">
      <h2>Stato motore</h2>
      <div class="mini">
        <div class="item"><div class="k">TPS</div><div class="v val-in" id="tps-val">0%</div></div>
        <div class="item"><div class="k">MAP</div><div class="v val-in" id="map-val">100 kPa</div></div>
        <div class="item"><div class="k">CLT</div><div class="v val-in" id="clt-val">20&#176;C</div></div>
        <div class="item"><div class="k">Ventola</div><div class="v" id="fan-val"><span class="led off"></span>OFF</div></div>
      </div>

      <div class="bar-band"><div class="bar-l">TPS</div><div class="bar"><div class="bar-f" id="tps-bar" style="background:linear-gradient(90deg,#00d4ff,#7c3aed)"></div></div><div class="bar-v" id="tps-v">0%</div></div>
      <div class="bar-band"><div class="bar-l">MAP</div><div class="bar"><div class="bar-f" id="map-bar" style="background:linear-gradient(90deg,#22c55e,#84cc16)"></div></div><div class="bar-v" id="map-v">100</div></div>
      <div class="bar-band"><div class="bar-l">CLT</div><div class="bar"><div class="bar-f" id="clt-bar" style="background:linear-gradient(90deg,#f59e0b,#ef4444)"></div></div><div class="bar-v" id="clt-v">20&#176;</div></div>

      <button id="btn-start" class="btn-run" onclick="toggleEngine()">AVVIA</button>
    </div>

    <!-- Timing iniettori -->
    <div class="card">
      <h2>Timing iniettori (Speeduino)</h2>
      <table>
        <thead><tr><th>Canale</th><th>Apertura</th><th>Frequenza</th><th>Duty</th></tr></thead>
        <tbody id="inj-body">
          <tr><td class="ch">INJ1</td><td class="val-in">--</td><td class="val-in">--</td><td class="val-in">--</td></tr>
          <tr><td class="ch">INJ2</td><td class="val-in">--</td><td class="val-in">--</td><td class="val-in">--</td></tr>
          <tr><td class="ch">INJ3</td><td class="val-in">--</td><td class="val-in">--</td><td class="val-in">--</td></tr>
          <tr><td class="ch">INJ4</td><td class="val-in">--</td><td class="val-in">--</td><td class="val-in">--</td></tr>
        </tbody>
      </table>
    </div>

    <!-- Timing candele -->
    <div class="card">
      <h2>Timing candele (Speeduino)</h2>
      <table>
        <thead><tr><th>Canale</th><th>Dwell</th><th>Anticipo</th><th>Stato</th></tr></thead>
        <tbody id="ign-body">
          <tr><td class="ch">IGN1</td><td class="val-ig">--</td><td class="val-ig">--</td><td><span class="led off"></span>--</td></tr>
          <tr><td class="ch">IGN2</td><td class="val-ig">--</td><td class="val-ig">--</td><td><span class="led off"></span>--</td></tr>
          <tr><td class="ch">IGN3</td><td class="val-ig">--</td><td class="val-ig">--</td><td><span class="led off"></span>--</td></tr>
          <tr><td class="ch">IGN4</td><td class="val-ig">--</td><td class="val-ig">--</td><td><span class="led off"></span>--</td></tr>
        </tbody>
      </table>
    </div>
  </div>

  <div class="foot">Ruota fonica attiva: <span id="wheel-name">-</span> &middot; Engine Simulator 2.0</div>
</div>

<script>
const MAXRPM = 8000;
let ARCLEN = 408.4;
function setArc(v){
  const a = document.getElementById('rpm-arc');
  const L = ARCLEN;
  a.style.strokeDasharray = (v/100*L)+' '+L;
}

async function poll(){
  try{
    const r = await fetch('/api/data');
    const d = await r.json();
    // RPM
    const rpm = Math.min(d.rpm, MAXRPM);
    document.getElementById('rpm-num').textContent = Math.round(rpm);
    document.getElementById('rpm-val').textContent = Math.round(rpm);
    setArc(rpm/MAXRPM*100);
    // Stato
    const b = document.getElementById('state-badge');
    b.textContent = d.state;
    b.className = '';
    if(d.state==='RUNNING') b.classList.add('on');
    else if(d.state==='CRANKING') b.classList.add('crank');
    else if(d.state==='STOPPING') b.classList.add('stop');
    // Bottone avvio/stop (riflette anche il pulsante fisico)
    const bs = document.getElementById('btn-start');
    const running = (d.state==='RUNNING' || d.state==='CRANKING');
    bs.textContent = running ? 'STOP' : 'AVVIA';
    bs.className = 'btn-run' + (running ? ' stop' : '');
    // Parametri
    document.getElementById('tps-val').textContent = d.tps_pct.toFixed(0)+'%';
    document.getElementById('tps-bar').style.width = d.tps_pct+'%';
    document.getElementById('tps-v').textContent = d.tps_pct.toFixed(0)+'%';
    document.getElementById('map-val').textContent = d.map_kpa.toFixed(0)+' kPa';
    document.getElementById('map-bar').style.width = (d.map_kpa/100*100)+'%';
    document.getElementById('map-v').textContent = d.map_kpa.toFixed(0);
    document.getElementById('clt-val').textContent = d.clt_c.toFixed(0)+'\u00B0C';
    document.getElementById('clt-bar').style.width = Math.min((d.clt_c/120*100),100)+'%';
    document.getElementById('clt-v').textContent = d.clt_c.toFixed(0)+'\u00B0';
    const fan = d.fan;
    const fanEl = document.getElementById('fan-val');
    fanEl.innerHTML = '<span class="led '+((fan)?'on':'off')+'"></span>'+((fan)?'ON':'OFF');
    // Client wifi
    document.getElementById('client-count').textContent = d.clients+' client';
    document.getElementById('wifi-dot').className = 'wifi-dot' + (d.clients>0?' on':'');
    document.getElementById('wheel-name').textContent = d.wheel_name;
    // Timing iniettori
    const ib = document.getElementById('inj-body');
    d.inj.forEach((c,i)=>{
      const row=ib.children[i];
      row.children[1].textContent = (c.active)? c.ms.toFixed(1)+' ms':'--';
      row.children[2].textContent = (c.active)? c.hz.toFixed(1)+' Hz':'--';
      row.children[3].textContent = (c.active)? c.duty+'%':'--';
    });
    // Timing candele
    const gb = document.getElementById('ign-body');
    d.ign.forEach((c,i)=>{
      const row=gb.children[i];
      row.children[1].textContent = (c.active)? c.dwell.toFixed(2)+' ms':'--';
      row.children[2].textContent = (c.active)? c.adv+'\u00B0':'--';
      row.children[3].innerHTML = '<span class="led '+((c.active)?'on':'off')+'"></span>'+((c.active)?'ACT':'IN');
    });
  }catch(e){ /* riprova al prossimo poll */ }
}
async function toggleEngine(){
  try{
    const r = await fetch('/api/start');
    const d = await r.json();
    const bs = document.getElementById('btn-start');
    const running = (d.state==='RUNNING' || d.state==='CRANKING');
    bs.textContent = running ? 'STOP' : 'AVVIA';
    bs.className = 'btn-run' + (running ? ' stop' : '');
    const b = document.getElementById('state-badge');
    b.textContent = d.state;
    b.className = '';
    if(d.state==='RUNNING') b.classList.add('on');
    else if(d.state==='CRANKING') b.classList.add('crank');
    else if(d.state==='STOPPING') b.classList.add('stop');
  }catch(e){}
}
setInterval(poll, 200);
poll();
</script>
</body>
</html>
)HTML";

// --- HTML SETUP (generato con la lista ruote) ---
static String setupHtml() {
  String page = String(R"SH(
<!DOCTYPE html>
<html lang="it">
<head>
<meta charset="UTF-8">
<meta name="viewport" content="width=device-width, initial-scale=1.0">
<title>Setup - Engine Simulator 2.0</title>
<style>
:root{--bg:#0b0f14;--panel:#141b24;--panel2:#1a232e;--line:#263341;--txt:#e6edf3;--dim:#8ba0b4;--acc:#00d4ff;--ok:#22c55e}
*{margin:0;padding:0;box-sizing:border-box}
body{background:radial-gradient(1200px 700px at 70% -10%,#12202e 0%,var(--bg) 55%);color:var(--txt);
  font-family:'Segoe UI',system-ui,sans-serif;min-height:100vh;padding:16px}
.wrap{max-width:720px;margin:0 auto}
.card{background:var(--panel);border:1px solid var(--line);border-radius:14px;padding:20px;margin-top:14px;
  box-shadow:0 4px 20px rgba(0,0,0,.28)}
h1{font-size:1.2rem;font-weight:700}
h2{font-size:.78rem;color:var(--dim);text-transform:uppercase;letter-spacing:1.4px;margin:16px 0 4px}
select{width:100%;padding:11px 12px;border-radius:10px;background:var(--panel2);color:var(--txt);
  border:1px solid var(--line);font-size:.92rem;margin:10px 0}
button{width:100%;padding:12px;border:none;border-radius:10px;font-weight:800;font-size:.9rem;cursor:pointer;
  background:linear-gradient(135deg,#00a2c9,#0077b6);color:#fff;letter-spacing:.6px;transition:all .2s}
button:hover{filter:brightness(1.15);box-shadow:0 0 18px rgba(0,212,255,.35)}
nav a{color:var(--acc);text-decoration:none;font-size:.82rem;font-weight:600}
#save-msg{margin-top:12px;font-size:.85rem;color:var(--ok);min-height:16px;text-align:center}
#wheel-desc{background:var(--panel2);border:1px solid var(--line);border-radius:10px;padding:12px;font-size:.85rem;line-height:1.7}
#wheel-desc b{color:var(--acc)}
</style>
</head>
<body>
<div class="wrap">
  <div class="card">
    <nav><a href="/">&#8592; Torna alla dashboard</a></nav>
    <h1>Setup ruota fonica / cam</h1>
    <h2>Seleziona il pattern</h2>
    <select id="wheel-select">
  )SH");

  for (uint8_t i = 0; i < MAX_WHEELS; i++) {
    page += "      <option value=\"";
    page += String(i);
    page += "\"";
    if (i == config.wheel) page += " selected";
    page += ">";
    page += wheelName(i);
    page += "</option>\n";
  }

  page += String(R"SH(
    </select>
    <div id="wheel-desc">Ruota attiva: <b><span id="cur-name"></span></b><br>
    Indice: <b><span id="cur-idx"></span></b> / N&deg; max: <b>)SH");
  page += String(MAX_WHEELS);
  page += String(R"SH(</b></div>
    <button onclick="saveWheel()" type="button">Salva e applica ruota</button>
    <div id="save-msg"></div>
  </div>
</div>
<script>
const sel = document.getElementById('wheel-select');
const nm = () => sel.options[sel.selectedIndex].text;
function upd(){
  document.getElementById('cur-name').textContent = nm();
  document.getElementById('cur-idx').textContent = sel.value;
}
sel.addEventListener('change', upd);
upd();
async function saveWheel(){
  const msg = document.getElementById('save-msg');
  msg.textContent = '...';
  try{
    const r = await fetch('/api/wheel?wheel='+sel.value);
    const d = await r.json();
    if(d.ok){
      const chk = await (await fetch('/api/wheel?get=1')).json();
      msg.textContent = 'Ruota salvata! attiva='+sel.options[chk.wheel].text+' (idx '+chk.wheel+')';
      sel.value = chk.wheel;
    } else {
      msg.textContent = 'Errore: '+d.error;
      msg.style.color = '#ef4444';
    }
    msg.style.color = d.ok ? 'var(--ok)' : '#ef4444';
  }catch(e){ msg.textContent='Errore di rete: '+e; msg.style.color='#ef4444'; }
}
</script>
</body>
</html>
)SH");

  return page;
}

// --- ROUTE ---
void webSetup() {
  prefs.begin(NVS_NAMESPACE, false);

  // Carica la ruota fonica salvata su NVS (default: 60-2 = SIXTY_MINUS_TWO).
  // getUChar ritorna il default se la key non esiste (robusto, senza isKey).
  uint8_t saved = prefs.getUChar(NVS_KEY_WHEEL, SIXTY_MINUS_TWO);
  if (saved < MAX_WHEELS) {
    config.wheel = saved;
    display_new_wheel();
  }

  // AVVIO ACCESS POINT (IP statico + DHCP per i client)
  WiFi.mode(WIFI_AP);
  IPAddress localIP(AP_IP);
  IPAddress gateway(AP_GATEWAY);
  IPAddress subnet(AP_SUBNET);
  WiFi.softAPConfig(localIP, gateway, subnet);
  WiFi.softAP(AP_SSID, AP_PASSWORD);

  // --- HOME / Dashboard ---
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *resp = request->beginResponse_P(200, "text/html", INDEX_HTML);
    resp->addHeader("Cache-Control", "no-store");
    request->send(resp);
  });

  // --- SETUP ruota fonica ---
  server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncWebServerResponse *resp = request->beginResponse(200, "text/html", setupHtml());
    resp->addHeader("Cache-Control", "no-store");
    request->send(resp);
  });

  // --- Avvio/Stop motore (toggle, specchio del pulsante fisico GPIO4) ---
  server.on("/api/start", HTTP_GET, [](AsyncWebServerRequest *request) {
    switch (engineState) {
      case ENGINE_OFF:
        engineState = ENGINE_CRANKING;
        currentRpmFloat = 0.0;
        break;
      case ENGINE_RUNNING:
      case ENGINE_CRANKING:
        engineState = ENGINE_STOPPING;
        break;
      default:
        break;
    }
    char b[64];
    snprintf(b, sizeof(b), "{\"ok\":true,\"state\":\"%s\"}", engineStateName());
    request->send(200, "application/json", b);
  });

  // --- API dati dashboard ---
  server.on("/api/data", HTTP_GET, [](AsyncWebServerRequest *request) {
    AsyncResponseStream *r = request->beginResponseStream("application/json");

    // Nome ruota corrente (PROGMEM)
    String wname = wheelName(config.wheel);

    char buf[900];
    int n = 0;
    n += snprintf(buf + n, sizeof(buf) - n,
      "{\"rpm\":%u,\"state\":\"%s\",\"tps_pct\":%.1f,\"map_kpa\":%.1f,\"clt_c\":%.1f,"
      "\"fan\":%s,\"clients\":%u,\"wheel\":%u,\"wheel_name\":\"%s\","
      "\"inj\":[",
      currentStatus.rpm, engineStateName(), tpsValue*100.0f, currentMapKpa, engineTemp,
      fanActive ? "true" : "false", connectedClients, config.wheel, wname.c_str());

    for (uint8_t i = 0; i < 4; i++) {
      n += snprintf(buf + n, sizeof(buf) - n,
        "{\"active\":%s,\"ms\":%.1f,\"hz\":%.1f,\"duty\":%u}%c",
        injectors[i].active ? "true" : "false",
        injectors[i].onTimeMs, injectors[i].frequencyHz, injectors[i].dutyPct,
        (i < 3) ? ',' : ' ');
    }
    n += snprintf(buf + n, sizeof(buf) - n, "],\"ign\":[");
    for (uint8_t i = 0; i < 4; i++) {
      n += snprintf(buf + n, sizeof(buf) - n,
        "{\"active\":%s,\"dwell\":%.2f,\"adv\":%d}%c",
        ignitions[i].active ? "true" : "false",
        ignitions[i].dwellMs, (int)ignitions[i].advanceDeg,
        (i < 3) ? ',' : ' ');
    }
    n += snprintf(buf + n, sizeof(buf) - n, "]}");

    r->print(buf);
    request->send(r);
  });

  // --- API salvataggio/lettura ruota fonica (GET querystring) ---
  server.on("/api/wheel", HTTP_GET, [](AsyncWebServerRequest *request) {
    // Lettura dello stato corrente: /api/wheel?get=1
    if (request->hasParam("get")) {
      char b[32];
      snprintf(b, sizeof(b), "{\"ok\":true,\"wheel\":%u}", config.wheel);
      request->send(200, "application/json", b);
      return;
    }
    if (!request->hasParam("wheel")) {
      request->send(400, "application/json",
        "{\"ok\":false,\"error\":\"param wheel mancante (usare /api/wheel?wheel=N)\"}");
      return;
    }
    uint8_t w = (uint8_t)request->getParam("wheel")->value().toInt();
    if (w >= MAX_WHEELS) {
      request->send(400, "application/json", "{\"ok\":false,\"error\":\"indice fuori range\"}");
      return;
    }
    // Applica in RAM + salva su NVS
    config.wheel = w;
    prefs.putUChar(NVS_KEY_WHEEL, w);
    uint8_t rd = prefs.getUChar(NVS_KEY_WHEEL, SIXTY_MINUS_TWO);
    display_new_wheel();
    Serial.print("[WEB] wheel applicato="); Serial.print(w);
    Serial.print(" riletto=NVS="); Serial.println(rd);
    request->send(200, "application/json", "{\"ok\":true}");
  });

  server.onNotFound([](AsyncWebServerRequest *request) {
    request->send(404, "application/json", "{\"ok\":false,\"error\":\"not found\"}");
  });

  server.begin();
}

void webLoop() {
  // Conta le stazioni associate all'Access Point (API Arduino, robusta)
  connectedClients = WiFi.softAPgetStationNum();
}