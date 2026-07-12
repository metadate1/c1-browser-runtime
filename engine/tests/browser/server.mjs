import { createServer } from "node:http";
import { readFile, readdir } from "node:fs/promises";
import { dirname, extname, join, normalize, resolve } from "node:path";
import { fileURLToPath } from "node:url";

const here = dirname(fileURLToPath(import.meta.url));
const repo = resolve(here, "../../..");
const streams = resolve(process.argv[2] || "");
const port = Number(process.env.PORT || 4174);
const types = {
  ".html": "text/html",
  ".js": "text/javascript",
  ".mjs": "text/javascript",
  ".wasm": "application/wasm",
  ".map": "application/json",
};

if (!process.argv[2]) {
  console.error("Usage: node engine/tests/browser/server.mjs <extracted-stream-directory>");
  process.exit(1);
}

createServer(async (request, response) => {
  try {
    const pathname = new URL(request.url, "http://127.0.0.1").pathname;
    let path;
    if (pathname === "/") path = join(here, "harness.html");
    else if (pathname === "/harness.mjs") path = join(here, "harness.mjs");
    else if (pathname === "/audio-regression.mjs") path = join(here, "audio-regression.mjs");
    else if (pathname === "/stream-manifest.json") {
      const names = (await readdir(streams)).filter((name) => /^s[0-9a-f]{7}\.(nsd|nsf)$/.test(name)).sort();
      response.writeHead(200, { "content-type": "application/json", "cache-control": "no-store" });
      response.end(JSON.stringify(names));
      return;
    }
    else if (pathname.startsWith("/dist/")) path = join(repo, normalize(pathname));
    else if (/^\/streams\/s[0-9a-f]{7}\.(nsd|nsf)$/.test(pathname)) path = join(streams, pathname.slice(9));
    else throw new Error("not found");
    const body = await readFile(path);
    response.writeHead(200, { "content-type": types[extname(path)] || "application/octet-stream", "cache-control": "no-store" });
    response.end(body);
  }
  catch (error) {
    response.writeHead(404, { "content-type": "text/plain" });
    response.end(String(error));
  }
}).listen(port, "127.0.0.1", () => {
  console.log(`C1 renderer parity harness: http://127.0.0.1:${port}`);
});
