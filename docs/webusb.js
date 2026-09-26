/**
 * NumWorks WebUSB & DFU Engine
 * Implementation pure ES6 pour navigateur (Chromium / Chrome / Brave / Edge).
 * Supporte :
 *  - Vendor ID STMicroelectronics / NumWorks : 0x0483
 *  - Product ID Calculatrice Epsilon : 0xA291
 *  - Product ID Bootloader DFU STM32 : 0xDF11
 */

const USB_VENDOR_ID = 0x0483;
const USB_PRODUCT_CALCULATOR = 0xA291;
const USB_PRODUCT_DFU = 0xDF11;

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

// Commandes étendues STMicroelectronics
const ST_COMMAND = {
  GET_COMMANDS: 0x00,
  SET_ADDRESS: 0x21,
  ERASE_SECTOR: 0x41
};

export class NumWorksDevice {
  constructor() {
    this.device = null;
    this.interfaceNumber = 0;
    this.alternateSetting = 0;
    this.transferSize = 2048;
    this.isConnected = false;
    this.flashStart = 0x90581000; // Adresse par défaut external apps N0120
    this.deviceModel = "NumWorks (Détection...)";
  }

  static isSupported() {
    return (typeof navigator !== "undefined" && "usb" in navigator);
  }

  async connect() {
    if (!NumWorksDevice.isSupported()) {
      throw new Error("WebUSB n'est pas pris en charge par ce navigateur. Veuillez utiliser Google Chrome, Brave ou Microsoft Edge.");
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
          dfuInterface = { number: iface.interfaceNumber, alt: alt.alternateSetting };
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
    this.deviceModel = (this.device.productId === USB_PRODUCT_CALCULATOR)
      ? "NumWorks N0120 / N0110 (OS Epsilon)"
      : "NumWorks (Mode DFU Bootloader)";

    return {
      productName: this.device.productName || "Calculatrice NumWorks",
      serialNumber: this.device.serialNumber || "N/A",
      productId: "0x" + this.device.productId.toString(16).padStart(4, "0"),
      model: this.deviceModel
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
    try {
      await this.controlTransferOut(DFU_REQUEST.ABORT);
    } catch (e) {
      // Ignorer si la calculatrice était déjà prête
    }

    let status = await this.getStatus();
    if (status.state === DFU_STATE.dfuERROR) {
      await this.clearStatus();
      status = await this.getStatus();
    }

    if (status.state !== DFU_STATE.dfuIDLE) {
      // Tentative supplémentaire de reset DFU
      await this.clearStatus();
    }
  }

  async pollUntil(predicate, timeoutMs = 10000) {
    const startTime = Date.now();
    while (Date.now() - startTime < timeoutMs) {
      const status = await this.getStatus();
      if (predicate(status.state)) {
        return status;
      }
      if (status.state === DFU_STATE.dfuERROR) {
        throw new Error(`Erreur DFU détectée (Code statut : ${status.status})`);
      }
      const delay = Math.max(status.pollTimeout, 10);
      await new Promise(r => setTimeout(r, delay));
    }
    throw new Error("Délai d'attente DFU dépassé (Timeout)");
  }

  async setAddress(address) {
    const buf = new ArrayBuffer(5);
    const view = new DataView(buf);
    view.setUint8(0, ST_COMMAND.SET_ADDRESS);
    view.setUint32(1, address, true);

    await this.controlTransferOut(DFU_REQUEST.DNLOAD, buf, 0);
    await this.pollUntil(state => state !== DFU_STATE.dfuDNBUSY);
  }

  async eraseSector(address) {
    await this.abortToIdle();

    const buf = new ArrayBuffer(5);
    const view = new DataView(buf);
    view.setUint8(0, ST_COMMAND.ERASE_SECTOR);
    view.setUint32(1, address, true);

    await this.controlTransferOut(DFU_REQUEST.DNLOAD, buf, 0);
    const status = await this.getStatus();

    if (status.state === DFU_STATE.dfuDNBUSY) {
      await this.pollUntil(state => state !== DFU_STATE.dfuDNBUSY, 20000);
    }
  }

  async installAppBinary(binaryData, onProgress = null) {
    if (!this.isConnected || !this.device) {
      throw new Error("Calculatrice non connectée.");
    }

    const totalBytes = binaryData.byteLength;
    const startAddress = this.flashStart;
    const sectorSize = 65536; // 64 Ko par secteur Flash externe OctoSPI

    // 1. Calcul des secteurs à effacer
    const sectorsToErase = [];
    const endAddress = startAddress + totalBytes;
    for (let addr = startAddress; addr < endAddress; addr += sectorSize) {
      sectorsToErase.push(addr);
    }

    if (onProgress) {
      onProgress({ stage: "erase", progress: 0, message: `Effacement de la mémoire Flash (0/${sectorsToErase.length})...` });
    }

    for (let i = 0; i < sectorsToErase.length; i++) {
      const sectorAddr = sectorsToErase[i];
      await this.eraseSector(sectorAddr);
      if (onProgress) {
        const p = (i + 1) / sectorsToErase.length;
        onProgress({
          stage: "erase",
          progress: p,
          message: `Effacement du secteur ${i + 1}/${sectorsToErase.length} (0x${sectorAddr.toString(16).toUpperCase()})...`
        });
      }
    }

    // 2. Écriture du binaire par blocs
    if (onProgress) {
      onProgress({ stage: "program", progress: 0, message: "Programmation des données NES..." });
    }

    let offset = 0;
    let currentAddress = startAddress;
    const u8Array = new Uint8Array(binaryData);

    while (offset < totalBytes) {
      const chunkSize = Math.min(totalBytes - offset, this.transferSize);
      const chunk = u8Array.subarray(offset, offset + chunkSize);

      await this.setAddress(currentAddress);
      await this.controlTransferOut(DFU_REQUEST.DNLOAD, chunk.buffer.slice(chunk.byteOffset, chunk.byteOffset + chunk.byteLength), 2);
      await this.pollUntil(state => state === DFU_STATE.dfuDNLOAD_IDLE);

      currentAddress += chunkSize;
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

    // 3. Sortie du mode DFU (Lancement de l'application)
    if (onProgress) {
      onProgress({ stage: "leave", progress: 1.0, message: "Finalisation et redémarrage de la calculatrice..." });
    }

    try {
      await this.setAddress(startAddress);
      await this.controlTransferOut(DFU_REQUEST.DNLOAD, new ArrayBuffer(0), 2);
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
    this.device = null;
  }
}
