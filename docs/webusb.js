/**
 * NumWorks WebUSB & DFU Engine
 * Implémentation pure ES6 pour navigateur (Chromium / Chrome / Brave / Edge).
 * Supporte :
 *  - Vendor ID STMicroelectronics / NumWorks : 0x0483
 *  - Product ID Calculatrice Epsilon : 0xA291
 *  - Product ID Bootloader DFU STM32 : 0xDF11
 *
 * Lit dynamiquement SlotInfo / KernelHeader / UserlandHeader via DFU UPLOAD
 * pour connaître la vraie carte mémoire (Slot A ou Slot B, Flash, RAM, Trampoline).
 */

const USB_VENDOR_ID = 0x0483;
const USB_PRODUCT_CALCULATOR = 0xA291;
const USB_PRODUCT_DFU = 0xDF11;

const DEFAULT_MAPS = {
  N0120: {
    flashStart: 0x90570000,
    flashEnd: 0x90770000,
    ramStart: 0x24012804,
    ramLength: 0x2D7FC,
    trampolineStart: 0x90420038
  },
  N0110: {
    flashStart: 0x90000000,
    flashEnd: 0x90800000,
    ramStart: 0x20000000,
    ramLength: 0x40000,
    trampolineStart: 0x90010030
  }
};

const RAM_CANDIDATES_N0120 = [
  { name: "N0120", ramStart: 0x24000000, ramEnd: 0x24050000 },
  { name: "N0110 / N0115", ramStart: 0x20000000, ramEnd: 0x20040000 }
];

const RAM_CANDIDATES_N0110 = [
  { name: "N0110 / N0115", ramStart: 0x20000000, ramEnd: 0x20040000 },
  { name: "N0120", ramStart: 0x24000000, ramEnd: 0x24050000 }
];

// Constantes d'en-tête Epsilon Header2
const HEADER2_SLOT_INFO_SIZE = 16;
const HEADER2_USERLAND_MAX_SIZE = 48;
const HEADER2_USERLAND_ISR_SIZE = 8;

// Commandes de base DFU standard
const DFU_REQUEST = {
  DETACH: 0,
  DNLOAD: 1,
  UPLOAD: 2,
  GETSTATUS: 3,
  CLRSTATUS: 4,
  GETSTATE: 5,
  ABORT: 6
};

// États DFU
const DFU_STATE = {
  appIDLE: 0,
  appDETACH: 1,
  dfuIDLE: 2,
  dfuDNLOAD_SYNC: 3,
  dfuDNBUSY: 4,
  dfuDNLOAD_IDLE: 5,
  dfuMANIFEST_SYNC: 6,
  dfuMANIFEST: 7,
  dfuMANIFEST_WAIT_RESET: 8,
  dfuUPLOAD_IDLE: 9,
  dfuERROR: 10
};

// Commandes étendues DFUse (STMicroelectronics)
const ST_COMMAND = {
  GET_COMMANDS: 0x00,
  SET_ADDRESS: 0x21,
  ERASE_SECTOR: 0x41
};

const DFU_STATUS_LABELS = {
  0: "OK",
  1: "errTARGET (adresse Flash refusée ou hors zone autorisée)",
  2: "errFILE",
  3: "errWRITE (échec d'écriture Flash)",
  4: "errERASE (échec d'effacement de secteur)",
  5: "errCHECK_ERASED",
  6: "errPROG",
  7: "errVERIFY",
  8: "errADDRESS (adresse hors plage DFU)",
  9: "errNOTDONE",
  10: "errFIRMWARE",
  11: "errVENDOR",
  12: "errUSBR",
  13: "errPOR",
  14: "errUNKNOWN",
  15: "errSTALLEDPKT (requête USB rejetée)"
};

function formatHex(addr) {
  return "0x" + (addr >>> 0).toString(16).toUpperCase().padStart(8, "0");
}

function describeDfuError(statusCode, context = "DFU") {
  const detail = DFU_STATUS_LABELS[statusCode] || `Code ${statusCode}`;
  return `Erreur DFU [${context}] : ${detail} (Code statut : ${statusCode})`;
}

export class NumWorksDevice {
  constructor() {
    this.device = null;
    this.interfaceNumber = 0;
    this.alternateSetting = 0;
    this.transferSize = 2048;
    this.isConnected = false;
    this.flashStart = DEFAULT_FLASH_START;
    this.flashEnd = DEFAULT_FLASH_END;
    this.flashSectorSize = 65536;
    this.memoryMap = null;
    this.deviceModel = "NumWorks (Détection...)";
  }

  static isSupported() {
    return (typeof navigator !== "undefined" && "usb" in navigator);
  }

  async connect() {
    if (!NumWorksDevice.isSupported()) {
      throw new Error("WebUSB n'est pas pris en charge par ce navigateur. Veuillez utiliser Google Chrome, Brave ou Microsoft Edge.");
    }

    if (this.device && this.device.opened) {
      await this.disconnect();
    }

    try {
      this.device = await navigator.usb.requestDevice({
        filters: [
          { vendorId: USB_VENDOR_ID, productId: USB_PRODUCT_CALCULATOR },
          { vendorId: USB_VENDOR_ID, productId: USB_PRODUCT_DFU }
        ]
      });
    } catch (err) {
      if (err.name === "NotFoundError") {
        throw new Error("Aucune calculatrice NumWorks sélectionnée.");
      }
      throw err;
    }

    await this.device.open();

    // Sélection de la configuration active
    if (this.device.configuration === null) {
      await this.device.selectConfiguration(1);
    }

    // Trouver l'interface DFU
    let dfuInterface = null;
    for (const iface of this.device.configuration.interfaces) {
      for (const alt of iface.alternates) {
        if (alt.interfaceClass === 0xFE && alt.interfaceSubclass === 0x01) {
          dfuInterface = {
            number: iface.interfaceNumber,
            alt: alt.alternateSetting,
            name: alt.interfaceName || ""
          };
          break;
        }
      }
      if (dfuInterface) break;
    }

    this.interfaceNumber = dfuInterface ? dfuInterface.number : 0;
    this.alternateSetting = dfuInterface ? dfuInterface.alt : 0;

    await this.device.claimInterface(this.interfaceNumber);
    await this.device.selectAlternateInterface(this.interfaceNumber, this.alternateSetting);

    // Initialisation DFU : remise à l'état IDLE
    await this.abortToIdle();

    this.isConnected = true;
    this.memoryMap = null;
    const family = this.getHardwareFamily();
    const defaultMap = DEFAULT_MAPS[family];
    this.flashStart = defaultMap.flashStart;
    this.flashEnd = defaultMap.flashEnd;
    this.flashSectorSize = 65536;

    // Détection automatique des adresses réelles de la calculatrice (Slot A/B, OS)
    let detected = null;
    if (this.device.productId === USB_PRODUCT_CALCULATOR) {
      try {
        detected = await this.detectMemoryMap();
      } catch (e) {
        // On laisse detected = null pour remonter une info claire
        detected = null;
      }
    }

    if (detected) {
      this.memoryMap = detected;
      this.flashStart = detected.flashStart;
      this.flashEnd = detected.flashEnd;
      const versionStr = detected.userlandVersion && detected.userlandVersion !== "unknown"
        ? `Epsilon ${detected.userlandVersion}`
        : "OS Epsilon";
      const slotLabel = detected.flashStart < 0x90400000 ? "Slot A" : "Slot B";
      this.deviceModel = `NumWorks ${detected.model} (${versionStr}, ${slotLabel})`;
    } else if (this.device.productId === USB_PRODUCT_CALCULATOR) {
      this.memoryMap = {
        model: family,
        userlandVersion: "default",
        ...defaultMap,
        flashLength: defaultMap.flashEnd - defaultMap.flashStart
      };
      this.deviceModel = `NumWorks ${family} (OS Epsilon — carte mémoire par défaut)`;
    } else {
      this.deviceModel = "NumWorks (Mode DFU Bootloader 0xDF11)";
    }

    return {
      productName: this.device.productName || "Calculatrice NumWorks",
      serialNumber: this.device.serialNumber || "N/A",
      productId: "0x" + this.device.productId.toString(16).padStart(4, "0"),
      model: this.deviceModel,
      memoryMap: this.memoryMap
    };
  }

  async controlTransferIn(request, length, value = 0) {
    const result = await this.device.controlTransferIn({
      requestType: "class",
      recipient: "interface",
      request: request,
      value: value,
      index: this.interfaceNumber
    }, length);

    if (result.status !== "ok") {
      throw new Error(`Échec du transfert USB In (${request}) : ${result.status}`);
    }
    return result.data;
  }

  async controlTransferOut(request, data = new ArrayBuffer(0), value = 0) {
    const result = await this.device.controlTransferOut({
      requestType: "class",
      recipient: "interface",
      request: request,
      value: value,
      index: this.interfaceNumber
    }, data);

    if (result.status !== "ok") {
      throw new Error(`Échec du transfert USB Out (${request}) : ${result.status}`);
    }
    return result.bytesWritten;
  }

  async getState() {
    const data = await this.controlTransferIn(DFU_REQUEST.GETSTATE, 1);
    return data.getUint8(0);
  }

  async getStatus() {
    const data = await this.controlTransferIn(DFU_REQUEST.GETSTATUS, 6);
    return {
      status: data.getUint8(0),
      pollTimeout: data.getUint32(1, true) & 0xFFFFFF,
      state: data.getUint8(4)
    };
  }

  async clearStatus() {
    return await this.controlTransferOut(DFU_REQUEST.CLRSTATUS);
  }

  async abortToIdle() {
    let status = await this.getStatus();
    if (status.state === DFU_STATE.dfuIDLE) {
      return;
    }
    if (status.state === DFU_STATE.dfuERROR) {
      await this.clearStatus();
      status = await this.getStatus();
    }
    if (status.state !== DFU_STATE.dfuIDLE) {
      try {
        await this.controlTransferOut(DFU_REQUEST.ABORT);
      } catch (e) {
        // Ignorer si ABORT non requis
      }
      status = await this.getStatus();
      if (status.state === DFU_STATE.dfuERROR) {
        await this.clearStatus();
        status = await this.getStatus();
      }
    }
  }

  async pollUntil(predicate, timeoutMs = 10000, context = "DFU") {
    const startTime = Date.now();
    while (Date.now() - startTime < timeoutMs) {
      const status = await this.getStatus();
      if (status.status !== 0) {
        throw new Error(describeDfuError(status.status, context));
      }
      if (status.state === DFU_STATE.dfuERROR) {
        throw new Error(describeDfuError(status.status, context));
      }
      if (predicate(status.state)) {
        return status;
      }
      const delay = Math.max(status.pollTimeout, 10);
      await new Promise(r => setTimeout(r, delay));
    }
    throw new Error(`Délai d'attente DFU dépassé [${context}]`);
  }

  async setAddress(address) {
    const ctx = `SET_ADDRESS ${formatHex(address)}`;
    const buf = new ArrayBuffer(5);
    const view = new DataView(buf);
    view.setUint8(0, ST_COMMAND.SET_ADDRESS);
    view.setUint32(1, address >>> 0, true);

    for (let attempt = 0; attempt < 5; attempt++) {
      await this.controlTransferOut(DFU_REQUEST.DNLOAD, buf, 0);
      const status = await this.pollUntil(
        state => state !== DFU_STATE.dfuDNBUSY,
        10000,
        ctx
      );
      if (status.state === DFU_STATE.dfuDNLOAD_IDLE) {
        return;
      }
    }
    throw new Error(`Impossible de positionner le pointeur d'adresse DFU (${formatHex(address)})`);
  }

  async upload(address, length) {
    const ctx = `UPLOAD ${formatHex(address)}`;
    try {
      const state = await this.getState();
      if (state !== DFU_STATE.dfuIDLE) {
        await this.abortToIdle();
      }

      await this.setAddress(address);
      await this.abortToIdle();

      const result = new Uint8Array(length);
      let blockNum = 2;
      let bytesRead = 0;

      while (bytesRead < length) {
        const chunkLen = Math.min(this.transferSize, length - bytesRead);
        const dv = await this.controlTransferIn(DFU_REQUEST.UPLOAD, chunkLen, blockNum++);
        if (dv.byteLength === 0) break;
        const slice = new Uint8Array(dv.buffer, dv.byteOffset, dv.byteLength);
        const copyLen = Math.min(slice.length, length - bytesRead);
        result.set(slice.subarray(0, copyLen), bytesRead);
        bytesRead += copyLen;
      }

      await this.abortToIdle();
      return result;
    } catch (err) {
      try {
        await this.abortToIdle();
      } catch (e) {
        // Ignorer
      }
      throw new Error(`${ctx} : ${err.message}`);
    }
  }

  getHardwareFamily() {
    if (!this.device) return "N0120";
    const major = this.device.deviceVersionMajor;
    const minor = this.device.deviceVersionMinor;
    if (major === 1 && (minor === 10 || minor === 15 || minor === 0)) {
      return "N0110";
    }
    return "N0120";
  }

  async detectMemoryMap() {
    const family = this.getHardwareFamily();
    const candidates = family === "N0110" ? RAM_CANDIDATES_N0110 : RAM_CANDIDATES_N0120;
    const defaultMap = DEFAULT_MAPS[family];

    for (const candidate of candidates) {
      let slotBuf;
      try {
        await this.abortToIdle();
        slotBuf = await this.upload(candidate.ramStart, HEADER2_SLOT_INFO_SIZE);
      } catch (e) {
        await this.abortToIdle();
        continue;
      }

      const slotDv = new DataView(slotBuf.buffer, slotBuf.byteOffset, slotBuf.byteLength);
      // Magic "BADBEEEF" en octets séquentiels (big-endian) au début et à la fin de SlotInfo
      if (slotDv.getUint32(0, false) !== 0xBADBEEEF || slotDv.getUint32(12, false) !== 0xBADBEEEF) {
        continue;
      }

      const kernelHeaderAddr = slotDv.getUint32(4, true);
      const userlandHeaderAddr = slotDv.getUint32(8, true);

      if (userlandHeaderAddr < 0x08000000 || userlandHeaderAddr > 0x91000000) {
        continue;
      }

      const uhBuf = await this.upload(userlandHeaderAddr, HEADER2_USERLAND_MAX_SIZE);
      const uhDv = new DataView(uhBuf.buffer, uhBuf.byteOffset, uhBuf.byteLength);

      // Magic "FEEDC0DE" en octets séquentiels au début
      if (uhDv.getUint32(0, false) !== 0xFEEDC0DE) {
        continue;
      }

      // Version UTF-8 (octets 4..11 terminés par NUL)
      let verEnd = 4;
      while (verEnd < 12 && uhBuf[verEnd] !== 0) verEnd++;
      const userlandVersion = new TextDecoder("utf-8").decode(uhBuf.subarray(4, verEnd)) || "unknown";

      let off = 12;
      const storageAddress = uhDv.getUint32(off, true); off += 4;
      const storageSize = uhDv.getUint32(off, true); off += 4;
      const flashStart = uhDv.getUint32(off, true); off += 4;
      const flashEnd = uhDv.getUint32(off, true); off += 4;

      let ramStart = defaultMap.ramStart;
      let ramEnd = defaultMap.ramStart + defaultMap.ramLength;
      let sentinelOff = -1;

      if (uhDv.getUint32(off, false) === 0xFEEDC0DE) {
        sentinelOff = off;
      } else {
        ramStart = uhDv.getUint32(off, true); off += 4;
        ramEnd = uhDv.getUint32(off, true); off += 4;
        if (uhDv.getUint32(off, false) === 0xFEEDC0DE) {
          sentinelOff = off;
        } else {
          // deviceNameFlashStart / deviceNameFlashEnd présents sur les OS récents
          off += 8;
          if (off <= HEADER2_USERLAND_MAX_SIZE - 4 && uhDv.getUint32(off, false) === 0xFEEDC0DE) {
            sentinelOff = off;
          }
        }
      }

      if (sentinelOff < 0) {
        // Fallback : chercher la seconde occurrence de FEEDC0DE
        for (let p = 28; p <= HEADER2_USERLAND_MAX_SIZE - 4; p += 4) {
          if (uhDv.getUint32(p, false) === 0xFEEDC0DE) {
            sentinelOff = p;
            break;
          }
        }
      }

      if (sentinelOff < 0 || flashEnd <= flashStart) {
        continue;
      }

      const trampolineStart = (userlandHeaderAddr + sentinelOff + 4 + HEADER2_USERLAND_ISR_SIZE) >>> 0;
      const ramLength = ramEnd > ramStart ? (ramEnd - ramStart) : defaultMap.ramLength;

      return {
        model: candidate.name,
        userlandVersion,
        kernelHeaderAddr,
        userlandHeaderAddr,
        storageAddress,
        storageSize,
        flashStart: flashStart >>> 0,
        flashEnd: flashEnd >>> 0,
        flashLength: (flashEnd - flashStart) >>> 0,
        ramStart: ramStart >>> 0,
        ramEnd: (ramStart + ramLength) >>> 0,
        ramLength: ramLength >>> 0,
        trampolineStart
      };
    }

    return null;
  }

  async eraseSector(address) {
    const ctx = `ERASE_SECTOR ${formatHex(address)}`;
    await this.abortToIdle();

    const buf = new ArrayBuffer(5);
    const view = new DataView(buf);
    view.setUint8(0, ST_COMMAND.ERASE_SECTOR);
    view.setUint32(1, address >>> 0, true);

    let status;
    for (let attempt = 0; attempt < 5; attempt++) {
      await this.controlTransferOut(DFU_REQUEST.DNLOAD, buf, 0);
      status = await this.getStatus();
      if (status.status !== 0 || status.state === DFU_STATE.dfuERROR) {
        throw new Error(describeDfuError(status.status, ctx));
      }
      if (status.state === DFU_STATE.dfuDNBUSY) {
        break;
      }
    }

    if (status.state !== DFU_STATE.dfuDNBUSY) {
      throw new Error(`La calculatrice n'a pas démarré l'effacement (${ctx}, état=${status.state})`);
    }

    await this.pollUntil(state => state !== DFU_STATE.dfuDNBUSY, 20000, ctx);
  }

  async installAppBinary(binaryData, onProgress = null) {
    if (!this.isConnected || !this.device) {
      throw new Error("Calculatrice non connectée.");
    }

    if (this.device.productId === USB_PRODUCT_DFU && !this.memoryMap) {
      throw new Error(
        "La calculatrice est en mode Bootloader STM32 (0xDF11). Appuyez sur le bouton RESET au dos pour démarrer Epsilon, puis reconnectez-la."
      );
    }

    const totalBytes = binaryData.byteLength;
    const startAddress = this.flashStart >>> 0;
    const sectorSize = this.flashSectorSize;

    if (this.flashEnd && (startAddress + totalBytes) > this.flashEnd) {
      const availKo = Math.floor((this.flashEnd - startAddress) / 1024);
      const reqKo = Math.ceil(totalBytes / 1024);
      throw new Error(
        `Le pack (${reqKo} Ko) dépasse l'espace Flash disponible pour les applications externes sur cette calculatrice (${availKo} Ko entre ${formatHex(startAddress)} et ${formatHex(this.flashEnd)}).`
      );
    }

    await this.abortToIdle();

    // 1. Calcul des secteurs à effacer
    const sectorsToErase = [];
    const endAddress = startAddress + totalBytes;
    for (let addr = startAddress; addr < endAddress; addr += sectorSize) {
      sectorsToErase.push(addr >>> 0);
    }

    if (onProgress) {
      onProgress({
        stage: "erase",
        progress: 0,
        message: `Effacement de la mémoire Flash (0/${sectorsToErase.length}) à partir de ${formatHex(startAddress)}...`
      });
    }

    for (let i = 0; i < sectorsToErase.length; i++) {
      const sectorAddr = sectorsToErase[i];
      if (onProgress) {
        onProgress({
          stage: "erase",
          progress: i / sectorsToErase.length,
          message: `Effacement du secteur ${i + 1}/${sectorsToErase.length} (${formatHex(sectorAddr)})...`
        });
      }
      await this.eraseSector(sectorAddr);
    }

    // Retour en IDLE après les commandes d'effacement avant d'écrire les blocs
    await this.abortToIdle();

    // 2. Écriture du binaire par blocs
    if (onProgress) {
      onProgress({ stage: "program", progress: 0, message: "Programmation des données NES..." });
    }

    let offset = 0;
    let currentAddress = startAddress;
    const u8Array = new Uint8Array(binaryData.buffer || binaryData, binaryData.byteOffset || 0, binaryData.byteLength);

    while (offset < totalBytes) {
      const chunkSize = Math.min(totalBytes - offset, this.transferSize);
      const chunk = u8Array.subarray(offset, offset + chunkSize);
      const ctx = `WRITE ${formatHex(currentAddress)}`;

      await this.setAddress(currentAddress);
      await this.controlTransferOut(
        DFU_REQUEST.DNLOAD,
        chunk.buffer.slice(chunk.byteOffset, chunk.byteOffset + chunk.byteLength),
        2
      );
      await this.pollUntil(state => state === DFU_STATE.dfuDNLOAD_IDLE, 10000, ctx);

      currentAddress = (currentAddress + chunkSize) >>> 0;
      offset += chunkSize;

      if (onProgress) {
        const p = offset / totalBytes;
        onProgress({
          stage: "program",
          progress: p,
          bytesWritten: offset,
          totalBytes: totalBytes,
          message: `Transfert : ${Math.round(p * 100)}% (${Math.round(offset / 1024)} Ko / ${Math.round(totalBytes / 1024)} Ko)`
        });
      }
    }

    // 3. Sortie du mode DFU (Lancement / validation)
    if (onProgress) {
      onProgress({ stage: "leave", progress: 1.0, message: "Finalisation et redémarrage de la calculatrice..." });
    }

    try {
      await this.setAddress(startAddress);
      await this.controlTransferOut(DFU_REQUEST.DNLOAD, new ArrayBuffer(0), 2);
      await this.pollUntil(state => state === DFU_STATE.dfuMANIFEST, 3000, "MANIFEST");
    } catch (e) {
      // Un reset immédiat par la calculatrice peut couper le port USB avant l'ACK, ce qui est normal
    }

    this.isConnected = false;
    try {
      await this.device.close();
    } catch (e) {
      // Ignorer
    }

    if (onProgress) {
      onProgress({ stage: "done", progress: 1.0, message: "Émulateur Nofrendo installé avec succès !" });
    }
  }

  async disconnect() {
    if (this.device && this.device.opened) {
      try {
        await this.device.close();
      } catch (e) {
        // Ignorer
      }
    }
    this.isConnected = false;
    this.memoryMap = null;
    this.device = null;
  }
}
