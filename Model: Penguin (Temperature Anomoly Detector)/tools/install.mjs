import { access } from "node:fs/promises";
import { spawnSync } from "node:child_process";
import { dirname, join } from "node:path";
import { fileURLToPath } from "node:url";

const projectRoot = join(dirname(fileURLToPath(import.meta.url)), "..");
const nodeMajor = Number.parseInt(process.versions.node.split(".")[0], 10);
const requiredFiles = [
  "dashboard/index.html",
  "dashboard/styles.css",
  "dashboard/app.js",
  "firmware/main.c",
  "firmware/CMakeLists.txt",
  "ai/baby_temp_ae.c",
  "ai/baby_temp_predictor.c",
  "ai/baby_temp_service.c",
  "model_examples/autoencoder_example.py",
  "model_examples/predictor_example.py",
];

function fail(message) {
  console.error(`Penguin setup failed: ${message}`);
  process.exit(1);
}

if (!Number.isInteger(nodeMajor) || nodeMajor < 24) {
  fail(`Node.js 24 or newer is required; detected ${process.versions.node}. Install it from https://nodejs.org/.`);
}

const missing = [];
for (const relativePath of requiredFiles) {
  try {
    await access(join(projectRoot, relativePath));
  } catch {
    missing.push(relativePath);
  }
}
if (missing.length > 0) {
  fail(`the checkout is incomplete; missing ${missing.join(", ")}`);
}

const syntaxCheck = spawnSync(process.execPath, ["--check", join(projectRoot, "dashboard", "app.js")], {
  encoding: "utf8",
});
if (syntaxCheck.status !== 0) {
  if (syntaxCheck.stdout) process.stdout.write(syntaxCheck.stdout);
  if (syntaxCheck.stderr) process.stderr.write(syntaxCheck.stderr);
  fail("dashboard JavaScript syntax validation failed");
}

console.log("Penguin setup verified successfully.");
console.log(`  Platform: ${process.platform} ${process.arch}`);
console.log(`  Node.js:  ${process.versions.node}`);
console.log(`  Project:  ${projectRoot}`);
console.log("");
console.log("Next steps:");
console.log("  1. Program the board with firmware from firmware/ (not automatic). ");
console.log("  2. Run: npm start");
console.log("  3. In Chrome or Edge, select Connect board.");

if (process.argv.includes("--verify")) {
  console.log("");
  console.log("Verification-only mode complete; no system settings were changed.");
}
