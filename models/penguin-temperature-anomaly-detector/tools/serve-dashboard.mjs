import { createReadStream } from "node:fs";
import { stat } from "node:fs/promises";
import { createServer } from "node:http";
import { spawn } from "node:child_process";
import { dirname, extname, join } from "node:path";
import { fileURLToPath } from "node:url";

const projectRoot = join(dirname(fileURLToPath(import.meta.url)), "..");
const dashboardRoot = join(projectRoot, "dashboard");
const port = Number.parseInt(process.env.PENGUIN_DASHBOARD_PORT ?? "4173", 10);
const contentTypes = {
  ".html": "text/html; charset=utf-8",
  ".css": "text/css; charset=utf-8",
  ".js": "text/javascript; charset=utf-8",
  ".png": "image/png",
};
const knownFiles = new Map([
  ["/", "index.html"],
  ["/index.html", "index.html"],
  ["/styles.css", "styles.css"],
  ["/app.js", "app.js"],
]);

function openBrowser(url) {
  if (process.env.PENGUIN_DASHBOARD_OPEN === "0") return;
  const commands = {
    darwin: ["open", [url]],
    win32: ["cmd.exe", ["/d", "/s", "/c", "start", "", url]],
  };
  const [command, args] = commands[process.platform] ?? ["xdg-open", [url]];
  const child = spawn(command, args, { detached: true, stdio: "ignore" });
  child.on("error", () => console.log(`Open this address in Chrome or Edge: ${url}`));
  child.unref();
}

const server = createServer(async (request, response) => {
  const requestedPath = new URL(request.url ?? "/", "http://localhost").pathname;
  const filename = knownFiles.get(requestedPath);
  if (!filename) {
    response.writeHead(404, { "Content-Type": "text/plain; charset=utf-8" }).end("Not found");
    return;
  }

  const file = join(dashboardRoot, filename);
  try {
    const information = await stat(file);
    response.writeHead(200, {
      "Content-Type": contentTypes[extname(file)] ?? "application/octet-stream",
      "Content-Length": information.size,
      "Cache-Control": "no-store",
    });
    createReadStream(file).pipe(response);
  } catch {
    response.writeHead(500, { "Content-Type": "text/plain; charset=utf-8" }).end("Dashboard asset unavailable");
  }
});

server.on("error", (error) => {
  console.error(`Unable to start Penguin on port ${port}: ${error.message}`);
  process.exitCode = 1;
});

server.listen(port, "127.0.0.1", () => {
  const url = `http://localhost:${port}`;
  console.log(`Penguin dashboard: ${url}`);
  openBrowser(url);
});
