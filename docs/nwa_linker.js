/**
 * Pure-JS ELF32 ARM Thumb-2 Relocatable Linker for NumWorks External Apps (.nwa)
 * Links web_crt0.o + nofrendo.nwa for any target memory map
 * (flashStart, ramStart, ramLength, trampolineStart) and embeds custom ROMs.
 */

const SHT_NULL = 0;
const SHT_PROGBITS = 1;
const SHT_SYMTAB = 2;
const SHT_STRTAB = 3;
const SHT_NOBITS = 8;
const SHT_REL = 9;
const SHT_ARM_EXIDX = 0x70000001;

const SHF_WRITE = 0x1;
const SHF_ALLOC = 0x2;
const SHF_EXECINSTR = 0x4;

const SHN_UNDEF = 0;
const SHN_ABS = 0xFFF1;
const SHN_COMMON = 0xFFF2;

const STB_LOCAL = 0;
const STB_GLOBAL = 1;
const STB_WEAK = 2;

const R_ARM_ABS32 = 2;
const R_ARM_THM_CALL = 10;
const R_ARM_THM_JUMP24 = 30;
const R_ARM_PREL31 = 42;

function alignUp(val, align) {
  const a = align > 1 ? align : 1;
  return (val + a - 1) & ~(a - 1);
}

function readCString(u8, offset) {
  let end = offset;
  while (end < u8.length && u8[end] !== 0) end++;
  return new TextDecoder("ascii").decode(u8.subarray(offset, end));
}

function parseElfRel(arrayBuffer, tag) {
  const u8 = new Uint8Array(arrayBuffer);
  const dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength);

  if (u8[0] !== 0x7F || u8[1] !== 0x45 || u8[2] !== 0x4C || u8[3] !== 0x46) {
    throw new Error(`[${tag}] Fichier ELF invalide`);
  }
  if (u8[4] !== 1 || u8[5] !== 1) {
    throw new Error(`[${tag}] Seul ELF32 little-endian est pris en charge`);
  }
  const eType = dv.getUint16(16, true);
  const eMachine = dv.getUint16(18, true);
  if (eType !== 1 || eMachine !== 40) {
    throw new Error(`[${tag}] Type/architecture ELF inattendu (${eType}/${eMachine})`);
  }

  const eShoff = dv.getUint32(32, true);
  const eShentsize = dv.getUint16(46, true);
  const eShnum = dv.getUint16(48, true);
  const eShstrndx = dv.getUint16(50, true);

  const rawSections = [];
  for (let i = 0; i < eShnum; i++) {
    const base = eShoff + i * eShentsize;
    rawSections.push({
      index: i,
      shName: dv.getUint32(base + 0, true),
      shType: dv.getUint32(base + 4, true),
      shFlags: dv.getUint32(base + 8, true),
      shAddr: dv.getUint32(base + 12, true),
      shOffset: dv.getUint32(base + 16, true),
      shSize: dv.getUint32(base + 20, true),
      shLink: dv.getUint32(base + 24, true),
      shInfo: dv.getUint32(base + 28, true),
      shAddralign: dv.getUint32(base + 32, true),
      shEntsize: dv.getUint32(base + 36, true),
      name: "",
      data: null,
      rels: [],
      objTag: tag,
      kept: false,
      vma: 0,
      foff: -1,
      category: null
    });
  }

  const shstr = rawSections[eShstrndx];
  for (const s of rawSections) {
    s.name = readCString(u8, shstr.shOffset + s.shName);
    if (s.shType !== SHT_NOBITS && s.shType !== SHT_NULL && s.shSize > 0) {
      s.data = u8.subarray(s.shOffset, s.shOffset + s.shSize);
    } else if (s.shType === SHT_PROGBITS) {
      s.data = new Uint8Array(0);
    }
  }

  // Parse symtab
  const symtabSec = rawSections.find(s => s.shType === SHT_SYMTAB);
  if (!symtabSec) {
    throw new Error(`[${tag}] Table des symboles absente`);
  }
  const strtabSec = rawSections[symtabSec.shLink];
  const numSyms = Math.floor(symtabSec.shSize / symtabSec.shEntsize);
  const symbols = [];
  for (let i = 0; i < numSyms; i++) {
    const base = symtabSec.shOffset + i * symtabSec.shEntsize;
    const stName = dv.getUint32(base + 0, true);
    const stValue = dv.getUint32(base + 4, true);
    const stSize = dv.getUint32(base + 8, true);
    const stInfo = u8[base + 12];
    const stOther = u8[base + 13];
    const stShndx = dv.getUint16(base + 14, true);
    symbols.push({
      index: i,
      name: stName ? readCString(u8, strtabSec.shOffset + stName) : "",
      value: stValue,
      size: stSize,
      bind: stInfo >> 4,
      type: stInfo & 0xF,
      other: stOther,
      shndx: stShndx,
      objTag: tag,
      commonAddr: 0,
      commonKept: false
    });
  }

  // Attach REL entries to target sections
  for (const s of rawSections) {
    if (s.shType === SHT_REL) {
      const target = rawSections[s.shInfo];
      const count = Math.floor(s.shSize / s.shEntsize);
      for (let i = 0; i < count; i++) {
        const base = s.shOffset + i * s.shEntsize;
        const rOffset = dv.getUint32(base + 0, true);
        const rInfo = dv.getUint32(base + 4, true);
        target.rels.push({
          offset: rOffset,
          type: rInfo & 0xFF,
          symIndex: rInfo >>> 8
        });
      }
    }
  }

  return { sections: rawSections, symbols };
}

/**
 * Link crt0 + nofrendo.nwa into a ready-to-flash EADK binary and inject ROMs.
 *
 * @param {ArrayBuffer} nwaBuffer
 * @param {ArrayBuffer} crt0Buffer
 * @param {{ flashStart: number, ramStart: number, ramLength: number, trampolineStart: number }} memMap
 * @param {Array<{ title: string, data: Uint8Array, size: number, crc32: number, mapper: number, prgKb: number, chrKb: number }>} romList
 * @returns {Uint8Array}
 */
export function linkNwaWithRoms(nwaBuffer, crt0Buffer, memMap, romList = []) {
  const objs = [
    parseElfRel(crt0Buffer, "crt0"),
    parseElfRel(nwaBuffer, "nwa")
  ];

  // Build global symbol map (non-local defined symbols + COMMON symbols)
  const globalSymbols = new Map();
  const commonSymbols = new Map();

  for (const obj of objs) {
    for (const sym of obj.symbols) {
      if (sym.bind === STB_LOCAL || !sym.name) continue;
      if (sym.shndx === SHN_COMMON) {
        const existing = commonSymbols.get(sym.name);
        if (!existing) {
          commonSymbols.set(sym.name, sym);
        } else if (sym.size > existing.size) {
          existing.size = sym.size;
          existing.value = Math.max(existing.value, sym.value);
        }
      } else if (sym.shndx !== SHN_UNDEF) {
        // Prefer non-weak over weak
        const existing = globalSymbols.get(sym.name);
        if (!existing || (existing.sym.bind === STB_WEAK && sym.bind !== STB_WEAK)) {
          globalSymbols.set(sym.name, { obj, sym });
        }
      }
    }
  }

  function resolveSymbolObj(obj, symIndex) {
    const sym = obj.symbols[symIndex];
    if (!sym) throw new Error(`Symbole introuvable #${symIndex} dans ${obj.sections[0].objTag}`);
    if (sym.shndx === SHN_UNDEF) {
      if (!sym.name) return { kind: "zero", value: 0 };
      const found = globalSymbols.get(sym.name);
      if (found) {
        if (found.sym.shndx === SHN_ABS) {
          return { kind: "abs", value: found.sym.value };
        }
        return { kind: "sec", obj: found.obj, sec: found.obj.sections[found.sym.shndx], sym: found.sym };
      }
      const comm = commonSymbols.get(sym.name);
      if (comm) {
        return { kind: "common", sym: comm };
      }
      // Linker-defined symbols handled later
      if (
        sym.name === "_data_section_start_flash" ||
        sym.name === "_data_section_start_ram" ||
        sym.name === "_data_section_end_ram" ||
        sym.name === "_bss_section_start_ram" ||
        sym.name === "_bss_section_end_ram" ||
        sym.name === "_heap_start" ||
        sym.name === "_heap_end" ||
        sym.name === "_userland_trampoline_address" ||
        sym.name === "_eadk_app_end"
      ) {
        return { kind: "linker", name: sym.name };
      }
      if (sym.bind === STB_WEAK) {
        return { kind: "zero", value: 0 };
      }
      throw new Error(`Symbole non résolu lors du link NWA : ${sym.name}`);
    }
    if (sym.shndx === SHN_ABS) {
      return { kind: "abs", value: sym.value };
    }
    if (sym.shndx === SHN_COMMON) {
      return { kind: "common", sym: commonSymbols.get(sym.name) || sym };
    }
    return { kind: "sec", obj, sec: obj.sections[sym.shndx], sym };
  }

  // Classify sections
  for (const obj of objs) {
    for (const s of obj.sections) {
      if ((s.shFlags & SHF_ALLOC) === 0 || s.shType === SHT_NULL) continue;
      if (s.name === ".rodata.eadk_app_name") s.category = "name";
      else if (s.name === ".rodata.eadk_app_icon") s.category = "icon";
      else if (s.name === ".rodata.eadk_api_level") s.category = "api";
      else if (s.shType === SHT_NOBITS && (s.shFlags & SHF_WRITE)) s.category = "bss";
      else if (s.shType === SHT_PROGBITS && (s.shFlags & SHF_WRITE)) s.category = "data";
      else if (s.shType === SHT_PROGBITS && (s.shFlags & SHF_EXECINSTR)) s.category = "text";
      else if (s.shType === SHT_PROGBITS || s.shType === SHT_ARM_EXIDX) s.category = "rodata";
    }
  }

  // Garbage-collect unreachable sections starting from entry points & EADK metadata
  const queue = [];
  function markSection(obj, sec) {
    if (!sec || sec.kept || !sec.category) return;
    sec.kept = true;
    queue.push({ obj, sec });
  }

  for (const rootName of ["_start", "main", "eadk_app_name", "eadk_app_icon", "eadk_api_level", "g_rom_catalog"]) {
    const g = globalSymbols.get(rootName);
    if (g && g.sym.shndx < g.obj.sections.length) {
      markSection(g.obj, g.obj.sections[g.sym.shndx]);
    }
  }

  while (queue.length > 0) {
    const { obj, sec } = queue.shift();
    for (const rel of sec.rels) {
      const target = resolveSymbolObj(obj, rel.symIndex);
      if (target.kind === "sec") {
        markSection(target.obj, target.sec);
      } else if (target.kind === "common") {
        target.sym.commonKept = true;
      }
    }
  }

  // Group kept sections by linker output bucket
  const buckets = {
    name: [],
    icon: [],
    api: [],
    text: [],
    rodata: [],
    data: [],
    bss: []
  };

  for (const obj of objs) {
    for (const s of obj.sections) {
      if (s.kept && s.category) {
        buckets[s.category].push(s);
      }
    }
  }

  if (buckets.name.length === 0 || buckets.icon.length === 0 || buckets.api.length === 0) {
    throw new Error("Sections EADK (.rodata.eadk_app_name / icon / api_level) manquantes dans le .nwa");
  }

  const flashStart = memMap.flashStart >>> 0;
  const ramStart = memMap.ramStart >>> 0;
  const ramLength = memMap.ramLength >>> 0;

  // Layout Flash (starting at flashStart + 32-byte .eadk_app_info header)
  let curFlashOff = 32;
  let apiLevelValue = 0;
  if (buckets.api[0].data && buckets.api[0].data.length >= 4) {
    const apidv = new DataView(buckets.api[0].data.buffer, buckets.api[0].data.byteOffset, 4);
    apiLevelValue = apidv.getUint32(0, true);
  }

  for (const cat of ["name", "icon", "text", "rodata"]) {
    for (const s of buckets[cat]) {
      if (cat === "text") {
        curFlashOff = alignUp(curFlashOff, Math.max(4, s.shAddralign));
      } else {
        curFlashOff = alignUp(curFlashOff, Math.max(1, s.shAddralign));
      }
      s.foff = curFlashOff;
      s.vma = (flashStart + curFlashOff) >>> 0;
      curFlashOff += s.shSize;
    }
  }

  // Normalize ROM entries so callers can pass minimal { name/title, data } objects too
  const numGames = Math.min(romList.length, 32);
  const normRoms = [];
  for (let i = 0; i < numGames; i++) {
    const r = romList[i];
    const data = r.data instanceof Uint8Array ? r.data : new Uint8Array(r.data);
    normRoms.push({
      title: String(r.title || r.name || `Jeu ${i + 1}`),
      data,
      size: (r.size !== undefined ? r.size : data.byteLength) >>> 0,
      crc32: (r.crc32 || 0) >>> 0,
      mapper: (r.mapper || 0) & 0xFF,
      prgKb: (r.prgKb || 0) & 0xFFFF,
      chrKb: (r.chrKb || 0) & 0xFFFF
    });
  }

  // Reserve ROMs right after .rodata (aligned to 4 bytes), still inside Flash before _eadk_app_end
  const romOffsets = [];
  for (let i = 0; i < numGames; i++) {
    curFlashOff = alignUp(curFlashOff, 4);
    romOffsets.push(curFlashOff);
    curFlashOff += normRoms[i].size;
  }

  curFlashOff = alignUp(curFlashOff, 4);
  const eadkAppEnd = (flashStart + curFlashOff) >>> 0;

  // Layout .data (LMA in Flash, VMA in RAM) preserving exact relative byte offsets
  let curRamAddr = alignUp(ramStart, 4);
  const firstDataAlign = buckets.data.length > 0 ? Math.max(4, buckets.data[0].shAddralign) : 4;
  curRamAddr = alignUp(curRamAddr, firstDataAlign);
  curFlashOff = alignUp(curFlashOff, firstDataAlign);
  const dataStartFlash = (flashStart + curFlashOff) >>> 0;
  const dataStartRam = curRamAddr;

  for (const s of buckets.data) {
    const a = Math.max(1, s.shAddralign);
    const alignedRam = alignUp(curRamAddr, a);
    const pad = alignedRam - curRamAddr;
    curRamAddr += pad;
    curFlashOff += pad;
    s.foff = curFlashOff;
    s.vma = curRamAddr >>> 0;
    curFlashOff += s.shSize;
    curRamAddr += s.shSize;
  }
  const dataEndRam = curRamAddr >>> 0;

  // Layout .bss + COMMON in RAM
  curRamAddr = alignUp(curRamAddr, 4);
  const bssStartRam = curRamAddr >>> 0;

  for (const s of buckets.bss) {
    const a = Math.max(1, s.shAddralign);
    curRamAddr = alignUp(curRamAddr, a);
    s.vma = curRamAddr >>> 0;
    curRamAddr += s.shSize;
  }
  for (const comm of commonSymbols.values()) {
    if (!comm.commonKept) continue;
    const a = Math.max(1, comm.value);
    curRamAddr = alignUp(curRamAddr, a);
    comm.commonAddr = curRamAddr >>> 0;
    curRamAddr += comm.size;
  }
  const bssEndRam = curRamAddr >>> 0;
  const heapStart = alignUp(bssEndRam, 4) >>> 0;
  const heapEnd = (ramStart + ramLength) >>> 0;

  if (heapStart >= heapEnd) {
    throw new Error(`RAM insuffisante pour Nofrendo (${bssEndRam - ramStart} o utilisés sur ${ramLength} o)`);
  }

  const linkerSyms = {
    _data_section_start_flash: dataStartFlash,
    _data_section_start_ram: dataStartRam,
    _data_section_end_ram: dataEndRam,
    _bss_section_start_ram: bssStartRam,
    _bss_section_end_ram: bssEndRam,
    _heap_start: heapStart,
    _heap_end: heapEnd,
    _userland_trampoline_address: memMap.trampolineStart >>> 0,
    _eadk_app_end: eadkAppEnd
  };

  // Allocate final output image
  const totalSize = curFlashOff;
  const outBuf = new Uint8Array(totalSize);
  const outDv = new DataView(outBuf.buffer);

  // Copy section payloads
  for (const cat of ["name", "icon", "text", "rodata", "data"]) {
    for (const s of buckets[cat]) {
      if (s.data && s.data.length > 0) {
        outBuf.set(s.data, s.foff);
      }
    }
  }

  // Write ROM payloads
  for (let i = 0; i < numGames; i++) {
    outBuf.set(normRoms[i].data, romOffsets[i]);
  }

  // Helper to resolve exact symbol address
  function evalSym(obj, symIndex) {
    const target = resolveSymbolObj(obj, symIndex);
    switch (target.kind) {
      case "zero":
        return 0;
      case "abs":
        return target.value >>> 0;
      case "common":
        return target.sym.commonAddr >>> 0;
      case "linker":
        return linkerSyms[target.name] >>> 0;
      case "sec":
        return (target.sec.vma + target.sym.value) >>> 0;
      default:
        throw new Error("Type de symbole inconnu");
    }
  }

  // Apply relocations
  for (const cat of ["text", "rodata", "data"]) {
    for (const s of buckets[cat]) {
      const obj = objs.find(o => o.sections[s.index] === s);
      for (const rel of s.rels) {
        const pos = s.foff + rel.offset;
        const P = (s.vma + rel.offset) >>> 0;
        const target = resolveSymbolObj(obj, rel.symIndex);
        if (target.kind === "zero" && (rel.type === R_ARM_THM_CALL || rel.type === R_ARM_THM_JUMP24)) {
          // Weak undefined branch target -> NOP.W (nop; nop)
          outDv.setUint16(pos, 0xBF00, true);
          outDv.setUint16(pos + 2, 0xBF00, true);
          continue;
        }
        const S = evalSym(obj, rel.symIndex);

        if (rel.type === R_ARM_ABS32) {
          const A = outDv.getInt32(pos, true);
          outDv.setUint32(pos, (S + A) >>> 0, true);
        } else if (rel.type === R_ARM_THM_CALL || rel.type === R_ARM_THM_JUMP24) {
          const upper = outDv.getUint16(pos, true);
          const lower = outDv.getUint16(pos + 2, true);

          const sBit = (upper >>> 10) & 1;
          const j1 = (lower >>> 13) & 1;
          const j2 = (lower >>> 11) & 1;
          const i1 = (~(j1 ^ sBit)) & 1;
          const i2 = (~(j2 ^ sBit)) & 1;
          const imm10 = upper & 0x3FF;
          const imm11 = lower & 0x7FF;

          let imm25 = (sBit << 24) | (i1 << 23) | (i2 << 22) | (imm10 << 12) | (imm11 << 1);
          if (imm25 & 0x01000000) imm25 -= 0x02000000;

          // Target without Thumb bit
          const targetEven = (S & ~1) >>> 0;
          const offset = (targetEven - P) + imm25;
          if (offset < -16777216 || offset > 16777214) {
            throw new Error(`Branchement ARM hors portée (${offset}) dans ${s.name}`);
          }

          const newS = (offset >>> 24) & 1;
          const newI1 = (offset >>> 23) & 1;
          const newI2 = (offset >>> 22) & 1;
          const newJ1 = (~(newI1 ^ newS)) & 1;
          const newJ2 = (~(newI2 ^ newS)) & 1;
          const newImm10 = (offset >>> 12) & 0x3FF;
          const newImm11 = (offset >>> 1) & 0x7FF;

          const newUpper = (upper & 0xF800) | (newS << 10) | newImm10;
          const newLower = (lower & 0xD000) | (newJ1 << 13) | (newJ2 << 11) | newImm11;
          outDv.setUint16(pos, newUpper, true);
          outDv.setUint16(pos + 2, newLower, true);
        } else if (rel.type === R_ARM_PREL31) {
          const word = outDv.getUint32(pos, true);
          const addend = (word << 1) >> 1;
          const val = ((S & ~1) + addend - P) | 0;
          outDv.setUint32(pos, (word & 0x80000000) | (val & 0x7FFFFFFF), true);
        } else {
          throw new Error(`Type de relocation ARM non géré : ${rel.type}`);
        }
      }
    }
  }

  // Patch g_rom_catalog ("NCAT") with the dynamic ROM entries
  const catalogSym = globalSymbols.get("g_rom_catalog");
  if (!catalogSym) {
    throw new Error("Symbole g_rom_catalog introuvable dans le .nwa");
  }
  const catalogSec = catalogSym.obj.sections[catalogSym.sym.shndx];
  const catalogOffset = catalogSec.foff + catalogSym.sym.value;
  if (outDv.getUint32(catalogOffset, true) !== 0x5441434E) {
    throw new Error("Magique NCAT invalide dans g_rom_catalog");
  }

  outDv.setUint32(catalogOffset + 4, numGames, true);
  const entryBaseOffset = catalogOffset + 8;
  const titlesBaseOffset = catalogOffset + 8 + 32 * 24;
  const textEncoder = new TextEncoder();

  for (let i = 0; i < numGames; i++) {
    const rom = normRoms[i];
    const titleOffset = titlesBaseOffset + i * 32;
    outBuf.fill(0, titleOffset, titleOffset + 32);
    const encodedTitle = textEncoder.encode(rom.title.slice(0, 31));
    outBuf.set(encodedTitle, titleOffset);
    const titlePtr = (flashStart + titleOffset) >>> 0;
    const romPtr = (flashStart + romOffsets[i]) >>> 0;

    const entryOffset = entryBaseOffset + i * 24;
    outDv.setUint32(entryOffset + 0, titlePtr, true);
    outDv.setUint32(entryOffset + 4, romPtr, true);
    outDv.setUint32(entryOffset + 8, rom.size, true);
    outDv.setUint32(entryOffset + 12, rom.crc32, true);
    outDv.setUint8(entryOffset + 16, rom.mapper);
    outDv.setUint8(entryOffset + 17, 0);
    outDv.setUint16(entryOffset + 18, rom.prgKb, true);
    outDv.setUint16(entryOffset + 20, rom.chrKb, true);
    outDv.setUint16(entryOffset + 22, 0, true);
  }

  // Write 32-byte .eadk_app_info header
  const startSym = globalSymbols.get("_start");
  const startSec = startSym.obj.sections[startSym.sym.shndx];
  const startAddr = (startSec.vma + (startSym.sym.value & ~1)) >>> 0;

  const nameSec = buckets.name[0];
  const iconSec = buckets.icon[0];

  outDv.setUint32(0, 0xDEC0BEBA, true);
  outDv.setUint32(4, apiLevelValue, true);
  outDv.setUint32(8, (nameSec.vma - flashStart) >>> 0, true);
  outDv.setUint32(12, iconSec.shSize >>> 0, true);
  outDv.setUint32(16, (iconSec.vma - flashStart) >>> 0, true);
  outDv.setUint32(20, (startAddr - flashStart) >>> 0, true);
  outDv.setUint32(24, (eadkAppEnd - flashStart) >>> 0, true);
  outDv.setUint32(28, 0xDEC0BEBA, true);

  return outBuf;
}
