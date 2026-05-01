// app.js - main frontend logic for the BGP simulator
// basically this file loads the wasm module, handles file uploads,
// runs the simulation, and draws the graph using d3
// took forever to get the wasm loading to work but it does now

// grab all the DOM elements we need up front
const annsFileInput = document.querySelector("#annsFile");
const rovFileInput = document.querySelector("#rovFile");
const targetAsnInput = document.querySelector("#targetAsn");
const runBtn = document.querySelector("#runBtn");
const downloadBtn = document.querySelector("#downloadBtn");
const statusEl = document.querySelector("#status");
const errorEl = document.querySelector("#error");
const summaryEl = document.querySelector("#summary");
const statusDot = document.querySelector("#statusDot");

let simModule = null; // will be set once the wasm loads
let lastFilteredCsv = ""; // save the last output so the download button can use it

init(); // run init right away when the page loads

// init - loads the wasm module when the page first opens
// BGPSimModule comes from bgp_sim.js which emscripten generates when we compile
async function init() {
  try {
    statusEl.textContent = "Loading simulator...";
    simModule = await BGPSimModule(); // this is async for some reason, have to await it
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

// when the user clicks Run:
// 1. validate everything
// 2. load the as-rel file from the server
// 3. read the uploaded csvs
// 4. call the simulator
// 5. filter the results and draw the graph
runBtn.addEventListener("click", async () => {
  clearError();

  // wasm might not be ready yet if the page just loaded
  if (!simModule) {
    showError("Simulator is still loading.");
    return;
  }

  if (!annsFileInput.files[0]) {
    showError("Please upload an announcements CSV.");
    return;
  }

  // asn has to be a positive integer, not 0 or negative or a decimal
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

    // fetch the as relationship data - this is the CAIDA dataset
    // it tells us which ASes are customers/providers/peers of each other
    const asRelText = await fetch("./20260401.as-rel2.txt").then((response) => {
      if (!response.ok) {
        throw new Error("Could not load AS relationship data.");
      }
      return response.text();
    });

    const annsText = await annsFileInput.files[0].text();
    const rovText = rovFileInput.files[0] ? await rovFileInput.files[0].text() : ""; // rov is optional

    // make sure the csv is formatted correctly before we send it to the simulator
    validateAnnouncementsCsv(annsText);

    // call the actual C++ simulator through wasm
    // it returns a csv string, or starts with "ERROR:" if something went wrong
    const rawOutput = simModule.runSimulation(asRelText, annsText, rovText);

    if (rawOutput.startsWith("ERROR:")) {
      throw new Error(rawOutput);
    }

    // only show rows for the target asn the user typed in
    const filteredRows = parseOutputCsv(rawOutput).filter((row) => Number(row.asn) === targetAsn);

    renderGraph(filteredRows, targetAsn);
    lastFilteredCsv = buildFilteredCsv(filteredRows); // save for download
    downloadBtn.disabled = filteredRows.length === 0;
    statusEl.textContent = "Simulation completed.";
    summaryEl.textContent = filteredRows.length === 0
      ? "No announcements reached the selected ASN."
      : `${filteredRows.length} announcement(s) reached ASN ${targetAsn}.`;
  } catch (error) {
    renderGraph([], targetAsn);
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

// checks that the announcements csv looks right before we run the simulation
// the C++ side is pretty strict about the format so we catch bad files early
// expected: asn,prefix,rov_invalid as the header, then data rows
function validateAnnouncementsCsv(text) {
  const lines = text.trim().split(/\r?\n/);
  if (lines.length < 2) {
    throw new Error("Announcements CSV must include a header and at least one data row.");
  }

  // has to be exactly this, the c++ parser doesn't handle anything else
  const header = lines[0].trim().toLowerCase();
  if (header !== "asn,prefix,rov_invalid") {
    throw new Error("Announcements CSV header must be exactly: asn,prefix,rov_invalid");
  }
}

// parses the csv output from the simulator into a list of row objects
// can't just do line.split(",") because the as_path column has commas inside it
// e.g.: 701,1.2.3.0/24,"(701,3356,15169)" - that middle quoted part breaks naive splitting
// so instead we manually find the first two commas and slice from there
function parseOutputCsv(text) {
  const lines = text.trim().split(/\r?\n/);
  const rows = [];

  for (let i = 1; i < lines.length; i += 1) { // start at 1 to skip header
    const line = lines[i];
    if (!line) {
      continue;
    }

    // find the first two commas manually
    const firstComma = line.indexOf(",");
    const secondComma = line.indexOf(",", firstComma + 1);

    if (firstComma === -1 || secondComma === -1) {
      continue; // skip malformed lines
    }

    const asn = line.slice(0, firstComma);
    const prefix = line.slice(firstComma + 1, secondComma);
    let asPath = line.slice(secondComma + 1).trim();

    // remove surrounding quotes if there are any
    if (asPath.startsWith("\"") && asPath.endsWith("\"")) {
      asPath = asPath.slice(1, -1);
    }

    rows.push({ asn, prefix, asPath });
  }

  return rows;
}

// takes the filtered rows and converts them back into a csv string for download
// need to quote the as_path since it has commas in it
function buildFilteredCsv(rows) {
  const lines = ["asn,prefix,as_path"];
  for (const row of rows) {
    lines.push(`${row.asn},${row.prefix},"${row.asPath}"`);
  }
  return lines.join("\n");
}

// renders the simulation results as a force-directed graph using d3
// each node is an AS number, edges connect ASes that are adjacent in a path
// node colors:
//   blue  = the target ASN the user searched for
//   green = origin AS (where the route was announced from)
//   gray  = transit AS (just passing the route along)
// TODO: maybe add a way to highlight a specific prefix?
function renderGraph(rows, targetAsn) {
  const container = document.querySelector("#graph");
  const legendEl = document.querySelector("#legend");
  container.innerHTML = ""; // clear the old graph first

  if (rows.length === 0) {
    container.innerHTML = `<p class="empty">No matching rows for that ASN.</p>`;
    legendEl.hidden = true;
    return;
  }

  const nodeMap = new Map(); // asn -> node, so we don't create duplicates
  const linkSet = new Set(); // keep track of edges we've already added
  const links = [];

  // small helper so we don't have to keep checking if a node exists
  function ensureNode(asn) {
    if (!nodeMap.has(asn)) nodeMap.set(asn, { id: asn, type: "transit" });
    return nodeMap.get(asn);
  }

  // go through each row and build up all the nodes and edges
  for (const row of rows) {
    const path = parseAsPath(row.asPath);
    if (path.length === 0) continue;
    for (let i = 0; i < path.length; i++) ensureNode(path[i]);
    const originNode = nodeMap.get(path[path.length - 1]); // last AS in path = origin
    if (originNode.type !== "target") originNode.type = "origin";
    for (let i = 0; i < path.length - 1; i++) {
      // store edge keys as "smaller-larger" so we don't add the same edge twice
      const a = Math.min(path[i], path[i + 1]);
      const b = Math.max(path[i], path[i + 1]);
      const key = `${a}-${b}`;
      if (!linkSet.has(key)) {
        linkSet.add(key);
        links.push({ source: path[i], target: path[i + 1] });
      }
    }
  }

  // mark the target node last so origin nodes don't accidentally overwrite it
  if (nodeMap.has(targetAsn)) nodeMap.get(targetAsn).type = "target";

  const nodes = Array.from(nodeMap.values());
  // set size and font based on node type - target is biggest
  for (const node of nodes) {
    if (node.type === "target") {
      node.radius = 26;
      node.labelSize = 13;
    } else if (node.type === "origin") {
      node.radius = 20;
      node.labelSize = 12;
    } else {
      node.radius = 16;
      node.labelSize = 11;
    }

    // shrink text a little if the asn has a lot of digits so it fits in the circle
    const digits = String(node.id).length;
    if (digits >= 5) node.labelSize -= 2;
    else if (digits === 4) node.labelSize -= 1;
  }

  const width = container.clientWidth || 900;
  const height = container.clientHeight || 640;
  const colorMap = { target: "#5ea6ff", origin: "#5ed39a", transit: "#95a5b8" };

  const svg = d3.select("#graph").append("svg")
    .attr("viewBox", [0, 0, width, height])
    .attr("style", "width:100%;height:100%;");

  const defs = svg.append("defs");
  defs.append("marker")
    .attr("id", "arrow").attr("viewBox", "0 -5 10 10")
    .attr("refX", 22).attr("refY", 0)
    .attr("markerWidth", 5).attr("markerHeight", 5)
    .attr("orient", "auto")
    .append("path").attr("fill", "rgba(149,165,184,0.42)").attr("d", "M0,-5L10,0L0,5");

  // d3 force simulation - the forces kind of work like physics
  // forceLink = connected nodes attract each other
  // forceManyBody = all nodes repel each other (negative = repulsion)
  // forceCenter = pulls everything to the middle so it doesn't drift off screen
  // forceCollide = stops nodes from sitting on top of each other
  const simulation = d3.forceSimulation(nodes)
    .force("link", d3.forceLink(links).id((d) => d.id).distance(120).strength(0.33))
    .force("charge", d3.forceManyBody().strength(-380))
    .force("center", d3.forceCenter(width / 2, height / 2))
    .force("collision", d3.forceCollide((d) => d.radius + 10));

  const link = svg.append("g").selectAll("line").data(links).join("line")
    .attr("stroke", "rgba(149,165,184,0.35)")
    .attr("stroke-width", 1.25)
    .attr("marker-end", "url(#arrow)");

  const node = svg.append("g").selectAll("g").data(nodes).join("g")
    .call(d3.drag()
      .on("start", (e, d) => { if (!e.active) simulation.alphaTarget(0.3).restart(); d.fx = d.x; d.fy = d.y; })
      .on("drag", (e, d) => { d.fx = e.x; d.fy = e.y; })
      .on("end", (e, d) => { if (!e.active) simulation.alphaTarget(0); d.fx = null; d.fy = null; }));

  node.append("circle")
    .attr("r", (d) => d.radius)
    .attr("fill", (d) => colorMap[d.type])
    .attr("fill-opacity", (d) => d.type === "transit" ? 0.8 : 0.96)
    .attr("stroke", "rgba(16,23,31,0.95)")
    .attr("stroke-width", (d) => d.type === "target" ? 2.4 : 1.15);

  node.append("text")
    .text((d) => d.id)
    .attr("text-anchor", "middle").attr("dy", "0.35em")
    .attr("fill", "#ffffff")
    .attr("font-family", "SF Mono, Fira Code, monospace")
    .attr("font-size", (d) => `${d.labelSize}px`)
    .attr("font-weight", "800")
    .attr("paint-order", "stroke")
    .attr("stroke", "rgba(0,0,0,0.35)")
    .attr("stroke-width", "1.5px")
    .attr("pointer-events", "none");

  const tooltip = d3.select("#graph").append("div").attr("class", "graph-tooltip").style("opacity", 0);

  node
    .on("mouseover", (event, d) => {
      const prefixes = rows
        .filter((r) => parseAsPath(r.asPath).includes(d.id))
        .map((r) => r.prefix);
      const prefixHtml = prefixes.length <= 5
        ? prefixes.map((p) => `<span style="color:#9fb1c4">${escapeHtml(p)}</span>`).join("<br>")
        : `${prefixes.length} prefixes`;
      tooltip.style("opacity", 1)
        .html(`<strong>AS${d.id}</strong><br>${d.type} &bull; ${prefixes.length} prefix(es)<br>${prefixHtml}`)
        .style("left", (event.offsetX + 14) + "px")
        .style("top", (event.offsetY - 10) + "px");
    })
    .on("mouseout", () => tooltip.style("opacity", 0));

  // every tick the simulation updates positions - we also clamp x/y
  // so nodes don't get dragged out of the visible area
  simulation.on("tick", () => {
    link
      .attr("x1", (d) => d.source.x).attr("y1", (d) => d.source.y)
      .attr("x2", (d) => d.target.x).attr("y2", (d) => d.target.y);
    node.attr("transform", (d) => {
      const edgePadding = d.radius + 6;
      const x = Math.max(edgePadding, Math.min(width - edgePadding, d.x));
      const y = Math.max(edgePadding, Math.min(height - edgePadding, d.y));
      return `translate(${x},${y})`;
    });
  });

  legendEl.hidden = false;
}

// converts something like "(701,3356,15169)" into [701, 3356, 15169]
// strips the parentheses and splits on commas, then converts to numbers
function parseAsPath(pathStr) {
  return pathStr
    .replace(/^\(|\)$/g, "") // strip outer parens
    .split(/,\s*/)
    .map((s) => s.trim())
    .filter((s) => s.length > 0)
    .map(Number)
    .filter((n) => !isNaN(n)); // filter out anything that didn't parse right
}

// shows an error message on the page
function showError(message) {
  errorEl.hidden = false;
  errorEl.textContent = message;
}

// hides the error message - called before each run so old errors go away
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