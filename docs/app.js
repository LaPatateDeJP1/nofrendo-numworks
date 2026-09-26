/**
 * Nofrendo NumWorks - Client-Side SPA Engine
 * Gestion des onglets, Drag & Drop, validation iNES,
 * calcul CRC32 STM32, assemblage binaire et pilotage WebUSB.
 */

import { NumWorksDevice } from "./webusb.js";

// Limite allouée pour la mémoire Flash externe dédiée aux ROMs (2 Mo)
const FLASH_MAX_BYTES = 2 * 1024 * 1024;
// Poids estimé du moteur émulateur sans ROMs
const EMULATOR_BASE_BYTES = 82 * 1024;

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
  cachedTemplateBin: null
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
      logConsole(consoleBox, `Calculatrice détectée : ${info.productName} (${info.productId})`, "success");
      logConsole(consoleBox, "Chargement du binaire Express précompilé (2048.nes)...", "info");

      const response = await fetch("nofrendo_express.bin");
      if (!response.ok) {
        throw new Error(`Impossible de récupérer nofrendo_express.bin (HTTP ${response.status})`);
      }
      const binData = await response.arrayBuffer();
      logConsole(consoleBox, `Binaire chargé (${Math.round(binData.byteLength / 1024)} Ko). Début du transfert...`, "info");

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

      // Validation stricte de l'en-tête iNES : 'N' 'E' 'S' 0x1A
      if (u8.length < 16 || u8[0] !== 0x4E || u8[1] !== 0x45 || u8[2] !== 0x53 || u8[3] !== 0x1A) {
        alert(`Le fichier "${file.name}" n'a pas d'en-tête iNES valide (NES\\x1A).`);
        continue;
      }

      const prgRom16Kb = u8[4];
      const chrRom8Kb = u8[5];
      const flags6 = u8[6];
      const flags7 = u8[7];
      const mapper = (flags6 >> 4) | (flags7 & 0xF0);

      const prgKb = prgRom16Kb * 16;
      const chrKb = chrRom8Kb * 8;
      const crc32 = calculateCrc32(u8);

      state.romList.push({
        id: "rom_" + Date.now() + "_" + Math.random().toString(36).substr(2, 5),
        filename: file.name,
        title: formatRomTitle(file.name),
        size: u8.length,
        mapper: mapper,
        prgKb: prgKb,
        chrKb: chrKb,
        crc32: crc32,
        data: u8
      });
    } catch (err) {
      alert(`Erreur lors de la lecture de "${file.name}": ` + err.message);
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
    container.innerHTML = `<div style="text-align: center; color: var(--text-muted); padding: 16px; font-family: var(--font-mono); font-size: 11px;">AUCUNE ROM CHARGEE. GLISSEZ DES FICHIERS .NES CI-DESSUS.</div>`;
  } else {
    const table = document.createElement("table");
    table.className = "rom-table";
    table.innerHTML = `
      <thead>
        <tr>
          <th style="width:24px">#</th>
          <th>TITRE (EDITABLE)</th>
          <th style="width:80px">MAPPER</th>
          <th style="width:80px">TAILLE</th>
          <th style="width:90px; text-align:right">ACTIONS</th>
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
        <td class="rom-meta">MAP ${rom.mapper}</td>
        <td class="rom-meta">${Math.round(rom.size / 1024)} Ko</td>
        <td class="rom-actions">
          <button class="icon-btn btn-up" data-idx="${index}" ${index === 0 ? "disabled" : ""}>UP</button>
          <button class="icon-btn btn-down" data-idx="${index}" ${index === state.romList.length - 1 ? "disabled" : ""}>DN</button>
          <button class="icon-btn btn-del" data-idx="${index}">DEL</button>
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
  const usedMb = (totalBytes / (1024 * 1024)).toFixed(2);
  const maxMb = (FLASH_MAX_BYTES / (1024 * 1024)).toFixed(1);
  const percentage = Math.min(100, Math.round((totalBytes / FLASH_MAX_BYTES) * 100));

  if (gaugeFill) {
    gaugeFill.style.width = `${percentage}%`;
    if (totalBytes > FLASH_MAX_BYTES) {
      gaugeFill.classList.add("warning");
    } else {
      gaugeFill.classList.remove("warning");
    }
  }

  if (gaugeText) {
    gaugeText.textContent = `${usedMb.replace(".", ",")} Mo / ${maxMb.replace(".", ",")} Mo alloués (${percentage}%)`;
    if (totalBytes > FLASH_MAX_BYTES) {
      gaugeText.style.color = "var(--status-red)";
    } else {
      gaugeText.style.color = "var(--text-muted)";
    }
  }

  if (flashBtn) {
    flashBtn.disabled = (state.romList.length === 0 || totalBytes > FLASH_MAX_BYTES);
  }
}

// --------------------------------------------------------------------------
// Assemblage du binaire Nofrendo personnalisé (Client-side)
// --------------------------------------------------------------------------
async function buildCustomBinary() {
  if (!state.cachedTemplateBin) {
    const res = await fetch("nofrendo_express.bin");
    if (!res.ok) {
      throw new Error("Impossible de charger le template de base nofrendo_express.bin");
    }
    state.cachedTemplateBin = await res.arrayBuffer();
  }

  // 1. Clonage du binaire de base
  const baseU8 = new Uint8Array(state.cachedTemplateBin);

  // 2. Détection de la position du catalogue et de la première ROM dans le binaire de base
  // Recherche du marqueur 2048 ou de la fin du code émulateur
  // Dans nofrendo_express.bin, le code moteur se termine vers 0x12268 (74 Ko)
  // et les données de 2048.nes commencent avec l'en-tête iNES 'N','E','S',0x1A
  let baseCodeLimit = -1;
  for (let i = 0x8000; i < baseU8.length - 4; i += 4) {
    if (baseU8[i] === 0x4E && baseU8[i+1] === 0x45 && baseU8[i+2] === 0x53 && baseU8[i+3] === 0x1A) {
      baseCodeLimit = i;
      break;
    }
  }

  if (baseCodeLimit === -1) {
    // Repli de secours : taille fixe du code
    baseCodeLimit = 74344;
  }

  // Taille du code pur sans la ROM par défaut
  const codeSection = baseU8.subarray(0, baseCodeLimit);

  // 3. Calcul de la taille totale nécessaire pour les nouvelles ROMs et la table
  const numGames = state.romList.length;
  const gameEntrySize = 24; // sizeof(GameEntry) = 24 octets
  const titleSize = 32;     // 32 octets par titre
  const catalogSize = numGames * gameEntrySize + numGames * titleSize;

  let totalRomsSize = 0;
  state.romList.forEach(r => {
    // Alignement de chaque ROM sur 4 octets
    const alignedSize = (r.size + 3) & ~3;
    totalRomsSize += alignedSize;
  });

  const totalAppSize = codeSection.length + totalRomsSize + catalogSize;
  const outBuf = new Uint8Array(totalAppSize);

  // Copie du code de base
  outBuf.set(codeSection, 0);

  // Écriture séquentielle des ROMs
  const flashStart = state.usbDevice.flashStart; // ex: 0x90581000
  let currentRomOffset = codeSection.length;
  const romPointers = [];

  state.romList.forEach(r => {
    outBuf.set(r.data, currentRomOffset);
    romPointers.push(currentRomOffset);
    currentRomOffset += ((r.size + 3) & ~3);
  });

  // Écriture des titres et de la table GameEntry
  let titlesStartOffset = currentRomOffset;
  const titlePointers = [];
  const textEncoder = new TextEncoder();

  state.romList.forEach(r => {
    const encoded = textEncoder.encode(r.title.slice(0, 31));
    outBuf.set(encoded, titlesStartOffset);
    outBuf[titlesStartOffset + encoded.length] = 0; // Terminateur
    titlePointers.push(titlesStartOffset);
    titlesStartOffset += titleSize;
  });

  // Écriture de la table GameEntry
  let tableOffset = titlesStartOffset;
  const dv = new DataView(outBuf.buffer);

  state.romList.forEach((r, idx) => {
    const entryOffset = tableOffset + idx * gameEntrySize;
    const titlePtr = flashStart + titlePointers[idx];
    const romPtr = flashStart + romPointers[idx];

    dv.setUint32(entryOffset + 0, titlePtr, true);
    dv.setUint32(entryOffset + 4, romPtr, true);
    dv.setUint32(entryOffset + 8, r.size, true);
    dv.setUint32(entryOffset + 12, r.crc32, true);
    dv.setUint8(entryOffset + 16, r.mapper);
    dv.setUint8(entryOffset + 17, 0); // padding
    dv.setUint16(entryOffset + 18, r.prgKb, true);
    dv.setUint16(entryOffset + 20, r.chrKb, true);
    dv.setUint16(entryOffset + 22, 0, true); // padding
  });

  // Mise à jour de la taille totale de l'app dans l'en-tête EADK (Offset 0x18 = 24)
  dv.setUint32(24, totalAppSize, true);

  return outBuf;
}

// Flashing du pack personnalisé vers la calculatrice
async function flashCustomPack() {
  const flashBtn = document.getElementById("btn-flash-custom");
  const consoleBox = document.getElementById("creator-console");
  const progressBar = document.getElementById("creator-progress");

  logConsole(consoleBox, "Génération du binaire personnalisé...", "info");
  flashBtn.disabled = true;
  updateUsbBadge(false, "Connexion...");

  try {
    const info = await state.usbDevice.connect();
    updateUsbBadge(true, info.model);
    logConsole(consoleBox, `Calculatrice détectée : ${info.productName} (${info.productId})`, "success");

    logConsole(consoleBox, "Assemblage des ROMs et de la table d'indexation...", "info");
    const customBin = await buildCustomBinary();
    logConsole(consoleBox, `Pack assemblé avec succès (${Math.round(customBin.byteLength / 1024)} Ko, ${state.romList.length} jeux).`, "success");

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
    flashBtn.disabled = false;
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
