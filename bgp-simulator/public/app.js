const annsFileInput = document.querySelector("#annsFile");
const rovFileInput = document.querySelector("#rovFile");
const targetAsnInput = document.querySelector("#targetAsn");
const runBtn = document.querySelector("#runBtn");
const downloadBtn = document.querySelector("#downloadBtn");
const statusEl = document.querySelector("#status");
const errorEl = document.querySelector("#error");
const summaryEl = document.querySelector("#summary");
const resultsBody = document.querySelector("#resultsBody");
const statusDot = document.querySelector("#statusDot");

let simModule = null;
let lastFilteredCsv = "";

init();

async function init() {
  try {
    statusEl.textContent = "Loading simulator...";
    simModule = await BGPSimModule();
    runBtn.disabled = false;
    runBtn.textContent = "Run Simulation";
    statusDot.className = "status-dot ready";
    statusEl.textContent = "Simulator ready.";
  } catch (error) {
    showError("Failed to load the WebAssembly simulator.");
    statusEl.textContent = "Initialization failed.";
    statusDot.className = "status-dot error";
    console.error(error);
  }
}

runBtn.addEventListener("click", async () => {
  clearError();

  if (!simModule) {
    showError("Simulator is still loading.");
    return;
  }

  if (!annsFileInput.files[0]) {
    showError("Please upload an announcements CSV.");
    return;
  }

  const targetAsn = Number(targetAsnInput.value);
  if (!Number.isInteger(targetAsn) || targetAsn <= 0) {
    showError("Please enter a valid target ASN.");
    return;
  }

  try {
    runBtn.disabled = true;
    downloadBtn.disabled = true;
    statusEl.textContent = "Running simulation...";
    statusDot.className = "status-dot busy";

    const asRelText = await fetch("./20260401.as-rel2.txt").then((response) => {
      if (!response.ok) {
        throw new Error("Could not load AS relationship data.");
      }
      return response.text();
    });

    const annsText = await annsFileInput.files[0].text();
    const rovText = rovFileInput.files[0] ? await rovFileInput.files[0].text() : "";

    validateAnnouncementsCsv(annsText);

    const rawOutput = simModule.runSimulation(asRelText, annsText, rovText);

    if (rawOutput.startsWith("ERROR:")) {
      throw new Error(rawOutput);
    }

    const filteredRows = parseOutputCsv(rawOutput).filter((row) => Number(row.asn) === targetAsn);

    renderRows(filteredRows);
    lastFilteredCsv = buildFilteredCsv(filteredRows);
    downloadBtn.disabled = filteredRows.length === 0;
    statusEl.textContent = "Simulation completed.";
    summaryEl.textContent = filteredRows.length === 0
      ? "No announcements reached the selected ASN."
      : `${filteredRows.length} announcement(s) reached ASN ${targetAsn}.`;
  } catch (error) {
    renderEmpty("Simulation failed.");
    showError(error.message || "Something went wrong while running the simulation.");
    statusEl.textContent = "Simulation failed.";
    statusDot.className = "status-dot error";
    console.error(error);
  } finally {
    runBtn.disabled = false;
    statusDot.className = "status-dot ready";
  }
});

downloadBtn.addEventListener("click", () => {
  if (!lastFilteredCsv) {
    return;
  }

  const blob = new Blob([lastFilteredCsv], { type: "text/csv;charset=utf-8" });
  const url = URL.createObjectURL(blob);
  const link = document.createElement("a");
  link.href = url;
  link.download = "filtered_results.csv";
  link.click();
  URL.revokeObjectURL(url);
});

function validateAnnouncementsCsv(text) {
  const lines = text.trim().split(/\r?\n/);
  if (lines.length < 2) {
    throw new Error("Announcements CSV must include a header and at least one data row.");
  }

  const header = lines[0].trim().toLowerCase();
  if (header !== "asn,prefix,rov_invalid") {
    throw new Error("Announcements CSV header must be exactly: asn,prefix,rov_invalid");
  }
}

function parseOutputCsv(text) {
  const lines = text.trim().split(/\r?\n/);
  const rows = [];

  for (let i = 1; i < lines.length; i += 1) {
    const line = lines[i];
    if (!line) {
      continue;
    }

    const firstComma = line.indexOf(",");
    const secondComma = line.indexOf(",", firstComma + 1);

    if (firstComma === -1 || secondComma === -1) {
      continue;
    }

    const asn = line.slice(0, firstComma);
    const prefix = line.slice(firstComma + 1, secondComma);
    let asPath = line.slice(secondComma + 1).trim();

    if (asPath.startsWith("\"") && asPath.endsWith("\"")) {
      asPath = asPath.slice(1, -1);
    }

    rows.push({ asn, prefix, asPath });
  }

  return rows;
}

function buildFilteredCsv(rows) {
  const lines = ["asn,prefix,as_path"];
  for (const row of rows) {
    lines.push(`${row.asn},${row.prefix},"${row.asPath}"`);
  }
  return lines.join("\n");
}

function renderRows(rows) {
  if (rows.length === 0) {
    renderEmpty("No matching rows for that ASN.");
    return;
  }

  resultsBody.innerHTML = rows.map((row) => `
    <tr>
      <td>${escapeHtml(row.asn)}</td>
      <td>${escapeHtml(row.prefix)}</td>
      <td>${escapeHtml(row.asPath)}</td>
    </tr>
  `).join("");
}

function renderEmpty(message) {
  resultsBody.innerHTML = `
    <tr>
      <td colspan="3" class="empty">${escapeHtml(message)}</td>
    </tr>
  `;
}

function showError(message) {
  errorEl.hidden = false;
  errorEl.textContent = message;
}

function clearError() {
  errorEl.hidden = true;
  errorEl.textContent = "";
}

function escapeHtml(value) {
  return String(value)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll("\"", "&quot;")
    .replaceAll("'", "&#39;");
}