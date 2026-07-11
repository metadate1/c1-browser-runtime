const LOGICAL_SECTOR_SIZE = 2048;
const RAW_SECTOR_SIZE = 2352;
const RAW_USER_DATA_OFFSET = 24;
const VOLUME_DESCRIPTOR_START = 16;
const MAX_VOLUME_DESCRIPTORS = 64;
const MAX_DIRECTORY_BYTES = 16 * 1024 * 1024;
const STREAM_DIRECTORIES = ["S0", "S1", "S2", "S3"];
const STREAM_NAME_PATTERN = /^s[0-9a-f]{7}\.(?:nsd|nsf)$/i;

const ISO_LAYOUT = Object.freeze({
  label: "ISO 2048",
  sectorSize: LOGICAL_SECTOR_SIZE,
  userDataOffset: 0,
  raw: false,
});

const RAW_LAYOUT = Object.freeze({
  label: "MODE2/2352",
  sectorSize: RAW_SECTOR_SIZE,
  userDataOffset: RAW_USER_DATA_OFFSET,
  raw: true,
});

function fail(message) {
  throw new Error(`Could not read disc image: ${message}`);
}

function assertBlobLike(file) {
  if (!file || typeof file.size !== "number" || typeof file.slice !== "function") {
    throw new TypeError("extractCrashStreamsFromDisc expects a File or Blob.");
  }
  if (!Number.isSafeInteger(file.size) || file.size <= 0) {
    throw new RangeError("The selected disc image has an invalid size.");
  }
}

function assertByteRange(file, start, length, context) {
  const end = start + length;
  if (!Number.isSafeInteger(start) || !Number.isSafeInteger(length) || start < 0 || length < 0
      || !Number.isSafeInteger(end) || end > file.size) {
    fail(`${context} points outside the selected file`);
  }
}

async function readBlobBytes(file, start, length, context) {
  assertByteRange(file, start, length, context);
  return new Uint8Array(await file.slice(start, start + length).arrayBuffer());
}

function hasIsoSignature(bytes) {
  return bytes.length >= 7
    && bytes[1] === 0x43
    && bytes[2] === 0x44
    && bytes[3] === 0x30
    && bytes[4] === 0x30
    && bytes[5] === 0x31
    && bytes[6] === 1;
}

function assertRawMode2Header(bytes, offset, lba) {
  if (bytes.length - offset < RAW_USER_DATA_OFFSET) fail(`sector ${lba} has a truncated raw header`);
  if (bytes[offset] !== 0 || bytes[offset + 11] !== 0) fail(`sector ${lba} has an invalid CD sync header`);
  for (let index = 1; index <= 10; index += 1) {
    if (bytes[offset + index] !== 0xff) fail(`sector ${lba} has an invalid CD sync header`);
  }
  if (bytes[offset + 15] !== 2) fail(`sector ${lba} is not a Mode 2 sector`);
  for (let index = 0; index < 4; index += 1) {
    if (bytes[offset + 16 + index] !== bytes[offset + 20 + index]) {
      fail(`sector ${lba} has mismatched Mode 2 subheaders`);
    }
  }
  if ((bytes[offset + 18] & 0x20) !== 0) fail(`sector ${lba} is Mode 2 Form 2, not 2,048-byte Form 1`);
}

function sectorCount(file, layout) {
  return Math.floor(file.size / layout.sectorSize);
}

async function readLogicalSector(file, layout, lba) {
  if (!Number.isInteger(lba) || lba < 0 || lba >= sectorCount(file, layout)) {
    fail(`logical sector ${lba} is outside the image`);
  }

  const physicalOffset = lba * layout.sectorSize;
  if (layout.raw) {
    const rawSector = await readBlobBytes(file, physicalOffset, RAW_SECTOR_SIZE, `sector ${lba}`);
    assertRawMode2Header(rawSector, 0, lba);
    return rawSector.subarray(RAW_USER_DATA_OFFSET, RAW_USER_DATA_OFFSET + LOGICAL_SECTOR_SIZE);
  }
  return readBlobBytes(file, physicalOffset, LOGICAL_SECTOR_SIZE, `sector ${lba}`);
}

async function findPrimaryVolumeDescriptor(file, layout) {
  for (let index = 0; index < MAX_VOLUME_DESCRIPTORS; index += 1) {
    const lba = VOLUME_DESCRIPTOR_START + index;
    const descriptor = await readLogicalSector(file, layout, lba);
    if (!hasIsoSignature(descriptor)) fail(`${layout.label} sector ${lba} is not an ISO9660 volume descriptor`);
    if (descriptor[0] === 1) return descriptor;
    if (descriptor[0] === 255) break;
  }
  fail(`${layout.label} image has no primary ISO9660 volume descriptor`);
}

async function detectLayout(file) {
  const attempts = [];
  for (const layout of [RAW_LAYOUT, ISO_LAYOUT]) {
    if (file.size < (VOLUME_DESCRIPTOR_START + 1) * layout.sectorSize) continue;
    try {
      const pvd = await findPrimaryVolumeDescriptor(file, layout);
      return { ...layout, pvd };
    } catch (error) {
      attempts.push(`${layout.label}: ${error.message}`);
    }
  }
  fail(`the file is neither a raw MODE2/2352 track nor a 2,048-byte ISO (${attempts.join("; ")})`);
}

function readUint16LE(bytes, offset, context) {
  if (offset < 0 || offset + 2 > bytes.length) fail(`${context} is truncated`);
  return bytes[offset] | (bytes[offset + 1] << 8);
}

function readUint16BE(bytes, offset, context) {
  if (offset < 0 || offset + 2 > bytes.length) fail(`${context} is truncated`);
  return (bytes[offset] << 8) | bytes[offset + 1];
}

function readUint32LE(bytes, offset, context) {
  if (offset < 0 || offset + 4 > bytes.length) fail(`${context} is truncated`);
  return (bytes[offset]
    + bytes[offset + 1] * 0x100
    + bytes[offset + 2] * 0x10000
    + bytes[offset + 3] * 0x1000000) >>> 0;
}

function readUint32BE(bytes, offset, context) {
  if (offset < 0 || offset + 4 > bytes.length) fail(`${context} is truncated`);
  return (bytes[offset] * 0x1000000
    + bytes[offset + 1] * 0x10000
    + bytes[offset + 2] * 0x100
    + bytes[offset + 3]) >>> 0;
}

function readBothEndian16(bytes, offset, context) {
  const little = readUint16LE(bytes, offset, context);
  const big = readUint16BE(bytes, offset + 2, context);
  if (little !== big) fail(`${context} has inconsistent little- and big-endian values`);
  return little;
}

function readBothEndian32(bytes, offset, context) {
  const little = readUint32LE(bytes, offset, context);
  const big = readUint32BE(bytes, offset + 4, context);
  if (little !== big) fail(`${context} has inconsistent little- and big-endian values`);
  return little;
}

function decodeIdentifier(bytes) {
  if (bytes.length === 1 && bytes[0] === 0) return ".";
  if (bytes.length === 1 && bytes[0] === 1) return "..";
  let value = "";
  for (const byte of bytes) {
    if (byte < 0x20 || byte > 0x7e) fail("an ISO9660 filename contains unsupported characters");
    value += String.fromCharCode(byte);
  }
  return value;
}

function parseDirectoryRecord(bytes, offset, context) {
  if (offset < 0 || offset >= bytes.length) fail(`${context} record offset is invalid`);
  const length = bytes[offset];
  if (length === 0) return null;
  if (length < 34 || offset + length > bytes.length) fail(`${context} contains a truncated directory record`);

  const identifierLength = bytes[offset + 32];
  const minimumLength = 33 + identifierLength + (identifierLength % 2 === 0 ? 1 : 0);
  if (identifierLength === 0 || minimumLength > length) fail(`${context} contains an invalid directory record`);
  if (bytes[offset + 1] !== 0) fail(`${context} uses unsupported extended attributes`);
  if (bytes[offset + 26] !== 0 || bytes[offset + 27] !== 0) fail(`${context} uses unsupported interleaving`);

  const flags = bytes[offset + 25];
  if ((flags & 0x80) !== 0) fail(`${context} uses unsupported multi-extent files`);
  const volumeSequence = readBothEndian16(bytes, offset + 28, `${context} volume sequence`);
  if (volumeSequence === 0) fail(`${context} has an invalid volume sequence number`);

  return {
    length,
    extent: readBothEndian32(bytes, offset + 2, `${context} extent`),
    size: readBothEndian32(bytes, offset + 10, `${context} size`),
    directory: (flags & 0x02) !== 0,
    identifier: decodeIdentifier(bytes.subarray(offset + 33, offset + 33 + identifierLength)),
  };
}

function assertExtent(entry, volumeSpaceSize, context) {
  const blocks = Math.ceil(entry.size / LOGICAL_SECTOR_SIZE);
  if (entry.extent >= volumeSpaceSize || entry.extent + blocks > volumeSpaceSize) {
    fail(`${context} extent points outside the ISO9660 volume`);
  }
}

function parsePrimaryVolumeDescriptor(file, layout) {
  const { pvd } = layout;
  if (pvd[0] !== 1 || !hasIsoSignature(pvd)) fail("the primary volume descriptor is invalid");
  const logicalBlockSize = readBothEndian16(pvd, 128, "logical block size");
  if (logicalBlockSize !== LOGICAL_SECTOR_SIZE) {
    fail(`unsupported ISO9660 logical block size ${logicalBlockSize}`);
  }
  const volumeSpaceSize = readBothEndian32(pvd, 80, "volume size");
  if (volumeSpaceSize === 0 || volumeSpaceSize > sectorCount(file, layout)) {
    fail("the ISO9660 volume size exceeds the selected file");
  }
  const root = parseDirectoryRecord(pvd, 156, "root directory");
  if (!root?.directory || root.identifier !== ".") fail("the primary volume descriptor has no valid root directory");
  assertExtent(root, volumeSpaceSize, "root directory");
  return { root, volumeSpaceSize };
}

async function readExtent(file, layout, entry, volumeSpaceSize, context) {
  if (entry.size > MAX_DIRECTORY_BYTES) fail(`${context} is unreasonably large`);
  assertExtent(entry, volumeSpaceSize, context);
  const output = new Uint8Array(entry.size);
  let written = 0;
  let lba = entry.extent;
  while (written < output.length) {
    const sector = await readLogicalSector(file, layout, lba);
    const count = Math.min(sector.length, output.length - written);
    output.set(sector.subarray(0, count), written);
    written += count;
    lba += 1;
  }
  return output;
}

function parseDirectory(bytes, context) {
  const records = [];
  let offset = 0;
  while (offset < bytes.length) {
    const length = bytes[offset];
    if (length === 0) {
      offset = Math.min(bytes.length, (Math.floor(offset / LOGICAL_SECTOR_SIZE) + 1) * LOGICAL_SECTOR_SIZE);
      continue;
    }
    const bytesLeftInSector = LOGICAL_SECTOR_SIZE - (offset % LOGICAL_SECTOR_SIZE);
    if (length > bytesLeftInSector) fail(`${context} has a directory record crossing a sector boundary`);
    const record = parseDirectoryRecord(bytes, offset, context);
    records.push(record);
    offset += record.length;
  }
  return records;
}

function withoutIsoVersion(identifier) {
  return identifier.replace(/;[0-9]+$/i, "");
}

function normalizeStreamName(identifier) {
  const value = withoutIsoVersion(identifier);
  return STREAM_NAME_PATTERN.test(value) ? value.toLowerCase() : null;
}

async function findStreamEntries(file, layout, root, volumeSpaceSize) {
  const rootBytes = await readExtent(file, layout, root, volumeSpaceSize, "root directory");
  const rootRecords = parseDirectory(rootBytes, "root directory");
  const streams = [];
  const seen = new Set();

  for (const directoryName of STREAM_DIRECTORIES) {
    const matches = rootRecords.filter((record) => record.directory
      && withoutIsoVersion(record.identifier).toUpperCase() === directoryName);
    if (matches.length !== 1) fail(`expected exactly one /${directoryName} directory`);
    const directory = matches[0];
    assertExtent(directory, volumeSpaceSize, `/${directoryName}`);
    const bytes = await readExtent(file, layout, directory, volumeSpaceSize, `/${directoryName}`);
    for (const record of parseDirectory(bytes, `/${directoryName}`)) {
      if (record.identifier === "." || record.identifier === "..") continue;
      const name = normalizeStreamName(record.identifier);
      if (!name) continue;
      if (record.directory) fail(`/${directoryName}/${record.identifier} is unexpectedly a directory`);
      if (record.size === 0) fail(`/${directoryName}/${record.identifier} is empty`);
      assertExtent(record, volumeSpaceSize, `/${directoryName}/${record.identifier}`);
      if (seen.has(name)) fail(`duplicate stream filename ${name}`);
      seen.add(name);
      streams.push({ ...record, name });
    }
  }

  streams.sort((left, right) => left.name.localeCompare(right.name));
  if (streams.length === 0) fail("the /S0–/S3 directories contain no NSD/NSF stream files");
  return streams;
}

async function assertRawExtentEndpoints(file, layout, entry) {
  if (!layout.raw) return;
  const blocks = Math.ceil(entry.size / LOGICAL_SECTOR_SIZE);
  const endpoints = blocks === 1 ? [entry.extent] : [entry.extent, entry.extent + blocks - 1];
  for (const lba of endpoints) {
    const offset = lba * RAW_SECTOR_SIZE;
    const header = await readBlobBytes(file, offset, RAW_USER_DATA_OFFSET, `sector ${lba}`);
    assertRawMode2Header(header, 0, lba);
  }
}

function createExtractedFile(parts, name, source) {
  const options = { type: "application/octet-stream" };
  if (Number.isFinite(source.lastModified)) options.lastModified = source.lastModified;
  if (typeof File === "function") return new File(parts, name, options);
  return new Blob(parts, options);
}

function blobPartsForExtent(file, layout, entry) {
  if (!layout.raw) {
    const start = entry.extent * LOGICAL_SECTOR_SIZE;
    assertByteRange(file, start, entry.size, entry.name);
    return [file.slice(start, start + entry.size)];
  }

  const parts = [];
  let remaining = entry.size;
  let lba = entry.extent;
  while (remaining > 0) {
    const byteCount = Math.min(LOGICAL_SECTOR_SIZE, remaining);
    const start = lba * RAW_SECTOR_SIZE + RAW_USER_DATA_OFFSET;
    assertByteRange(file, start, byteCount, entry.name);
    parts.push(file.slice(start, start + byteCount));
    remaining -= byteCount;
    lba += 1;
  }
  return parts;
}

async function report(onProgress, progress) {
  if (typeof onProgress === "function") await onProgress(progress);
}

/**
 * Extract Crash Bandicoot NSD/NSF files from a local ISO or raw PS1 data track.
 * File bytes remain backed by Blob slices; the complete disc is never read into memory.
 *
 * @param {File|Blob} file
 * @param {(progress: {phase: string, loaded: number, total: number, files: number, message: string}) => void|Promise<void>} [onProgress]
 * @returns {Promise<Map<string, File|Blob>>}
 */
export async function extractCrashStreamsFromDisc(file, onProgress) {
  assertBlobLike(file);
  await report(onProgress, {
    phase: "detecting", loaded: 0, total: file.size, files: 0, message: "Detecting disc image format…",
  });

  const layout = await detectLayout(file);
  const { root, volumeSpaceSize } = parsePrimaryVolumeDescriptor(file, layout);
  await report(onProgress, {
    phase: "scanning", loaded: 0, total: file.size, files: 0, message: `Reading ${layout.label} ISO9660 directories…`,
  });
  const entries = await findStreamEntries(file, layout, root, volumeSpaceSize);
  const total = entries.reduce((sum, entry) => sum + entry.size, 0);
  const output = new Map();
  let loaded = 0;

  for (const entry of entries) {
    await assertRawExtentEndpoints(file, layout, entry);
    const extracted = createExtractedFile(blobPartsForExtent(file, layout, entry), entry.name, file);
    if (extracted.size !== entry.size) fail(`${entry.name} was assembled at the wrong size`);
    output.set(entry.name, extracted);
    loaded += entry.size;
    await report(onProgress, {
      phase: "extracting",
      loaded,
      total,
      files: output.size,
      message: `Preparing ${entry.name}…`,
    });
  }

  await report(onProgress, {
    phase: "complete",
    loaded: total,
    total,
    files: output.size,
    message: `Ready: ${output.size} stream files from ${layout.label}.`,
  });
  return output;
}
