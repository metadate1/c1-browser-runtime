#!/usr/bin/env node

import { createHash } from "node:crypto";
import { constants as fsConstants } from "node:fs";
import { lstat, mkdir, open, stat, unlink } from "node:fs/promises";
import { join, resolve } from "node:path";

const LOGICAL_SECTOR_SIZE = 2048;
const RAW_SECTOR_SIZE = 2352;
const RAW_USER_DATA_OFFSET = 24;
const VOLUME_DESCRIPTOR_START = 16;
const MAX_VOLUME_DESCRIPTORS = 64;
const MAX_DIRECTORY_BYTES = 16 * 1024 * 1024;
const RAW_BATCH_SECTORS = 512;
const ISO_BATCH_BYTES = 1024 * 1024;
const STREAM_DIRECTORIES = ["S0", "S1", "S2", "S3"];
const STREAM_NAME_PATTERN = /^s[0-9a-f]{7}\.(?:nsd|nsf)$/i;

const ISO_LAYOUT = Object.freeze({
  label: "ISO 2048",
  sectorSize: LOGICAL_SECTOR_SIZE,
  raw: false,
});

const RAW_LAYOUT = Object.freeze({
  label: "MODE2/2352",
  sectorSize: RAW_SECTOR_SIZE,
  raw: true,
});

function fail(message) {
  throw new Error(message);
}

function usage() {
  return [
    "Usage: node scripts/extract-disc.mjs [--force] <disc.bin|disc.iso> <output-directory>",
    "",
    "Extracts only S0-S3/*.NSD and *.NSF from a legally owned Crash Bandicoot disc image.",
    "The output directory must not already exist unless --force is supplied.",
  ].join("\n");
}

function parseArguments(argv) {
  let force = false;
  const positional = [];
  for (const argument of argv) {
    if (argument === "--force") force = true;
    else if (argument === "--help" || argument === "-h") return { help: true };
    else if (argument.startsWith("-")) fail(`Unknown option: ${argument}\n\n${usage()}`);
    else positional.push(argument);
  }
  if (positional.length !== 2) fail(usage());
  return {
    help: false,
    force,
    imagePath: resolve(positional[0]),
    outputDirectory: resolve(positional[1]),
  };
}

async function readExactly(handle, buffer, position, context) {
  let offset = 0;
  while (offset < buffer.length) {
    const { bytesRead } = await handle.read(buffer, offset, buffer.length - offset, position + offset);
    if (bytesRead === 0) fail(`${context} is truncated`);
    offset += bytesRead;
  }
  return buffer;
}

async function writeExactly(handle, buffer, context) {
  let offset = 0;
  while (offset < buffer.length) {
    const { bytesWritten } = await handle.write(buffer, offset, buffer.length - offset, null);
    if (bytesWritten === 0) fail(`could not finish writing ${context}`);
    offset += bytesWritten;
  }
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

function sectorCount(imageSize, layout) {
  return Math.floor(imageSize / layout.sectorSize);
}

async function readLogicalSector(handle, imageSize, layout, lba) {
  if (!Number.isInteger(lba) || lba < 0 || lba >= sectorCount(imageSize, layout)) {
    fail(`logical sector ${lba} is outside the image`);
  }
  const sector = Buffer.allocUnsafe(layout.sectorSize);
  await readExactly(handle, sector, lba * layout.sectorSize, `sector ${lba}`);
  if (layout.raw) {
    assertRawMode2Header(sector, 0, lba);
    return sector.subarray(RAW_USER_DATA_OFFSET, RAW_USER_DATA_OFFSET + LOGICAL_SECTOR_SIZE);
  }
  return sector;
}

async function findPrimaryVolumeDescriptor(handle, imageSize, layout) {
  for (let index = 0; index < MAX_VOLUME_DESCRIPTORS; index += 1) {
    const lba = VOLUME_DESCRIPTOR_START + index;
    const descriptor = await readLogicalSector(handle, imageSize, layout, lba);
    if (!hasIsoSignature(descriptor)) fail(`${layout.label} sector ${lba} is not an ISO9660 volume descriptor`);
    if (descriptor[0] === 1) return descriptor;
    if (descriptor[0] === 255) break;
  }
  fail(`${layout.label} image has no primary ISO9660 volume descriptor`);
}

async function detectLayout(handle, imageSize) {
  const attempts = [];
  for (const layout of [RAW_LAYOUT, ISO_LAYOUT]) {
    if (imageSize < (VOLUME_DESCRIPTOR_START + 1) * layout.sectorSize) continue;
    try {
      const pvd = await findPrimaryVolumeDescriptor(handle, imageSize, layout);
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

function parsePrimaryVolumeDescriptor(imageSize, layout) {
  const { pvd } = layout;
  if (pvd[0] !== 1 || !hasIsoSignature(pvd)) fail("the primary volume descriptor is invalid");
  const logicalBlockSize = readBothEndian16(pvd, 128, "logical block size");
  if (logicalBlockSize !== LOGICAL_SECTOR_SIZE) fail(`unsupported ISO9660 logical block size ${logicalBlockSize}`);
  const volumeSpaceSize = readBothEndian32(pvd, 80, "volume size");
  if (volumeSpaceSize === 0 || volumeSpaceSize > sectorCount(imageSize, layout)) {
    fail("the ISO9660 volume size exceeds the selected file");
  }
  const root = parseDirectoryRecord(pvd, 156, "root directory");
  if (!root?.directory || root.identifier !== ".") fail("the primary volume descriptor has no valid root directory");
  assertExtent(root, volumeSpaceSize, "root directory");
  return { root, volumeSpaceSize };
}

async function readExtent(handle, imageSize, layout, entry, volumeSpaceSize, context) {
  if (entry.size > MAX_DIRECTORY_BYTES) fail(`${context} is unreasonably large`);
  assertExtent(entry, volumeSpaceSize, context);
  const output = Buffer.alloc(entry.size);
  let written = 0;
  let lba = entry.extent;
  while (written < output.length) {
    const sector = await readLogicalSector(handle, imageSize, layout, lba);
    const count = Math.min(sector.length, output.length - written);
    sector.copy(output, written, 0, count);
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

async function findStreamEntries(handle, imageSize, layout, root, volumeSpaceSize) {
  const rootBytes = await readExtent(handle, imageSize, layout, root, volumeSpaceSize, "root directory");
  const rootRecords = parseDirectory(rootBytes, "root directory");
  const streams = [];
  const seen = new Set();

  for (const directoryName of STREAM_DIRECTORIES) {
    const matches = rootRecords.filter((record) => record.directory
      && withoutIsoVersion(record.identifier).toUpperCase() === directoryName);
    if (matches.length !== 1) fail(`expected exactly one /${directoryName} directory`);
    const directory = matches[0];
    assertExtent(directory, volumeSpaceSize, `/${directoryName}`);
    const bytes = await readExtent(handle, imageSize, layout, directory, volumeSpaceSize, `/${directoryName}`);
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
  if (streams.length === 0) fail("the /S0-/S3 directories contain no NSD/NSF stream files");
  return streams;
}

function outputOpenFlags(force) {
  let flags = fsConstants.O_WRONLY | fsConstants.O_CREAT;
  flags |= force ? fsConstants.O_TRUNC : fsConstants.O_EXCL;
  if (Number.isInteger(fsConstants.O_NOFOLLOW)) flags |= fsConstants.O_NOFOLLOW;
  return flags;
}

async function writeIsoExtent(input, output, entry, hash) {
  let remaining = entry.size;
  let sourceOffset = entry.extent * LOGICAL_SECTOR_SIZE;
  while (remaining > 0) {
    const count = Math.min(ISO_BATCH_BYTES, remaining);
    const chunk = Buffer.allocUnsafe(count);
    await readExactly(input, chunk, sourceOffset, entry.name);
    await writeExactly(output, chunk, entry.name);
    hash.update(chunk);
    sourceOffset += count;
    remaining -= count;
  }
}

async function writeRawExtent(input, output, entry, hash) {
  let remaining = entry.size;
  let lba = entry.extent;
  while (remaining > 0) {
    const sectorTotal = Math.min(RAW_BATCH_SECTORS, Math.ceil(remaining / LOGICAL_SECTOR_SIZE));
    const raw = Buffer.allocUnsafe(sectorTotal * RAW_SECTOR_SIZE);
    await readExactly(input, raw, lba * RAW_SECTOR_SIZE, entry.name);
    const cookedSize = Math.min(remaining, sectorTotal * LOGICAL_SECTOR_SIZE);
    const cooked = Buffer.allocUnsafe(cookedSize);
    let cookedOffset = 0;

    for (let index = 0; index < sectorTotal && cookedOffset < cooked.length; index += 1) {
      const rawOffset = index * RAW_SECTOR_SIZE;
      assertRawMode2Header(raw, rawOffset, lba + index);
      const count = Math.min(LOGICAL_SECTOR_SIZE, cooked.length - cookedOffset);
      raw.copy(cooked, cookedOffset, rawOffset + RAW_USER_DATA_OFFSET, rawOffset + RAW_USER_DATA_OFFSET + count);
      cookedOffset += count;
    }

    await writeExactly(output, cooked, entry.name);
    hash.update(cooked);
    remaining -= cooked.length;
    lba += sectorTotal;
  }
}

async function extractEntry(input, layout, entry, outputDirectory, force) {
  const outputPath = join(outputDirectory, entry.name);
  const flags = outputOpenFlags(force);
  let output;
  let completed = false;
  try {
    output = await open(outputPath, flags, 0o600);
    const hash = createHash("sha256");
    if (layout.raw) await writeRawExtent(input, output, entry, hash);
    else await writeIsoExtent(input, output, entry, hash);
    await output.close();
    output = undefined;
    const written = await stat(outputPath);
    if (!written.isFile() || written.size !== entry.size) fail(`${entry.name} was written at the wrong size`);
    completed = true;
    return hash.digest("hex");
  } finally {
    await output?.close().catch(() => {});
    if (!completed && !force) await unlink(outputPath).catch(() => {});
  }
}

async function prepareOutputDirectory(outputDirectory, force) {
  try {
    const existing = await lstat(outputDirectory);
    if (!force) fail(`output path already exists; refusing to overwrite: ${outputDirectory}`);
    if (!existing.isDirectory() || existing.isSymbolicLink()) {
      fail(`--force output must be a real directory, not a file or symbolic link: ${outputDirectory}`);
    }
  } catch (error) {
    if (error.code !== "ENOENT") throw error;
    await mkdir(outputDirectory, { recursive: true, mode: 0o755 });
  }
}

function countCompletePairs(entries) {
  const typesByStem = new Map();
  for (const entry of entries) {
    const stem = entry.name.slice(0, -4);
    if (!typesByStem.has(stem)) typesByStem.set(stem, new Set());
    typesByStem.get(stem).add(entry.name.slice(-3));
  }
  return [...typesByStem.values()].filter((types) => types.has("nsd") && types.has("nsf")).length;
}

function formatBytes(value) {
  return new Intl.NumberFormat("en-US").format(value);
}

async function main() {
  const args = parseArguments(process.argv.slice(2));
  if (args.help) {
    console.log(usage());
    return;
  }

  const inputStat = await stat(args.imagePath);
  if (!inputStat.isFile()) fail(`disc image is not a regular file: ${args.imagePath}`);
  if (!Number.isSafeInteger(inputStat.size) || inputStat.size <= 0) fail("disc image has an invalid size");

  const input = await open(args.imagePath, "r");
  try {
    const layout = await detectLayout(input, inputStat.size);
    const { root, volumeSpaceSize } = parsePrimaryVolumeDescriptor(inputStat.size, layout);
    const entries = await findStreamEntries(input, inputStat.size, layout, root, volumeSpaceSize);
    const totalSize = entries.reduce((sum, entry) => sum + entry.size, 0);
    const pairCount = countCompletePairs(entries);

    console.error(`Detected ${layout.label}; ${entries.length} stream files, ${pairCount} complete pair${pairCount === 1 ? "" : "s"}, ${formatBytes(totalSize)} bytes.`);
    await prepareOutputDirectory(args.outputDirectory, args.force);

    const hashes = [];
    for (let index = 0; index < entries.length; index += 1) {
      const entry = entries[index];
      const digest = await extractEntry(input, layout, entry, args.outputDirectory, args.force);
      hashes.push({ name: entry.name, size: entry.size, digest });
      console.error(`[${String(index + 1).padStart(String(entries.length).length, " ")}/${entries.length}] ${entry.name}  ${formatBytes(entry.size)} bytes  sha256:${digest.slice(0, 12)}`);
    }

    const manifest = createHash("sha256");
    for (const item of hashes) manifest.update(`${item.name}\t${item.size}\t${item.digest}\n`);
    const manifestDigest = manifest.digest("hex");
    console.log(`Layout: ${layout.label}`);
    console.log(`Files: ${entries.length}`);
    console.log(`Complete pairs: ${pairCount}`);
    console.log(`Total bytes: ${totalSize}`);
    console.log(`Manifest SHA-256: ${manifestDigest}`);
    console.log(`Output: ${args.outputDirectory}`);
  } finally {
    await input.close();
  }
}

main().catch((error) => {
  console.error(`extract-disc: ${error.message}`);
  process.exitCode = 1;
});
