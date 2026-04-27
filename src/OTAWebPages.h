#pragma once
#ifndef OTA_WEB_PAGES_H
#define OTA_WEB_PAGES_H

/**
 * OTAWebPages.h — Inline HTML page builders for the /update endpoint.
 */

#include <Arduino.h>

// ─── Shared CSS / shell ──────────────────────────────────────────────────────
static const char OTA_PAGE_HEAD[] PROGMEM = R"rawhtml(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>ESPFlexOTA – Firmware Update</title>
  <style>
    *{box-sizing:border-box;margin:0;padding:0}
    body{font-family:system-ui,-apple-system,sans-serif;background:#0f172a;color:#e2e8f0;
         display:flex;align-items:center;justify-content:center;min-height:100vh;padding:1rem}
    .card{background:#1e293b;border:1px solid #334155;border-radius:12px;
          padding:2rem;max-width:480px;width:100%;box-shadow:0 25px 50px rgba(0,0,0,.5)}
    h1{font-size:1.4rem;font-weight:700;margin-bottom:.25rem;color:#f8fafc}
    .subtitle{font-size:.85rem;color:#94a3b8;margin-bottom:1.5rem}
    .badge{display:inline-block;background:#0f172a;border:1px solid #475569;
           border-radius:999px;font-size:.75rem;padding:.2rem .65rem;
           color:#94a3b8;margin-bottom:1.5rem}
    label{font-size:.85rem;color:#94a3b8;display:block;margin-bottom:.4rem}

    /* Drop zone — no full-cover invisible input */
    .drop-zone{border:2px dashed #475569;border-radius:8px;padding:1.5rem;
               text-align:center;transition:.2s;background:#0f172a;
               color:#64748b;font-size:.9rem}
    .drop-zone.drag-over{border-color:#3b82f6;background:#0f2040;color:#93c5fd}
    .drop-zone .icon{font-size:2rem;margin-bottom:.4rem}

    /* Hidden real file input — triggered only by the browse button */
    #fw{display:none}

    /* Browse button inside the drop zone */
    .browse-btn{display:inline-block;margin-top:.5rem;padding:.35rem 1rem;
                background:#1e3a5f;border:1px solid #3b82f6;border-radius:6px;
                color:#93c5fd;font-size:.82rem;cursor:pointer;transition:.2s;
                user-select:none}
    .browse-btn:hover{background:#1d4ed8;color:#fff}

    .filename{margin-top:.55rem;font-size:.8rem;color:#38bdf8;min-height:1.2em}

    /* Upload button — full width, clearly separated */
    .btn{display:block;width:100%;padding:.8rem;border:none;border-radius:8px;
         font-size:1rem;font-weight:600;cursor:pointer;transition:.2s;margin-top:1.1rem}
    .btn-primary{background:#3b82f6;color:#fff}
    .btn-primary:hover:not(:disabled){background:#2563eb}
    .btn-primary:disabled{background:#1e3a8a;color:#93c5fd;cursor:not-allowed;opacity:.7}

    .progress-wrap{margin-top:1.25rem;display:none}
    .progress-bar-bg{background:#0f172a;border-radius:999px;height:10px;
                     overflow:hidden;border:1px solid #334155}
    .progress-bar{height:100%;background:linear-gradient(90deg,#3b82f6,#06b6d4);
                  width:0%;transition:width .25s;border-radius:999px}
    .progress-label{font-size:.8rem;color:#94a3b8;text-align:right;margin-top:.3rem}

    .status{margin-top:1rem;font-size:.85rem;padding:.6rem .9rem;
            border-radius:6px;display:none}
    .status.info   {background:#1e3a5f;color:#7dd3fc;border:1px solid #1d4ed8}
    .status.error  {background:#450a0a;color:#fca5a5;border:1px solid #991b1b}
    .status.success{background:#052e16;color:#86efac;border:1px solid #166534}

    a{color:#38bdf8;text-decoration:none}a:hover{text-decoration:underline}
    .divider{border:none;border-top:1px solid #334155;margin:1.5rem 0}
    .info-row{display:flex;justify-content:space-between;font-size:.8rem;
              color:#64748b;margin-bottom:.35rem}
    .info-row span:last-child{color:#94a3b8}
  </style>
</head>
<body><div class="card">
)rawhtml";

static const char OTA_PAGE_FOOT[] PROGMEM = R"rawhtml(
</div></body></html>
)rawhtml";

// ─────────────────────────────────────────────────────────────────────────────

inline String ESPFlexOTAClass::_buildUpdatePage() {
    String page = FPSTR(OTA_PAGE_HEAD);

    page += F("<h1>&#128640; Firmware Update</h1>"
              "<p class='subtitle'>ESPFlexOTA &mdash; Local OTA</p>");

    // Version / platform badge
    page += F("<div class='badge'>Running: ");
    page += _config.currentVersion;
    page += F("&nbsp;&nbsp;|&nbsp;&nbsp;");
    page += OTA_PLATFORM;
    page += F("</div>");

    // Info rows
    page += F("<div class='info-row'><span>Free flash</span><span>");
    page += String(ESP.getFreeSketchSpace() / 1024);
    page += F(" KB</span></div>"
              "<div class='info-row'><span>IP Address</span><span>");
    page += getServerIP();
    page += F("</span></div>");

    // Form — note: the file <input> is hidden; only .browse-btn opens it
    page += F("<hr class='divider'>"
              "<form id='upd' method='POST' action='/update'"
              "      enctype='multipart/form-data'>"

              // Hidden real file input
              "<input type='file' id='fw' name='firmware' accept='.bin'>"

              // Drop zone (visual only — no invisible full-cover input)
              "<label>Firmware binary (.bin)</label>"
              "<div class='drop-zone' id='dz'>"
              "  <div class='icon'>&#128190;</div>"
              "  <div>Drag &amp; drop a <strong>.bin</strong> file here</div>"
              "  <span class='browse-btn' id='browseBtn'>&#128065; Browse&hellip;</span>"
              "  <div class='filename' id='fname'></div>"
              "</div>"

              // Upload button — outside the drop zone, no accidental file dialog
              "<button class='btn btn-primary' id='uploadBtn'"
              "        type='button' disabled>"
              "  &#9652;&nbsp;Upload &amp; Install"
              "</button>"

              "</form>"

              // Progress bar
              "<div class='progress-wrap' id='pw'>"
              "  <div class='progress-bar-bg'>"
              "    <div class='progress-bar' id='pb'></div>"
              "  </div>"
              "  <div class='progress-label' id='pl'>0%</div>"
              "</div>"

              // Status message
              "<div class='status' id='st'></div>");

    // JavaScript — clean, no ambiguous click handlers
    page += F("<script>"
              "var fw=document.getElementById('fw'),"
              "    dz=document.getElementById('dz'),"
              "    browseBtn=document.getElementById('browseBtn'),"
              "    uploadBtn=document.getElementById('uploadBtn'),"
              "    fname=document.getElementById('fname'),"
              "    pw=document.getElementById('pw'),"
              "    pb=document.getElementById('pb'),"
              "    pl=document.getElementById('pl'),"
              "    st=document.getElementById('st');"

              // Only the browse button opens the file picker
              "browseBtn.addEventListener('click',function(e){"
              "  e.stopPropagation();"
              "  fw.click();"
              "});"

              // File selected via dialog
              "fw.addEventListener('change',function(){"
              "  if(fw.files&&fw.files[0])setFile(fw.files[0]);"
              "});"

              // Drag & drop on the drop zone
              "dz.addEventListener('dragenter',function(e){e.preventDefault();dz.classList.add('drag-over');});"
              "dz.addEventListener('dragover', function(e){e.preventDefault();dz.classList.add('drag-over');});"
              "dz.addEventListener('dragleave',function(e){dz.classList.remove('drag-over');});"
              "dz.addEventListener('drop',function(e){"
              "  e.preventDefault();"
              "  dz.classList.remove('drag-over');"
              "  var files=e.dataTransfer.files;"
              "  if(files&&files[0]){"
              "    if(!files[0].name.endsWith('.bin')){"
              "      showStatus('Please select a .bin firmware file.','error');return;"
              "    }"
              "    setFile(files[0]);"
              "  }"
              "});"

              // Shared helper: update UI when a file is chosen
              "function setFile(f){"
              "  if(!f.name.endsWith('.bin')){"
              "    showStatus('Please select a .bin firmware file.','error');"
              "    uploadBtn.disabled=true;return;"
              "  }"
              "  fname.textContent=f.name+' ('+Math.round(f.size/1024)+' KB)';"
              "  uploadBtn.disabled=false;"
              "  st.style.display='none';"
              "}"

              // Upload button click — send via XHR, no form submit
              "uploadBtn.addEventListener('click',function(){"
              "  if(!fw.files||!fw.files[0]){showStatus('No file selected.','error');return;}"
              "  uploadBtn.disabled=true;"
              "  browseBtn.style.pointerEvents='none';"
              "  pw.style.display='block';"
              "  showStatus('Uploading firmware, please wait\u2026','info');"
              "  var fd=new FormData();"
              "  fd.append('firmware',fw.files[0]);"
              "  var xhr=new XMLHttpRequest();"
              "  xhr.open('POST','/update');"
              "  xhr.upload.onprogress=function(e){"
              "    if(e.lengthComputable){"
              "      var p=Math.round(e.loaded/e.total*100);"
              "      pb.style.width=p+'%';pl.textContent=p+'%';"
              "    }"
              "  };"
              "  xhr.onload=function(){"
              "    document.open();document.write(xhr.responseText);document.close();"
              "  };"
              "  xhr.onerror=function(){"
              "    showStatus('Upload failed. Check connection and retry.','error');"
              "    uploadBtn.disabled=false;"
              "    browseBtn.style.pointerEvents='';"
              "  };"
              "  xhr.send(fd);"
              "});"

              "function showStatus(msg,cls){"
              "  st.textContent=msg;"
              "  st.className='status '+cls;"
              "  st.style.display='block';"
              "}"
              "</script>");

    page += FPSTR(OTA_PAGE_FOOT);
    return page;
}

inline String ESPFlexOTAClass::_buildSuccessPage() {
    String page = FPSTR(OTA_PAGE_HEAD);
    page += F("<h1>&#9989; Update Successful</h1>"
              "<p class='subtitle' style='margin-bottom:1rem'>"
              "Firmware flashed successfully.</p>"
              "<div class='status success' style='display:block'>"
              "Device is rebooting&hellip; Page will reload in 8 seconds."
              "</div>"
              "<script>setTimeout(function(){"
              "window.location.replace('/update');},"
              "8000);</script>");
    page += FPSTR(OTA_PAGE_FOOT);
    return page;
}

inline String ESPFlexOTAClass::_buildErrorPage(const String& msg) {
    String page = FPSTR(OTA_PAGE_HEAD);
    page += F("<h1>&#10060; Update Failed</h1>"
              "<p class='subtitle' style='margin-bottom:1rem'>"
              "The firmware could not be installed.</p>"
              "<div class='status error' style='display:block'>");
    page += msg;
    page += F("</div><br><a href='/update'>&larr; Try again</a>");
    page += FPSTR(OTA_PAGE_FOOT);
    return page;
}

#endif // OTA_WEB_PAGES_H
