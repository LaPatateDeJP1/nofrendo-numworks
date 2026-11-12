/**
 * Nofrendo NumWorks - Client-Side SPA Engine
 * Gestion des onglets, Drag & Drop, validation iNES,
 * calcul CRC32 STM32, assemblage binaire et pilotage WebUSB.
 */

import { NumWorksDevice } from "./webusb.js?v=2";
import { linkNwaWithRoms } from "./nwa_linker.js?v=2";

// Limite par défaut allouée aux applications externes (2 Mo, mise à jour dynamiquement à la connexion)
const FLASH_DEFAULT_MAX_BYTES = 2 * 1024 * 1024;
// Poids estimé du moteur émulateur sans ROMs
const EMULATOR_BASE_BYTES = 82 * 1024;

const DEFAULT_MEMORY_MAP = {
  flashStart: 0x90570000,
  flashEnd: 0x90770000,
  flashLength: FLASH_DEFAULT_MAX_BYTES,
  ramStart: 0x24012804,
  ramLength: 0x2D7FC,
  trampolineStart: 0x90420038
};

// Calculateur de CRC32 compatible STM32 / Ethernet (Polynôme 0x04C11DB7)
function calculateCrc32(uint8Array) {
  const POLYNOMIAL = 0x04C11DB7;
  let remainder = 0;
  for (let i = 0; i < uint8Array.length; i++) {
    remainder ^= (uint8Array[i] << 24) >>> 0;
    for (let bit = 0; bit < 8; bit++) {
      if ((remainder & 0x80000000) !== 0) {
        remainder = (((remainder << 1) >>> 0) ^ POLYNOMIAL) >>> 0;
      } else {
        remainder = (remainder << 1) >>> 0;
      }
    }
  }
  return remainder >>> 0;
}

// Nettoyage et formatage du titre extrait du nom de fichier
function formatRomTitle(filename) {
  let name = filename.replace(/\.nes$/i, "");
  name = name.replace(/[-_]+/g, " ").trim();
  // Capitalisation automatique si tout est en minuscules ou majuscules
  if (name === name.toLowerCase() || name === name.toUpperCase()) {
    name = name.replace(/\b\w/g, c => c.toUpperCase());
  }
  return name.slice(0, 31); // Max 31 caractères (+ terminateur null)
}

// État global de l'application
const state = {
  usbDevice: new NumWorksDevice(),
  romList: [], // Liste des ROMs ajoutées : { file, name, size, data, mapper, prgKb, chrKb, crc32 }
  flashMaxBytes: FLASH_DEFAULT_MAX_BYTES,
  cachedNwa: null,
  cachedCrt0: null
};

// Initialisation au chargement du DOM
document.addEventListener("DOMContentLoaded", () => {
  initTabs();
  initDropzone();
  initExpressView();
  initCreatorView();
  checkBrowserCompatibility();
});

// Vérification de la compatibilité WebUSB du navigateur
function checkBrowserCompatibility() {
  const banner = document.getElementById("compat-banner");
  if (!NumWorksDevice.isSupported()) {
    if (banner) {
      banner.style.display = "flex";
      banner.innerHTML = `<strong>Attention :</strong> Votre navigateur actuel ne prend pas en charge WebUSB. 
      Veuillez utiliser un navigateur basé sur Chromium (Google Chrome, Brave, Microsoft Edge ou Opera) pour communiquer directement avec la NumWorks.`;
    }
    const btns = document.querySelectorAll(".requires-webusb");
    btns.forEach(b => b.disabled = true);
  }
}

// --------------------------------------------------------------------------
// Navigation par onglets
// --------------------------------------------------------------------------
function initTabs() {
  const tabs = document.querySelectorAll(".nav-tab-btn");
  const contents = document.querySelectorAll(".tab-content");

  tabs.forEach(tab => {
    tab.addEventListener("click", () => {
      const targetId = tab.getAttribute("data-tab");

      tabs.forEach(t => t.classList.remove("active"));
      contents.forEach(c => c.classList.remove("active"));

      tab.classList.add("active");
      const targetContent = document.getElementById(targetId);
      if (targetContent) {
        targetContent.classList.add("active");
      }
    });
  });
}

// --------------------------------------------------------------------------
// Onglet 1 : Flash Express
// --------------------------------------------------------------------------
function initExpressView() {
  const flashBtn = document.getElementById("btn-flash-express");
  const consoleBox = document.getElementById("express-console");
  const progressBar = document.getElementById("express-progress");

  if (!flashBtn) return;

  flashBtn.addEventListener("click", async () => {
    logConsole(consoleBox, "Initialisation de la connexion WebUSB...", "info");
    flashBtn.disabled = true;
    updateUsbBadge(false, "Connexion...");

    try {
      const info = await state.usbDevice.connect();
      updateUsbBadge(true, info.model);
      logConsole(consoleBox, `Calculatrice détectée : ${info.productName} (${info.productId}) — ${info.model}`, "success");

      if (info.memoryMap) {
        state.flashMaxBytes = info.memoryMap.flashLength;
        updateCreatorUI();
        logMemoryMap(consoleBox, info.memoryMap);
      } else {
        logConsole(consoleBox, "Carte mémoire non détectée via DFU UPLOAD : utilisation des adresses par défaut (Slot B).", "info");
      }

      logConsole(consoleBox, "Téléchargement du moteur Nofrendo (.nwa) et de la ROM 2048.nes...", "info");
      const romRes = await fetch("2048.nes");
      if (!romRes.ok) {
        throw new Error(`Impossible de récupérer 2048.nes (HTTP ${romRes.status})`);
      }
      const romU8 = new Uint8Array(await romRes.arrayBuffer());
      const expressRom = parseInesRom(romU8, "2048.nes", "2048");

      const binData = await buildCustomBinary(getTargetMemoryMap(), [expressRom]);
      logConsole(consoleBox, `Binaire lié avec succès (${Math.round(binData.byteLength / 1024)} Ko). Début du transfert...`, "info");

      await state.usbDevice.installAppBinary(binData, (p) => {
        if (progressBar) {
          progressBar.style.width = `${Math.round(p.progress * 100)}%`;
        }
        if (p.stage === "erase") {
          logConsole(consoleBox, p.message, "info");
        } else if (p.stage === "program" && Math.round(p.progress * 100) % 25 === 0) {
          logConsole(consoleBox, p.message, "cmd");
        } else if (p.stage === "done") {
          logConsole(consoleBox, p.message, "success");
        }
      });

      logConsole(consoleBox, "Succès ! Débranchez la calculatrice ou lancez NES depuis le menu.", "success");
      updateUsbBadge(false, "Prêt");
    } catch (err) {
      logConsole(consoleBox, `Erreur : ${err.message}`, "error");
      updateUsbBadge(false, "Déconnecté");
    } finally {
      flashBtn.disabled = false;
    }
  });
}

// --------------------------------------------------------------------------
// Onglet 2 : Créateur de Pack (Constructeur dynamique)
// --------------------------------------------------------------------------
function initCreatorView() {
  const fileInput = document.getElementById("file-input");
  const dropzone = document.getElementById("dropzone");
  const flashCustomBtn = document.getElementById("btn-flash-custom");
  const downloadBinBtn = document.getElementById("btn-download-bin");

  if (dropzone && fileInput) {
    dropzone.addEventListener("click", () => fileInput.click());
    fileInput.addEventListener("change", (e) => {
      handleFiles(e.target.files);
      fileInput.value = "";
    });
  }

  if (flashCustomBtn) {
    flashCustomBtn.addEventListener("click", async () => {
      if (state.romList.length === 0) {
        alert("Veuillez ajouter au moins une ROM .nes avant de flasher.");
        return;
      }
      await flashCustomPack();
    });
  }

  if (downloadBinBtn) {
    downloadBinBtn.addEventListener("click", async () => {
      if (state.romList.length === 0) {
        alert("Veuillez ajouter au moins une ROM .nes.");
        return;
      }
      try {
        const binData = await buildCustomBinary();
        downloadBlob(binData, "nofrendo_custom.bin");
      } catch (err) {
        alert("Erreur lors de la génération du binaire : " + err.message);
      }
    });
  }

  updateCreatorUI();
}

function initDropzone() {
  const dropzone = document.getElementById("dropzone");
  if (!dropzone) return;

  ["dragenter", "dragover"].forEach(evtName => {
    dropzone.addEventListener(evtName, (e) => {
      e.preventDefault();
      e.stopPropagation();
      dropzone.classList.add("dragover");
    }, false);
  });

  ["dragleave", "drop"].forEach(evtName => {
    dropzone.addEventListener(evtName, (e) => {
      e.preventDefault();
      e.stopPropagation();
      dropzone.classList.remove("dragover");
    }, false);
  });

  dropzone.addEventListener("drop", (e) => {
    const files = e.dataTransfer.files;
    handleFiles(files);
  });
}

// Extraction et validation d'une ROM iNES
function parseInesRom(u8, filename, overrideTitle = null) {
  if (u8.length < 16 || u8[0] !== 0x4E || u8[1] !== 0x45 || u8[2] !== 0x53 || u8[3] !== 0x1A) {
    throw new Error(`Le fichier "${filename}" n'a pas d'en-tête iNES valide (NES\\x1A).`);
  }

  const prgRom16Kb = u8[4];
  const chrRom8Kb = u8[5];
  const flags6 = u8[6];
  const flags7 = u8[7];
  const mapper = (flags6 >> 4) | (flags7 & 0xF0);

  return {
    id: "rom_" + Date.now() + "_" + Math.random().toString(36).slice(2, 7),
    filename,
    title: overrideTitle || formatRomTitle(filename),
    size: u8.length,
    mapper,
    prgKb: prgRom16Kb * 16,
    chrKb: chrRom8Kb * 8,
    crc32: calculateCrc32(u8),
    data: u8
  };
}

function getTargetMemoryMap() {
  return state.usbDevice.memoryMap || DEFAULT_MEMORY_MAP;
}

function logMemoryMap(consoleBox, memMap) {
  const fStart = "0x" + memMap.flashStart.toString(16).toUpperCase();
  const fEnd = "0x" + memMap.flashEnd.toString(16).toUpperCase();
  const rStart = "0x" + memMap.ramStart.toString(16).toUpperCase();
  const tStart = "0x" + memMap.trampolineStart.toString(16).toUpperCase();
  const availMb = (memMap.flashLength / (1024 * 1024)).toFixed(2);
  logConsole(
    consoleBox,
    `Carte mémoire lue : Flash ${fStart}..${fEnd} (${availMb} Mo) | RAM ${rStart} | Trampoline ${tStart}`,
    "cmd"
  );
}

// Analyse et validation de chaque fichier ajouté
async function handleFiles(files) {
  if (!files || files.length === 0) return;

  for (const file of files) {
    if (!file.name.toLowerCase().endsWith(".nes")) {
      alert(`Le fichier "${file.name}" n'a pas l'extension .nes.`);
      continue;
    }

    try {
      const buffer = await file.arrayBuffer();
      const u8 = new Uint8Array(buffer);
      state.romList.push(parseInesRom(u8, file.name));
    } catch (err) {
      alert(err.message);
    }
  }

  updateCreatorUI();
}

// Mise à jour de l'affichage du créateur de pack (Liste + Jauge)
function updateCreatorUI() {
  const container = document.getElementById("rom-list-container");
  const countSpan = document.getElementById("rom-count");
  const gaugeFill = document.getElementById("gauge-fill");
  const gaugeText = document.getElementById("gauge-text");
  const flashBtn = document.getElementById("btn-flash-custom");

  if (!container) return;

  container.innerHTML = "";
  if (countSpan) countSpan.textContent = state.romList.length;

  let totalBytes = EMULATOR_BASE_BYTES;

  if (state.romList.length === 0) {
    container.innerHTML = `<div style="text-align: center; color: var(--text-muted); padding: 16px; font-size: 12px;">Aucune ROM chargée. Glissez des fichiers .nes ci-dessus.</div>`;
  } else {
    const table = document.createElement("table");
    table.className = "rom-table";
    table.innerHTML = `
      <thead>
        <tr>
          <th style="width:24px">#</th>
          <th>Titre (éditable)</th>
          <th style="width:70px">Mapper</th>
          <th style="width:70px">Taille</th>
          <th style="width:70px; text-align:right">Actions</th>
        </tr>
      </thead>
      <tbody></tbody>
    `;

    const tbody = table.querySelector("tbody");

    state.romList.forEach((rom, index) => {
      totalBytes += rom.size;
      const tr = document.createElement("tr");
      tr.className = "rom-row";

      tr.innerHTML = `
        <td class="rom-idx">${String(index + 1).padStart(2, "0")}</td>
        <td>
          <input type="text" class="rom-title-input" value="${escapeHtml(rom.title)}" maxlength="31" data-idx="${index}">
        </td>
        <td class="rom-meta">Mapper ${rom.mapper}</td>
        <td class="rom-meta">${Math.round(rom.size / 1024)} Ko</td>
        <td class="rom-actions">
          <button class="icon-btn btn-up" data-idx="${index}" ${index === 0 ? "disabled" : ""} title="Monter">↑</button>
          <button class="icon-btn btn-down" data-idx="${index}" ${index === state.romList.length - 1 ? "disabled" : ""} title="Descendre">↓</button>
          <button class="icon-btn btn-del" data-idx="${index}" title="Supprimer">×</button>
        </td>
      `;

      tbody.appendChild(tr);
    });

    container.appendChild(table);

    // Écouteurs pour le renommage direct
    container.querySelectorAll(".rom-title-input").forEach(input => {
      input.addEventListener("input", (e) => {
        const idx = parseInt(e.target.getAttribute("data-idx"), 10);
        state.romList[idx].title = e.target.value.trim() || "Sans titre";
      });
    });

    // Écouteurs pour le déplacement et suppression
    container.querySelectorAll(".btn-up").forEach(btn => {
      btn.addEventListener("click", () => {
        const idx = parseInt(btn.getAttribute("data-idx"), 10);
        if (idx > 0) {
          const item = state.romList.splice(idx, 1)[0];
          state.romList.splice(idx - 1, 0, item);
          updateCreatorUI();
        }
      });
    });

    container.querySelectorAll(".btn-down").forEach(btn => {
      btn.addEventListener("click", () => {
        const idx = parseInt(btn.getAttribute("data-idx"), 10);
        if (idx < state.romList.length - 1) {
          const item = state.romList.splice(idx, 1)[0];
          state.romList.splice(idx + 1, 0, item);
          updateCreatorUI();
        }
      });
    });

    container.querySelectorAll(".btn-del").forEach(btn => {
      btn.addEventListener("click", () => {
        const idx = parseInt(btn.getAttribute("data-idx"), 10);
        state.romList.splice(idx, 1);
        updateCreatorUI();
      });
    });
  }

  // Mise à jour de la jauge
  const maxBytes = state.flashMaxBytes || FLASH_DEFAULT_MAX_BYTES;
  const usedMb = (totalBytes / (1024 * 1024)).toFixed(2);
  const maxMb = (maxBytes / (1024 * 1024)).toFixed(2);
  const percentage = Math.min(100, Math.round((totalBytes / maxBytes) * 100));

  if (gaugeFill) {
    gaugeFill.style.width = `${percentage}%`;
    if (totalBytes > maxBytes) {
      gaugeFill.classList.add("warning");
    } else {
      gaugeFill.classList.remove("warning");
    }
  }

  if (gaugeText) {
    gaugeText.textContent = `${usedMb.replace(".", ",")} Mo / ${maxMb.replace(".", ",")} Mo alloués (${percentage}%)`;
    if (totalBytes > maxBytes) {
      gaugeText.style.color = "var(--status-red)";
    } else {
      gaugeText.style.color = "var(--text-muted)";
    }
  }

  if (flashBtn) {
    flashBtn.disabled = (state.romList.length === 0 || totalBytes > maxBytes);
  }
}

// --------------------------------------------------------------------------
// Assemblage et édition de liens dynamique (.nwa -> .bin adapté à la NumWorks)
// --------------------------------------------------------------------------
async function loadLinkerAssets() {
  if (!state.cachedNwa) {
    const res = await fetch("nofrendo.nwa");
    if (!res.ok) {
      throw new Error(`Impossible de charger nofrendo.nwa (HTTP ${res.status})`);
    }
    state.cachedNwa = await res.arrayBuffer();
  }
  if (!state.cachedCrt0) {
    const res = await fetch("web_crt0.o");
    if (!res.ok) {
      throw new Error(`Impossible de charger web_crt0.o (HTTP ${res.status})`);
    }
    state.cachedCrt0 = await res.arrayBuffer();
  }
  return { nwaBuf: state.cachedNwa, crt0Buf: state.cachedCrt0 };
}

async function buildCustomBinary(memMap = getTargetMemoryMap(), roms = state.romList) {
  const { nwaBuf, crt0Buf } = await loadLinkerAssets();
  return linkNwaWithRoms(nwaBuf, crt0Buf, memMap, roms);
}

// Flashing du pack personnalisé vers la calculatrice
async function flashCustomPack() {
  const flashBtn = document.getElementById("btn-flash-custom");
  const consoleBox = document.getElementById("creator-console");
  const progressBar = document.getElementById("creator-progress");

  logConsole(consoleBox, "Connexion à la calculatrice et lecture de la carte mémoire...", "info");
  flashBtn.disabled = true;
  updateUsbBadge(false, "Connexion...");

  try {
    const info = await state.usbDevice.connect();
    updateUsbBadge(true, info.model);
    logConsole(consoleBox, `Calculatrice détectée : ${info.productName} (${info.productId}) — ${info.model}`, "success");

    if (info.memoryMap) {
      state.flashMaxBytes = info.memoryMap.flashLength;
      updateCreatorUI();
      logMemoryMap(consoleBox, info.memoryMap);
    } else {
      logConsole(consoleBox, "Carte mémoire non détectée via DFU UPLOAD : utilisation des adresses par défaut (Slot B).", "info");
    }

    const memMap = getTargetMemoryMap();
    logConsole(consoleBox, "Édition de liens dynamique de Nofrendo (.nwa) et intégration des ROMs...", "info");
    const customBin = await buildCustomBinary(memMap, state.romList);
    logConsole(consoleBox, `Pack lié avec succès (${Math.round(customBin.byteLength / 1024)} Ko, ${state.romList.length} jeux).`, "success");

    await state.usbDevice.installAppBinary(customBin, (p) => {
      if (progressBar) {
        progressBar.style.width = `${Math.round(p.progress * 100)}%`;
      }
      if (p.stage === "erase") {
        logConsole(consoleBox, p.message, "info");
      } else if (p.stage === "program" && Math.round(p.progress * 100) % 20 === 0) {
        logConsole(consoleBox, p.message, "cmd");
      } else if (p.stage === "done") {
        logConsole(consoleBox, p.message, "success");
      }
    });

    logConsole(consoleBox, "Installation terminée avec succès !", "success");
    updateUsbBadge(false, "Prêt");
  } catch (err) {
    logConsole(consoleBox, `Erreur : ${err.message}`, "error");
    updateUsbBadge(false, "Déconnecté");
  } finally {
    updateCreatorUI();
  }
}

// --------------------------------------------------------------------------
// Utilitaires UI
// --------------------------------------------------------------------------
function logConsole(consoleEl, msg, type = "info") {
  if (!consoleEl) return;
  const line = document.createElement("div");
  line.className = `console-line ${type}`;
  const time = new Date().toLocaleTimeString("fr-FR", { hour12: false });
  line.textContent = `[${time}] ${msg}`;
  consoleEl.appendChild(line);
  consoleEl.scrollTop = consoleEl.scrollHeight;

  // Mise à jour de la ligne de statut discrète
  const statusId = consoleEl.id === "express-console" ? "express-status" : (consoleEl.id === "creator-console" ? "creator-status" : null);
  if (statusId) {
    const statusEl = document.getElementById(statusId);
    if (statusEl) {
      statusEl.textContent = msg;
      statusEl.className = `status-line ${type}`;
    }
  }

  // Dépliage automatique des logs en cas d'erreur
  if (type === "error") {
    const details = consoleEl.closest("details");
    if (details) details.open = true;
  }
}

function updateUsbBadge(connected, text) {
  const dot = document.getElementById("usb-dot");
  const label = document.getElementById("usb-text");
  if (dot) {
    if (connected) dot.classList.add("connected");
    else dot.classList.remove("connected");
  }
  if (label) {
    label.textContent = text;
  }
}

function escapeHtml(str) {
  return str.replace(/&/g, "&amp;").replace(/</g, "&lt;").replace(/>/g, "&gt;").replace(/"/g, "&quot;");
}

function downloadBlob(data, filename) {
  const blob = new Blob([data], { type: "application/octet-stream" });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  document.body.removeChild(a);
  URL.revokeObjectURL(url);
}
